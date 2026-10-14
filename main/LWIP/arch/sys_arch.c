/*
 * Copyright (c) 2001-2003 Swedish Institute of Computer Science.
 * All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 * 
 * Author: Adam Dunkels <adam@sics.se>
 *
 */
/*  Porting by Michael Vysotsky <michaelvy@hotmail.com> August 2011   */

#define SYS_ARCH_GLOBALS

/* lwIP includes. */
#include "lwip/debug.h"
#include "lwip/def.h"
#include "lwip/lwip_sys.h"
#include "lwip/mem.h"
#include "includes.h"
#include "delay.h"
#include "arch/sys_arch.h"
#include "malloc.h"


//当消息指针为空时,指向一个常量pvNullPointer所指向的值.
//在UCOS中如果OSQPost()中的msg==NULL会返回一条OS_ERR_POST_NULL
//错误,而在lwip中会调用sys_mbox_post(mbox,NULL)发送一条空消息,我们
//在本函数中把NULL变成一个常量指针0Xffffffff
const void * const pvNullPointer = (mem_ptr_t*)0xffffffff;
 
/* 资源池(队列/事件)耗尽的自愈兜底:
   这些池只在 OSInit() 时建立, 运行期无法重建; 一旦被泄漏耗尽, 设备会永久无法接受
   新连接(对新连接一律回 RST)且只能人工复位/断电。此处做兜底——连续分配失败达到阈值
   就整机复位, 复位后自动恢复服务。正常运行时该计数应长期为 0。 */
#define SYS_POOL_FAIL_RESET_THRESHOLD   500u
static u32_t s_pool_fail_run = 0;
static void sys_arch_pool_fail_check(void)
{
	if(++s_pool_fail_run >= SYS_POOL_FAIL_RESET_THRESHOLD)
	{
		s_pool_fail_run = 0;
		NVIC_SystemReset();                                  /* 整机复位: 重建 uCOS 队列/事件池 */
	}
}

/* ================= 邮箱"分配/释放来源"追踪(诊断用) =================
   目的: 定位"非属主释放/重复释放"——它会让一对 uCOS OS_Q+OS_EVENT 丢失(池退化),
   最终 netconn_alloc 失败、设备对新连接一律回 RST。
   做法: 记录最近若干次分配/释放的 TQ_DESCR 地址与调用者返回地址;
        命中重复释放时保存"两次释放各自的调用者", 离线用 Keil map 文件即可反查函数名。
   查询: g_dbg_mbox_df_cnt(次数)、g_dbg_mbox_df_lr(后一次释放者)、
        g_dbg_mbox_df_prev_lr(首次释放者)、g_dbg_mbox_df_addr,
        以及镜像数组 g_dbg_mbox_hist_addr/lr 与 g_dbg_mbox_alc_addr/lr 供 SWD 直接读取。
   注: 该追踪只在 sys_mbox_new/free 里做几十条指令, 对性能影响可忽略, 定位完成后可整体去掉。 */
#define SYS_MBOX_HIST        8
static sys_mbox_t s_mbox_hist[SYS_MBOX_HIST];            /* 最近释放的邮箱地址(环形) */
static u32_t      s_mbox_hist_lr[SYS_MBOX_HIST];         /* 对应释放者返回地址 */
static u32_t      s_mbox_hist_idx = 0;
static sys_mbox_t s_mbox_alc[SYS_MBOX_HIST];             /* 最近分配的邮箱地址(环形) */
static u32_t      s_mbox_alc_lr[SYS_MBOX_HIST];          /* 对应分配者返回地址 */
static u32_t      s_mbox_alc_idx = 0;

volatile u32_t g_dbg_mbox_hist_addr[SYS_MBOX_HIST];      /* 上述两个环形的镜像, 便于 SWD 直接读取 */
volatile u32_t g_dbg_mbox_hist_lr[SYS_MBOX_HIST];
volatile u32_t g_dbg_mbox_hist_ptr;
volatile u32_t g_dbg_mbox_alc_addr[SYS_MBOX_HIST];
volatile u32_t g_dbg_mbox_alc_lr[SYS_MBOX_HIST];
volatile u32_t g_dbg_mbox_alc_ptr;

