#include "os/smp.h"
#include <os/fs.h>
#include <os/sched.h>
#include <os/time.h>
#include <printk.h>
#include <os/string.h>
#include <os/kernel.h>

inode_t* current_dir = 0;
int judge1;

char temp_block[BLOCK_SIZ];//每次读出8个扇区并首先暂存于此

char inode_block[BLOCK_SIZ];//每次读出的关于inode，都将先存放在这个block中，并且返回这个block中对应的inode指针
int judge;

uint32_t blockid2sectorid(uint32_t block_id){
    return (SUPER_START + block_id) << 3;
}

void init_bcache(void){
    for(int i = 0;i < BCACHE_NUM;i++)
        bcaches[i].bcache_valid = 0;
}

uint8_t* bread(uint32_t block_id){//选择要读的硬盘数据块号，读出并且返回读到的bcache地址
    for(uint32_t i = 0;i < BCACHE_NUM;i++)
        if((block_id == bcaches[i].block_id) && bcaches[i].bcache_valid){
            return bcaches[i].bcache_block;
        }
    //如果有，那么直接从bcache中读
    bios_sd_read(kva2pa((uintptr_t)bcaches[bcache_point].bcache_block), 8, blockid2sectorid(block_id));
    bcaches[bcache_point].block_id = block_id;
    bcaches[bcache_point].bcache_valid = 1;
    //如果没有先拷贝到bcache中，再返回bcache对应地址
    uint32_t temp_read_point = bcache_point;
    bcache_point = (bcache_point+1)%BCACHE_NUM;

    return bcaches[temp_read_point].bcache_block;
}

void bwrite(uint32_t block_id, uint8_t* bcache_write){
    judge = (block_id == 0);
    bios_sd_write(kva2pa((uintptr_t)bcache_write), 8, blockid2sectorid(block_id));
    return;
}
//直接将bcache中的数据写回硬盘

uint8_t* balloc(uint32_t block_id){//分配一块bcache空间并进行修改，注意这个时候是先修改然后调用bwrite来返回
//balloc的唯一作用就是初始化的时候，先在bcache中直接预留出一块位置，方便之后bread时可以直接找到
    for(uint32_t i = 0;i < BCACHE_NUM;i++)
        if((block_id == bcaches[i].block_id) && bcaches[i].bcache_valid){
            return bcaches[i].bcache_block;
        }
    //如果有，那么直接返回对应的bcache地址
    bcaches[bcache_point].block_id = block_id;
    bcaches[bcache_point].bcache_valid = 1;
    //如果没有先拷贝到bcache中，然后直接写入硬盘
    uint32_t temp_write_point = bcache_point;
    bcache_point = (bcache_point+1)%BCACHE_NUM;

    return bcaches[temp_write_point].bcache_block;
}







int check_fs(){
	superblock_t *superblock = (superblock_t *)super_block;
    bios_sd_read(kva2pa((uintptr_t)super_block), 1, blockid2sectorid(0));
	return superblock->magic == SUPER_MAGIC;    
}

int check_dir(inode_t* inode){//是目录则返回1
	if(inode->mode != INODE_DIR){
		return 0;
	}
	return 1;
}

int strcheck(char *src, char c)
{
	while(*src){
		if(*src++ == c)
			return 1;
    }
	return 0;
}

//以上是check小函数，不与下面重要函数放在一起



uint32_t countbit(uint8_t* count_block){//根据已有的count_block来读出其中有效的bit数
    uint32_t cnt = 0;

    uint64_t bit_test;
    uint64_t* bit_test_block = (uint64_t*)count_block;
    uint32_t size = BLOCK_SIZ/8;//可以分成多少个这样的数，然后进行test

    for(uint32_t i = 0;i < size;i++){
        bit_test = bit_test_block[i];

        uint32_t oneloop = 64;
        while(oneloop){
            if(bit_test & 1)
                cnt++;
            bit_test = bit_test >> 1;//如果该位为1,证明有效
            oneloop--;
        }
    }
    return cnt;
}

