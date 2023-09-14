#include <common.h>
#include <asm.h>
#include <os/kernel.h>
#include <os/task.h>
#include <os/string.h>
#include <os/loader.h>
#include <type.h>

#define VERSION_BUF 50

uint16_t task_num;
int version = 2; // version must between 0 and 9
char buf[VERSION_BUF];

// Task info array
task_info_t tasks[TASK_MAXNUM];

static void enter_app(unsigned task_entrypoint)
{
    asm volatile(
        "jalr   %0\n\t"
        :
        :"r"(task_entrypoint)
        :"ra"
    );
}

static int bss_check(void)
{
    for (int i = 0; i < VERSION_BUF; ++i)
    {
        if (buf[i] != 0)
        {
            return 0;
        }
    }
    return 1;
}

static void init_jmptab(void)
{
    volatile long (*(*jmptab))() = (volatile long (*(*))())KERNEL_JMPTAB_BASE;

    jmptab[CONSOLE_PUTSTR]  = (long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (long (*)())port_read_ch;
    jmptab[SD_READ]         = (long (*)())sd_read;
}

static void init_task_info(void)
{
    uint32_t task_info_block_phyaddr;
    uint32_t task_info_block_size;

    uint32_t task_info_block_id;
    uint32_t task_info_block_num;
    uint32_t task_info_block_offset;

    task_info_block_phyaddr = *(uint32_t *)(0x502001fc - 0x8);
    task_info_block_size    = *(uint32_t *)(0x502001fc - 0x4);
    task_num                = *(uint16_t *)(0x502001fc + 0x2);

    task_info_block_id      = task_info_block_phyaddr / SECTOR_SIZE;
    task_info_block_num     = (task_info_block_phyaddr + task_info_block_size) / SECTOR_SIZE- task_info_block_id + 1;
    task_info_block_offset  = task_info_block_phyaddr % SECTOR_SIZE;
    //得到task_info的一系列信息，下面从sd卡读到内存中

    bios_sd_read(TASK_MEM_BASE, task_info_block_num, task_info_block_id);
    memcpy(tasks, TASK_MEM_BASE + task_info_block_offset, task_info_block_size);
    //将task_info数组拷贝到tasks数组中

    // TODO: [p1-task4] Init 'tasks' array via reading app-info sector
    // NOTE: You need to get some related arguments from bootblock first
}

/************************************************************/
/* Do not touch this comment. Reserved for future projects. */
/************************************************************/

int main(void)
{
    // Check whether .bss section is set to zero
    unsigned task_entrypoint;
    int input_valid;
    int k = 0;
    int task_id = 0;
    int check = bss_check();
    long c;

    // Init jump table provided by kernel and bios(ΦωΦ)
    init_jmptab();

    // Init task information (〃'▽'〃)
    init_task_info();

    // Output 'Hello OS!', bss check result and OS version
    char output_str[] = "bss check: _ version: _\n\r";
    char output_val[2] = {0};
    int i, output_val_pos = 0;

    output_val[0] = check ? 't' : 'f';
    output_val[1] = version + '0';
    for (i = 0; i < sizeof(output_str); ++i)
    {
        buf[i] = output_str[i];
        if (buf[i] == '_')
        {
            buf[i] = output_val[output_val_pos++];
        }
    }

    bios_putstr("Hello OS!\n\r");
    bios_putstr(buf);
    bios_putstr("$ ");
    
    /*while (1){
        long c;
        if ((c = bios_getchar()) != -1)
            bios_putchar(c);
    }*/

    while(1){
        if((c = bios_getchar()) != -1)
        {
            bios_putchar(c);

            if(c == 'z')
            {
                buf[k] = '\0';
                for(task_id = 0;task_id < task_num;task_id++)
                {
                    input_valid = strcmp(buf,tasks[task_id].task_name);
                    if(input_valid == 0)
                        break;
                }

                if(input_valid == 0)
                {
                    load_task_img(task_id);
                    task_entrypoint = tasks[task_id].task_entrypoint;
                    enter_app(task_entrypoint);
                    bios_putstr("$ ");
                }
                else
                    bios_putstr("invalid name!\n$ ");
                k = 0;
            }
            else
                buf[k++] = c;
        }
        task_id = 0;
    }

    // TODO: Load tasks by either task id [p1-task3] or task name [p1-task4],
    //   and then execute them.

    // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
    while (1)
    {
        asm volatile("wfi");
    }

    return 0;
}