/* ===== 邮箱所有权表: sys_mbox_new 登记(地址+代数), sys_mbox_free 校验后注销 =====
   作用: sys_mbox_free 收到的指针若是"已释放过的旧 TQ_DESCR"(已被注销, 或其内存已被
   新邮箱复用而指针代数不符), 一律拒绝释放并计数——从根上杜绝"重复释放/释放别人的邮箱"
   造成的 uCOS OS_Q+OS_EVENT 成对丢失(此前表现为 free_invalid 与队列/事件池缓慢枯竭)。 */
#define MBOX_LIVE_HIST  32
static PQ_DESCR s_ml_addr[MBOX_LIVE_HIST];
static u32_t    s_ml_gen[MBOX_LIVE_HIST];
static u32_t    s_ml_idx = 0;
volatile u32_t g_dbg_mbox_gen = 0;         /* 邮箱分配代数计数 */
volatile u32_t g_dbg_mbox_double_real = 0; /* 真·重复释放(地址已被注销) */
volatile u32_t g_dbg_mbox_stale_ptr = 0;   /* 陈旧指针(地址存活但代数不符) */

/* 取"本函数的调用者返回地址"(即调用 sys_mbox_new/free 那条 BL 的下一条指令地址,
   位于调用者内部; Thumb 位为1, 查 map 时用 (lr & ~1))。
   注意: 必须在本函数体内直接使用 __builtin_return_address(0), 若放到辅助函数里
   取到的会是"辅助函数自己的返回地址"(即本函数内部地址), 无意义。 */


