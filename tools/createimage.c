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
#define BOOT_LOADER_SIG_OFFSET 0x1fe
#define DECOMPRESS_SIZE_LOC (BOOT_LOADER_SIG_OFFSET - 2)
#define TASK_INFO_LOC (DECOMPRESS_SIZE_LOC - 8)
#define KERNEL_INFO_LOC (DECOMPRESS_SIZE_LOC -16)
#define BOOT_LOADER_SIG_1 0x55
#define BOOT_LOADER_SIG_2 0xaa

#define NBYTES2SEC(nbytes) (((nbytes) / SECTOR_SIZE) + ((nbytes) % SECTOR_SIZE != 0))

/* TODO: [p1-task4] design your own task_info_t */
typedef struct {
    char        task_name[32];
    uint32_t    task_block_phyaddr;
    uint32_t    task_block_size;
    uint32_t    task_entrypoint;
} task_info_t;

#define TASK_MAXNUM 16
static task_info_t taskinfo[TASK_MAXNUM];

/* structure to store command line options */
static struct {
    int vm;
    int extended;
} options;

char kernel_data_buf[0x10000];
char kernel_compress_buf[0x10000];

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
static void write_img_info(int nbytes_decompress, task_info_t *taskinfo,
                           short tasknum, FILE *img,int phyaddr,int kernel_compressed_phyaddr,int kernel_compressed_size);

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

