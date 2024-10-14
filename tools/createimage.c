#include <assert.h>
#include <elf.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tinylibdeflate.h>

#define IMAGE_FILE "./image"
#define ARGS "[--extended] [--vm] <bootblock> <executable-file> ..."

#define SECTOR_SIZE 512
#define ENDBLOCK 0x200
#define DECOMPRESS_SIZE_LOC (ENDBLOCK - 2)
#define TASK_INFO_LOC (DECOMPRESS_SIZE_LOC - 10)
#define KERNEL_INFO_LOC (TASK_INFO_LOC - 8)
#define BOOT_LOADER_SIG_1 0x55
#define BOOT_LOADER_SIG_2 0xaa

#define NBYTES2SEC(nbytes) (((nbytes) / SECTOR_SIZE) + ((nbytes) % SECTOR_SIZE != 0))

/** TODO: [p1-task4] design your own task_info_t */
typedef struct {
    char task_name[32];
    uint32_t task_block_phyaddr;
    uint32_t task_block_size;
    uint32_t task_entrypoint;
} task_info_t;

#define TASK_MAXNUM 16
static task_info_t taskinfo[TASK_MAXNUM];

/** structure to store command line options */
static struct {
    int vm;
    int extended;
} options;

char kernel_data_buf[0x10000];
char kernel_compress_buf[0x10000];

/** prototypes of local functions */
static void create_image(int nfiles, char *files[]);
static void error(char *fmt, ...);
static void read_ehdr(Elf64_Ehdr *ehdr, FILE *fp);
static void read_phdr(Elf64_Phdr *phdr, FILE *fp, int ph, Elf64_Ehdr ehdr);
static uint64_t get_entrypoint(Elf64_Ehdr ehdr);
static uint32_t get_filesz(Elf64_Phdr phdr);
static uint32_t get_memsz(Elf64_Phdr phdr);
static void write_segment(Elf64_Phdr phdr, FILE *fp, FILE *img, int *phyaddr);
static void write_padding(FILE *img, int *phyaddr, int new_phyaddr);
static void write_img_info(int nbytes_decompress, task_info_t *taskinfo, short tasknum, FILE *img,
                           int phyaddr, int kernel_compressed_phyaddr, int kernel_compressed_size);

int main(int argc, char **argv) {
    char *progname = argv[0];

    /** process command line options */
    options.vm = 0;
    options.extended = 0;
    while ((argc > 1) && (argv[1][0] == '-') && (argv[1][1] == '-')) {
        char *option = &argv[1][2];

        if (strcmp(option, "vm") == 0) {
            options.vm = 1;
        } else if (strcmp(option, "extended") == 0) {
            options.extended = 1;
        } else {
            error("%s: invalid option\nusage: %s %s\n", progname, progname, ARGS);
        }
        argc--;
        argv++;
    }
    if (options.vm == 1) {
        error("%s: option --vm not implemented\n", progname);
    }
    if (argc < 3) {
        /** at least 3 args (createimage bootblock main) */
        error("usage: %s %s\n", progname, ARGS);
    }
    create_image(argc - 1, argv + 1);
    return 0;
}

/** TODO: [p1-task4] assign your task_info_t somewhere in 'create_image' */
static void create_image(int nfiles, char *files[]) {
    int tasknum = nfiles - 3;
    int data_size = 0;
    struct libdeflate_compressor *compressor;
    int phyaddr = 0;
    int nbytes_decompress = 0;
    int phyaddr_old = 0;

    FILE *fp = NULL, *img = NULL;
    Elf64_Ehdr ehdr;
    Elf64_Phdr phdr;
    int kernel_compressed_phyaddr;
    int kernel_compressed_size;
    char *kernel_data_buf_temp = kernel_data_buf;

    /** open the image file */
    img = fopen(IMAGE_FILE, "w");
    assert(img != NULL);

    /** for each input file */
    for (int fidx = 0; fidx < nfiles; ++fidx) {

        int taskidx = fidx - 3;

        /** open input file */
        fp = fopen(*files, "r");
        assert(fp != NULL);

        /** read ELF header */
        read_ehdr(&ehdr, fp);
        printf("0x%04lx: %s\n", ehdr.e_entry, *files);

        /** for each program header */
        for (int ph = 0; ph < ehdr.e_phnum; ph++) {

            /** read program header */
            read_phdr(&phdr, fp, ph, ehdr);

            if (phdr.p_type != PT_LOAD)
                continue;

            /** write segment to the image */
            /**
             * 1.如果是内核，先将其读入到数组中，在读文件结束并压缩后写到image中
             * 2.如果是压缩函数，注意统计字节数信息
             * 3.如果是测试程序，直接向image中写入段即可
             * 4.如果是bootblock，暂时不进行处理，其后用write_padding函数将其写入image中
             */
            if (!strcmp(*files, "main")) {
                fseek(fp, phdr.p_offset, SEEK_SET);
                if (get_memsz(phdr) != 0 && phdr.p_type == PT_LOAD) {
                    fread(kernel_data_buf_temp, sizeof(char), phdr.p_filesz, fp);
                    kernel_data_buf_temp += phdr.p_filesz;
                }
                data_size += get_filesz(phdr);
            } else {
                if (!strcmp(*files, "decompress"))
                    nbytes_decompress += get_filesz(phdr);
                write_segment(phdr, fp, img, &phyaddr);
            }
            /** update nbytes_decompress */
        }

        /**
         * 1.如果是内核，压缩后写到image中
         * 2.如果是压缩函数，不用再进行处理
         * 3.如果是测试程序，注意统计taskinfo信息
         * 4.如果是bootblock，调用write_padding函数将其写入image中
         */
        if (!strcmp(*files, "main")) {
            //内核压缩
            deflate_set_memory_allocator((void *(*)(int))malloc, free);
            compressor = deflate_alloc_compressor(1);
            //获取内核压缩后信息并写入image
            kernel_compressed_size = deflate_deflate_compress(
                compressor, kernel_data_buf, data_size, kernel_compress_buf, 0x10000);
            kernel_compressed_phyaddr = phyaddr_old;
            fwrite(kernel_compress_buf, sizeof(char), kernel_compressed_size, img);

            printf("kernel size: %d ", data_size);
            printf("compressed kernel size: %d ", kernel_compressed_size);
            printf("compressed kernel phyaddr: %d\n", kernel_compressed_phyaddr);
            phyaddr = phyaddr_old + kernel_compressed_size;
        }
        if (!strcmp(*files, "bootblock")) {
            write_padding(img, &phyaddr, SECTOR_SIZE);
        }
        if (taskidx >= 0) {
            strcpy(taskinfo[taskidx].task_name, *files);
            taskinfo[taskidx].task_block_phyaddr = phyaddr_old;
            taskinfo[taskidx].task_block_size = phyaddr - phyaddr_old;
            taskinfo[taskidx].task_entrypoint = get_entrypoint(ehdr);
        }

        phyaddr_old = phyaddr;
        fclose(fp);
        files++;
    }
    write_img_info(nbytes_decompress, taskinfo, tasknum, img, phyaddr, kernel_compressed_phyaddr,
                   kernel_compressed_size);

    fclose(img);
}

