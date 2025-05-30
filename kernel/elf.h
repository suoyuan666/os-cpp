#pragma once
#include <cstdint>

// clang-format off
constexpr uint32_t ELF_MAGIC {0x464C457FU};  // "\x7FELF" in little endian

// File header
struct elfhdr {
  uint32_t magic;
  unsigned char elf[12];
  uint16_t type;
  uint16_t machine;
  uint32_t version;
  uint64_t entry;
  uint64_t phoff;
  uint64_t shoff;
  uint32_t flags;
  uint16_t ehsize;
  uint16_t phentsize;
  uint16_t phnum;
  uint16_t shentsize;
  uint16_t shnum;
  uint16_t shstrndx;
};

struct proghdr {
  uint32_t type;
  uint32_t flags;
  uint64_t off;
  uint64_t vaddr;
  uint64_t paddr;
  uint64_t filesz;
  uint64_t memsz;
  uint64_t align;
};

constexpr uint32_t ELF_PROG_LOAD      {1};

constexpr uint32_t ELF_PROG_FLAG_EXEC  {1};
constexpr uint32_t ELF_PROG_FLAG_WRITE {2};
constexpr uint32_t ELF_PROG_FLAG_READ  {4};
