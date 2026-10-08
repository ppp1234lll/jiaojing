#include "lfs_port.h"

#include "lfs.h"

#include "stdio.h"

#include "sys.h"

#include "w25qxx.h"

int lfs_test(void);

// Read a region in a block. Negative error codes are propogated
// to the user.
int block_device_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size)
{
//	const sfud_flash *flash = sfud_get_device_table() + SFUD_W25Q128FV_DEVICE_INDEX;

//	sfud_read(flash, (block*(c->block_size))+off, size, (uint8_t *)buffer);
	W25QXX_Read((uint8_t *)buffer,(block*(c->block_size))+off,size);
	return 0;
}

// Program a region in a block. The block must have previously
// been erased. Negative error codes are propogated to the user.
// May return LFS_ERR_CORRUPT if the block should be considered bad.
int block_device_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size)
{
//	const sfud_flash *flash = sfud_get_device_table() + SFUD_W25Q128FV_DEVICE_INDEX;

//	sfud_write(flash, (block*(c->block_size))+off, size, (uint8_t *)buffer);
	W25QXX_Write_NoCheck((uint8_t *)buffer,(block*(c->block_size))+off,size);
	return 0;
}

// Erase a block. A block must be erased before being programmed.
// The state of an erased block is undefined. Negative error codes
// are propogated to the user.
// May return LFS_ERR_CORRUPT if the block should be considered bad.
int block_device_erase(const struct lfs_config *c, lfs_block_t block)
{
//	const sfud_flash *flash = sfud_get_device_table() + SFUD_W25Q128FV_DEVICE_INDEX;

//	sfud_erase(flash, (block*(c->block_size)), c->block_size);
	W25QXX_Erase_Sector(block);
	return 0;
}

// Sync the state of the underlying block device. Negative error codes
// are propogated to the user.
int block_device_sync(const struct lfs_config *c)
{
	return 0;
}

// configuration of the filesystem is provided by this struct
const struct lfs_config cfg = 
{
    // block device operations
    .read  = block_device_read,
    .prog  = block_device_prog,
    .erase = block_device_erase,
    .sync  = block_device_sync,

    // block device configuration
    .read_size = 16,
    .prog_size = 16,
    .block_size  = 4096,
    .block_count = 512, /* 25Q16 共计 16Mbit，即2MB, 4K一个扇区，共计 2*1024*1024/4096 = 512 个扇区 */
    .lookahead 	 = 128,
};

// variables used by the filesystem
lfs_t 		lfs;
lfs_file_t  lfs_file;
lfs_dir_t 	lfs_dir;

#define LFS_DEVICE_PARAM ("device")

// entry point
int lfs_init_function(void) 
{
	/* 挂载设备 */
    int err = lfs_mount(&lfs, &cfg);
	
    if (err) 
	{
        err = lfs_format(&lfs, &cfg);
        err = lfs_mount(&lfs, &cfg);
    }
	/* 检测目录 */
	err = lfs_dir_open(&lfs,&lfs_dir,LFS_DEVICE_PARAM);
	if(err != 0)
	{
		/* 创建目录 */
		err = lfs_mkdir(&lfs,LFS_DEVICE_PARAM);
		if(err == 0)
		{
			/* 目录创建成功 */
			err = 0;
		}
	}
	else
	{
		lfs_dir_close(&lfs,&lfs_dir);
	}
	
//	lfs_test();
	
	return 0;
}


int lfs_test(void)
{
	int err = 0;
	lfs_file_t lfs_fp;
	// read current count
    uint32_t boot_count = 0;
	uint32_t boot_count1 = 0;
	
    err = lfs_file_open(&lfs, &lfs_file, "boot_count.txt", LFS_O_RDWR | LFS_O_CREAT);
	err = lfs_file_open(&lfs, &lfs_fp, "boot_test.txt", LFS_O_RDWR | LFS_O_CREAT);
    err = lfs_file_read(&lfs, &lfs_file, &boot_count, sizeof(boot_count));
	err = lfs_file_read(&lfs, &lfs_fp, &boot_count1, sizeof(boot_count1));
	
    // update boot count
    boot_count += 1;
	boot_count1 = boot_count%2 + boot_count;
	
    err = lfs_file_rewind(&lfs, &lfs_file);
    err = lfs_file_write(&lfs, &lfs_file, &boot_count, sizeof(boot_count));
	err = lfs_file_rewind(&lfs, &lfs_fp);
	err = lfs_file_write(&lfs, &lfs_fp, &boot_count1, sizeof(boot_count1));
	
    // remember the storage is not updated until the file is closed successfully
    err = lfs_file_close(&lfs, &lfs_file);
	err = lfs_file_close(&lfs, &lfs_fp);
	
    // release any resources we were using
    err = lfs_unmount(&lfs);
	
	return err;

}