int32_t allocbit(char* count_block){//根据已有的count_block来读出其中还没有写进的bit位并返回对应偏移实现分配,同时也会将这一个bit位拉高
    uint32_t cnt = 0;

    uint64_t bit_test;
    uint64_t* bit_test_block = (uint64_t*)count_block;
    uint32_t size = BLOCK_SIZ/8;//可以分成多少个这样的数，然后进行test

    for(uint32_t i = 0;i < size;i++){
        bit_test = bit_test_block[i];

        uint32_t oneloop = 64;//一次bittest要循环64次才能退出这次循环
        while(oneloop){
            if(!(bit_test & 1)){//当前位为0
                bit_test |= 1;
                bit_test_block[i] = (bit_test << cnt) | bit_test_block[i];
                return cnt;
            }

            bit_test = bit_test >> 1;//如果该位为1,证明有效
            cnt++;
            oneloop--;
        }
    }

    return -1;
}

uint8_t* init_block(uint32_t block_id,int zero){//在分配一个硬盘块之前，必然会进行初始化为0
    uint8_t* zero_block = balloc(block_id);
    memset(zero_block, 0, BLOCK_SIZ);
    if(zero)
        bwrite(block_id, zero_block);
    return zero_block;
}

uint32_t alloc_block(int zero, int64_t bcache_offset){
    superblock_t *superblock = (superblock_t *) super_block;
    uint32_t block_id;
    uint32_t block_indirect_id;
    uint32_t block_indirect_offset;
    uint8_t* blockbitmap;
    uint32_t i;

    if(bcache_offset != -1){
        uint32_t bcache_id = ((uint8_t*)bcache_offset-(uint8_t*)bcaches)/sizeof(bcache_t);
        block_indirect_id = bcaches[bcache_id].block_id;
        block_indirect_offset = ((uint8_t*)bcache_offset-(uint8_t*)bcaches)%sizeof(bcache_t) - 8;//这里应该是对应的bcache_block中的偏移
    }//如果是-1代表并没有间址块，因此不用写这些
    //因为可能会被覆盖掉，因此这里先将间址块的blockid和对应的offset记下来，等到blockbitmap操作完成之后，再读出来并且完成bcache的写回

    for (i = 0; i<superblock->blockbitmap_siz; i++) {
        blockbitmap = bread(superblock->blockbitmap_offset + i);
        if((block_id = allocbit((char*)blockbitmap)) != -1)
            break;
    }//通过blockbitmap进行查找，找到可以分配的进行返回，同时也已经拉高
    bwrite(superblock->blockbitmap_offset + i, blockbitmap);

    if(bcache_offset != -1){
        uint8_t* bcache_indirect = bread(block_indirect_id);
        *(uint32_t*)(bcache_indirect + block_indirect_offset) = block_id;
        bwrite(block_indirect_id, bcache_indirect);
    }//如果是-1代表并没有间址块，因此不用写这些
    //这里有一个小点，就是说新读出的blockbitmap会不会将前面的bcacheoffset给覆盖掉呢？
    //不会，因为在前面的函数中bcacheoffset是先读出的
    //这里一定要秉承一个思想，相将分配的块的相关元数据写好，然后再进行分配！
    init_block(block_id,zero);
    return block_id;
}

int freebit(uint8_t* blockbitmap, uint32_t blockid){
    uint32_t byteid;
    uint32_t byteoffset;

    byteid = blockid/8;//看看是第几个byte
    byteoffset = blockid%8;
    blockbitmap[byteid] &= ~(1 << byteoffset);//将该位清空

    return 0;
}

