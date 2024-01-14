本项目的目录结构如下：
|--include/
目录，描述SBI提供的函数。
| |--asm/
目录，包含宏SBI_CALL的相关内容
| | |--sbiasm.h
宏SBI_CALL的定义
| | |--sbidef.h
宏SBI_CALL可供调用的接口类型定义
| |--sbi.h
SBI提供函数的C语言版本
|--bootblock
bootblock的ELF文件
|--bootblock.S
引导程序代码
|--createimage
将bootblock和kernel制作成引导块的工具
|--decompress.c
将kernel解压缩到内存特定位置
|--createimage.c
引导块工具代码
|--design_document.pdf
设计文档
|--disk
供qemu使用的内核镜像
|--elf2char
|--generateMapping
|--head.S
内核入口代码，负责准备C语言执行环境
|--image
内核镜像
|--kernel
kernel的ELF文件
|--kernel.c
内核的代码
|--Makefile
Makefile文件
|--README.txt
本文件
|--riscv.lds
链接器脚本文件

主要文件的功能：
一、bootblock.S
(1) 打印字符串"It's a bootloader..."。
(2) 将位于SD第二个扇区的压缩代码段移动至内存。
(3) 跳转到压缩程序代码的入口。
二、head.S
(1) 清空BSS段。
(2) 设置栈指针。
(3) 跳转到链接的内核或压缩程序入口
三、kernel.c
(1) 输出字符串“Hello OS”。
(2) 连续接收输入，回显输入。同时根据输入的名称确定是无效或有效，并且显示"invalid name"或是跳转到相关用户程序入口。
四、createimage.c
(1) 将bootblock和kernel结合为一个操作系统镜像，并将compress程序和用户程序全部密排在镜像文件中。
(2) 打印操作系统镜像的一些信息。
(3) 将kernel段和compress段的大小和物理位置写入镜像中特定位置供引导块执行时取用。
(4) 将用户程序的相关信息的写入镜像最后，并且将其位置与大小写入bootblock后的padding区域，来将taskinfo存储的相关信息取出来。
五、decompress.c
(1) 将kernel压缩映像先加载到内存中，并且通过该解压缩程序将内核解压缩，并将其放到0x50200000处，作为内核入口

编译指令:make all
运行过程:make run即可。输入loadboot进入，然后输入对应用户程序名称运行对应用户程序
