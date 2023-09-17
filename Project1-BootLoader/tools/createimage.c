#include <assert.h>
#include <elf.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tinylibdeflate.h"

#define IMAGE_FILE "./image"
#define ARGS "[--extended] [--vm] <bootblock> <executable-file> ..."

#define SECTOR_SIZE 512
#define BOOT_LOADER_SIG_OFFSET 0x1fe
// now we are using bytes as unit so this should take 4B
// And I want it to be word aligned
#define OS_SIZE_LOC (BOOT_LOADER_SIG_OFFSET - 6) 
#define META_OFFSET_LOC (OS_SIZE_LOC - 4)
#define USER_START_LOC (META_OFFSET_LOC - 4)
#define BOOT_LOADER_SIG_1 0x55
#define BOOT_LOADER_SIG_2 0xaa

#define NBYTES2SEC(nbytes) (((nbytes) / SECTOR_SIZE) + ((nbytes) % SECTOR_SIZE != 0))

/* TODO: [p1-task4] design your own task_info_t */
typedef struct {
  uint32_t offset;
  uint32_t name_offset;
} task_info_t;

#define TASK_MAXNUM 16
static task_info_t taskinfo[TASK_MAXNUM];
/* structure to store command line options */
static struct {
    int vm;
    int extended;
} options;

/* prototypes of local functions */
static void create_image(int nfiles, char *files[]);
static void error(char *fmt, ...);
static void read_ehdr(Elf64_Ehdr *ehdr, FILE *fp);
static void read_phdr(Elf64_Phdr *phdr, FILE *fp, int ph, Elf64_Ehdr ehdr);
static uint64_t get_entrypoint(Elf64_Ehdr ehdr);
static uint32_t get_filesz(Elf64_Phdr phdr);
static uint32_t get_memsz(Elf64_Phdr phdr);
static void write_segment(Elf64_Phdr phdr, FILE *fp, FILE *img, int *phyaddr);
static void write_padding(FILE *img, int *phyaddr, int new_phyaddr);
static void write_img_info(int nbytes_kernel, task_info_t *taskinfo, char* files[], 
                           short tasknum, FILE *img);
static void write_padding_bootblock(FILE *img, int *phyaddr, int nbytes_kernel);
static uint32_t meta_offset;
static uint32_t user_start;
int main(int argc, char **argv)
{
    char *progname = argv[0];

    /* process command line options */
    options.vm = 0;
    options.extended = 0;
    while ((argc > 1) && (argv[1][0] == '-') && (argv[1][1] == '-')) {
        char *option = &argv[1][2];

        if (strcmp(option, "vm") == 0) {
            options.vm = 1;
        } else if (strcmp(option, "extended") == 0) {
            options.extended = 1;
        } else {
            error("%s: invalid option\nusage: %s %s\n", progname,
                  progname, ARGS);
        }
        argc--;
        argv++;
    }
    if (options.vm == 1) {
        error("%s: option --vm not implemented\n", progname);
    }
    if (argc < 3) {
        /* at least 3 args (createimage bootblock main) */
        error("usage: %s %s\n", progname, ARGS);
    }
    create_image(argc - 1, argv + 1);
    return 0;
}
#define BUFFER_SIZE 1000000
static char compress_input_buf[BUFFER_SIZE];
static char compress_output_buf[BUFFER_SIZE];
static void write_kernel(Elf64_Phdr phdr, uint32_t ptr, FILE *fp)
{
    if (phdr.p_memsz != 0 && phdr.p_type == PT_LOAD) {
        /* write the segment itself */
        /* NOTE: expansion of .bss should be done by kernel or runtime env! */
        if (options.extended == 1) {
            printf("\t\twriting 0x%04lx bytes to compressor buffer\n", phdr.p_filesz);
        }
        fseek(fp, phdr.p_offset, SEEK_SET);
        while (phdr.p_filesz-- > 0) {
            char c = fgetc(fp);
            compress_input_buf[ptr++] = c;
        }
    }
}
static uint32_t compressed_size = 0;
static uint32_t compress_kernel(uint32_t size) {
    printf("compressing kernel...\n");
    deflate_set_memory_allocator((void * (*)(int))malloc, free);
    struct libdeflate_compressor * compressor = deflate_alloc_compressor(1);
    
    int out_nbytes = deflate_deflate_compress(compressor, compress_input_buf, 
                                              size, compress_output_buf, BUFFER_SIZE);
    printf("original kernel size: %d\n", size);
    printf("compressed kernel size: %d\n", out_nbytes);
    return out_nbytes;
}