//创建一个消息邮箱
//*mbox:消息邮箱
//size:邮箱大小
//返回值:ERR_OK,创建成功
//         其他,创建失败
err_t sys_mbox_new( sys_mbox_t *mbox, int size)
{
	u32_t lr = (u32_t)__builtin_return_address(0);   // 创建者返回地址(诊断: 反查邮箱创建来源)
	(*mbox)=mymalloc(SRAMIN,sizeof(TQ_DESCR));	//为消息邮箱申请内存
	mymemset((*mbox),0,sizeof(TQ_DESCR)); 		//清除mbox的内存
	if(*mbox)//内存分配成功
	{
		if(size>MAX_QUEUE_ENTRIES)size=MAX_QUEUE_ENTRIES;		//消息队列最多容纳MAX_QUEUE_ENTRIES消息数目 
 		(*mbox)->pQ=OSQCreate(&((*mbox)->pvQEntries[0]),size);  //使用UCOS创建一个消息队列
		LWIP_ASSERT("OSQCreate",(*mbox)->pQ!=NULL); 
		if((*mbox)->pQ!=NULL)
		{
			u32_t k, slot = MBOX_LIVE_HIST;
			g_dbg_mbox_new++;                                // 调试计数
			(*mbox)->dbg_gen = ++g_dbg_mbox_gen;             // 所有权/诊断: 代数标记
			for(k=0;k<MBOX_LIVE_HIST;k++){                   // 存活表登记(地址复用原位更新)
				if(s_ml_addr[k]==*mbox){slot=k;break;}
				if((slot==MBOX_LIVE_HIST)&&(s_ml_addr[k]==NULL)){slot=k;}
			}
			if(slot==MBOX_LIVE_HIST){slot=s_ml_idx;s_ml_idx=(s_ml_idx+1u)%MBOX_LIVE_HIST;}
			s_ml_addr[slot]=*mbox;s_ml_gen[slot]=(*mbox)->dbg_gen;
			g_dbg_mbox_alloc_lr = lr;                        // 诊断: 记录最近一次创建来源
			s_mbox_alc[s_mbox_alc_idx]    = *mbox;           /* 诊断: 记录分配(环形) */
			s_mbox_alc_lr[s_mbox_alc_idx] = lr;
			g_dbg_mbox_alc_addr[s_mbox_alc_idx] = (u32_t)*mbox;
			g_dbg_mbox_alc_lr[s_mbox_alc_idx]   = lr;
			s_mbox_alc_idx = (s_mbox_alc_idx + 1u) % SYS_MBOX_HIST;
			g_dbg_mbox_alc_ptr = s_mbox_alc_idx;
			s_pool_fail_run = 0;                             // 分配成功: 清零连续失败计数
			return ERR_OK;  //返回ERR_OK,表示消息队列创建成功 ERR_OK=0
		}
		else
		{ 
			g_dbg_mbox_new_fail++;                           // 调试计数(OSQCreate 失败)
			sys_arch_pool_fail_check();                      // 池耗尽兜底: 连续失败达阈值则整机复位
			myfree(SRAMIN,(*mbox));
			return ERR_MEM;  		//消息队列创建错误
		}
	}
	else
	{
		g_dbg_mbox_new_fail++;                               // 调试计数(mymalloc 失败)
		sys_arch_pool_fail_check();                          // 池耗尽兜底
		return ERR_MEM; 			//消息队列创建错误 
	}
} 
//释放并删除一个消息邮箱
//*mbox:要删除的消息邮箱
void sys_mbox_free(sys_mbox_t * mbox)
{
	u8_t ucErr;
	u32_t lr, i;
	sys_mbox_t m_box=*mbox;   
	g_dbg_mbox_free++;                                   // 调试计数
	lr = (u32_t)__builtin_return_address(0);             // 释放来源(调用者返回地址), 诊断用
	if(m_box==NULL)                                      // 空指针保护: 重复释放时不要继续操作系统对象
	{
		*mbox=NULL;
		return;
	}
	/* 所有权校验: 只有"存活表中地址匹配且代数一致"的邮箱才允许真正释放。
	   - 地址已注销(不在表): 真·重复释放 → 拒绝;
	   - 地址在表但代数不符: 该 TQ_DESCR 内存已被新邮箱复用, 本指针是陈旧指针 → 拒绝。
	   两种情况都直接返回, 不再 OSQDel/myfree, 保证 uCOS 队列/事件池与 SRAM 堆不受破坏。 */
	{
		u32_t k;
		u8_t found = 0;
		for(k=0;k<MBOX_LIVE_HIST;k++)
		{
			if(s_ml_addr[k]==m_box)
			{
				found = 1;
				if(s_ml_gen[k]!=m_box->dbg_gen)
				{
					g_dbg_mbox_stale_ptr++;              // 陈旧指针(内存已被新邮箱复用)
					*mbox=NULL;
					return;
				}
				s_ml_addr[k]=NULL;s_ml_gen[k]=0;         // 注销所有权
				break;
			}
		}
		if(!found)
		{
			g_dbg_mbox_double_real++;                    // 真·重复释放(地址已被注销)
			*mbox=NULL;
			return;
		}
	}

	/* 真·重复释放/脏指针判定: 事件对象已不是"队列" → 该邮箱此前已被释放并回收。
	   (注意: 不能只比较 TQ_DESCR 地址——SRAM 堆块会被复用, 同一地址会合法地反复出现。) */
	if((m_box->pQ==NULL)||(m_box->pQ->OSEventType!=OS_EVENT_TYPE_Q))
	{
		u32_t prev = 0;
		for(i=0;i<SYS_MBOX_HIST;i++)                      /* 反查首次释放该邮箱的调用者 */
		{
			if(s_mbox_hist[i]==m_box) { prev = s_mbox_hist_lr[i]; break; }
		}
		g_dbg_mbox_free_invalid++;                        // 调试计数: 释放时事件已不是队列
		g_dbg_mbox_del_err++;
		g_dbg_mbox_df_cnt++;                              // 调试计数: 真·重复释放次数
		g_dbg_mbox_df_addr    = (u32_t)m_box;
		g_dbg_mbox_df_lr      = lr;                       // 后一次(本次)释放的调用者
		g_dbg_mbox_df_prev_lr = prev;                     // 首次释放的调用者(0=历史中未找到)
		*mbox=NULL;
		return;                                           /* 不再 OSQDel/myfree, 避免破坏池与堆 */
	}

	/* 正常路径: 记录本次释放(环形 + SWD 可读镜像), 供"首次释放者"反查。 */
	s_mbox_hist[s_mbox_hist_idx]    = m_box;
	s_mbox_hist_lr[s_mbox_hist_idx] = lr;
	g_dbg_mbox_hist_addr[s_mbox_hist_idx] = (u32_t)m_box;
	g_dbg_mbox_hist_lr[s_mbox_hist_idx]   = lr;
	s_mbox_hist_idx = (s_mbox_hist_idx + 1u) % SYS_MBOX_HIST;
	g_dbg_mbox_hist_ptr = s_mbox_hist_idx;

	(void)OSQDel(m_box->pQ,OS_DEL_ALWAYS,&ucErr);
	if(ucErr!=OS_ERR_NONE)
	{
		g_dbg_mbox_free_fail++;                          // 调试计数: OSQDel 失败(队列未回收 → 池退化)
		g_dbg_mbox_del_err++;
		*mbox=NULL;
		return;                                          // 失败时不再 myfree, 避免重复释放堆块
	}
	myfree(SRAMIN,m_box); 
	*mbox=NULL;
}
//向消息邮箱中发送一条消息(必须发送成功)
//*mbox:消息邮箱
//*msg:要发送的消息
void sys_mbox_post(sys_mbox_t *mbox,void *msg)
{    
	if(msg==NULL)msg=(void*)&pvNullPointer;//当msg为空时 msg等于pvNullPointer指向的值 
	while(OSQPost((*mbox)->pQ,msg)!=OS_ERR_NONE);//死循环等待消息发送成功 
}
//尝试向一个消息邮箱发送消息
//此函数相对于sys_mbox_post函数只发送一次消息，
//发送失败后不会尝试第二次发送
//*mbox:消息邮箱
//*msg:要发送的消息
//返回值:ERR_OK,发送OK
// 	     ERR_MEM,发送失败
err_t sys_mbox_trypost(sys_mbox_t *mbox, void *msg)
{ 
	if(msg==NULL)msg=(void*)&pvNullPointer;//当msg为空时 msg等于pvNullPointer指向的值 
	if((OSQPost((*mbox)->pQ, msg))!=OS_ERR_NONE)return ERR_MEM;
	return ERR_OK;
}