int free_inodeblock(inode_t* inode){//将一个inode所有的block块全部给清空
    superblock_t *superblock = (superblock_t *) super_block;

    uint8_t* blockbitmap;//因为有4个blockbitmap，因此要根据指针对应的blockid编号来判断
    if(inode->direct[0]){
        blockbitmap = bread(superblock->blockbitmap_offset + inode->direct[0]/BLOCK_BIT_SIZ);
        freebit(blockbitmap,inode->direct[0]%BLOCK_BIT_SIZ);
        bwrite(superblock->blockbitmap_offset + inode->direct[0]/BLOCK_BIT_SIZ, blockbitmap);
    }
    if(inode->direct[1]){
        blockbitmap = bread(superblock->blockbitmap_offset + inode->direct[1]/BLOCK_BIT_SIZ);
        freebit(blockbitmap,inode->direct[1]%BLOCK_BIT_SIZ);
        bwrite(superblock->blockbitmap_offset + inode->direct[1]/BLOCK_BIT_SIZ, blockbitmap);
    }
    if(inode->direct[2]){
        blockbitmap = bread(superblock->blockbitmap_offset + inode->direct[2]/BLOCK_BIT_SIZ);
        freebit(blockbitmap,inode->direct[2]%BLOCK_BIT_SIZ);
        bwrite(superblock->blockbitmap_offset + inode->direct[2]/BLOCK_BIT_SIZ, blockbitmap);
    }
    if(inode->indirect_1){//第一层
        uint32_t* indirect1map = (uint32_t*)bread(inode->indirect_1);//这里读的不是blockbitmap，而是数据块中的间指块
        for(uint32_t i = 0;i < BLOCK_SIZ/4;i++){


            if(indirect1map[i]){//第二层
                blockbitmap = bread(superblock->blockbitmap_offset + indirect1map[i]/BLOCK_BIT_SIZ);
                freebit(blockbitmap,indirect1map[i]%BLOCK_BIT_SIZ);
                bwrite(superblock->blockbitmap_offset + indirect1map[i]/BLOCK_BIT_SIZ, blockbitmap);
            }


        }
        //如果有间接指针，那么必定要全部free掉
        blockbitmap = bread(superblock->blockbitmap_offset + inode->indirect_1/BLOCK_BIT_SIZ);
        freebit(blockbitmap, inode->indirect_1%BLOCK_BIT_SIZ);
        bwrite(superblock->blockbitmap_offset + inode->indirect_1/BLOCK_BIT_SIZ, blockbitmap);
    }
    if(inode->indirect_2){//第一层
        uint32_t* indirect2map1 = (uint32_t*)bread(inode->indirect_2);
        for(uint32_t i = 0;i < BLOCK_SIZ/4;i++){


            if(indirect2map1[i]){//第二层
                uint32_t* indirect2map2 = (uint32_t*)bread(indirect2map1[i]);
                for(uint32_t j = 0;j < BLOCK_SIZ/4;j++){


                    if(indirect2map2[j]){//第三层
                        blockbitmap = bread(superblock->blockbitmap_offset + indirect2map2[j]/BLOCK_BIT_SIZ);
                        freebit(blockbitmap,indirect2map2[j]%BLOCK_BIT_SIZ);
                        bwrite(superblock->blockbitmap_offset + indirect2map2[j]/BLOCK_BIT_SIZ, blockbitmap);
                    }


                }
                blockbitmap = bread(superblock->blockbitmap_offset + indirect2map1[i]/BLOCK_BIT_SIZ);
                freebit(blockbitmap,indirect2map1[i]%BLOCK_BIT_SIZ);
                bwrite(superblock->blockbitmap_offset + indirect2map1[i]/BLOCK_BIT_SIZ, blockbitmap);
            }


        }
        //如果有间接指针，那么必定要全部free掉
        blockbitmap = bread(superblock->blockbitmap_offset + inode->indirect_2/BLOCK_BIT_SIZ);
        freebit(blockbitmap, inode->indirect_2%BLOCK_BIT_SIZ);
        bwrite(superblock->blockbitmap_offset + inode->indirect_2/BLOCK_BIT_SIZ, blockbitmap);
    }

    return 0;
}

/*
block相关的分配函数见上
每一次分配一个block通过函数做到以下几点：
1.allocbit()负责找到可分配的块并返回对应的blockid
2.allocblock()负责返回blockid，但是在此之前需要init_block()
init_block()的主要作用在于在bcache中alloc出一段空间，这段空间不用从盘上读
balloc()只负责进行刷零和bcache对应盘的处理，作用只有一个，就是之后读和写的时候能够直接从bcache中拿到，然后再落盘
*/




