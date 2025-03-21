#pragma once
#include "fs.h"
#include "bio.h"
#include "kernel/fs"

namespace log {

auto init(int dev, struct fs::superblock &supblock) -> void;
auto lwrite(class bio::buf *b) -> void;
auto begin_op() -> void;
auto end_op() -> void;
auto lwrite(class bio::buf *b) -> void;
} // namespace log
