#include "lfs_port.h"

#include "lfs.h"

#include "stdio.h"

#include "bsp.h"
#include "includes.h"
#include "w25qxx.h"

int lfs_test(void);

// Read a region in a block. Negative error codes are propogated
// to the user.
int block_device_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size)
{
	W25QXX_Read((uint8_t *)buffer,(block*(c->block_size))+off,size);
	return 0;
}

// Program a region in a block. The block must have previously
// been erased. Negative error codes are propogated to the user.
// May return LFS_ERR_CORRUPT if the block should be considered bad.
int block_device_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size)
{
	W25QXX_Write_NoCheck((uint8_t *)buffer,(block*(c->block_size))+off,size);
	return 0;
}

// Erase a block. A block must be erased before being programmed.
// The state of an erased block is undefined. Negative error codes
// are propogated to the user.
// May return LFS_ERR_CORRUPT if the block should be considered bad.
int block_device_erase(const struct lfs_config *c, lfs_block_t block)
{
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
    .read_size = 256,
    .prog_size = 256,
    .block_size  = 4096,
    .block_count = 512, /* 25Q16 共计 16Mbit，即2MB, 4K一个扇区，共计 2*1024*1024/4096 = 512 个扇区 */
						            /* 25Q128 共计 128Mbit，即16MB, 4K一个扇区，共计 16*1024*1024/4096 = 4096 个扇区 */
    .lookahead 	 = 128,
};

// variables used by the filesystem
lfs_t 		g_lfs_t;


#define LFS_DEVICE_PARAM ("device")

// entry point
int lfs_init_function(void) 
{
	lfs_file_t  lfs_file;
	/* 挂载设备 */
    int err = lfs_mount(&g_lfs_t, &cfg);
	if(SAVE_DEBUG)  printf("lfs_mount:%d\r\n",err);
    if (err) 
	{
		err = lfs_format(&g_lfs_t, &cfg);
		err = lfs_mount(&g_lfs_t, &cfg);
		if (err) {
			/* 格式化flash */
			W25QXX_Erase_Chip();
			
			err = lfs_format(&g_lfs_t, &cfg);
			err = lfs_mount(&g_lfs_t, &cfg);
		}
  }
	err = lfs_file_open(&g_lfs_t, &lfs_file, "boot_count.txt", LFS_O_RDWR | LFS_O_CREAT);
		
	if(SAVE_DEBUG)  printf("lfs_file_open:%d\r\n",err);
		
	err = lfs_file_close(&g_lfs_t, &lfs_file);
		
	return 0;
}



int lfs_test(void)
{
	int err = 0;
//	lfs_file_t lfs_fp;
//	// read current count
//    uint32_t boot_count = 0;
//	uint32_t boot_count1 = 0;
//	
//    err = lfs_file_open(&lfs, &lfs_file, "boot_count.txt", LFS_O_RDWR | LFS_O_CREAT);
//	err = lfs_file_open(&lfs, &lfs_fp, "boot_test.txt", LFS_O_RDWR | LFS_O_CREAT);
//    err = lfs_file_read(&lfs, &lfs_file, &boot_count, sizeof(boot_count));
//	err = lfs_file_read(&lfs, &lfs_fp, &boot_count1, sizeof(boot_count1));
//	
//    // update boot count
//    boot_count += 1;
//	boot_count1 = boot_count%2 + boot_count;
//	
//    err = lfs_file_rewind(&lfs, &lfs_file);
//    err = lfs_file_write(&lfs, &lfs_file, &boot_count, sizeof(boot_count));
//	err = lfs_file_rewind(&lfs, &lfs_fp);
//	err = lfs_file_write(&lfs, &lfs_fp, &boot_count1, sizeof(boot_count1));
//	
//    // remember the storage is not updated until the file is closed successfully
//    err = lfs_file_close(&lfs, &lfs_file);
//	err = lfs_file_close(&lfs, &lfs_fp);
//	
//    // release any resources we were using
//    err = lfs_unmount(&lfs);
	
	return err;

}