uint32_t countbyte(uint8_t* count_block){//根据已有的count_block来读出其中有效的byte数
    uint32_t cnt = 0;

    for(uint32_t i = 0;i < BLOCK_SIZ;i++){
        if(count_block[i] & 1)
            cnt++;
    }

    return cnt;
}

int allocbyte(uint8_t* count_block){
    uint32_t cnt = 0;

    for(uint32_t i = 0;i < BLOCK_SIZ;i++){
        if(!count_block[i]){
            count_block[i] = 1;
            return cnt;
        }
        cnt++;
    }

    return -1;
}

unsigned alloc_inode(uint8_t mode){//returns an inode id
    pcb_t* current_running = get_pcb();

    superblock_t *superblock = (superblock_t *) super_block;
    uint32_t inode_id;
    uint32_t inodetable_id;
    uint32_t inodetable_offset;

    uint8_t* inodebytemap;
    inode_t* inodetable;

    inodebytemap = bread(superblock->inodebytemap_offset);
    inode_id = allocbyte(inodebytemap);//相当于ino号
    //分配出空闲的ino号
    bwrite(superblock->inodebytemap_offset, (uint8_t*)inodebytemap);
    //立刻落盘，新增了一位掩码

    inodetable_id = inode_id/(BLOCK_SIZ/sizeof(inode_t));
    inodetable_offset = inode_id%(BLOCK_SIZ/sizeof(inode_t));
    inodetable = (inode_t*)bread(superblock->inodetable_offset + inodetable_id);
    //算出inode号对应的inodetable

    memset(&inodetable[inodetable_offset], 0, sizeof(inode_t));
    //一定一定要先刷0！！！！就和分配出一个block一样的道理！！先刷零了再赋值！！
    inodetable[inodetable_offset].mode = mode;
    inodetable[inodetable_offset].owner_pid = current_running->pid;
    inodetable[inodetable_offset].filesz = 0;
    inodetable[inodetable_offset].mtime = get_timer();
    inodetable[inodetable_offset].fd_index = 0xff;
    bwrite(superblock->inodetable_offset + inodetable_id, (uint8_t*)inodetable);

    return inode_id;
}

int rmfile(uint32_t ino){//这个函数用于删除一个目录或者文件
//首先将这个inode的所有数据快删除，然后把这个inode给无效掉
    uint32_t dentryoffset = 0x40;
    dentry_t* dentry;
    inode_t* inode;
    superblock_t *superblock = (superblock_t *) super_block;
    inode = ino2inode_t(ino);

    if((inode->mode == INODE_DIR) && inode->filesz > 0x40){//是目录项并且有表项//表项当然不能是.和..!!
        dentry = (dentry_t*)search_datapoint(inode, dentryoffset);
        while(dentry->dentry_valid && (dentryoffset < inode->filesz)){//注意不能比filesz大的部分还找
            rmfile(dentry->ino);
            dentryoffset += 32;
            dentry = (dentry_t*)search_datapoint(inode, dentryoffset);
        }
    }
    //如果这个时候该文件有对应的文件描述符（且是文件），则应该将文件描述符置为used=0
    if (inode->mode == INODE_FILE && ((int)inode->fd_index != 0xff) && !inode->hardlinks)//必须要满足没有硬链接才释放 
        fdescs[inode->fd_index].used = 0;

    if ((inode->mode == INODE_FILE) && (inode->hardlinks != 0))//当为文件并且硬链接不为0时不能释放相应数据块，其他情况均应释放
        inode->hardlinks--;
    else {
        free_inodeblock(inode);
        //先废除掉所有数据快
        uint8_t* inodebytemap = bread(superblock->inodebytemap_offset);
        inodebytemap[ino] = 0;
        bwrite(superblock->inodebytemap_offset, inodebytemap);
        //然后把inodebytemap中的mask无效掉
    }

    return 0;
}
/*
inode相关的分配函数见上
与block基本相似，不同点在于不用init，直接分配完之后落盘即可
*/

