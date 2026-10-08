/********************************************************************************
* @File name  : 4G模块
* @Description: 串口2-对应4G
* @Author     : ZHLE
*  Version Date        Modification Description
	12、ML302（4G模块）：串口4，波特率115200，引脚分配为	
			USART4_TX： PC10
			USART4_RX： PC11
			4G_PWRK: PD0
			4G_NRST: PC12
			SIM_DET: PD1
********************************************************************************/

#include "GPRS.h"
#include "gsm_usart.h"
#include "includes.h"
#include "com.h"
#include "update.h"
#include "bsp.h"
#include "rtc.h"
#include "app.h"
#include "led.h"
#include "delay.h"

/* 控制IO */
#define GPRS_NRST_GPIO_CLK		RCC_AHB1Periph_GPIOB
#define GPRS_NRST_GPIO 				GPIOB
#define GPRS_NRST_PIN  				GPIO_Pin_6

#define GPRS_PWRK_GPIO_CLK		RCC_AHB1Periph_GPIOB
#define GPRS_PWRK_GPIO 				GPIOB
#define GPRS_PWRK_PIN 				GPIO_Pin_7

#define GPRS_CTRL_GPIO_CLK		RCC_AHB1Periph_GPIOD
#define GPRS_CTRL_GPIO 				GPIOD
#define GPRS_CTRL_PIN 				GPIO_Pin_1

#define GPRS_Sel_GPIO_CLK		  RCC_APB2Periph_GPIOE
#define GPRS_Sel_GPIO 				GPIOE
#define GPRS_Sel_PIN 				  GPIO_Pin_3

#define GPRS_NRST_H GPIO_WriteBit(GPRS_NRST_GPIO,GPRS_NRST_PIN,Bit_SET)
#define GPRS_NRST_L GPIO_WriteBit(GPRS_NRST_GPIO,GPRS_NRST_PIN,Bit_RESET)

#define GPRS_PWRK_H GPIO_WriteBit(GPRS_PWRK_GPIO,GPRS_PWRK_PIN,Bit_SET)
#define GPRS_PWRK_L GPIO_WriteBit(GPRS_PWRK_GPIO,GPRS_PWRK_PIN,Bit_RESET)

#define GPRS_CTRL_H GPIO_WriteBit(GPRS_CTRL_GPIO,GPRS_CTRL_PIN,Bit_SET)
#define GPRS_CTRL_L GPIO_WriteBit(GPRS_CTRL_GPIO,GPRS_CTRL_PIN,Bit_RESET)

#define GPRS_Sel_H GPIO_WriteBit(GPRS_Sel_GPIO,GPRS_Sel_PIN,Bit_SET)
#define GPRS_Sel_L GPIO_WriteBit(GPRS_Sel_GPIO,GPRS_Sel_PIN,Bit_RESET)


/* 串口初始化 */
#define GPRS_BAUDRATE (115200)
#define GPRS_UART_INIT(baudrate) gsm_usart_init(baudrate)
#define GPRS_STR_SEND(data,len)  gsm_usart_send_str(data,len)

//static int gprs_wait_feedback(const unsigned char *feedback, int feedback_len, int waittime);
////

// GPRS接收数据流
uint16_t gprs_rx_status = 0; // 是否有数据:(gprs_rx_status & 0x8000), 数据长度:(gprs_rx_status & 0x7fff)
uint8_t gprs_rx_buff[GSM_USART_RX_MAX];
uint16_t gprs_rx_take_point = 0; // 读取扫描位置

/* 数据 */
struct gprs_status_t sg_gprs_status_t = {0};

