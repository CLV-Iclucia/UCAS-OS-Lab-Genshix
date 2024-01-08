#ifndef OS_FS_H
#define OS_FS_H

#include <common.h>
#include <os/fs-defs.h>
#include <os/fs-cache.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/string.h>
#include <os/task.h>
#include <os/time.h>
#include <pgtable.h>

#define LOWBIT(x) ((x) & (-(x)))

// from stackoverflow
static inline int fast_log(uint32_t x) {
// #ifdef __GNUC__
  // return __builtin_ctz(x);
// #else
  unsigned int c = 32;
  if (x) c--;
  if (x & 0x0000FFFF) c -= 16;
  if (x & 0x00FF00FF) c -= 8;
  if (x & 0x0F0F0F0F) c -= 4;
  if (x & 0x33333333) c -= 2;
  if (x & 0x55555555) c -= 1;
  return c;
// #endif
}

typedef enum file_type {
  FT_FILE,
  FT_DIR,
} file_type;

typedef struct inode {
  uint32_t direct[NDIRECT];
  spin_lock_t lock;
  uint32_t indir[NINDIR_1];
  uint32_t double_indir[NINDIR_2];
  uint32_t triple_indir;
  uint32_t size;
  file_type type;
  uint8_t id;
  uint8_t ref;
  uint8_t nlinks;
} inode;

typedef struct super_block {
  uint32_t magic; // should be FS_MAGIC
  uint32_t n_inodes;
  uint32_t block_map_offset;
  uint32_t inode_map_offset;
  uint32_t inode_offset;
  uint32_t data_offset;
} super_block_t;

static inline void dump_super_block(super_block_t* super_block, int super_block_sector) {
  char buf[SECTOR_SIZE];
  super_block_t* sb = (super_block_t*)buf;
  sb->magic = super_block->magic;
  ker_sd_write(buf, 1, super_block_sector);
}

typedef struct file {
  char name[MAX_NAME_LENGTH];
  uint8_t rights;
} file_t;

typedef struct dentry {
  char name[MAX_NAME_LENGTH];
  uint32_t inode_id;
} dentry_t;

extern spin_lock_t block_map_lock;
void do_mkfs(int super_block_sector);

int alloc_block();
static inline void free_block(int block_id) {
  static char buf[BLOCK_SIZE];
  spin_lock_acquire(&block_map_lock);
  int block_map_offset = block_id / BLOCK_SIZE;
  int bitmap_offset = (block_id - block_map_offset * BLOCK_SIZE) / sizeof(uint32_t);
  int bit_offset = block_id - block_map_offset * BLOCK_SIZE - bitmap_offset * sizeof(uint32_t);
  read_disk_block(buf, BMAP_BLOCK(block_map_offset));
  uint32_t* bitmaps = (uint32_t*)buf;
  assert((bitmaps[bitmap_offset] & (1 << bit_offset)) == 0);
  bitmaps[bitmap_offset] |= (1 << bit_offset);
  spin_lock_release(&block_map_lock);
}

inode* alloc_inode();
inode* read_inode(int id);
static inline void free_inode() {

}

void do_remove();
void do_create();
void do_file_write();
void do_file_read();
#endif