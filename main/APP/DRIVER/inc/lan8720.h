#ifndef __LAN8720_H
#define __LAN8720_H
#include "sys.h"
#include "stm32f4x7_eth.h"

#define LAN8720_PHY_ADDRESS  	0x00				// LAN8720 PHY芯片地址.


extern ETH_DMADESCTypeDef *DMARxDscrTab;			// 以太网DMA接收描述符数据结构体指针
extern ETH_DMADESCTypeDef *DMATxDscrTab;			// 以太网DMA发送描述符数据结构体指针 
extern uint8_t *Rx_Buff; 							// 以太网底层驱动接收buffers指针 
extern uint8_t *Tx_Buff; 							// 以太网底层驱动发送buffers指针
extern ETH_DMADESCTypeDef  *DMATxDescToSet;			// DMA发送描述符追踪指针
extern ETH_DMADESCTypeDef  *DMARxDescToGet; 		// DMA接收描述符追踪指针 
extern ETH_DMA_Rx_Frame_infos *DMA_RX_FRAME_infos;	// DMA最后接收到的帧信息指针


u8 LAN8720_Init(void);
u8 LAN8720_Get_Speed(void);
u8 LAN8720_Get_Duplex(void);
u8 ETH_MACDMA_Config(void);

FrameTypeDef ETH_Rx_Packet(void);

u8 ETH_Tx_Packet(u16 FrameLength);
u32 ETH_GetCurrentTxBuffer(void);
u8 ETH_Mem_Malloc(void);
void ETH_Mem_Free(void);

void eth_reset_function(void);

/* 0514 */
uint8_t LAN8720_Get_Network_Cable_Status(void);		// 网线接入状态

/* ==== ETH 诊断计数(临时, 定义在 lan8720.c; 可用 SWD 读取; 定位完成后可删除) ==== */
extern volatile uint32_t g_dbg_eth_init;      /* ETH_MACDMA_Config() 调用次数 */
extern volatile uint32_t g_dbg_eth_irq;       /* ETH_IRQHandler 进入次数 */
extern volatile uint32_t g_dbg_eth_rx;        /* ETH_Rx_Packet 成功收帧次数 */
extern volatile uint32_t g_dbg_eth_rx_err;    /* ETH_Rx_Packet 失败次数 */
extern volatile uint32_t g_dbg_eth_tx;        /* ETH_Tx_Packet 成功发送次数 */
extern volatile uint32_t g_dbg_eth_tx_err;    /* ETH_Tx_Packet 失败次数 */
extern volatile uint32_t g_dbg_net_reset;     /* eth_set_network_reset() 调用次数 */
extern volatile uint32_t g_dbg_eth_reset;     /* lwipdev.reset 实际执行网络重启次数 */
extern volatile uint32_t g_dbg_link_down;     /* 检测到 PHY 链路 down 次数 */
extern volatile uint32_t g_dbg_ll_in_null;    /* low_level_input 返回 NULL 次数 */
extern volatile uint32_t g_dbg_ll_out_err;    /* low_level_output 发送失败次数 */

#endif 