static void write_compressed(uint32_t size, FILE* img, int* phyaddr) {
    int ptr = 0;
    printf("0x%hx bytes of compressed kernel written to image.\n", size);
    while (size-- > 0) {
        char c = compress_output_buf[ptr++];
        fputc(c, img);
        (*phyaddr)++;
    }
}

/* 
now the image should go like this
this arrangement changes the least compared with that in task-4

description                     size/offset

--------------
bootloader code                 unknown, but much less than 512 bytes
--------------
user tasks offset               4B
--------------
meta info offset                4B
--------------
compressed kernel size          4B
--------------
padding                         2B
--------------
boot loader sig                 2B
--------------                  512 B
decompressor                    should be sector aligned
--------------
compressed kernel               compressed kernel size
--------------                  user tasks offset
task 1
--------------
task 2
--------------
...
--------------
task n
--------------
#tasks                          4B
--------------
task info array                 8n B
--------------
name region
*/
static void create_image(int nfiles, char *files[])
{
    int tasknum = nfiles - 3;
    int nbytes_kernel = 0;
    int phyaddr = 0;
    FILE *fp = NULL, *img = NULL;
    Elf64_Ehdr ehdr;
    Elf64_Phdr phdr;
    /* first read info of the kernel and compress it */
    fp = fopen("main", "r");
    assert(fp != NULL);
    read_ehdr(&ehdr, fp);
    printf("reading kernel file: 0x%04lx: main\n", ehdr.e_entry);
    for (int ph = 0; ph < ehdr.e_phnum; ph++) {
        read_phdr(&phdr, fp, ph, ehdr);
        if (phdr.p_type == PT_LOAD) {
            write_kernel(phdr, nbytes_kernel, fp);
            nbytes_kernel += get_filesz(phdr);
        }
    }
    fclose(fp);
    assert(nbytes_kernel > 0);
    compressed_size = compress_kernel(nbytes_kernel);
    /* open the image file */
    img = fopen(IMAGE_FILE, "w");
    assert(img != NULL);
    char** all_files = files;
    /* for each input file */
    uint32_t last_task_len = 0;
    for (int fidx = 0; fidx < nfiles; ++fidx) {
        int taskidx = fidx - 3;
        /* open input file */
        fp = fopen(*files, "r");
        assert(fp != NULL);
        /* read ELF header */
        read_ehdr(&ehdr, fp);
        printf("0x%04lx: %s\n", ehdr.e_entry, *files);
        int nbytes = 0;    
        if (taskidx >= 0) {
            taskinfo[taskidx].offset = phyaddr;
            // compute name_offset relative to the meta_offset
            taskinfo[taskidx].name_offset = taskidx ? 
              taskinfo[taskidx - 1].name_offset + last_task_len + 1 : 0;
            last_task_len = strlen(*files);
        }
        if (strcmp(*files, "main") == 0) {
            printf("compressed kernel starts at %hx.\n", phyaddr);
            write_compressed(compressed_size, img, &phyaddr);
            user_start = phyaddr;
            goto inc;
        }
        /* for each program header */
        for (int ph = 0; ph < ehdr.e_phnum; ph++) {
            /* read program header */
            read_phdr(&phdr, fp, ph, ehdr);
            if (phdr.p_type != PT_LOAD) continue;
            /* write segment to the image */
            write_segment(phdr, fp, img, &phyaddr);
            nbytes += get_filesz(phdr);
        }
        /* write padding bytes */
        /**
         * TODO:
         * 1. [p1-task3] do padding so that the kernel and every app program
         *  occupies the same number of sectors
         * 2. [p1-task4] only padding bootblock is allowed!
         */
        
        if (strcmp(*files, "bootblock") == 0) {
            if (phyaddr >= USER_START_LOC) {
                printf("Error: bootblock too large.\n");
                exit(-1);
            }
            write_padding_bootblock(img, &phyaddr, compressed_size);
            assert(phyaddr == SECTOR_SIZE);
        }
        if (strcmp(*files, "decompressor") == 0) {
            int new_phyaddr = NBYTES2SEC(phyaddr) * SECTOR_SIZE;
            write_padding(img, &phyaddr, new_phyaddr);
            printf("pad decompressor to 0x%hx.\n", phyaddr);
        }
inc:
        fclose(fp);
        files++;
    }
    meta_offset = phyaddr;
    printf("user tasks start at %hx.\n", user_start);
    write_img_info(nbytes_kernel, taskinfo, all_files, tasknum, img);
    fseek(img, META_OFFSET_LOC, SEEK_SET);
    fputc(*((char*)(&meta_offset)), img);
    fputc(*((char*)(&meta_offset) + 1), img);
    fputc(*((char*)(&meta_offset) + 2), img);
    fputc(*((char*)(&meta_offset) + 3), img);
    fseek(img, USER_START_LOC, SEEK_SET);
    fputc(*((char*)(&user_start)), img);
    fputc(*((char*)(&user_start) + 1), img);
    fputc(*((char*)(&user_start) + 2), img);
    fputc(*((char*)(&user_start) + 3), img);
    fseek(img, OS_SIZE_LOC, SEEK_SET);
    fclose(img);
}