static void read_ehdr(Elf64_Ehdr *ehdr, FILE *fp) {
    int ret;

    ret = fread(ehdr, sizeof(*ehdr), 1, fp);
    assert(ret == 1);
    assert(ehdr->e_ident[EI_MAG1] == 'E');
    assert(ehdr->e_ident[EI_MAG2] == 'L');
    assert(ehdr->e_ident[EI_MAG3] == 'F');
}

static void read_phdr(Elf64_Phdr *phdr, FILE *fp, int ph, Elf64_Ehdr ehdr) {
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

static uint64_t get_entrypoint(Elf64_Ehdr ehdr) {
    return ehdr.e_entry;
}

static uint32_t get_filesz(Elf64_Phdr phdr) {
    return phdr.p_filesz;
}

static uint32_t get_memsz(Elf64_Phdr phdr) {
    return phdr.p_memsz;
}

static void write_segment(Elf64_Phdr phdr, FILE *fp, FILE *img, int *phyaddr) {
    if (get_memsz(phdr) != 0 && phdr.p_type == PT_LOAD) {
        /** write the segment itself */
        /** NOTE: expansion of .bss should be done by kernel or runtime env! */
        if (options.extended == 1) {
            printf("\t\twriting 0x%04lx bytes\n", phdr.p_filesz);
        }
        fseek(fp, phdr.p_offset, SEEK_SET);
        while (phdr.p_memsz-- > 0) {
            fputc(fgetc(fp), img);
            (*phyaddr)++;
        }
    }
}

static void write_padding(FILE *img, int *phyaddr, int new_phyaddr) {
    if (options.extended == 1 && *phyaddr < new_phyaddr) {
        printf("\t\twrite 0x%04x bytes for padding\n", new_phyaddr - *phyaddr);
    }

    while (*phyaddr < new_phyaddr) {
        fputc(0, img);
        (*phyaddr)++;
    }
}

//向bootblock padding部分些信息
static void write_img_info(int nbytes_decompress, task_info_t *taskinfo, short tasknum, FILE *img,
                           int phyaddr, int kernel_compressed_phyaddr, int kernel_compressed_size) {
    uint32_t task_info_block_phyaddr = phyaddr;
    uint32_t task_info_block_size = sizeof(task_info_t) * tasknum;
    uint16_t decompress_SEC_number = NBYTES2SEC(nbytes_decompress);

    //打印taskinfo信息
    printf("\ntasknum : %d\n", tasknum);
    for (short i = 0; i < tasknum; i++) {
        printf("task %d: %s\n", i, taskinfo[i].task_name);
        printf("phyaddr: %d size: %d entrypoint: %x\n", taskinfo[i].task_block_phyaddr,
               taskinfo[i].task_block_size, taskinfo[i].task_entrypoint);
    }

    //写入taskinfo信息
    fseek(img, task_info_block_phyaddr, SEEK_SET);
    fwrite(taskinfo, sizeof(task_info_t), tasknum, img);

    //写入并打印压缩函数位置信息
    fseek(img, DECOMPRESS_SIZE_LOC, SEEK_SET);
    fwrite(&decompress_SEC_number, 2, 1, img);
    printf("decompress:\n");
    printf("nbytes: %d SEC_number: %d\n", nbytes_decompress, decompress_SEC_number);

    //写入并打印taskinfo位置信息
    fseek(img, TASK_INFO_LOC, SEEK_SET);
    fwrite(&task_info_block_phyaddr, 4, 1, img);
    fwrite(&task_info_block_size, 4, 1, img);
    fwrite(&tasknum, 2, 1, img);
    printf("task_info:\n");
    printf("phyaddr: %d size: %d\n", task_info_block_phyaddr, task_info_block_size);

    //写入并打印kernel位置信息
    fseek(img, KERNEL_INFO_LOC, SEEK_SET);
    fwrite(&kernel_compressed_phyaddr, 4, 1, img);
    fwrite(&kernel_compressed_size, 4, 1, img);
    printf("compressed kernel:\n");
    printf("phyaddr: %d size: %d\n\n", kernel_compressed_phyaddr, kernel_compressed_size);
}

/** print an error message and exit */
static void error(char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    if (errno != 0) {
        perror(NULL);
    }
    exit(EXIT_FAILURE);
}
