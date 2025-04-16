#pragma once
#include <cstdint>

#include "file.h"

namespace fs {
constexpr uint8_t ROOTDEV{1};
constexpr uint8_t ROOTINO{1};

auto init(int dev) -> void;
auto namei(const char *path) -> struct file::inode *;
auto iput(struct file::inode *ip) -> void;
auto ilock(struct file::inode *ip) -> void;
auto stati(struct file::inode &ip, struct file::stat &st) -> void;
auto iunlock(struct file::inode *ip) -> void;
auto readi(struct file::inode *ip, bool user_dst, uint64_t dst, uint32_t offset,
           uint32_t n) -> uint32_t;
auto writei(struct file::inode *ip, bool user_src, uint64_t src,
            uint32_t offset, uint32_t n) -> int;
auto idup(struct file::inode *ip) -> struct file::inode *;
auto nameiparent(char *path, char *name) -> struct file::inode *;
auto dir_lookup(struct file::inode *__restrict__ dp,
                const char *__restrict__ name, uint32_t *__restrict__ poff)
    -> struct file::inode *;
auto iunlockput(struct file::inode *ip) -> void;
auto ialloc(uint32_t dev, int16_t type) -> struct file::inode *;
auto iupdate(struct file::inode *ip) -> void;
auto dir_link(struct file::inode *dp, const char *name, uint32_t inum) -> int;
auto dir_link(struct file::inode *dp, const char *name, struct file::inode *other)
    -> int;
auto itrunc(struct file::inode *ip) -> void;
}  // namespace fs