/************************************************************
*
* Function name	: gprs_gpio_init_function
* Description	: 引脚初始化函数
* Parameter		: 
* Return		: 
*	
************************************************************/
void gprs_gpio_init_function(void)
{
	
	GPIO_InitTypeDef GPIO_InitStructure;
	RCC_AHB1PeriphClockCmd(GPRS_NRST_GPIO_CLK|GPRS_PWRK_GPIO_CLK|GPRS_CTRL_GPIO_CLK,ENABLE);

	GPIO_InitStructure.GPIO_Pin    = GPRS_NRST_PIN;
	GPIO_InitStructure.GPIO_Mode   = GPIO_Mode_OUT;			// 输出
	GPIO_InitStructure.GPIO_OType  = GPIO_OType_PP;  		// 推挽输出
	GPIO_InitStructure.GPIO_PuPd   = GPIO_PuPd_NOPULL;   	// 上拉
	GPIO_InitStructure.GPIO_Speed  = GPIO_Speed_100MHz; 	// 高速GPIO
	GPIO_Init(GPRS_NRST_GPIO,&GPIO_InitStructure); 

	GPIO_InitStructure.GPIO_Pin    = GPRS_PWRK_PIN;
	GPIO_InitStructure.GPIO_Mode   = GPIO_Mode_OUT;			// 输出
	GPIO_InitStructure.GPIO_OType  = GPIO_OType_PP;  		// 推挽输出
	GPIO_InitStructure.GPIO_PuPd   = GPIO_PuPd_NOPULL;   	// 上拉
	GPIO_InitStructure.GPIO_Speed  = GPIO_Speed_100MHz; 	// 高速GPIO
	GPIO_Init(GPRS_PWRK_GPIO,&GPIO_InitStructure); 
	
	GPIO_InitStructure.GPIO_Pin    = GPRS_CTRL_PIN;
	GPIO_InitStructure.GPIO_Mode   = GPIO_Mode_OUT;			// 输出
	GPIO_InitStructure.GPIO_OType  = GPIO_OType_PP;  		// 推挽输出
	GPIO_InitStructure.GPIO_PuPd   = GPIO_PuPd_NOPULL;   	// 上拉
	GPIO_InitStructure.GPIO_Speed  = GPIO_Speed_100MHz; 	// 高速GPIO
	GPIO_Init(GPRS_CTRL_GPIO,&GPIO_InitStructure); 
	
	GPIO_InitStructure.GPIO_Pin   = GPRS_Sel_PIN;
	GPIO_InitStructure.GPIO_Mode   = GPIO_Mode_OUT;			// 输出
	GPIO_InitStructure.GPIO_OType  = GPIO_OType_PP;  		// 推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;

	GPIO_Init(GPRS_Sel_GPIO,&GPIO_InitStructure);
	
	GPRS_CTRL_H; // 默认打开电源
	GPRS_NRST_L;
	GPRS_PWRK_L;
	GPRS_Sel_L;
}	

/************************************************************
*
* Function name	: gprs_init_function
* Description	: 初始化函数
* Parameter		: 
* Return		: 
*	
************************************************************/
void gprs_init_function(void)
{
	gprs_gpio_init_function();
	GPRS_UART_INIT(GPRS_BAUDRATE);
}

/************************************************************
*
* Function name	: gprs_boot_up_function
* Description	: 模块开机函数
* Parameter		: 
* Return		: 
*	ML307: 拉低PWR_ON/OFF引脚2s~3.5s使模组开机
************************************************************/
void gprs_boot_up_function(void)
{
	GPRS_PWRK_H;
	GPRS_DELAY_MS(2010); // 开机需要拉低PWRK至少1s
	GPRS_PWRK_L;
	GPRS_DELAY_MS(100);
}

/************************************************************
*
* Function name	: gprs_shutdown_function
* Description	: 模块关机函数
* Parameter		: 
* Return		: 
*	 EC800E: RESET拉低至少50ms，或者PWR拉低至少650ms
*	 ML307: 拉低PWR_ON/OFF引脚3.5s~4s后释放，模组将执行关机流程

************************************************************/
void gprs_shutdown_function(void)
{
	GPRS_PWRK_H;
	GPRS_DELAY_MS(3600); // 关机需要拉低PWRK至少2s
	GPRS_PWRK_L;
	
}

/************************************************************
*
* Function name	: gprs_reset_function
* Description	: 重启函数
* Parameter		: 
* Return		: 
*	ML307: 拉低RESET引脚至少300ms或更长时间实现系统复位
************************************************************/
void gprs_reset_function(void)
{
	GPRS_NRST_H;
	GPRS_DELAY_MS(500); // 复位需要将NRST拉低50ms到100ms
	GPRS_NRST_L;
	GPRS_DELAY_MS(100);
}

/************************************************************
*
* Function name	: gprs_v_reset_function
* Description	: 断电重启函数
* Parameter		: 
* Return		: 
*	ML307: 关闭模块供电
************************************************************/
void gprs_v_reset_function(void)
{
	GPRS_CTRL_L;
	GPRS_DELAY_MS(10000); // 复位需要将NRST拉低50ms到100ms
	GPRS_CTRL_H;
}


