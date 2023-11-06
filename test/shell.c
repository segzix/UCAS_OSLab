/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 *               Author : Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *                  The shell acts as a task running in user mode.
 *       The main function is to make system calls through the user's output.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#define SHELL_WIDTH 80
#define SHELL_BEGIN 10
#define SHELL_END 35
#define MAX_ARGC 5 

#define EMPTY_COMMAND 0
#define PS_COMMAND 1
#define CLEAR_COMMAND 2
#define EXEC_COMMAND 3
#define KILL_COMMAND 4
#define TASKSET_COMMAND 5

int command;
int argc;
char arg[MAX_ARGC][200]; 
char *argv[MAX_ARGC];

char buff[SHELL_WIDTH];
int print_location_x;
int print_location_y;

int getchar()
{
  int c;
  while ((c = sys_getchar()) == -1)
      ;
  return c;
}

void check_clear(){
    if(print_location_y >= SHELL_END){
        print_location_y = SHELL_BEGIN;
        sys_clear(SHELL_BEGIN + 1, SHELL_END - 1);
        sys_move_cursor(0, print_location_y);
    }
}

void parse_arg(){
    argc = 0;
    int i = 0;
    int argp = 0; 
    // int in_arg = 0;//是否在argv内部，是否有命令行参数
        
    while(1){

        if(buff[i] == '\0')
            break;
        while(buff[i] == ' ')
            i++;

        while(buff[i] != '\0' && buff[i] != ' '){
            arg[argc][argp++] = buff[i];
            i++;
        }

        arg[argc][argp] = '\0';
        argc++;
        argp = 0;
    }
        
    for (int i = 0; i < argc-1; i++){
        argv[i] = arg[i+1];
    }
    argc--;
}

void parse_command(){

    int pid;
    int mask;

    if(argc == -1)
        command = EMPTY_COMMAND;
    else if(!strcmp(arg[0], "ps"))
        command = PS_COMMAND;
    else if(!strcmp(arg[0], "clear"))
        command = CLEAR_COMMAND;
    else if(!strcmp(arg[0], "exec"))
        command = EXEC_COMMAND;
    else if(!strcmp(arg[0], "kill"))
        command = KILL_COMMAND;
    else if(!strcmp(arg[0], "taskset"))
        command = TASKSET_COMMAND;
        

    switch (command) {
        case EMPTY_COMMAND:
            break;
        case PS_COMMAND:
            if(argc > 0){

                check_clear();
                printf(" \n(QAQ)Too many arguments!\n");
                print_location_y++;
                break;
            }
            print_location_y += sys_ps();
            break; 
        case CLEAR_COMMAND:
            if(argc > 0){
                check_clear();
                printf(" (QAQ)Too many arguments!\n");
                print_location_y++;
                break;
            }

            sys_clear(SHELL_BEGIN + 1, SHELL_END - 1);
            print_location_y = SHELL_BEGIN;
            break;
        case EXEC_COMMAND:
            if((argc > 2) || (argc < 1)){
                check_clear();

                if(argc > 2)
                    printf(" \n(QAQ)Too many arguments!\n");
                else if(argc < 1)
                    printf(" \n(QAQ)Too few arguments!\n");

                print_location_y++;
                break;
            }
            pid = sys_exec(argv[0], argc, argv);

            if(pid)
                printf("\nInfo: execute %s successfully, pid = %d\n",argv[0],pid);
            else 
                printf("\nInfo: execute task name not found\n");
            print_location_y++;
            
            if (argc == 2 && (*(char*)argv[1]) != '&')
                sys_waitpid(pid);
            break;
        case KILL_COMMAND:
            if(argc > 1){
                check_clear();
                printf(" \n(QAQ)Too many arguments!\n");
                print_location_y++;
                break;
            }
            else if(argc < 1){
                check_clear();
                printf(" \n(QAQ)Too few arguments!\n");
                print_location_y++;
                break;
            }
            
            pid = atoi(argv[0]);
            if(pid == 0 || pid == 1){
                check_clear();
                printf(" \n(QAQ)Cannot kill task (pid=%d): Permission Denied!\n", pid);
                print_location_y++;
                break;
            }
            
            if(!sys_kill(pid)){
                check_clear();
                printf(" \n(QAQ)Cannot kill task (pid=%d): Task Does Not Exist!\n", pid);
                print_location_y++;
            }
            else{
                check_clear();
                printf(" \n(QAQ)Task (pid=%d) killed\n", pid);
                print_location_y++;
            }
            break;
        case TASKSET_COMMAND:
            if(!strcmp(argv[0], "-p")){
                mask = atoi(argv[1]);
                pid = atoi(argv[2]);
                sys_task_set_p(pid,mask);
            }
            else{
                mask = atoi(argv[0]);
                sys_task_set(mask,argv[1],argc,argv);
            }
            break;
        default:
            check_clear();
            printf(" \n(QAQ)Unknown Command: %s\n", arg[0]);
            print_location_y++;
            break;
    }
}

int main(void){
    int usr_command_begin = strlen("> root@UCAS_OS: ");  
    int bufp; 
    int c;
    
    sys_clear(0, SHELL_END);
    sys_move_cursor(0, SHELL_BEGIN);
    printf("---------- COMMAND -------------------\n"); 
    sys_move_cursor(0, SHELL_END);
    printf("--------------------------------------"); 
    
    print_location_y = SHELL_BEGIN;  
    while (1)
    {
        // TODO [P3-task1]: call syscall to read UART port 
        check_clear();
        
        print_location_y++;
        sys_move_cursor(0,print_location_y);
        printf("> root@UCAS_OS: "); 
        bufp = 0;
        print_location_x = usr_command_begin;
        
        while((c = getchar()) != '\r' && c != '\n' && bufp < SHELL_WIDTH){
        // TODO [P3-task1]: parse input
        // note: backspace maybe 8(' \b') or 127(delete)
            if(c == '\b' || c == 127){
                if(bufp> 0){
                    print_location_x--; 
                    bufp--;
                    sys_move_cursor(print_location_x,print_location_y); 
                    printf(" ");
                    sys_move_cursor(print_location_x, print_location_y);
                }
            }
            else{
                buff[bufp++] = (char)c; 
                print_location_x++; 
                printf("%c", (char)c);
            }
        }
        buff[bufp++]='\0';
        
        parse_arg();
        parse_command();
        // TODO [P3-task1]: ps,exec, kill, clear 
        
    }
}