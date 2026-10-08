#ifndef _MALLOC_H
#define _MALLOC_H
#include "sys.h"


#ifndef NULL
#define NULL 0
#endif

//定义三个内存池
#define SRAMIN 	0  //内部内存池

#define SRAMBANK  1 //定义支持的SRAM块数


//mem1内存参数设定,mem1完全处于内部SRAM里面
#define MEM1_BLOCK_SIZE	32  			//内存块大小为32字节
#define MEM1_MAX_SIZE		80*1024 	//最大管理内存 110k
#define MEM1_ALLOC_TABLE_SIZE MEM1_MAX_SIZE/MEM1_BLOCK_SIZE  //内存表大小

//内存管理控制器
struct _m_mallco_dev
{
	void (*init)(u8);  		//初始化
	u8 (*perused)(u8); 		//内存使用率
	u8 *membase[SRAMBANK]; //内存池,管理SRAMBANK个区域的内存
	u16 *memmap[SRAMBANK];  //内存状态表
	u8 memrdy[SRAMBANK];   //内存管理是否就绪
};
extern struct _m_mallco_dev mallco_dev;  //在malloc.c里面定义

void mymemset(void *s,u8 c,u32 count);	 //设置内存
void mymemcpy(void *des,void *src,u32 n);//复制内存     
void mymem_init(u8 memx);					 //内存管理初始化函数(外/内部调用)
u32 mymem_malloc(u8 memx,u32 size);		 //内存分配(内部调用)
u8 mymem_free(u8 memx,u32 offset);		 //内存释放(内部调用)
u8 mem_perused(u8 memx);				 //获得内存使用率(外/内部调用) 
////////////////////////////////////////////////////////////////////////////////
//用户调用函数
void myfree(u8 memx,void *ptr);  			//内存释放(外部调用)
void *mymalloc(u8 memx,u32 size);			//内存分配(外部调用)
void *myrealloc(u8 memx,void *ptr,u32 size);//重新分配内存(外部调用)

void *mymalloc_sramin(uint32_t size);
void myfree_sramin(void *ptr);

#endif