/************************************************************
*
* Function name	: gprs_check_cmd_function
* Description	: 验证响应数据
* Parameter		: 
*	@str		: 期望应答的数据
* Return		: 0-没有的到期望数据 other-得到了期望数据
*	
************************************************************/
uint8_t* gprs_check_cmd_function(uint8_t *str) 
{
	char *strx=0;
	
//	if(GPRS_DEBUG)  printf("check_cmd:%s....",gprs_rx_buff);
	
	if(gprs_rx_status&0x8000) 
	{
		gprs_rx_status &= 0x7fff;
//		gprs_rx_buff[gprs_rx_status&0x7fff] = 0;
		strx = my_strstr((const char*)gprs_rx_buff,(const char*)str);
//		strx = strstr((const char*)gprs_rx_buff,(const char*)str);
	}
	
	return ((uint8_t*)strx);
}

/************************************************************
*
* Function name	: gprs_send_cmd_function
* Description	: 数据发送函数
* Parameter		: 
*	@cmd		: 命令
*	@ack		: 响应
*	@waittime	: 命令等待时间
* Return		: 
*	
************************************************************/
uint8_t gprs_send_cmd_function(uint8_t *cmd, uint8_t *ack, uint16_t waittime)
{
	uint8_t res = 0;
	
	sg_gprs_status_t.cmdon = 1;
	
	GPRS_STR_SEND(cmd,strlen((char*)cmd));
	
	if(ack && waittime) {
		while(--waittime) {
			if(gprs_check_cmd_function(ack) != NULL) {
				res = 0;
				break;
			}
			GPRS_DELAY_MS(10);
		}
		if(waittime == 0) {
			res = 2;
		}
	}
	
	return res;
}

/************************************************************
*
* Function name	: gprs_send_cmd_over_function
* Description	: 退出命令发送函数
* Parameter		: 
* Return		: 
*	
************************************************************/
void gprs_send_cmd_over_function(void)
{
	gprs_rx_status = 0;
	sg_gprs_status_t.cmdon = 0;
	memset(gprs_rx_buff,0,sizeof(gprs_rx_buff));
}

/************************************************************
*
* Function name	: gprs_deinit_function
* Description	: 初始化-清除
* Parameter		: 
* Return		: 
*	
************************************************************/
void gprs_deinit_function(void)
{
    memset(&sg_gprs_status_t,0,sizeof(struct gprs_status_t));
}