static void read_ehdr(Elf64_Ehdr * ehdr, FILE * fp)
{
    int ret;

    ret = fread(ehdr, sizeof(*ehdr), 1, fp);
    assert(ret == 1);
    assert(ehdr->e_ident[EI_MAG1] == 'E');
    assert(ehdr->e_ident[EI_MAG2] == 'L');
    assert(ehdr->e_ident[EI_MAG3] == 'F');
}

static void read_phdr(Elf64_Phdr * phdr, FILE * fp, int ph,
                      Elf64_Ehdr ehdr)
{
    int ret;

    fseek(fp, ehdr.e_phoff + ph * ehdr.e_phentsize, SEEK_SET);
    ret = fread(phdr, sizeof(*phdr), 1, fp);
    assert(ret == 1);
    if (options.extended == 1) {
        printf("\tsegment %d\n", ph);
        printf("\t\toffset 0x%04lx", phdr->p_offset);
        printf("\t\tvaddr 0x%04lx\n", phdr->p_vaddr);
        printf("\t\tfilesz 0x%04lx", phdr->p_filesz);
        printf("\t\tmemsz 0x%04lx\n", phdr->p_memsz);
    }
}

static uint64_t get_entrypoint(Elf64_Ehdr ehdr)
{
    return ehdr.e_entry;
}

static uint32_t get_filesz(Elf64_Phdr phdr)
{
    return phdr.p_filesz;
}

static uint32_t get_memsz(Elf64_Phdr phdr)
{
    return phdr.p_memsz;
}
static void write_segment(Elf64_Phdr phdr, FILE *fp, FILE *img, int *phyaddr)
{
    if (phdr.p_memsz != 0 && phdr.p_type == PT_LOAD) {
        /* write the segment itself */
        /* NOTE: expansion of .bss should be done by kernel or runtime env! */
        if (options.extended == 1) {
            printf("\t\twriting 0x%04lx bytes\n", phdr.p_filesz);
        }
        fseek(fp, phdr.p_offset, SEEK_SET);
        while (phdr.p_filesz-- > 0) {
            char c = fgetc(fp);
            fputc(c, img);
            (*phyaddr)++;
        }
    }
}