/* TODO: [p1-task4] assign your task_info_t somewhere in 'create_image' */
static void create_image(int nfiles, char *files[])
{
    int i;
    int tasknum = nfiles - 3;
    int data_size = 0;
    struct libdeflate_compressor * compressor;
    int phyaddr = 0;
    int nbytes_decompress = 0;
    int phyaddr_old = 0;

    FILE *fp = NULL, *img = NULL;
    Elf64_Ehdr ehdr;
    Elf64_Phdr phdr;
    int kernel_compressed_phyaddr;
    int kernel_compressed_size;
    char* kernel_data_buf_temp = kernel_data_buf;

    /* open the image file */
    img = fopen(IMAGE_FILE, "w");
    assert(img != NULL);

    /* for each input file */
    for (int fidx = 0; fidx < nfiles; ++fidx) {

        int taskidx = fidx - 3;

        /* open input file */
        fp = fopen(*files, "r");
        assert(fp != NULL);

        /* read ELF header */
        read_ehdr(&ehdr, fp);
        printf("0x%04lx: %s\n", ehdr.e_entry, *files);

        /* for each program header */
        for (int ph = 0; ph < ehdr.e_phnum; ph++) {

            /* read program header */
            read_phdr(&phdr, fp, ph, ehdr);

            if (phdr.p_type != PT_LOAD) continue;

            /* write segment to the image */
            if(strcmp(*files,"main") == 0)
            {
                fseek(fp, phdr.p_offset, SEEK_SET);
                if (phdr.p_memsz != 0 && phdr.p_type == PT_LOAD){
                    fread(kernel_data_buf_temp, sizeof(char), phdr.p_filesz, fp);
                    kernel_data_buf_temp += phdr.p_filesz;
                }
                data_size += get_filesz(phdr);
            }
            else
            {
                if (strcmp(*files, "decompress") == 0) 
                    nbytes_decompress += get_filesz(phdr);
                write_segment(phdr, fp, img, &phyaddr);
            }
            //如果是kernel则先通过write_compress函数写进kernel_data_buf数组里，同时可以得到data_size
            /* update nbytes_decompress */
        }

        /* write padding bytes */
        /**
         * TODO:
         * 1. [p1-task3] do padding so that the kernel and every app program
         *  occupies the same number of sectors
         * 2. [p1-task4] only padding bootblock is allowed!
         */


        if(strcmp(*files,"main") == 0){
            printf("kernel_data_size : %d\n",data_size);
            deflate_set_memory_allocator((void * (*)(int))malloc, free);
            compressor = deflate_alloc_compressor(1);

            kernel_compressed_size = deflate_deflate_compress(compressor, kernel_data_buf, data_size, kernel_compress_buf, 0x10000);
            kernel_compressed_phyaddr = phyaddr_old;

            phyaddr = phyaddr_old + kernel_compressed_size;
            fwrite(kernel_compress_buf, sizeof(char), kernel_compressed_size, img);
            
            printf("kernel_compressed_size : %d\n",kernel_compressed_size);
            printf("kernel_compressed_phyaddr : %d\n",kernel_compressed_phyaddr);
        }
        //如果是kernel则首先打印出原本的data_size，压缩之后全部写进img文件中密排；然后打印出压缩之后的compress_size
        if (strcmp(*files, "bootblock") == 0) {
            write_padding(img, &phyaddr, SECTOR_SIZE);
        }
        /*else{
            write_padding(img, &phyaddr, SECTOR_SIZE * (1 + fidx * NBYTES2SEC(nbytes_decompress));
        }*/
        /*if (strcmp(*files, "decompress") == 0) {
            write_padding(img, &phyaddr, SECTOR_SIZE + NBYTES2SEC(nbytes_decompress) * SECTOR_SIZE);
        }*/

        if(taskidx >= 0)
        {
            strcpy(taskinfo[taskidx].task_name,*files);
            taskinfo[taskidx].task_block_phyaddr  =  phyaddr_old;
            taskinfo[taskidx].task_block_size     =  phyaddr - phyaddr_old;
            taskinfo[taskidx].task_entrypoint     =  get_entrypoint(ehdr);
        }
        //对应其他的用户程序

        phyaddr_old = phyaddr;
        printf("phyaddr_old : %d\n",phyaddr_old);
        fclose(fp);
        files++;
    }
    write_img_info(nbytes_decompress, taskinfo, tasknum, img, phyaddr,kernel_compressed_phyaddr,kernel_compressed_size);

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
            fputc(fgetc(fp), img);
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

static void write_img_info(int nbytes_decompress, task_info_t *taskinfo,
                           short tasknum, FILE *img,int phyaddr,int kernel_compressed_phyaddr,int kernel_compressed_size)
{
    uint32_t* check;
    uint32_t task_info_block_phyaddr;
    uint32_t task_info_block_size;

    uint16_t decompress_SEC_number;
    short i;
    decompress_SEC_number = NBYTES2SEC(nbytes_decompress);

    task_info_block_phyaddr     = phyaddr;
    task_info_block_size        = sizeof(task_info_t) * tasknum;

    printf("nbytes_decompress : %d\n", nbytes_decompress);
    printf("decompress_SEC_number : %d\n", decompress_SEC_number);
    printf("tasknum : %d\n", tasknum);

    for(i = 0;i < tasknum;i++)
    {
        printf("task %d\n", i);
        printf("task_name : %s\n",taskinfo[i].task_name);
        printf("task_block_phyaddr : %d\n",taskinfo[i].task_block_phyaddr);
        printf("task_block_size : %d\n",taskinfo[i].task_block_size);
        printf("task_entrypoint : %x\n",taskinfo[i].task_entrypoint);
    }

    printf("task_info_block_phyaddr : %d\n", task_info_block_phyaddr);
    printf("task_info_block_size : %d\n", task_info_block_size);
    printf("kernel_compressed_phyaddr : %d\n", kernel_compressed_phyaddr);
    printf("kernel_compressed_size : %d\n", kernel_compressed_size);

    fseek(img,task_info_block_phyaddr,SEEK_SET);
    fwrite(taskinfo,sizeof(task_info_t),tasknum,img);

    fseek(img, DECOMPRESS_SIZE_LOC, SEEK_SET);
    fwrite(&decompress_SEC_number,2,1,img);
    fwrite(&tasknum,2,1,img);

    fseek(img, TASK_INFO_LOC, SEEK_SET);
    fwrite(&task_info_block_phyaddr,4,1,img);
    fwrite(&task_info_block_size,4,1,img);

    fseek(img, KERNEL_INFO_LOC, SEEK_SET);
    fwrite(&kernel_compressed_phyaddr,4,1,img);
    fwrite(&kernel_compressed_size,4,1,img);

    /*fseek(img, 0x1ec, SEEK_SET);
    fread(check,4,1,img);
    printf("kernel_compressed_phyaddr_img : %d\n", *check);
    fread(check,4,1,img);
    printf("kernel_compressed_size_img : %d\n", *check);
    fread(check,4,1,img);
    printf("task_info_block_phyaddr_img : %d\n", *check);
    fread(check,4,1,img);
    printf("task_info_block_size_img : %d\n", *check);*/
    
    // TODO: [p1-task3] & [p1-task4] write image info to some certain places
    // NOTE: os size, infomation about app-info sector(s) ...
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
