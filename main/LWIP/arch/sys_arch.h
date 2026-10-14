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
#ifndef __ARCH_SYS_ARCH_H__
#define __ARCH_SYS_ARCH_H__ 
#include <includes.h>
#include "arch/cc.h"
#include "includes.h"
 
#ifdef SYS_ARCH_GLOBALS
#define SYS_ARCH_EXT
#else
#define SYS_ARCH_EXT extern
#endif
 
#define MAX_QUEUES        		10	// 消息邮箱的数量
#define MAX_QUEUE_ENTRIES 		20	// 每个消息邮箱的大小
//LWIP消息邮箱结构体
typedef struct {
    OS_EVENT*   pQ;    //UCOS中指向事件控制块的指针
    void*       pvQEntries[MAX_QUEUE_ENTRIES];//消息队列 MAX_QUEUE_ENTRIES消息队列中最多消息数
} TQ_DESCR, *PQ_DESCR;


typedef OS_EVENT *sys_sem_t;    //LWIP使用的信号量
typedef OS_EVENT *sys_mutex_t; 	//LWIP使用的互斥信号量
typedef PQ_DESCR sys_mbox_t;    //LWIP使用的消息邮箱,其实就是UCOS中的消息队列
typedef INT8U sys_thread_t;     //线程ID,也就是任务优先级

/* ==== 临时调试计数(用于定位资源泄漏, 可随时删除) ====
 * 由 sys_arch.c 定义, 其它模块通过 SYS_ARCH_EXT(extern) 引用; 可通过 SWD 读取或串口打印。
 * 判读方法: 约定 (new - free) 为"存活数"; 若某类存活数持续增长即为泄漏源。 */
SYS_ARCH_EXT volatile u32_t g_dbg_mbox_new;        /* sys_mbox_new 成功次数 */
SYS_ARCH_EXT volatile u32_t g_dbg_mbox_new_fail;   /* sys_mbox_new 失败次数(队列池耗尽) */
SYS_ARCH_EXT volatile u32_t g_dbg_mbox_free;       /* sys_mbox_free 次数 */
SYS_ARCH_EXT volatile u32_t g_dbg_sem_new;         /* sys_sem_new 成功次数 */
SYS_ARCH_EXT volatile u32_t g_dbg_sem_free;        /* sys_sem_free 次数 */
SYS_ARCH_EXT volatile u32_t g_dbg_nc_alloc;        /* netconn_alloc 成功次数 */
SYS_ARCH_EXT volatile u32_t g_dbg_nc_free;         /* netconn_free 次数 */
SYS_ARCH_EXT volatile u32_t g_dbg_nc_free_leak_rcv;/* netconn_free 时 recvmbox 仍有效(泄漏!) */
SYS_ARCH_EXT volatile u32_t g_dbg_nc_free_leak_acc;/* netconn_free 时 acceptmbox 仍有效(泄漏!) */
SYS_ARCH_EXT volatile u32_t g_dbg_accept_postfail; /* accept_function 向 acceptmbox 投递失败 */
SYS_ARCH_EXT volatile u32_t g_dbg_delconn_inprog;  /* do_delconn 走 ERR_INPROGRESS 分支 */
SYS_ARCH_EXT volatile u32_t g_dbg_close_inprog;    /* do_close   走 ERR_INPROGRESS 分支 */
SYS_ARCH_EXT volatile u32_t g_dbg_close_fail;      /* do_close_internal 中 tcp_close 未成功(ERR_MEM) */
SYS_ARCH_EXT volatile u32_t g_dbg_err_tcp;         /* err_tcp(连接被 abort/出错) 次数 */
SYS_ARCH_EXT volatile u32_t g_dbg_mbox_del_err;    /* OSQDel 失败次数(邮箱实际未回收 → 池退化) */
/* ===== 邮箱重复释放来源追踪(诊断): 命中时记录"两次释放各自的调用者返回地址", 可用 map 反查函数 ===== */
SYS_ARCH_EXT volatile u32_t g_dbg_mbox_df_cnt;        /* 命中"重复释放同一邮箱"的总次数 */
SYS_ARCH_EXT volatile u32_t g_dbg_mbox_df_addr;       /* 最近一次被重复释放的邮箱(TQ_DESCR)地址 */
SYS_ARCH_EXT volatile u32_t g_dbg_mbox_df_lr;         /* 本次(后一次)释放的调用者返回地址 */
SYS_ARCH_EXT volatile u32_t g_dbg_mbox_df_prev_lr;    /* 首次释放该邮箱的调用者返回地址 */
SYS_ARCH_EXT volatile u32_t g_dbg_mbox_free_invalid;  /* 释放时事件已不是队列(已被回收 → 重复释放/脏指针) */
SYS_ARCH_EXT volatile u32_t g_dbg_mbox_free_fail;     /* OSQDel 返回错误(队列实际未回收) */
SYS_ARCH_EXT volatile u32_t g_dbg_mbox_alloc_lr;      /* 最近一次 sys_mbox_new 的调用者返回地址 */
SYS_ARCH_EXT volatile u32_t g_dbg_sem_del_err;     /* OSSemDel 失败次数(事件实际未回收 → 池退化) */
#endif 


/* __SYS_RTXC_H__ */

