/************************************************************
*
* Function name	: gprs_status_check_function
* Description	: 状态监测函数
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t gprs_status_check_function(void)
{
	static uint8_t init_repeat = 0;
	static uint8_t repeat = 0;
	uint32_t temp1 = 0;
	uint32_t temp2 = 0;
	uint8_t  res   = 0;
	uint8_t  index = 0;
	uint8_t *p1 = NULL;
	uint32_t time[6] = {0};
    uint8_t addr_len = 0;
	
	switch(sg_gprs_status_t.step) {
		case 0:
			/* 数据清零 */
			repeat = 0;
			init_repeat = 0;
			/* 设备开机 */
			gprs_boot_up_function();
			sg_gprs_status_t.step = 1;
			break;
		case 1:
			gprs_reset_function();
			sg_gprs_status_t.step = 2;
			repeat = 0;
			if((++init_repeat) >= 3) {
				/* 初始化失败 */
				repeat = 0;
				init_repeat = 0;
				gprs_send_cmd_over_function();
				sg_gprs_status_t.mount = 0;
				sg_gprs_status_t.step = 0;
				return -1;
			}
			break;
		case 2:
			/* 通信检测 */
			if(gprs_send_cmd_function((uint8_t*)"AT\r\n",(uint8_t*)"OK",25) == 0) {
				gprs_send_cmd_function((uint8_t*)"ATE0\r\n",0,0); // 关闭回显
				sg_gprs_status_t.step = 3;
				sg_gprs_status_t.status.com = 1; // 通信正常
//				GPRS_DELAY_MS(2010); // 模组开机返回+MATREADY后，间隔至少2s才能执行AT+CFUN=0或AT+CFUN=1
				repeat = 0;
			}
			else {
				GPRS_DELAY_MS(10);
				sg_gprs_status_t.status.com = 0; // 通信异常：模块未启动、串口异常等
				repeat++;
				if(repeat > 30) {
					sg_gprs_status_t.step = 1;
				}
			}
			break;
		case 3:
			/* SIM卡状态检测 */
			memset(gprs_rx_buff,0,sizeof(gprs_rx_buff));
			if(gprs_send_cmd_function((uint8_t*)"AT+CPIN?\r\n",(uint8_t*)"READY",100) == 0) 
			{
				gprs_send_cmd_function((uint8_t*)"AT+MCFG=\"simhot\",0\"\r\n",0,0); // 关闭SIM检测
				GPRS_DELAY_MS(20);
				for(index=0; index<3; index++) {
					res = gprs_send_cmd_function((uint8_t*)"AT+MCCID\r\n",(uint8_t*)"+MCCID:",100);
					if(res == 0) {
						p1 = (uint8_t*)strstr((char*)gprs_rx_buff,"+MCCID: ");
						p1 += 8;
						memcpy(sg_gprs_status_t.ccid,p1,20);
						break;
					}
				}
				sg_gprs_status_t.step = 4;
				sg_gprs_status_t.status.sim = 1;
				repeat = 0;
			} else {
				GPRS_DELAY_MS(20);
				sg_gprs_status_t.status.sim = 0;
				repeat++;
				if(repeat > 30) {
					sg_gprs_status_t.step = 1;
					
					if((GPIO_ReadOutputDataBit(GPRS_Sel_GPIO, GPRS_Sel_PIN)) == 1)
					{
					  GPRS_Sel_L;	
					}
					else
					{
						GPRS_Sel_H;
						
					}
				}
			}
			break;
		case 4:
			/* 协议栈状态 */
			memset(gprs_rx_buff,0,sizeof(gprs_rx_buff));
			if(gprs_send_cmd_function((uint8_t*)"AT+CFUN?\r\n",(uint8_t*)"+CFUN: 1",25) == 0) {
				sg_gprs_status_t.step = 5;
				repeat = 0;
			} else {
				GPRS_DELAY_MS(20);
				repeat++;
				if(repeat > 10) {
					sg_gprs_status_t.step = 1;
				}
			}
			break;
		case 5:
			/* 信号强度 */
			memset(gprs_rx_buff,0,sizeof(gprs_rx_buff));
			if(gprs_send_cmd_function((uint8_t*)"AT+CSQ\r\n",(uint8_t*)"+CSQ: ",25) == 0) 
			{
				p1 = (uint8_t*)strstr((char*)gprs_rx_buff,"+CSQ: ");
				temp2 = 0;
				temp1 = 0;
				res = sscanf((char*)p1,"+CSQ: %d,%d",&temp1,&temp2);
				if(temp1 != 99 && res == 2) {
					sg_gprs_status_t.status.csq = temp1+1;
					sg_gprs_status_t.step = 6;
					repeat = 0;
				} else {
					GPRS_DELAY_MS(200);
					repeat++;
					if(repeat > 30) {
						sg_gprs_status_t.step = 1;
					}
				}
			} else {
				GPRS_DELAY_MS(200);
				repeat++;
				if(repeat > 30) {
					sg_gprs_status_t.step = 1;
				}
			}
			break;
		case 6:
			/* 网络注册状态 */
			memset(gprs_rx_buff,0,sizeof(gprs_rx_buff));                                                        
			if(gprs_send_cmd_function((uint8_t*)"AT+CEREG?\r\n",(uint8_t*)"+CEREG:",25) == 0) 
			{
				p1 = (uint8_t*)strstr((char*)gprs_rx_buff,"+CEREG:");
				temp2 = 0;
				temp1 = 0;
				res = sscanf((char*)p1,"+CEREG: %d,%d",&temp1,&temp2);
				if(temp1 == 0 && res == 2) 
				{
					gprs_send_cmd_function((uint8_t*)"AT+CEREG=2\r\n",0,0); //启用带有位置信息的网络注册 URC
				}
				
				if(GPRS_DEBUG)  printf("检测1:%s.....%d.....%d.....%d...\n",p1,temp1,temp2,res);
				
				if((temp2 == 1 || temp2 == 5) && res == 2) 
				{
					sg_gprs_status_t.status.net = 1;
					sg_gprs_status_t.step = 7;
					repeat = 0;
				} 
				else 
				{
					sg_gprs_status_t.status.net = 0;
					GPRS_DELAY_MS(260);
					repeat++;
					if(repeat > 50) {
						sg_gprs_status_t.step = 1;
					}
				}
			} else {
				sg_gprs_status_t.status.net = 0;
				GPRS_DELAY_MS(260);
				repeat++;
				if(repeat > 50) {
					sg_gprs_status_t.step = 1;
				}
			}
			break;
		case 7:
			/* 同步时间 */
			memset(gprs_rx_buff,0,sizeof(gprs_rx_buff));                                                        
			if(gprs_send_cmd_function((uint8_t*)"AT+CCLK?\r\n",(uint8_t*)"+CCLK: ",25) == 0) {
				p1 = (uint8_t*)strstr((char*)gprs_rx_buff,"+CCLK: ");
				if(p1 != NULL) {
					p1 += 8;
					memset(time,0,sizeof(time));
					sscanf((char*)p1,"%d/%d/%d,%d:%d:%d",&time[0],&time[1],&time[2],&time[3],&time[4],&time[5]);
					time[0] += 2000;
					app_set_current_time((int*)time,1);
					repeat = 0;
					sg_gprs_status_t.step = 8;
				} else {
					sg_gprs_status_t.status.net = 0;
					GPRS_DELAY_MS(200);
					repeat++;
					if(repeat > 20) {
						sg_gprs_status_t.step = 1;
					}
				}
			} else {
				sg_gprs_status_t.status.net = 0;
				GPRS_DELAY_MS(200);
				repeat++;
				if(repeat > 20) {
					sg_gprs_status_t.step = 1;
				}
			}
			break;
		case 8:  // 首先判断是否激活，未激活则手动激活
		  if(gprs_send_cmd_function((uint8_t*)"AT+MIPCALL?\r\n",(uint8_t*)"+MIPCALL:",100) == 0) 
			{
				p1 = (uint8_t*)strstr((char*)gprs_rx_buff,"+MIPCALL:");
				temp2 = 0;
				temp1 = 0;
				res = sscanf((char*)p1,"+MIPCALL: %d,%d",&temp1,&temp2);		
				if((temp2 == 1) && res == 2) 
				{
					sg_gprs_status_t.step = 10;
					repeat = 0;
				} 
				else 
				{
					GPRS_DELAY_MS(100);
					repeat++;
					if(repeat > 20) 
					{
						/* 设置移动APN   AT+CGDCONT=1,"IPV4V6","cmnet" //配置PDP上下文*/
						gprs_send_cmd_function((uint8_t*)"AT+CGDCONT=1,\"IP\",\"CMIOT\"\r\n",0,0);
						// AT+QICSGP=1,1,"UNINET","","",1
						// 场景ID  协议类型  APN接入点名称
						sg_gprs_status_t.step = 9;
					}
				}			
			}
			else 
			{
				sg_gprs_status_t.status.net = 0;
				GPRS_DELAY_MS(100);
				repeat++;
				if(repeat > 20) {
					sg_gprs_status_t.step = 1;
				}
			}
			break;
		case 9:
			/* 激活 PDP 场景 */
			memset(gprs_rx_buff,0,sizeof(gprs_rx_buff));
			if(gprs_send_cmd_function((uint8_t*)"AT+MIPCALL=1,1\r\n",(uint8_t*)"OK",25) == 0) 
			{
				gprs_send_cmd_function(0,(uint8_t*)"+MIPCALL:",200);
				p1 = (uint8_t*)strstr((char*)gprs_rx_buff,"+MIPCALL: ");
				temp2 = 0;
				temp1 = 0;
				res = sscanf((char*)p1,"+MIPCALL: %d,%d",&temp1,&temp2);

				if((temp2 == 1) && res == 2) {
					sg_gprs_status_t.step = 10;
					repeat = 0;
				} 
				else 
				{
					GPRS_DELAY_MS(260);
					repeat++;
					if(repeat > 20) {
						sg_gprs_status_t.step = 1;
					}
				}
			} 
			else 
			{
				GPRS_DELAY_MS(20);
				repeat++;
				if(repeat > 20) {
					sg_gprs_status_t.step = 1;
				}
			}
			break;
		case 10:
			/* 获取IP地址 */
			memset(gprs_rx_buff,0,sizeof(gprs_rx_buff));
			if(gprs_send_cmd_function((uint8_t*)"AT+CGPADDR=1\r\n",(uint8_t*)"+CGPADDR",50) == 0) // 读取场景ID为1 的IP地址
			{
				p1 = (uint8_t*)strstr((char*)gprs_rx_buff,"+CGPADDR: ");
				memset(sg_gprs_status_t.status.ip,0,sizeof(sg_gprs_status_t.status.ip));
				res = sscanf((char*)p1,"+CGPADDR: 1,\"%[^\"]",sg_gprs_status_t.status.ip);
				if(res == 1) 
				{
           sg_gprs_status_t.step = 11;
				} 
				else 
				{
					GPRS_DELAY_MS(200);
					repeat++;
					if(repeat > 20) {
					sg_gprs_status_t.status.net = 0;
					sg_gprs_status_t.step = 1;
					}
				}
			} 
			else 
			{
				GPRS_DELAY_MS(200);
				repeat++;
				if(repeat > 20) 
				{
					sg_gprs_status_t.step = 1;
          sg_gprs_status_t.status.net = 0;
				}
			}
			break;
		case 11:
			/* 查询模块版本信息 */
			memset(gprs_rx_buff,0,sizeof(gprs_rx_buff));
			if(gprs_send_cmd_function((uint8_t*)"AT+CGMR\r\n",(uint8_t*)"OK",100) == 0) // 读取场景ID为1 的IP地址
			{
				p1 = (uint8_t*)strstr((char*)gprs_rx_buff,"OK");
				if(p1 != NULL) 
				{
					addr_len = p1 - gprs_rx_buff - 6;
					memset(sg_gprs_status_t.model,0,sizeof(sg_gprs_status_t.model));
					memcpy(sg_gprs_status_t.model,gprs_rx_buff+2,addr_len);	
				}
			}
			sg_gprs_status_t.step = 12;
			break;		
		case 12:
			/* 查询模块IMEI */			
			memset(gprs_rx_buff,0,sizeof(gprs_rx_buff));
			if(gprs_send_cmd_function((uint8_t*)"AT+CGSN=1\r\n",(uint8_t*)"+CGSN: ",50) == 0) 
			{
				p1 = (uint8_t*)strstr((char*)gprs_rx_buff,"+CGSN: ");
				if(p1 != NULL) 
				{
					memset(sg_gprs_status_t.imei,0,sizeof(sg_gprs_status_t.imei));
					memcpy(sg_gprs_status_t.imei,p1+7,15);	
				}
			}
			sg_gprs_status_t.step = 13;
			break;
		default:
			/* 初始化完成 */
			sg_gprs_status_t.mount = 1;
			repeat = 0;
			init_repeat = 0;
			gprs_send_cmd_over_function();
			GPRS_DELAY_MS(1000);
			return 0; // 初始化完成
			//break;
	}
	
	/* 正在初始化 */
	return 1;
	
}

