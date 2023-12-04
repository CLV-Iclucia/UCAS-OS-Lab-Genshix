// 1. this should be compiled using --static into riscv64
// 2. it is better not to use any static data
#include "tinylibdeflate.h"
#include <asm/biosdef.h>
#include <type.h>
#include <common.h>

#define KERNEL_ENTRY_POINT 0x50202000
#define SECTOR_SIZE 512
#define SECTOR_OFFSET(loc) ((loc) & (SECTOR_SIZE - 1))
#define SECTOR_IDX(loc) ((loc) >> 9)
#define STACK_BOTTOM 0x50500000
#define ENTRY_POINT 0x54000000
#define MAX_KERNEL_SIZE 0x100000

#define NBYTES2SEC(nbytes) (((nbytes) / SECTOR_SIZE) + ((nbytes) % SECTOR_SIZE != 0))
// this function will be put at 0x52000000
// and the stack will be put at 0x50500000
// at this time no kernel or user task is loaded so we can use these locations
// decompress a compressed kernel to 0x50201000
// the compressed kernel will be temporarily put at address higher than 0x50500000 
extern struct libdeflate_decompressor global_decompressor;
int main(uint32_t start, uint32_t size) {
    // kernel offset should be aligned (just for simplification)
    if (SECTOR_OFFSET(start) != 0) {
        port_write("decompressing kernel: kernel storage not aligned\n\r");
        return 0;
    }
    // kernel should not be too large
    if (size >= MAX_KERNEL_SIZE) {
        port_write("decompressing kernel: kernel too large\n\r");
        return 0;
    }
    sd_read(STACK_BOTTOM, NBYTES2SEC(size), SECTOR_IDX(start));
    uint32_t decompressed_size = 0;
    int result = deflate_deflate_decompress(&global_decompressor, 
                  (void *)STACK_BOTTOM, size, (void*)KERNEL_ENTRY_POINT, 
                    MAX_KERNEL_SIZE, &decompressed_size); 
    if (result == 0)
        port_write("decompressing kernel succeeded\n\r");
    else {
        if (result == 1)
            port_write("decompressing kernel: data corrupted\n\r");
        else if (result == 3)
            port_write("decompressing kernel: available memory too small\n\r");
        else port_write("decompressing kernel: unexpected return result\n\r");
        return 0;
    }
    return decompressed_size;
}
