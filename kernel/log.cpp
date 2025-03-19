#include <cstdint>
#include <cstring>

#include "bio.h"
#include "fmt.h"
#include "fs.h"
#include "kernel/fs"
#include "lock.h"
#include "proc.h"

namespace log {
struct logheader {
  int n;
  int block[fs::LOGSIZE];
};

struct log {
  class lock::spinlock lock{"log"};
  uint32_t start;
  uint32_t size;
  uint32_t outstanding;  // how many FS sys calls are executing.
  uint32_t committing;   // in commit(), please wait.
  uint32_t dev;
  struct logheader lh;
};

struct log log;

auto rhead() -> void {
  auto *buf = bio::bread(log.dev, log.start);
  auto *lh = (struct logheader *)(buf->data);
  log.lh.n = lh->n;

  for (auto i{0}; i < log.lh.n; ++i) {
    log.lh.block[i] = lh->block[i];
  }

  bio::brelse(*buf);
}

auto whead() -> void {
  auto *buf = bio::bread(log.dev, log.start);
  auto *hb = (struct logheader *)(buf->data);

  hb->n = log.lh.n;
  for (auto i{0}; i < log.lh.n; ++i) {
    hb->block[i] = log.lh.block[i];
  }

  bio::bwrite(buf);
  bio::brelse(*buf);
}

auto install_trans(bool recovering) -> void {
  for (auto i{0}; i < log.lh.n; ++i) {
    auto *lbuf = bio::bread(log.dev, log.start + i + 1);
    auto *dbuf = bio::bread(log.dev, log.lh.block[i]);

    std::memmove(dbuf->data, lbuf->data, fs::BSIZE);
    bio::bwrite(dbuf);
    if (recovering) {
      bio::bwrite(dbuf);
    }

    bio::brelse(*lbuf);
    bio::brelse(*dbuf);
  }
}

auto recover() -> void {
  rhead();
  install_trans(true);
  log.lh.n = 0;
  whead();
}

auto init(int dev, struct fs::superblock &supblock) -> void {
  log.start = supblock.logstart;
  log.size = supblock.nlog;
  log.dev = dev;
  recover();
}

auto write_log() -> void {
  for (auto tail{0}; tail < log.lh.n; tail++) {
    auto *to = bio::bread(log.dev, log.start + tail + 1);  // log block
    auto *from = bio::bread(log.dev, log.lh.block[tail]);  // cache block
    std::memmove(to->data, from->data, fs::BSIZE);
    bio::bwrite(to);  // write the log
    bio::brelse(*from);
    bio::brelse(*to);
  }
}

auto commit() -> void {
  if (log.lh.n > 0) {
    write_log();
    whead();
    install_trans(false);
    log.lh.n = 0;
    whead();
  }
}

auto begin_op() -> void {
  log.lock.acquire();
  while (true) {
    if (log.committing) {
      proc::sleep(&log, log.lock);
    } else if (log.lh.n + (log.outstanding + 1) * fs::MAXOPBLOCKS >
               fs::LOGSIZE) {
      proc::sleep(&log, log.lock);
    } else {
      log.outstanding += 1;
      log.lock.release();
      break;
    }
  }
}

auto end_op() -> void {
  bool do_commit{false};

  log.lock.acquire();
  log.outstanding -= 1;
  if (log.committing) {
    fmt::panic("log::end_op: log committing");
  } else if (log.outstanding == 0) {
    do_commit = true;
    log.committing = 1;
  } else {
    proc::wakeup(&log);
  }
  log.lock.release();

  if (do_commit) {
    commit();
    log.lock.acquire();
    log.committing = 0;
    proc::wakeup(&log);
    log.lock.release();
  }
}

auto lwrite(struct bio::buf *b) -> void {
  log.lock.acquire();
  if (log.lh.n >= static_cast<int>(fs::LOGSIZE) ||
      log.lh.n >= static_cast<int>(log.size) - 1) {
    fmt::panic("too big a transaction");
  }
  if (log.outstanding < 1) {
    fmt::panic("log_write outside of trans");
  }
  auto i = 0;
  for (; i < log.lh.n; i++) {
    if (log.lh.block[i] == static_cast<int32_t>(b->blockno)) {
      // log absorption
      break;
    }
  }
  log.lh.block[i] = static_cast<int>(b->blockno);
  if (i == log.lh.n) {  // Add new block to log?
    bio::bpin(b);
    log.lh.n++;
  }
  log.lock.release();
}

}  // namespace log