static void write_padding(FILE *img, int *phyaddr, int new_phyaddr)
{
    if (options.extended == 1 && *phyaddr < new_phyaddr) {
        printf("\t\twrite 0x%04x bytes for padding\n", new_phyaddr - *phyaddr);
    }

    while (*phyaddr < new_phyaddr) {
        fputc(0, img);
        (*phyaddr)++;
    }
}

static void write_padding_bootblock(FILE *img, int *phyaddr, int nbytes_kernel) {
    if (options.extended == 1 && *phyaddr < SECTOR_SIZE) {
        printf("\t\twrite 0x%04x bytes for padding\n", SECTOR_SIZE - *phyaddr);
    }
    while (*phyaddr < SECTOR_SIZE) {
        if (*phyaddr == OS_SIZE_LOC) {
            fputc(*((char*)(&nbytes_kernel)), img);
            fputc(*((char*)(&nbytes_kernel) + 1), img);
            fputc(*((char*)(&nbytes_kernel) + 2), img);
            fputc(*((char*)(&nbytes_kernel) + 3), img);
            printf("writing kernel size to 0x%hx, the kernel takes up 0x%hx bytes\n", 
                   *phyaddr, nbytes_kernel);
            (*phyaddr) += 4;
            continue;
        }
        fputc(0, img);
        (*phyaddr)++;
    }
}


/* The user applications are stored as follows:
 * Task 1
 * Task 2
 * Task 3
 * ...
 * Task n
 * #tasks        ------------- 4 bytes
 * offset 1      ------------- 4 bytes
 * name_offset 1 ------------- 4 bytes
 * offset 2
 * name_offset 2
 * ...
 * offset n
 * name offset n
 * name 1
 * name 2
 * ...
 * */
static void write_img_info(int nbytes_kernel, task_info_t *taskinfo, char* files[],
                           short tasknum, FILE * img)
{
    // todo: [p1-task3] & [p1-task4] write image info to some certain places
    // note: os size, infomation about app-info sector(s) ...
    
    // Since I have already written the nbytes_kernel in write_padding_bootblock
    // there is no need to write it here

    // tasknum is a short, so to make it aligned I should cast it to int
    uint32_t tasknum_aligned = (uint32_t)tasknum;
    uint32_t phyaddr = meta_offset;
    printf("meta_offset is 0x%hx\n", meta_offset);
    for (int i = 0; i < tasknum; i++) {
        // name offset relative to the start
        taskinfo[i].name_offset += (sizeof(uint32_t) + sizeof(task_info_t) * tasknum);
        taskinfo[i].name_offset += meta_offset;
    }
    fputc(*((char*)(&tasknum_aligned)), img);
    fputc(*((char*)(&tasknum_aligned) + 1), img);
    fputc(*((char*)(&tasknum_aligned) + 2), img);
    fputc(*((char*)(&tasknum_aligned) + 3), img);
    phyaddr += 4;
    printf("%d user tasks\n", tasknum);
    uint32_t name_region_offset = taskinfo[0].name_offset;
    for (int i = 0; i < tasknum; i++) {
        printf("The elf of task %d starts at offset 0x%hx\n", i, taskinfo[i].offset);
        printf("The name of task %d starts at offset 0x%hx\n", i, taskinfo[i].name_offset);
    }
    for (char* p = (char*)taskinfo; phyaddr < name_region_offset; phyaddr++, p++)
        fputc(*p, img);
    printf("writing task names..\n");
    for (int i = 0; i < tasknum; i++) {
        fputs(files[i + 3], img);
        fputc('\0', img); // !!! fputs doesn't output '\0' !!!
        printf("task %d: ", i);
        puts(files[i + 3]);
    }
}

/* print an error message and exit */
static void error(char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    if (errno != 0) {
        perror(NULL);
    }
    exit(EXIT_FAILURE);
}
