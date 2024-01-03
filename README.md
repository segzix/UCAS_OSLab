# Project6 - FileSystem

## Shell Commands

目前我们已经实现了路径解析的函数get_parent_dir，但还没来得及应用到所有函数内，所以只有do_link支持相对路径和绝对路径，其余都只能在当前目录下使用

`mkfs`：创建文件系统，正常情况下由于文件系统已经初始化好，所以使用该命令会报错

`statfs`：显示文件系统信息，包括魔术字，FS起始块号，inode map起始块号，block map起始块号，inode使用情况及起始块号，data block使用情况及起始块号，inode大小，dentry大小

`cd`：切换目录，比如 `cd ..`切换至上级目录

`mkdir`：创建目录，如果已存在会报错

`rmdir`：删除目录，如果不存在会报错

`ls`：打印目录项，可以加上 `-l`参数，打印inode号、文件大小、链接数、修改时间

`touch`：创建空文件

`cat`：显示文件内容

`ln`：创建硬链接，支持相对路径和绝对路径

`rmfile`：删除文件/链接，目前仅支持一个direct block，超出会报错

## Test Sets

`rwfile`: 先touch 1.txt,2.txt,然后运行改程序，会打印1.txt中的内容

`rwbigfile`: 以256KB为步长，循环32次每次写入一个数并立刻读出，如果读出的书与前面括号中的数相匹配则正确

`bigfile_persist`: 检测大文件的持久化，从硬盘中读出并与对应的数进行比较，如果出错会报"read error",同时可以检测bcache的性能

## Debug

P6的debug部分没有特别难，基本只要投入一定的时间就肯定会有回报，希望能够注意的有以下几点：

* 关于bcache的问题。由于我们采取的是写穿策略，因此注意bcache一定要随时与硬盘同步。注意每次分配出一块bcache之后，可能会被后面从硬盘上读出的块给冲掉，这点一定要注意(尽量保证强序)
* 在debug的过程中，可以用checkblock的策略来检测持久化是否做得很好