// ino2inode_t(sector2,base_ino)
inode_t* ino2inode_t(uint32_t ino){
    uint32_t block_inodenum = BLOCK_SIZ/sizeof(inode_t);//一个block中有多少个inode

    uint32_t inodetable_id = ino / block_inodenum;//选取的inodetable_id
    uint32_t inodetable_offset = ino%block_inodenum;//对应table中的偏移
    uint8_t* inodetable;//用来记录返回的inodetable地址

    superblock_t *superblock = (superblock_t *)super_block;
    inodetable = bread(superblock->inodetable_offset + inodetable_id);

    return (inode_t *)(inodetable + inodetable_offset * sizeof(inode_t));
}

uint8_t* search_datapoint(inode_t* inode, uint32_t offset){//根据给出的偏移，直接返回对应的数据块中的地址，同时注意如果发现中间有映射没有建立好，那么同样的会把所有映射建立好
    uint8_t* ret_point;
    
    if(offset < DIRECT_BLOCK_SIZ * BLOCK_SIZ){//标号位于直接块内
        if(offset < BLOCK_SIZ){
            if(!inode->direct[0])//说明还没有进行过对应数据块的分配
                inode->direct[0] = alloc_block(0,-1);
            uint8_t* direct0 = (uint8_t*)bread(inode->direct[0]);
            ret_point = direct0 + offset%BLOCK_SIZ;//如果在第一个块内，那么可以直接相加就行
        }
        else if(offset < BLOCK_SIZ*2){
            if(!inode->direct[1])//说明还没有进行过对应数据块的分配
                inode->direct[1] = alloc_block(0,-1);
            uint8_t* direct1 = (uint8_t*)bread(inode->direct[1]);
            ret_point = direct1 + offset%BLOCK_SIZ;//第二个直接指针（已转换）加上偏移量
        }
        else{
            if(!inode->direct[2])//说明还没有进行过对应数据块的分配
                inode->direct[2] = alloc_block(0,-1);
            uint8_t* direct2 = (uint8_t*)bread(inode->direct[2]);
            ret_point = direct2 + offset%BLOCK_SIZ;//第二个直接指针（已转换）加上偏移量
        }
    }
    else if(offset < INDIRECT1_BLOCK_SIZ * BLOCK_SIZ){//标号位于一级间接块内
        if(!inode->indirect_1)//说明还没有进行过对应数据块的分配
            inode->indirect_1 = alloc_block(0,-1);

        uint8_t* indirect1_point = (uint8_t*)bread(inode->indirect_1);
        uint32_t* point = (uint32_t*)indirect1_point + (offset - DIRECT_BLOCK_SIZ * BLOCK_SIZ) / BLOCK_SIZ;//point块，还不是目录块
        uint32_t temp_point = *point;

        if(!(*point))//说明还没有进行过对应数据块的分配
            temp_point = alloc_block(0,(int64_t)point);//分配，分配的同时,point指针出的blockid号也已经置好

        uint8_t* indirect1 = (uint8_t*)bread(temp_point);
        ret_point = indirect1 + offset%BLOCK_SIZ;//第二个直接指针（已转换）加上偏移量
    }
    else{//标号位于二级间接块内
        if(!inode->indirect_2){//说明还没有进行过对应数据块的分配
            inode->indirect_2 = alloc_block(0,-1);
        }
        uint8_t* indirect2_point1 = (uint8_t*)bread(inode->indirect_2);
        uint32_t* point1 = (uint32_t*)indirect2_point1 + (offset - INDIRECT1_BLOCK_SIZ * BLOCK_SIZ) / (BLOCK_SIZ * POINT_PER_BLOCK);//point块，还不是目录块
        uint32_t temp_point1 = *point1;

        if(!(*point1))//说明还没有进行过对应数据块的分配
            temp_point1 = alloc_block(0,(int64_t)point1);//分配，分配的同时,point指针出的blockid号也已经置好
        //如果是现在才分配，那么point1指向的blockid会返回，但是由于可能会被覆盖掉，因此不能采用之前的写法

        uint8_t* indirect2_point2 = (uint8_t*)bread(temp_point1);
        uint32_t* point2 = (uint32_t*)indirect2_point2 + ((offset - INDIRECT1_BLOCK_SIZ * BLOCK_SIZ) % (BLOCK_SIZ * POINT_PER_BLOCK)) / BLOCK_SIZ;//point块，还不是目录块
        uint32_t temp_point2 = *point2;
        //这里的操作需要稍微注意一下

        if(!(*point2))//说明还没有进行过对应数据块的分配
            temp_point2 = alloc_block(0,(int64_t)point2);
            
        uint8_t* indirect2 = (uint8_t*)bread(temp_point2);
        ret_point = indirect2 + offset%BLOCK_SIZ;//三级间接指针（已转换）加上偏移量
    }

    return ret_point;
}