//等待邮箱中的消息
//*mbox:消息邮箱
//*msg:消息
//timeout:超时时间，如果timeout为0的话,就一直等待
//返回值:当timeout不为0时如果成功的话就返回等待的时间，
//		失败的话就返回超时SYS_ARCH_TIMEOUT
u32_t sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout)
{ 
	u8_t ucErr;
	u32_t ucos_timeout,timeout_new,elapsed;
	void *temp;
	sys_mbox_t m_box=*mbox;
	if(timeout!=0)
	{
		ucos_timeout=(timeout*OS_TICKS_PER_SEC)/1000; //转换为节拍数,因为UCOS延时使用的是节拍数,而LWIP是用ms
		if(ucos_timeout<1)ucos_timeout=1;//至少1个节拍
	}else ucos_timeout = 0; 
	timeout = OSTimeGet(); //获取系统时间 
	temp=OSQPend(m_box->pQ,(u16_t)ucos_timeout,&ucErr); //请求消息队列,等待时限为ucos_timeout

	if(ucErr!=OS_ERR_NONE)   /* 请求超时(OS_ERR_TIMEOUT), 或事件对象无效等异常情况:
	                            一律按"未取到消息"处理, 避免调用者把它当成取到了消息而死循环
	                            (netconn_drain 的取空循环就依赖这里的语义) */
	{
		if(msg!=NULL)*msg=NULL;   // 修补: 必须清掉消息指针, 否则调用者会拿到上一次的陈旧指针
		return SYS_ARCH_TIMEOUT;
	}

	if(msg!=NULL)
	{	
		if(temp==(void*)&pvNullPointer)*msg = NULL;   	//因为lwip发送空消息的时候我们使用了pvNullPointer指针,所以判断pvNullPointer指向的值
 		else *msg=temp;									//就可知道请求到的消息是否有效
	}    

	/* 修补: 计算等待耗时(ms)。OSTimeGet() 是32位节拍计数, 必须按无符号回绕计算。
	   原实现 `if(new>old) d=new-old; else d=0xffffffff-old+new;` 在"同一节拍内取到
	   消息"(new==old, 高频发生)时会得到 0xffffffff → 返回约 21474832ms 的假耗时;
	   LwIP 超时链表用相对时间(sys_timeout/sys_timeouts_mbox_fetch), 假耗时会把它扣成0,
	   使链表出现"0时间"节点 → sys_timeouts_mbox_fetch() 在"定时器已到期"分支里反复
	   执行定时器却不再取邮箱 → tcpip 核心线程永久空转(输入消息池耗尽、设备静默)。*/
	timeout_new=OSTimeGet();
	if(timeout_new>=timeout)elapsed=timeout_new-timeout;
	else elapsed=(0xffffffffu-timeout)+timeout_new+1u;   // 节拍计数器回绕
	if(elapsed>0x001fffffu)elapsed=0x001fffffu;          // 上限保护, 避免下面乘1000溢出
	elapsed=elapsed*1000/OS_TICKS_PER_SEC;               // 节拍→毫秒
	if(elapsed>=SYS_ARCH_TIMEOUT)elapsed=SYS_ARCH_TIMEOUT-1u; // 绝不能被误判为"超时"
	return elapsed; 
}
//尝试获取消息
//*mbox:消息邮箱
//*msg:消息
//返回值:等待消息所用的时间/SYS_ARCH_TIMEOUT
u32_t sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg)
{
	return sys_arch_mbox_fetch(mbox,msg,1);//尝试获取一个消息
}
//检查一个消息邮箱是否有效
//*mbox:消息邮箱
//返回值:1,有效.
//      0,无效
int sys_mbox_valid(sys_mbox_t *mbox)
{  
	sys_mbox_t m_box=*mbox;
	u8_t ucErr;
	int ret;
	OS_Q_DATA q_data;
	memset(&q_data,0,sizeof(OS_Q_DATA));
	ucErr=OSQQuery (m_box->pQ,&q_data);
	ret=(ucErr<2&&(q_data.OSNMsgs<q_data.OSQSize))?1:0;
	return ret; 
} 
//设置一个消息邮箱为无效
//*mbox:消息邮箱
void sys_mbox_set_invalid(sys_mbox_t *mbox)
{
	*mbox=NULL;
} 
//创建一个信号量
//*sem:创建的信号量
//count:信号量值
//返回值:ERR_OK,创建OK
// 	     ERR_MEM,创建失败
err_t sys_sem_new(sys_sem_t * sem, u8_t count)
{  
	u8_t err; 
	*sem=OSSemCreate((u16_t)count);
	if(*sem==NULL)
	{
		sys_arch_pool_fail_check();                      // 事件池耗尽兜底: 连续失败达阈值则整机复位
		return ERR_MEM; 
	}
	OSEventNameSet(*sem,"LWIP Sem",&err);
	LWIP_ASSERT("OSSemCreate ",*sem != NULL );
	g_dbg_sem_new++;                                     // 调试计数
	s_pool_fail_run = 0;                                 // 分配成功: 清零连续失败计数
	return ERR_OK;
} 
//等待一个信号量
//*sem:要等待的信号量
//timeout:超时时间
//返回值:当timeout不为0时如果成功的话就返回等待的时间，
//		失败的话就返回超时SYS_ARCH_TIMEOUT
u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout)
{ 
	u8_t ucErr;
	u32_t ucos_timeout, timeout_new; 
	if(	timeout!=0) 
	{
		ucos_timeout = (timeout * OS_TICKS_PER_SEC) / 1000;//转换为节拍数,因为UCOS延时使用的是节拍数,而LWIP是用ms
		if(ucos_timeout < 1)
		ucos_timeout = 1;
	}else ucos_timeout = 0; 
	timeout = OSTimeGet();  
	OSSemPend (*sem,(u16_t)ucos_timeout, (u8_t *)&ucErr);
 	if(ucErr == OS_ERR_TIMEOUT)timeout=SYS_ARCH_TIMEOUT;//请求超时	
	else
	{     
 		timeout_new = OSTimeGet(); 
		if (timeout_new>=timeout) timeout_new = timeout_new - timeout;
		else timeout_new = 0xffffffff - timeout + timeout_new;
 		timeout = (timeout_new*1000/OS_TICKS_PER_SEC + 1);//算出请求消息或使用的时间(ms)
	}
	return timeout;
}
//发送一个信号量
//sem:信号量指针
void sys_sem_signal(sys_sem_t *sem)
{
	OSSemPost(*sem);
}
//释放并删除一个信号量
//sem:信号量指针
void sys_sem_free(sys_sem_t *sem)
{
	u8_t ucErr;
	if(*sem==NULL)                                       // 空指针保护: 重复释放时不要继续操作系统对象
	{
		return;
	}
	/* 幂等保护: 只有当事件对象确实还是"信号量"时才真正删除, 避免重复释放破坏事件池 */
	if((*sem)->OSEventType!=OS_EVENT_TYPE_SEM)
	{
		g_dbg_sem_del_err++;                             // 调试计数: 命中重复释放/无效事件
		*sem = NULL;
		return;
	}
	(void)OSSemDel(*sem,OS_DEL_ALWAYS,&ucErr );
	if(ucErr!=OS_ERR_NONE)g_dbg_sem_del_err++;           // 调试计数: OSSemDel 失败
	*sem = NULL;
	g_dbg_sem_free++;                                    // 调试计数
} 
//查询一个信号量的状态,无效或有效
//sem:信号量指针
//返回值:1,有效.
//      0,无效
int sys_sem_valid(sys_sem_t *sem)
{
	OS_SEM_DATA  sem_data;
	return (OSSemQuery (*sem,&sem_data) == OS_ERR_NONE )? 1:0;              
} 
//设置一个信号量无效
//sem:信号量指针
void sys_sem_set_invalid(sys_sem_t *sem)
{
	*sem=NULL;
} 
//arch初始化
void sys_init(void)
{ 
    //这里,我们在该函数,不做任何事情
} 
extern OS_STK * TCPIP_THREAD_TASK_STK;//TCP IP内核任务堆栈,在lwip_comm函数定义
//创建一个新进程
//*name:进程名称
//thred:进程任务函数
//*arg:进程任务函数的参数
//stacksize:进程任务的堆栈大小
//prio:进程任务的优先级
sys_thread_t sys_thread_new(const char *name, lwip_thread_fn thread, void *arg, int stacksize, int prio)
{
	OS_CPU_SR cpu_sr;
	INT8U err;
	if(strcmp(name,TCPIP_THREAD_NAME)==0)//创建TCP IP内核任务
	{
		OS_ENTER_CRITICAL();  //进入临界区 
//		OSTaskCreate(thread,arg,(OS_STK*)&TCPIP_THREAD_TASK_STK[stacksize-1],prio);//创建TCP IP内核任务 
		OSTaskCreateExt(	 thread, 																					//建立扩展任务(任务代码指针) 
											(void *)0,																					//传递参数指针 
											(OS_STK*)&TCPIP_THREAD_TASK_STK[stacksize-1], 					//分配任务堆栈栈顶指针 
											(INT8U)prio, 															//分配任务优先级 
											(INT16U)prio,															//(未来的)优先级标识(与优先级相同) 
											(OS_STK *)&TCPIP_THREAD_TASK_STK[0], 											//分配任务堆栈栈底指针 
											(INT32U)stacksize, 															//指定堆栈的容量(检验用) 
											(void *)0,																					//指向用户附加的数据域的指针 
											(INT16U)OS_TASK_OPT_STK_CHK|OS_TASK_OPT_STK_CLR);		//建立任务设定选项 
		OSTaskNameSet(prio, (INT8U *)(void *)TCPIP_THREAD_NAME, &err);
		OS_EXIT_CRITICAL();  //退出临界区
	} 
	return 0;
} 
//lwip延时函数
//ms:要延时的ms数
void sys_msleep(u32_t ms)
{
	delay_ms(ms);
}
//获取系统时间,LWIP1.4.1增加的函数
//返回值:当前系统时间(单位:毫秒)
u32_t sys_now(void)
{
	u32_t ucos_time, lwip_time;
	ucos_time=OSTimeGet();	//获取当前系统时间 得到的是UCSO的节拍数
	lwip_time=(ucos_time*1000/OS_TICKS_PER_SEC+1);//将节拍数转换为LWIP的时间MS
	return lwip_time; 		//返回lwip_time;
}













































