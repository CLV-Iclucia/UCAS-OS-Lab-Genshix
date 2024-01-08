#include <os/fs-cache.h>
#include <os/fs-defs.h>
#include <assert.h>
#include <common.h>
#include <os/fs.h>
#include <os/lock.h>
#include <pgtable.h>

disk_cache_meta_t disk_cache[DISK_CACHE_BLOCK_NUM];
char disk_cache_data[DISK_CACHE_BLOCK_NUM][BLOCK_SIZE];
spin_lock_t disk_cache_lock;  // one lock to lock them all
spin_lock_t block_map_lock;
spin_lock_t inode_map_lock;
spin_lock_t data_lock;
spin_lock_t inode_lock;
inode inodes[NINODE];

void do_mkfs(int start_sector) {
  char buf[SECTOR_SIZE];
  ker_sd_read(buf, 1, start_sector);
  super_block_t* sb = (super_block_t*)buf;
  if (sb->magic == FS_MAGIC) return;
  
}

int select_cache_block(int block_id) {
  assert(spin_lock_holding(&disk_cache_lock));
  int to_be_casted = -1;
  uint32_t oldest_time_stamp = (uint32_t)-1;
  uint32_t oldest_dirty_time_stamp = (uint32_t)-1;
  int invalid_cache_block = -1;
  int oldest_dirty_cache_block = -1;
  for (int i = 0; i < DISK_CACHE_BLOCK_NUM; i++) {
    if ((disk_cache[i].block_id == block_id) &&
        (disk_cache[i].status & DISK_CACHE_VALID)) {
      spin_lock_release(&disk_cache_lock);
      return i;
    }
    if (invalid_cache_block >= 0) continue;
    if (!(disk_cache[i].status & DISK_CACHE_VALID)) {
      invalid_cache_block = i;
      continue;
    }
    if (disk_cache[i].status & DISK_CACHE_DIRTY) {
      if (oldest_dirty_time_stamp <= disk_cache[i].time_stamp) continue;
      oldest_dirty_time_stamp = disk_cache[i].time_stamp;
      oldest_dirty_cache_block = i;
    }
    if (oldest_dirty_cache_block >= 0) continue;
    if (disk_cache[i].time_stamp < oldest_time_stamp) {
      oldest_time_stamp = disk_cache[i].time_stamp;
      to_be_casted = i;
    }
  }
  // if reach here then we can't find sector_number
  if (invalid_cache_block >= 0) return invalid_cache_block;
  if (oldest_dirty_cache_block >= 0) to_be_casted = oldest_dirty_cache_block;
  int sec_no = disk_cache[to_be_casted].block_id;
  ker_sd_write(disk_cache_data[to_be_casted], SECTORS_PER_BLOCK, sec_no);
  return to_be_casted;
}

int alloc_block() {
  static char buf[BLOCK_SIZE];
  spin_lock_acquire(&block_map_lock);
  uint32_t* bitmaps = (uint32_t*)buf;
  for (int i = 0; i < NUM_BLOCK_MAPS; i++) {
    read_disk_block(buf, BMAP_BLOCK(i));
    for (int j = 0; j < BLOCK_SIZE / sizeof(uint32_t); j++) {
      if (bitmaps[j] == -1) continue;
      int offset = fast_log(LOWBIT(~bitmaps[j]));
      spin_lock_release(&block_map_lock);
      return i * BLOCK_SIZE + j * sizeof(uint32_t) + offset;
    }
  }
  spin_lock_release(&block_map_lock);
  return -1;
}

inode* alloc_inode() {
  spin_lock_acquire(&inode_map_lock);
  int ava_idx = -1;
  for (int i = 0; i < MAX_INODE_ON_DISK / sizeof(uint32_t); i++) {
    uint32_t bitmap;
    read_from_disk(uint32_t, &bitmap, IMAP_BLOCK, i * sizeof(uint32_t));
    if (bitmap == (uint32_t)-1) continue;
    uint32_t ava_bit = LOWBIT(~bitmap);
    ava_idx = i * 32 + fast_log(ava_bit);
    bitmap |= ava_bit;
    write_to_disk(uint32_t, &bitmap, IMAP_BLOCK, (ava_idx / sizeof(uint32_t)) * sizeof(uint32_t));
    break;
  }
  if (ava_idx == -1) {
    spin_lock_release(&inode_map_lock);
    return NULL;
  }
  spin_lock_release(&inode_map_lock);
  spin_lock_acquire(&inode_lock);
  int inode_idx = -1;
  for (int i = 0; i < NINODE; i++) {
    if (inodes[i].ref) continue;
    spin_lock_acquire(&inodes[i].lock);
    inodes[i].ref = 1;
    inodes[i].id = ava_idx;
    inode_idx = i;
    break;
  }
  spin_lock_release(&inode_lock);
  return inode_idx == -1 ? NULL : inodes + inode_idx;
}

inode* get_inode(int idx) {
  spin_lock_acquire(&inode_lock);
  for (int i = 0; i < NINODE; i++) {
    if (inodes[i].id != idx) continue;
    spin_lock_acquire(&inodes[i].lock);
    spin_lock_release(&inode_lock);
    return inodes + i;
  }
  spin_lock_release(&inode_lock);
  return NULL;
}