/************************************************************
*
* Function name	: gprs_network_connection_restart_function
* Description	: 网络连接重启函数
* Parameter		: 
* Return		: 
*	
************************************************************/
void gprs_network_connection_restart_function(void)
{
	gprs_network_disconnect_function(0);
	sg_gprs_status_t.network = 0;
}

/************************************************************
*
* Function name	: gprs_module_restart_function
* Description	: 模块重启函数
* Parameter		: 
* Return		: 
*	
************************************************************/
void gprs_module_restart_function(void)
{
	gprs_network_disconnect_function(0);
	sg_gprs_status_t.mount = 0;
}


/************************************************************
*
* Function name	: gprs_network_data_send_function
* Description	: 网络数据发送函数
* Parameter		: 
*	@data		: 数据指针
*	@len		: 数据长度
* Return		: 
*	
************************************************************/
uint8_t gprs_network_data_send_function(uint8_t *data, uint16_t len)
{
	uint8_t buff[32] = {0};
	uint8_t res      = 0;
//	sprintf((char*)buff,"AT+MIPSEND=%d,%d\r\n",1,len);
//	sprintf((char*)buff,"AT+QISEND=0\r\n"); // 发送可变长度
//	sprintf((char*)buff,"AT+QISEND=%d,%d\r\n",1,len);// 发送固定长度
//	res = gprs_send_cmd_function(buff,(uint8_t*)">",20);
//	gprs_send_cmd_over_function();

//	GPRS_STR_SEND(data,len);
////	usart3_send_char(0x1A); // 最后发送1A
//	GPRS_DELAY_MS(10);

	sprintf((char*)buff,"AT+MIPSEND=%d,%d\r\n",1,len);
	res = gprs_send_cmd_function(buff,(uint8_t*)">",20);
	gprs_send_cmd_over_function();
	if(res == 0) {
		GPRS_STR_SEND(data,len);
	} else {
		GPRS_STR_SEND(data,len);
	}
	
	GPRS_DELAY_MS(10);
	
	return res;
}

