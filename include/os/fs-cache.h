#ifndef OS_FS_CACHE_H
#define OS_FS_CACHE_H

#include <os/fs-defs.h>
#include <os/lock.h>
#include <os/time.h>
#include <os/string.h>

#define DISK_CACHE_BLOCK_NUM 16
#define DISK_CACHE_VALID 0x1
#define DISK_CACHE_DIRTY 0x2

typedef struct disk_cache_meta {
  int block_id;
  uint32_t time_stamp;
  uint8_t status;
} disk_cache_meta_t;

extern disk_cache_meta_t disk_cache[DISK_CACHE_BLOCK_NUM];
extern char disk_cache_data[DISK_CACHE_BLOCK_NUM][BLOCK_SIZE];
extern spin_lock_t disk_cache_lock;  // one lock to lock them all

int select_cache_block(int sector_number);
// these are used for all the blocks EXCEPT inodes
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
    ker_sd_write(disk_cache_data[available_block_index], SECTORS_PER_BLOCK,
               BI2SI(block_id));
    disk_cache[available_block_index].status |= DISK_CACHE_DIRTY;
    disk_cache[available_block_index].time_stamp = get_ticks();
    spin_lock_release(&disk_cache_lock);
    return;
  }
  memcpy((void*)disk_cache_data[available_block_index], (void*)buf, BLOCK_SIZE);
  ker_sd_write(disk_cache_data[available_block_index], SECTORS_PER_BLOCK,
               BI2SI(block_id));
  disk_cache[available_block_index].block_id = block_id;
  disk_cache[available_block_index].status &= ~DISK_CACHE_DIRTY;
  disk_cache[available_block_index].status |= DISK_CACHE_VALID;
  disk_cache[available_block_index].time_stamp = get_ticks();
  spin_lock_release(&disk_cache_lock);
}

static inline void read_disk_region(void* buf, int block_id, int offset, int size) {
  spin_lock_acquire(&disk_cache_lock);
  int available_block_index = select_cache_block(block_id);
  disk_cache[available_block_index].time_stamp = get_ticks();
  if (disk_cache[available_block_index].block_id == block_id) {
    memcpy(buf, (void*)disk_cache_data[available_block_index] + offset, size);
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

static inline void write_disk_region(const char* buf, int block_id, int offset, int size) {
  spin_lock_acquire(&disk_cache_lock);
  int available_block_index = select_cache_block(block_id);
  disk_cache[available_block_index].time_stamp = get_ticks();
  if (disk_cache[available_block_index].block_id == block_id) {
    memcpy((void*)disk_cache_data[available_block_index], (void*)buf, size);
    disk_cache[available_block_index].status |= DISK_CACHE_DIRTY;
    disk_cache[available_block_index].time_stamp = get_ticks();
    spin_lock_release(&disk_cache_lock);
    return;
  }
  memcpy((void*)disk_cache_data[available_block_index] + offset, (void*)buf, size);
  ker_sd_write(disk_cache_data[available_block_index], SECTORS_PER_BLOCK,
               BI2SI(block_id));
  disk_cache[available_block_index].block_id = block_id;
  disk_cache[available_block_index].status &= ~DISK_CACHE_DIRTY;
  disk_cache[available_block_index].status |= DISK_CACHE_VALID;
  disk_cache[available_block_index].time_stamp = get_ticks();
  spin_lock_release(&disk_cache_lock);
}

#define read_from_disk(T, ptr, block_id, offset) read_disk_region(ptr, block_id, offset, sizeof(T))
#define write_to_disk(T, ptr, block_id, offset) write_disk_region(ptr, block_id, offset, sizeof(T))

#endif