void bigfile_alloc(inode_t* inode, uint32_t offset){//与searchdatapoint基本一致，唯一的不同在于这个时候block肯定是没有被分配过的，因此分配就行了不用读
    
    judge1 = (offset == 0x800000);
    if(offset < DIRECT_BLOCK_SIZ * BLOCK_SIZ){//标号位于直接块内
        if(offset < BLOCK_SIZ){
            if(!inode->direct[0])//说明还没有进行过对应数据块的分配
                inode->direct[0] = alloc_block(0,-1);
        }
        else if(offset < BLOCK_SIZ*2){
            if(!inode->direct[1])//说明还没有进行过对应数据块的分配
                inode->direct[1] = alloc_block(0,-1);
        }
        else{
            if(!inode->direct[2])//说明还没有进行过对应数据块的分配
                inode->direct[2] = alloc_block(0,-1);
        }
    }
    else if(offset < INDIRECT1_BLOCK_SIZ * BLOCK_SIZ){//标号位于一级间接块内
        if(!inode->indirect_1)//说明还没有进行过对应数据块的分配
            inode->indirect_1 = alloc_block(0,-1);

        uint8_t* indirect1_point = (uint8_t*)bread(inode->indirect_1);
        uint32_t* point = (uint32_t*)indirect1_point + (offset - DIRECT_BLOCK_SIZ * BLOCK_SIZ) / BLOCK_SIZ;//point块，还不是目录块

        if(!(*point))//说明还没有进行过对应数据块的分配
            alloc_block(0,(int64_t)point);//分配，分配的同时,point指针出的blockid号也已经置好
    }
    else{//标号位于二级间接块内
        if(!inode->indirect_2){//说明还没有进行过对应数据块的分配
            inode->indirect_2 = alloc_block(0,-1);
        }
        uint8_t* indirect2_point1 = (uint8_t*)bread(inode->indirect_2);
        uint32_t* point1 = (uint32_t*)indirect2_point1 + (offset - INDIRECT1_BLOCK_SIZ * BLOCK_SIZ) / (BLOCK_SIZ * POINT_PER_BLOCK);//point块，还不是目录块
        uint32_t temp_point1 = *point1;

        if(!(*point1))//说明还没有进行过对应数据块的分配
            temp_point1 = alloc_block(0,(int64_t)point1);//分配，分配的同时,point指针出的blockid号也已经置好
        //如果是现在才分配，那么point1指向的blockid会返回，但是由于可能会被覆盖掉，因此不能采用之前的写法

        uint8_t* indirect2_point2 = (uint8_t*)bread(temp_point1);
        uint32_t* point2 = (uint32_t*)indirect2_point2 + ((offset - INDIRECT1_BLOCK_SIZ * BLOCK_SIZ) % (BLOCK_SIZ * POINT_PER_BLOCK)) / BLOCK_SIZ;//point块，还不是目录块
        //这里的操作需要稍微注意一下

        if(!(*point2))//说明还没有进行过对应数据块的分配
            alloc_block(0,(int64_t)point2);
    }
}

