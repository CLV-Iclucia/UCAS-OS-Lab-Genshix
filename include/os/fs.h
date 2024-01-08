#ifndef OS_FS_H
#define OS_FS_H

#include <common.h>
#include <os/fs-defs.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/string.h>
#include <os/task.h>
#include <os/time.h>
#include <pgtable.h>

#define DISK_CACHE_BLOCK_NUM 16
#define DISK_CACHE_VALID 0x1
#define DISK_CACHE_DIRTY 0x2

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

typedef struct disk_cache_meta {
  int block_id;
  uint32_t time_stamp;
  uint8_t status;
} disk_cache_meta_t;

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
} inode;

typedef struct super_block {
  uint32_t magic;
} super_block;

typedef struct file {
  char name[MAX_NAME_LENGTH];
  uint8_t rights;
} file_t;

extern disk_cache_meta_t disk_cache[DISK_CACHE_BLOCK_NUM];
extern char disk_cache_data[DISK_CACHE_BLOCK_NUM][BLOCK_SIZE];
extern spin_lock_t disk_cache_lock;  // one lock to lock them all
extern spin_lock_t block_map_lock;
extern inode inodes[NINODE];
void init_fs(int super_block_sector);
int select_cache_block(int sector_number);

static inline void ker_sd_read(char* buf, int sec_num, int start_sec) {
  sd_read(ADDR(kva2pa(KVA(buf))), sec_num, start_sec);
}

static inline void ker_sd_write(const char* buf, int sec_num, int start_sec) {
  sd_write(ADDR(kva2pa(KVA(buf))), sec_num, start_sec);
}

static inline void read_disk_block(char* buf, int block_id) {
  spin_lock_acquire(&disk_cache_lock);
  int available_block_index = select_cache_block(block_id);
  disk_cache[available_block_index].time_stamp = get_ticks();
  if (disk_cache[available_block_index].block_id == block_id) {
    memcpy((void*)buf, (void*)disk_cache_data[available_block_index],
           SECTOR_SIZE);
    disk_cache[available_block_index].time_stamp = get_ticks();
    spin_lock_release(&disk_cache_lock);
    return;
  }
  ker_sd_read(disk_cache_data[available_block_index], SECTORS_PER_BLOCK,
              BI2SI(block_id));
  disk_cache[available_block_index].block_id = block_id;
  disk_cache[available_block_index].status &= ~DISK_CACHE_DIRTY;
  disk_cache[available_block_index].status |= DISK_CACHE_VALID;
  disk_cache[available_block_index].time_stamp = get_ticks();
  spin_lock_release(&disk_cache_lock);
}

static inline void write_disk_block(const char* buf, int block_id) {
  spin_lock_acquire(&disk_cache_lock);
  int available_block_index = select_cache_block(block_id);
  disk_cache[available_block_index].time_stamp = get_ticks();
  if (disk_cache[available_block_index].block_id == block_id) {
    memcpy((void*)disk_cache_data[available_block_index], (void*)buf,
           BLOCK_SIZE);
    disk_cache[available_block_index].status |= DISK_CACHE_DIRTY;
    disk_cache[available_block_index].time_stamp = get_ticks();
    spin_lock_release(&disk_cache_lock);
    return;
  }
  ker_sd_write(disk_cache_data[available_block_index], SECTORS_PER_BLOCK,
               BI2SI(block_id));
  disk_cache[available_block_index].block_id = block_id;
  disk_cache[available_block_index].status &= ~DISK_CACHE_DIRTY;
  disk_cache[available_block_index].status |= DISK_CACHE_VALID;
  disk_cache[available_block_index].time_stamp = get_ticks();
  spin_lock_release(&disk_cache_lock);
}

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
inode* get_inode(int id);
static inline void free_inode() {

}

void do_remove();
void do_create();
void do_file_write();
void do_file_read();
#endif