/************************************************************
*
* Function name	: gprs_network_disconnect_function
* Description	: 连接断开函数
* Parameter		: 
* Return		: 
*	
************************************************************/
void gprs_network_disconnect_function(uint8_t data)
{
	uint8_t buff[32] = {0};
	
	sprintf((char*)buff,"AT+MIPCLOSE=%d\r\n",1);
	gprs_send_cmd_function(buff,0,0);
	GPRS_DELAY_MS(10);
	sg_gprs_status_t.network = 0;
	gprs_send_cmd_over_function();
	
}

/************************************************************
*
* Function name	: gprs_network_connect_function
* Description	: 网络连接函数
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t gprs_network_connect_function(uint8_t *ip, uint8_t *port) 
{
	uint8_t buff[64] = {0};
	int8_t ret 	 = 0;
	uint32_t temp1 = 0;
	uint32_t temp2 = 0;
	uint8_t  res   = 0;
	uint8_t *p1 = NULL;
	
	/* 连接断开操作 */
	gprs_network_disconnect_function(0);
	/* 开始连接 */
	sprintf((char*)buff,"AT+MIPOPEN=%d,\"TCP\",\"%s\",%s,100,0\r\n",1,ip,port);
	if(gprs_send_cmd_function((uint8_t*)buff,(uint8_t*)"OK",100) == 0) 
	{
		gprs_send_cmd_function(0,(uint8_t*)"+MIPOPEN:",500);
		p1 = (uint8_t*)strstr((char*)gprs_rx_buff,"+MIPOPEN: ");
		temp2 = 0;
		temp1 = 0;
		res = sscanf((char*)p1,"+MIPOPEN: %d,%d",&temp1,&temp2);
		if(temp2 == 0 && res == 2)
		{ 
			sg_gprs_status_t.network = 1;
		} 
		else 
		{
			ret = -1;
		}
	} 
	else 
	{
		ret = -1;
	}
	
	gprs_send_cmd_over_function();
	
	return ret;
}

