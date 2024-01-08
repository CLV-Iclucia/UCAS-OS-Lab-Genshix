#ifndef OS_FS_META_H
#define OS_FS_META_H
#include <common.h>
#include <pgtable.h>

#define SECTOR_SIZE 512
#define BLOCK_SIZE 4096
#define SUPER_BLOCK_SECTOR 1048576
#define FS_SIZE_GB 1

#define FILE_NAME_MAXLEN 16
#define NINODE 32
#define NDIRECT 12
#define NINDIR_1 3
#define NINDIR_2 2
#define NINDIR_3 1
#define FS_MAGIC 114514
#define MAX_INODE_ON_DISK 1024

#define FS_SIZE_BYTES (FS_SIZE_GB << 30)
#define BLOCK_MAP_BYTES (NUM_DATA_BLOCKS >> 3)
#define SECTORS_PER_BLOCK (BLOCK_SIZE / SECTOR_SIZE)
#define NUM_DATA_BLOCKS (FS_SIZE_BYTES / BLOCK_SIZE)
#define NUM_BLOCK_MAPS (BLOCK_MAP_BYTES / BLOCK_SIZE)

#define BI2SI(block_id) (SUPER_BLOCK_SECTOR + 1 + ((block_id) * SECTORS_PER_BLOCK))
#define FIRST_BMAP_BLOCK (BI2SI(0))
#define BMAP_BLOCK(offset) (FIRST_BMAP_BLOCK + (offset))
#define FIRST_ORDINARY_BLOCK (BMAP_BLOCK(NUM_DATA_BLOCKS) + 1)
#define ORIDINARY_BLOCK(offset) (FIRST_ORDINARY_BLOCK + (offset))
#define IMAP_BLOCK (FIRST_ORDINARY_BLOCK)
#define INODE_BLOCK ()
#define FIRST_DATA_BLOCK (FIRST_DATA_BLOCK)
#define DATA_BLOCK(offset) (FIRST_DATA_BLOCK + (offset))

#define FS_R (1 << 0)
#define FS_W (1 << 1)
#define FS_X (1 << 2)

static inline void ker_sd_read(char* buf, int sec_num, int start_sec) {
  sd_read(ADDR(kva2pa(KVA(buf))), sec_num, start_sec);
}

static inline void ker_sd_write(const char* buf, int sec_num, int start_sec) {
  sd_write(ADDR(kva2pa(KVA(buf))), sec_num, start_sec);
}
#endif