int write_file(inode_t* inode, char* string, uint32_t start, uint32_t length){//inode指定了文件，string指定内容，其他指定起始位置和长度
    uint8_t* bcache_start;
    uint8_t* bcache_end;
    uint32_t bcache_index;
    char* start_string = string;

    uint32_t seg_length;

    inode_t buff_inode;
    uint32_t buff_ino = ((((uint8_t*)inode - (uint8_t*)bcaches)%sizeof(bcache_t)) - 8)/sizeof(inode_t);
    //通过inode来获得ino//这个实际上只考虑了一个inodetable
    memcpy((void*)&buff_inode, (void*)inode, sizeof(inode_t));//栈上暂时保存

    while(length){
        bcache_start = search_datapoint(&buff_inode, start);
        bcache_index = (bcache_start - (uint8_t*)bcaches)/sizeof(bcache_t);//获得是第几个bcache
        bcache_end = (uint8_t*)&(bcaches[bcache_index+1]);//得到结束的位置

        seg_length = bcache_end-bcache_start;
        seg_length = (seg_length > length) ? length : seg_length;
        memcpy(bcache_start, (uint8_t*)start_string, seg_length);//拷贝一段
        bwrite(bcaches[bcache_index].block_id, bcaches[bcache_index].bcache_block);//落盘

        length -= seg_length;
        start += seg_length;
        start_string += seg_length;
        buff_inode.filesz = (start > buff_inode.filesz) ? start : buff_inode.filesz;
        //现在的filesiz相当于永远指着最后那一个地址，中间有空洞也不管
        //因此filesz只管往大了就行，再新添加之后和之前的filesz里面选大的
        if (buff_inode.mode == INODE_FILE && ((int)buff_inode.fd_index != 0xff)) 
            fdescs[buff_inode.fd_index].memsiz += seg_length;//中间你写是覆盖空洞还是什么我不管，只要你写了我就加
        
    }

    buff_inode.mtime = get_timer();

    inode = ino2inode_t(buff_ino);//前面都是用栈上的inode做的，这里再次读出来并且翻译后写入
    memcpy((void*)inode, (void*)&buff_inode, sizeof(inode_t));//栈上暂时保存的值放到其中
    uint32_t bcache_inode_index = ((uint8_t*)inode - (uint8_t*)bcaches)/sizeof(bcache_t);
    bwrite(bcaches[bcache_inode_index].block_id, bcaches[bcache_inode_index].bcache_block);//落盘

    bios_sd_read(kva2pa((uintptr_t)check_block), 8, blockid2sectorid(bcaches[bcache_inode_index].block_id));//测试
    //写文件之后inode必然会被修改，因此必须落盘

    return 1;
}

int read_file(inode_t* inode, char* string, uint32_t start, uint32_t length){//inode指定了文件，string指定内容，其他指定起始位置和长度
    uint8_t* bcache_start;
    uint8_t* bcache_end;
    uint32_t bcache_index;
    char* start_string = string;

    uint32_t seg_length;

    inode_t buff_inode;
    uint32_t buff_ino = ((((uint8_t*)inode - (uint8_t*)bcaches)%sizeof(bcache_t)) - 8)/sizeof(inode_t);
    //通过inode来获得ino
    memcpy((void*)&buff_inode, (void*)inode, sizeof(inode_t));//栈上暂时保存

    while(length){
        bcache_start = search_datapoint(&buff_inode, start);
        bcache_index = (bcache_start - (uint8_t*)bcaches)/sizeof(bcache_t);//获得是第几个bcache
        bcache_end = (uint8_t*)&(bcaches[bcache_index+1]);//得到结束的位置

        seg_length = bcache_end-bcache_start;
        seg_length = (seg_length > length) ? length : seg_length;
        memcpy((uint8_t*)start_string, bcache_start, seg_length);//拷贝一段

        length -= seg_length;
        start += seg_length;
        start_string += seg_length;
    }

    inode = ino2inode_t(buff_ino);//前面都是用栈上的inode做的，这里再次读出来并且翻译后写入
    memcpy((void*)inode, (void*)&buff_inode, sizeof(inode_t));//栈上暂时保存的值放到其中
    //这里需要吗？可能不需要，但是我认为要把inode对应的东西重新加载到bcache中来

    return 1;
}