/************************************************************
*
* Function name	: gprs_mult_network_connect_function
* Description	: 多链路网络连接函数
* Parameter		: 
*	@ip			: ip地址
*	@port		: 端口
*	@mult		: 多链路
* Return		: 0-正常 other-异常
*	
************************************************************/
uint8_t gprs_mult_network_connect_function(uint8_t *ip, uint8_t *port, uint8_t mult)
{
	uint16_t dport = mult+1102;
	uint8_t buff[64] = {0};
	int8_t ret 	 = 0;
	
	sprintf((char*)buff,"AT+MIPOPEN=%d,\"TCP\",\"%s\",%s,100,0,1,1,%d\r\n",mult,ip,port,dport);
	if(gprs_send_cmd_function((uint8_t*)buff,(uint8_t*)"OK",100) == 0) {
		ret = gprs_send_cmd_function(0,(uint8_t*)"CONNECT OK",500);
		if(ret == 0) {
			gprs_send_cmd_over_function();
		}
	} else {
		ret = -1;
	}
	
	return ret;
}

/************************************************************
*
* Function name	: gprs_network_status_monitoring_function
* Description	: 网络状态监测函数
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t gprs_network_status_monitoring_function(void)
{
	uint32_t temp1;
	uint32_t temp2;
	uint8_t  res = 0;
	uint8_t  index = 0;
	uint8_t  *p1 = 0;
	
	for(index=0; index<3; index++) 
	{
		if(GPRS_DEBUG)  printf("发送 \n");
		memset(gprs_rx_buff,0,sizeof(gprs_rx_buff));
		if(gprs_send_cmd_function((uint8_t*)"AT+CEREG?\r\n",(uint8_t*)"+CEREG:",50) == 0) 
		{
			p1 = (uint8_t*)strstr((char*)gprs_rx_buff,"+CEREG:");
			res = sscanf((char*)p1,"+CEREG: %d,%d",&temp1,&temp2);
			
			if(GPRS_DEBUG)  printf("检测2:%s.....%d.....%d.....%d...\n",p1,temp1,temp2,res);
			
			if(res == 2 && (temp2 == 1 || temp2 == 5)) 
			{
				gprs_send_cmd_over_function();
				return 0;
			}
		}
		GPRS_DELAY_MS(200);
	}
	gprs_send_cmd_over_function();
	return -1;
}

/************************************************************
*
* Function name	: gprs_get_module_status_function
* Description	: 获取模块状态
* Parameter		: 
* Return		: 
*	
************************************************************/
uint8_t gprs_get_module_status_function(void)
{
	return sg_gprs_status_t.mount;
}

/************************************************************
*
* Function name	: gprs_get_module_init_state
* Description	: 获取模块初始化状态
* Parameter		: 
* Return		: 
*	
************************************************************/
uint8_t gprs_get_module_init_state(void)
{
    switch(sg_gprs_status_t.step) {
        case 3:
            return 1; // 查找sim卡
        case 4: 
        case 5:
            return 2; // 查找信号
        case 6:
            return 3; // 注册网络
        case 7:
            return 4; // 同步时间
        case 8:
        case 9:
            return 5; // 启动网络服务
        default:
            return 0; // 模块初始化
    } 
}