/*
以上是两个重量级函数，所有除开inodetable以外的写，都必须是经过write_file()函数实现
1.searchdatapoint()负责根据给的inode_t，从其各级指针和给出的抽象整体中的偏移，出发去寻找对应的数据块并返回最后的指针
在这个过程中，如果发现offset没有找到，那么所有的映射都会被建立好
如果不是单纯的找而是建立了映射，那么一定要注意inode和中间的point块都会被修改
中间的point块在建立映射的时候就必须直接落盘,inode在修改完成之后再全部落盘
*/


dentry_t* name_search_dentry(inode_t* inode_current, char* name){//对于目录项而言，给一个当前目录级的名字，可以找到其在数据块中的具体位置并返回地址
    dentry_t* dentry;
    uint32_t dentry_offset = 0;

    dentry = (dentry_t*)search_datapoint(inode_current, dentry_offset);

    while(dentry->dentry_valid && (dentry_offset < inode_current->filesz)){//没有到尽头//同时还要注意不能够比filesz大
        if(!strcmp(dentry->name, name))
            return dentry;

        dentry_offset += sizeof(dentry_t);
        dentry = (dentry_t*)search_datapoint(inode_current, dentry_offset);
    }
    
    return 0;
}

int name_search_offset(inode_t* inode_current, char* name){//对于目录项而言，给一个当前目录级的名字，可以找到其在数据块中的具体位置并返回地址
    dentry_t* dentry;
    uint32_t dentry_offset = 0;

    dentry = (dentry_t*)search_datapoint(inode_current, dentry_offset);

    while(dentry->dentry_valid && (dentry_offset < inode_current->filesz)){//没有到尽头
        if(!strcmp(dentry->name, name))
            return dentry_offset;

        dentry_offset += sizeof(dentry_t);
        dentry = (dentry_t*)search_datapoint(inode_current, dentry_offset);
    }
    
    return -1;
}

/*
以上两个函数最主要的区别在于，一个是返回内存中的地址，另一个是返回文件中相对的偏移
*/

int inopath2ino(uint32_t base_ino, char * dir_name){//根据给出的目录和路径，返回对应文件或目录的ino号；因为最终要做的时更新pwd
//此函数根据当前的ino号和路径得到处理后的ino号
    char name[32];
    strcpy(name, dir_name);

    char* nxt_name = name;
    uint8_t sign;//这里的sign用来判断当级有没有结束目录
    if(*nxt_name == '\0')
        return base_ino;//直接就是当级的目录,直接返回

    while(*nxt_name  != '\0' && *nxt_name != '/')
        nxt_name++;
    //如果这一级直接发现是'\0'，可能要做处理

    if(*nxt_name == '/'){
        sign = 0;//当级没有结束
        *(nxt_name++) = '\0';
    }
    else
        sign = 1;

    // if(*nxt_name == '\0')
    //     return 0;//直接就是根目录
    //这一级有几种可能的异常情况
    //1.已经不是目录，自然不能cd，打印不可打开后返回
    //2.这一级已经结束，找到这一级的inode之后直接返回，这里的判断条件就是最后不是'/'而是'\0'
    inode_t* base_inode = ino2inode_t(base_ino);
    dentry_t* dentry;

    if(!sign){//当级没有结束
        if(base_inode->mode != INODE_DIR){
            printk("> [FS] Can not open file %s\n",name);
            return -1;
        } //当级没有结束，则必须保证是目录不是文件
        if((dentry = name_search_dentry(base_inode, name)) == 0){
            printk("> [FS] %s doesn't exist\n",name);
            return -1;//如果发现这一级名字没有完，但是已经找不到了
        }
        return inopath2ino(dentry->ino, nxt_name);
    }
    else {
        if(base_inode->mode != INODE_DIR){
            printk("> [FS] Can not open file %s\n",name);
            return -1;
        } //当级没有结束，则必须保证是目录不是文件
        if((dentry = name_search_dentry(base_inode, name)) == 0){
            printk("> [FS] %s doesn't exist\n",name);
            return -1;//如果发现这一级名字没有完，但是已经找不到了
        }
        return dentry->ino;
    }
    return -1;
}

/*
以上两个函数专供cd系统调用
namesearch()函数会根据inode_t项逐级的对数据块中的目录项进行查询，成功查到则返回对应的ino号
*/