/************************************************************
*
* Function name	: gprs_get_tcp_status
* Description	: 获取TCP连接状态
* Parameter		: 
* Return		: 
*	
************************************************************/
uint8_t gprs_get_tcp_status(void)
{
	return sg_gprs_status_t.network;
}

/************************************************************
*
* Function name	: gprs_get_csq_function
* Description	: 获取模块信号强度
* Parameter		: 
* Return		: 
*	
************************************************************/
uint8_t gprs_get_csq_function(void)
{
	return sg_gprs_status_t.status.csq;
}

/************************************************************
*
* Function name	: gprs_get_ip_function
* Description	: 获取ip地址信息
* Parameter		: 
* Return		: 
*	
************************************************************/
void *gprs_get_ip_addr_function(void)
{
	return sg_gprs_status_t.status.ip;
}

/************************************************************
*
* Function name	: gprs_get_receive_data_function
* Description	: 获取通信数据或命令数据
* Parameter		: 
* Return		: 
*	
************************************************************/
void gprs_get_receive_data_function(uint8_t *buff, uint16_t len)
{
	uint16_t index = 0; 
	int8_t   ret   = 0;

	if(len == 0 || buff == NULL) 
		return;
	
	/* 更新数据检测 */
	ret = update_gsm_recevie_data_function(buff,len);
	if(ret == 0) {
		return;
	}
		
	/* 检测当前模块模式 */
	if(sg_gprs_status_t.cmdon == 1) 
	{
		for(index=0; index < len; index++)
		{
			gprs_rx_buff[index] = buff[index];
			buff[index] 		= 0;
		}
		gprs_rx_status = len | 0x8000;
	} 
	else 
	{
		/* 检测数据是否有接收数据 */
		com_stroage_cache_data(buff,len);
	}
}

/************************************************************
*
* Function name	: gprs_get_infor_data_function
* Description	: 获取模块数据指针
* Parameter		: 
* Return		: 指针
*	
************************************************************/
void* gprs_get_infor_data_function(void)
{
	return &sg_gprs_status_t;
}

/************************************************************
*
* Function name	: gprs_get_rec_buff_function
* Description	: 获取接收数据
* Parameter		: 
* Return		: 
*	
************************************************************/
uint8_t* gprs_get_rec_buff_function(uint16_t *len)
{
	*len = gprs_rx_status&0x7fff;
	return gprs_rx_buff;
}

/************************************************************
*
* Function name	: gprs_get_ccid_function
* Description	: 获取卡号
* Parameter		: 
* Return		: 
*	
************************************************************/
uint8_t *gprs_get_ccid_function(void)
{
	return sg_gprs_status_t.ccid;
}
/************************************************************
*
* Function name	: gprs_get_model_soft_function
* Description	: 获取模块型号
* Parameter		: 
* Return		: 
*	
************************************************************/
uint8_t *gprs_get_model_soft_function(void)
{
	return sg_gprs_status_t.model;
}
/************************************************************
*
* Function name	: gprs_get_imei_function
* Description	: 获取模块imei
* Parameter		: 
* Return		: 
*	
************************************************************/
uint8_t *gprs_get_imei_function(void)
{
	return sg_gprs_status_t.imei;
}

/*
当*start！='\0'的时候，就把start赋值给s1，让他去查找,把str2赋值给s2
让s2也从起始位置开始，然后循环的判断条件 *s1 != '\0' && *s2 != '\0' && *s1 == *s2
然后s1和s2进行加加，加加完了之后，再上去判断，当有一次，s1或者s2
等于'\0’的时候，或者他们不相等的时候，就跳出来，如果*str2=='\0'的时候，就是找到了，然后跳出来
如果找不到的话，就返回空指针。
如果要找一个空字符串的话（特殊情况）：
在库里面，对于这种特殊情况的处理，就是直接返回str1
*/
char* my_strstr(const char* str1, const char* str2)
{
	const char* s1 = str1;
	const char* s2 = str2;
	const char* start = str1;
	if (*str2 == '\0')
	{
		return (char *)str1;   //找空字符串，直接返回str1
	}
	while (*start!='\0')//当start遇到'\0'的时候就没有比要再继续查找了，那一定是查找不到的了
	{
		s2 = str2;
		s1 = start;
		while ( *s1 == *s2)
		{
			s1++;
			s2++;
		}
		if (*s2 == '\0')
		{
			return (char *)start;
		}
		start++;
	}
	return NULL;
}

