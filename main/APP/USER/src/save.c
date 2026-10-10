#include "save.h"
#include "lfs.h"
#include "lfs_port.h"


/* 其它参数 */
#define SAVEL_OTHER_PARAM_NAME  ("localnetwork.bin")

/* 本地网络信息 */
#define SAVE_LOCAL_NETWORK_NAME  ("network_name")

/* 远端网络信息 */
#define SAVE_REMOTE_NETWORK_NAME ("remote_name")

/* 远端网络——备份 20231023*/ 
#define SAVE_REMOTE_BACKUPS_NAME ("backups")
/* 设备详细:id、名称、密码等 */
#define SAVE_DEVICE_PARAMETER_NAME ("device_name")

/* 外设控制参数：温度控制、摄像头IP */
#define SAVE_COMPARISION_PARAMETER ("comparision_param")

/* 摄像头相关信息 */
#define SAVE_CAREMA_PARAMETER ("carema_param")

/* 通信相关参数 */
#define SAVE_COM_PARAMETER_NAME ("comparameter")

/* 更新地址 */
#define SAVE_UPDATE_FILE_NAME ("updateaddr")

/* 新增服务器信息 */
#define SAVE_ONLY_SEND_IP_NAME ("only_send_ip")

/* 相关阈值：电压 电流 角度 */
#define SAVE_THRESHOLD_PARAMETER ("threshold_params")  // 20230720

/* 更新文件信息 */
#define SAVE_UPDATE_FILE_INFOR_NAME ("upfileinfor.bin")


/************************************************************
*
* Function name	: save_init_function
* Description	: 存储功能初始化函数
* Parameter		: 
* Return		: 
*	
************************************************************/
void save_init_function(void)
{
	lfs_init_function();     						// 挂载文件系统
}

/************************************************************
*
* Function name	: save_clear_file_function
* Description	: 恢复出厂化
* Parameter		: 
* Return		: 
*	
************************************************************/
void save_clear_file_function(uint8_t mode)
{
	if(mode == 0)
	{
		lfs_remove(&g_lfs_t,SAVEL_OTHER_PARAM_NAME);
		lfs_remove(&g_lfs_t,SAVE_LOCAL_NETWORK_NAME);
		lfs_remove(&g_lfs_t,SAVE_REMOTE_NETWORK_NAME);
		lfs_remove(&g_lfs_t,SAVE_DEVICE_PARAMETER_NAME);
		lfs_remove(&g_lfs_t,SAVE_COMPARISION_PARAMETER);
		lfs_remove(&g_lfs_t,SAVE_COM_PARAMETER_NAME);
		lfs_remove(&g_lfs_t,SAVE_UPDATE_FILE_NAME);
		lfs_remove(&g_lfs_t,SAVE_CAREMA_PARAMETER);  		//	20230712
		lfs_remove(&g_lfs_t,SAVE_THRESHOLD_PARAMETER);  //	20230720
		
	}
	else if(mode == 1)
	{
		lfs_remove(&g_lfs_t,SAVE_LOCAL_NETWORK_NAME);
	} 
	else if(mode == 2) 
	{
    lfs_remove(&g_lfs_t,SAVE_UPDATE_FILE_INFOR_NAME);
  }
}

/************************************************************
*
* Function name	: save_stroage_local_network
* Description	: 存储本地网络参数
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t save_stroage_local_network(struct local_ip_t *local)
{
	int8_t		ret      = 0;
 	int 		err 	 = 0;
	lfs_file_t  lfs_fp   = {0};
	
	/* 数据保存 */
	err = lfs_file_open(&g_lfs_t, &lfs_fp, SAVE_LOCAL_NETWORK_NAME, LFS_O_RDWR | LFS_O_CREAT);
	if(err == 0)
	{
		err = lfs_file_rewind(&g_lfs_t, &lfs_fp);
		err = lfs_file_write(&g_lfs_t, &lfs_fp, (uint8_t*)local, sizeof(struct local_ip_t));
		if(err != sizeof(struct local_ip_t)) {
			err = lfs_file_write(&g_lfs_t, &lfs_fp, (uint8_t*)local, sizeof(struct local_ip_t));
		}
	}
	else
	{
		ret = -1;
	}
	err = lfs_file_close(&g_lfs_t, &lfs_fp);
	
	return ret;
	
}

/************************************************************
*
* Function name	: save_read_local_network
* Description	: 读取本地网络设置
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t save_read_local_network(struct local_ip_t *local)
{
	int8_t		ret      = 0;
	int 		err 	 = 0;
	lfs_file_t  lfs_fp   = {0};
	
	err = lfs_file_open(&g_lfs_t, &lfs_fp, SAVE_LOCAL_NETWORK_NAME, LFS_O_RDWR);
	if(SAVE_DEBUG)  printf("save_read_local_network:%d\r\n",err);
	
	if(err == 0)
	{
		err = lfs_file_rewind(&g_lfs_t, &lfs_fp);
		err = lfs_file_read(&g_lfs_t, &lfs_fp, local,sizeof(struct local_ip_t));
	}
	else
	{
		/* 读取默认值 */
		err = lfs_file_close(&g_lfs_t, &lfs_fp);
		
		save_read_default_local_network(local);
		save_stroage_local_network(local);
		ret = -1;
	}
	err = lfs_file_close(&g_lfs_t, &lfs_fp);
	
	return ret;
}

/************************************************************
*
* Function name	: save_read_default_local_network
* Description	: 读取默认参数
* Parameter		: 
* Return		: 
*	
************************************************************/
void save_read_default_local_network(struct local_ip_t *local)
{
	uint32_t data= 0;
	uint8_t  mac[8] = {0}; 			// MAC地址 
	uint8_t  ret = 0;
	
	data = *(vu32*)(0X1FFF7A10);
	
	/* 本机ip地址 */
	local->ip[0] = DEFALUT_LOCAL_IP0;
	local->ip[1] = DEFALUT_LOCAL_IP1;
	local->ip[2] = DEFALUT_LOCAL_IP2;
	local->ip[3] = DEFALUT_LOCAL_IP3;
	sprintf(local->ip_str,"%d.%d.%d.%d",local->ip[0],local->ip[1],local->ip[2],local->ip[3]);
	
	/* 本机MAC */
	STMFLASH_Read(DEVICE_MAC_ADDR, (uint32_t *)mac, 2);
	for(uint8_t i=0;i<6;i++)
	{
		if(mac[i] == 0xFF)
			ret++;	
	}
	if(ret == 6)
	{
		local->mac[0]=2;//高三字节(IEEE称之为组织唯一ID,OUI)地址固定为:2.0.0
		local->mac[1]=0;
		local->mac[2]=0;
		local->mac[3]=(data>>16)&0XFF;//低三字节用STM32的唯一ID
		local->mac[4]=(data>>8)&0XFF;
		local->mac[5]=data&0XFF; 	
	}	
	else
	{
		local->mac[0]=mac[0];
		local->mac[1]=mac[1];
		local->mac[2]=mac[2];
		local->mac[3]=mac[3];
		local->mac[4]=mac[4];
		local->mac[5]=mac[5]; 
	}
	STMFLASH_Write_SAVE(DEVICE_FLASH_STORE,DEVICE_MAC_ADDR,(uint32_t *)&local->mac,2);		

	/* 本机子网掩码 */
	local->netmask[0]=DEFALUT_NETMASK0;	
	local->netmask[1]=DEFALUT_NETMASK1;
	local->netmask[2]=DEFALUT_NETMASK2;
	local->netmask[3]=DEFALUT_NETMASK3;
	/* 本机默认网关 */
	local->gateway[0]=DEFALUT_GATEWAY0;	
	local->gateway[1]=DEFALUT_GATEWAY1;
	local->gateway[2]=DEFALUT_GATEWAY2;
	local->gateway[3]=DEFALUT_GATEWAY3;	
	
	/* 本机DNS */
	local->dns[0] = DEFALUT_DNS0;
	local->dns[1] = DEFALUT_DNS1;
	local->dns[2] = DEFALUT_DNS2;
	local->dns[3] = DEFALUT_DNS3;
	
	local->port = DEFALUT_PORT; // 自动模式
	
	local->server_mode = DEFALUT_SERVERMODE; // 同时连接
	
	/* 组播地址 */
	local->multicast_ip[0] = DEFALUT_MULTICAST_IP0;
	local->multicast_ip[1] = DEFALUT_MULTICAST_IP1;
	local->multicast_ip[2] = DEFALUT_MULTICAST_IP2;
	local->multicast_ip[3] = DEFALUT_MULTICAST_IP3;

	local->multicast_port = DEFALUT_MULTICAST_PORT;
	memset(local->ping_ip,0,sizeof(local->ping_ip));
	memset(local->ping_sub_ip,0,sizeof(local->ping_sub_ip));
	
	local->search_mode = 1; // PING模式
}

/************************************************************
*
* Function name	: save_stroage_remote_ip_function
* Description	: 
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t save_stroage_remote_ip_function(struct remote_ip *remote)
{
	int8_t		ret      = 0;
 	int 		err 	 = 0;
	lfs_file_t  lfs_fp	 = {0};
	
	/* 数据保存 */
	err = lfs_file_open(&g_lfs_t, &lfs_fp, SAVE_REMOTE_NETWORK_NAME, LFS_O_RDWR | LFS_O_CREAT);
	if(err == 0)
	{
		err = lfs_file_rewind(&g_lfs_t, &lfs_fp);
		err = lfs_file_write(&g_lfs_t, &lfs_fp, (uint8_t*)remote, sizeof(struct remote_ip));
		if(err != sizeof(struct remote_ip)) {
			err = lfs_file_write(&g_lfs_t, &lfs_fp, (uint8_t*)remote, sizeof(struct remote_ip));
		}
	}
	else
	{
		ret = -1;
	}
	err = lfs_file_close(&g_lfs_t, &lfs_fp);
	
	return ret;
}

/************************************************************
*
* Function name	: save_read_remote_ip_function
* Description	: 
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t save_read_remote_ip_function(struct remote_ip *remote)
{
	int8_t		ret      = 0;
	int 		err 	 = 0;
	lfs_file_t  lfs_fp   = {0};
	
	err = lfs_file_open(&g_lfs_t, &lfs_fp, SAVE_REMOTE_NETWORK_NAME, LFS_O_RDWR);
	if(SAVE_DEBUG)  printf("save_read_remote_ip_function:%d\r\n",err);
	if(err == 0)
	{
		err = lfs_file_rewind(&g_lfs_t, &lfs_fp);
		err = lfs_file_read(&g_lfs_t, &lfs_fp, remote,sizeof(struct remote_ip));
	}
	else
	{
		/* 读取默认值 */
		err = lfs_file_close(&g_lfs_t, &lfs_fp);
		
		save_read_default_remote_ip(remote);
		save_stroage_remote_ip_function(remote);
		
		ret = -1;
	}
	err = lfs_file_close(&g_lfs_t, &lfs_fp);
	
	return ret;

}

/************************************************************
*
* Function name	: save_read_default_remote_ip
* Description	: 读取默认值
* Parameter		: 
* Return		: 
*	
************************************************************/
void save_read_default_remote_ip(struct remote_ip *remote)
{
	/* 远程服务器数据 */
	memset(remote->outside_iporname,0,sizeof(remote->outside_iporname));
	strcpy((char*)remote->outside_iporname,"test1.fnwlw.net");
	remote->outside_port  = 6102;
}


/************************************************************
*
* Function name	: save_stroage_comparision_parameter
* Description	: 
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t save_stroage_comparision_parameter(comparision_parameter_t *param)
{
	int8_t		ret      = 0;
 	int 		err 	 = 0;
	lfs_file_t  lfs_fp   = {0};
	
	/* 数据保存 */
	err = lfs_file_open(&g_lfs_t, &lfs_fp, SAVE_COMPARISION_PARAMETER, LFS_O_RDWR | LFS_O_CREAT);
	if(err == 0)
	{
		err = lfs_file_rewind(&g_lfs_t, &lfs_fp);
		err = lfs_file_write(&g_lfs_t, &lfs_fp, (uint8_t*)param, sizeof(comparision_parameter_t));
		if(err != sizeof(comparision_parameter_t)) {
			err = lfs_file_write(&g_lfs_t, &lfs_fp, (uint8_t*)param, sizeof(comparision_parameter_t));
		}
	}
	else
	{
		ret = -1;
	}
	err = lfs_file_close(&g_lfs_t, &lfs_fp);
	return ret;
	
}

/************************************************************
*
* Function name	: save_read_default_comparision_parameter
* Description	: 读取默认参数
* Parameter		: 
* Return		: 
*	
************************************************************/
void save_read_default_comparision_parameter(comparision_parameter_t *param)
{
	memset(param->ip,0,sizeof(param->ip));
}

/************************************************************
*
* Function name	: save_read_comparision_parameter
* Description	: 
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t save_read_comparision_parameter(comparision_parameter_t *param)
{
	int8_t		ret      = 0;
	int 		err 	 = 0;
	lfs_file_t  lfs_fp   = {0};
	
	err = lfs_file_open(&g_lfs_t, &lfs_fp, SAVE_COMPARISION_PARAMETER, LFS_O_RDWR);
	if(SAVE_DEBUG)  printf("save_read_comparision_parameter:%d\r\n",err);
	if(err == 0)
	{
		err = lfs_file_rewind(&g_lfs_t, &lfs_fp);
		err = lfs_file_read(&g_lfs_t, &lfs_fp, param,sizeof(comparision_parameter_t));
	}
	else
	{
		err = lfs_file_close(&g_lfs_t, &lfs_fp);
		save_read_default_comparision_parameter(param);
		/* 存储 */
		save_stroage_comparision_parameter(param);
		ret = -1;
	}
	err = lfs_file_close(&g_lfs_t, &lfs_fp);
	
	return ret;
	
}

/************************************************************
*
* Function name	: save_storage_device_parameter_function
* Description	: 存储设备相关参数：ID、密码等
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t save_storage_device_parameter_function(struct device_param *param)
{
	int8_t		ret      = 0;
 	int 		err 	 = 0;
	lfs_file_t  lfs_fp   = {0};
	
	/* 数据保存 */
	err = lfs_file_open(&g_lfs_t, &lfs_fp, SAVE_DEVICE_PARAMETER_NAME, LFS_O_RDWR | LFS_O_CREAT);
	if(err == 0)
	{
		err = lfs_file_rewind(&g_lfs_t, &lfs_fp);
		err = lfs_file_write(&g_lfs_t, &lfs_fp, (uint8_t*)param, sizeof(struct device_param));
		if(err != sizeof(struct device_param)) {
			err = lfs_file_write(&g_lfs_t, &lfs_fp, (uint8_t*)param, sizeof(struct device_param));
		}
	}
	else
	{
		ret = -1;
	}
	err = lfs_file_close(&g_lfs_t, &lfs_fp);
	
	return ret;
	
}

/************************************************************
*
* Function name	: save_read_default_device_paramter_function
* Description	: 读取设备相关参数：ID、密码等
* Parameter		: 
* Return		: 
*	
************************************************************/
void save_read_default_device_paramter_function(struct device_param *param)
{
	union i_c data_id;		  		// id
	
	STMFLASH_Read(DEVICE_ID_ADDR,(uint32_t*)data_id.c,1);
	if(data_id.i == 0xFFFFFFFF)
	{
		param->id.i = 3;
	}	
	else
	{
		param->id.i = data_id.i;
	}
	STMFLASH_Write_SAVE(DEVICE_FLASH_STORE,DEVICE_ID_ADDR,(uint32_t*)param->id.c,1);
	memset(param->name,0,sizeof(param->name));
	memset(param->password,0,sizeof(param->password));
	strcpy((char*)param->password,DEFALUT_PASSWORD);
	param->default_password = 1; // 默认开机需要修改密码
}

/************************************************************
*
* Function name	: save_read_device_paramter_function
* Description	: 读取设备相关参数：ID、密码等
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t save_read_device_paramter_function(struct device_param *param)
{
	int8_t		ret      = 0;
	int 		err 	 = 0;
	lfs_file_t  lfs_fp   = {0};
	
	err = lfs_file_open(&g_lfs_t, &lfs_fp, SAVE_DEVICE_PARAMETER_NAME, LFS_O_RDWR);
	if(SAVE_DEBUG)  printf("save_read_device_paramter_function:%d\r\n",err);
	if(err == 0)
	{
		err = lfs_file_rewind(&g_lfs_t, &lfs_fp);
		err = lfs_file_read(&g_lfs_t, &lfs_fp, param,sizeof(struct device_param));
	}
	else
	{
		err = lfs_file_close(&g_lfs_t, &lfs_fp);
		save_read_default_device_paramter_function(param);
		/* 存储 */
		save_storage_device_parameter_function(param);
		ret = -1;
	}
	err = lfs_file_close(&g_lfs_t, &lfs_fp);
	
	return ret;
}

/************************************************************
*
* Function name	: save_stroage_com_param_function
* Description	: 存储通信相关参数
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t save_stroage_com_param_function(com_param_t *param)
{
	int8_t		ret      = 0;
 	int 		err 	 = 0;
	lfs_file_t  lfs_fp   = {0};
	
	/* 数据保存 */
	err = lfs_file_open(&g_lfs_t, &lfs_fp, SAVE_COM_PARAMETER_NAME, LFS_O_RDWR | LFS_O_CREAT);
	if(err == 0)
	{
		err = lfs_file_rewind(&g_lfs_t, &lfs_fp);
		err = lfs_file_write(&g_lfs_t, &lfs_fp, (uint8_t*)param, sizeof(com_param_t));
		if(err != sizeof(com_param_t)) {
			err = lfs_file_write(&g_lfs_t, &lfs_fp, (uint8_t*)param, sizeof(com_param_t));
		}
	}
	else
	{
		ret = -1;
	}
	err = lfs_file_close(&g_lfs_t, &lfs_fp);
	
	return ret;
}

/************************************************************
*
* Function name	: 
* Description	: 
* Parameter		: 
* Return		: 
*	
************************************************************/
void save_read_default_com_param_function(com_param_t *param)
{
	param->heart  			= DEFALUT_HEART; // 90s
	param->report 			= DEFALUT_REPORT; // 60s
	param->ping   			= DEFALUT_PING;
	param->dev_ping 		= DEFALUT_DEV_PING;
	param->network_time = DEFALUT_NETWORK_DELAY;  // 网络延时时间  20220308
	param->fan_time  	= 0; 
	param->reload  	    = DEFALUT_RELOAD_TIME; // 重启时间       20240904
}

/************************************************************
*
* Function name	: save_read_com_param_function
* Description	: 读取通信相关参数
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t save_read_com_param_function(com_param_t *param)
{
	int8_t		ret      = 0;
	int 		err 	 = 0;
	lfs_file_t  lfs_fp   = {0};
	
	err = lfs_file_open(&g_lfs_t, &lfs_fp, SAVE_COM_PARAMETER_NAME, LFS_O_RDWR);
	if(SAVE_DEBUG)  printf("save_read_com_param_function:%d\r\n",err);
	if(err == 0)
	{
		err = lfs_file_rewind(&g_lfs_t, &lfs_fp);
		err = lfs_file_read(&g_lfs_t, &lfs_fp, param,sizeof(com_param_t));
	}
	else
	{
		err = lfs_file_close(&g_lfs_t, &lfs_fp);
		save_read_default_com_param_function(param);
		/* 存储 */
		save_stroage_com_param_function(param);
		ret = -1;
	}
	err = lfs_file_close(&g_lfs_t, &lfs_fp);
	
	return ret;

}


/************************************************************
*
* Function name	: save_stroage_update_addr
* Description	: 存储更新地址
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t save_stroage_update_addr(uint8_t *ip,uint32_t port) 
{
	int8_t		ret      = 0;
 	int 		err 	 = 0;
	lfs_file_t  lfs_fp   = {0};
	uint8_t 	temp[8]  = {0};
	
	temp[0] = ip[0];
	temp[1] = ip[1];
	temp[2] = ip[2];
	temp[3] = ip[3];
	temp[4] = (port)&0xff;
	temp[5] = (port>>8)&0xff;
	temp[6] = (port>>16)&0xff;
	temp[7] = (port>>24)&0xff;
	
	/* 数据保存 */
	err = lfs_file_open(&g_lfs_t, &lfs_fp, SAVE_UPDATE_FILE_NAME, LFS_O_RDWR | LFS_O_CREAT);
	if(err == 0)
	{
		err = lfs_file_rewind(&g_lfs_t, &lfs_fp);
		err = lfs_file_write(&g_lfs_t, &lfs_fp, temp, sizeof(temp));
		if(err != sizeof(temp)) {
			err = lfs_file_write(&g_lfs_t, &lfs_fp, temp, sizeof(temp));
		}
	}
	else
	{
		ret = -1;
	}
	err = lfs_file_close(&g_lfs_t, &lfs_fp);
	
	return ret;
	
}

/************************************************************
*
* Function name	: save_read_update_addr
* Description	: 读取更新地址
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t save_read_update_addr(uint8_t *ip,uint32_t *port)
{
	int8_t		ret      = 0;
	int 		err 	 = 0;
	lfs_file_t  lfs_fp   = {0};
	uint32_t	data	 = 0;
	uint8_t 	temp[8]  = {0};
	
	err = lfs_file_open(&g_lfs_t, &lfs_fp, SAVE_UPDATE_FILE_NAME, LFS_O_RDWR);
	if(SAVE_DEBUG)  printf("save_read_update_addr:%d\r\n",err);
	
	if(err == 0)
	{
		err = lfs_file_rewind(&g_lfs_t, &lfs_fp);
		err = lfs_file_read(&g_lfs_t, &lfs_fp, temp,sizeof(temp));
		
		ip[0] = temp[0];
		ip[1] = temp[1];
		ip[2] = temp[2];
		ip[3] = temp[3];
		data  = temp[7];
		data  = data<<8 | temp[6];
		data  = data<<8 | temp[5];
		data  = data<<8 | temp[4];
		*port = data;
	}
	else
	{
		err = lfs_file_close(&g_lfs_t, &lfs_fp);
		ip[0] = 0;
		ip[1] = 0;
		ip[2] = 0;
		ip[3] = 0;
		*port = 0;
		
		ret = -1;
	}
	err = lfs_file_close(&g_lfs_t, &lfs_fp);
	
	return ret;

}

/************************************************************
*
* Function name	: save_stroage_carema_parameter
* Description	: 
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t save_stroage_carema_parameter(carema_t *param)
{
	int8_t		ret      = 0;
 	int 		err 	 = 0;
	lfs_file_t  lfs_fp   = {0};
	
	/* 数据保存 */
	err = lfs_file_open(&g_lfs_t, &lfs_fp, SAVE_CAREMA_PARAMETER, LFS_O_RDWR | LFS_O_CREAT);
	if(err == 0)
	{
		err = lfs_file_rewind(&g_lfs_t, &lfs_fp);
		err = lfs_file_write(&g_lfs_t, &lfs_fp, (uint8_t*)param, sizeof(carema_t));
		if(err != sizeof(carema_t)) 
		{
			err = lfs_file_write(&g_lfs_t, &lfs_fp, (uint8_t*)param, sizeof(carema_t));
		}
	}
	else
	{
		ret = -1;
	}
	err = lfs_file_close(&g_lfs_t, &lfs_fp);
	return ret;
	
}

/************************************************************
*
* Function name	: save_read_default_carema_parameter
* Description	: 读取默认参数
* Parameter		: 
* Return		: 
*	
************************************************************/
void save_read_default_carema_parameter(carema_t *param)
{
	memset(param->name,0,sizeof(param->name));	
	memset(param->pwd,0,sizeof(param->pwd));	
	memset(param->port,0,sizeof(param->port));	
}

/************************************************************
*
* Function name	: save_read_carema_parameter
* Description	: 
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t save_read_carema_parameter(carema_t *param)
{
	int8_t		ret      = 0;
	int 		err 	 = 0;
	lfs_file_t  lfs_fp   = {0};
	
	err = lfs_file_open(&g_lfs_t, &lfs_fp, SAVE_CAREMA_PARAMETER, LFS_O_RDWR);
	if(SAVE_DEBUG)  printf("save_read_carema_parameter:%d\r\n",err);
	if(err == 0)
	{
		err = lfs_file_rewind(&g_lfs_t, &lfs_fp);
		err = lfs_file_read(&g_lfs_t, &lfs_fp, param,sizeof(carema_t));
	}
	else
	{
		err = lfs_file_close(&g_lfs_t, &lfs_fp);
		save_read_default_carema_parameter(param);
		save_stroage_carema_parameter(param);
		ret = -1;
	}
	err = lfs_file_close(&g_lfs_t, &lfs_fp);
	
	return ret;
	
}

/************************************************************
*
* Function name	: save_stroage_threshold_parameter
* Description	: 
* Parameter		: 
* Return		: 
*	        20230720
************************************************************/
int8_t save_stroage_threshold_parameter(struct threshold_params *param)
{
	int8_t	ret    = 0;
 	int 		err 	 = 0;
	lfs_file_t  lfs_fp   = {0};
	
	/* 数据保存 */
	err = lfs_file_open(&g_lfs_t, &lfs_fp, SAVE_THRESHOLD_PARAMETER, LFS_O_RDWR | LFS_O_CREAT);
	if(err == 0)
	{
		err = lfs_file_rewind(&g_lfs_t, &lfs_fp);
		err = lfs_file_write(&g_lfs_t, &lfs_fp, (uint8_t*)param, sizeof(struct threshold_params));
		if(err != sizeof(struct threshold_params)) 
		{
			err = lfs_file_write(&g_lfs_t, &lfs_fp, (uint8_t*)param, sizeof(struct threshold_params));
		}
	}
	else
	{
		ret = -1;
	}
	err = lfs_file_close(&g_lfs_t, &lfs_fp);
	return ret;
	
}

/************************************************************
*
* Function name	: save_read_default_carema_parameter
* Description	: 读取默认参数
* Parameter		: 
* Return		: 
*	   20230720
************************************************************/
void save_read_default_threshold_parameter(struct threshold_params *param)
{
	param->volt_max 	 		= DEFALUT_VOLT_MAX;
	param->volt_min 	 		= DEFALUT_VOLT_MIN;
	param->current 		 		= DEFALUT_CURRENT_MAX;
	param->angle  		 		= DEFAULT_ANGLE;
	param->humi_high 			= DEFALUT_HUMI_HIGH;
	param->humi_low 			= DEFAULT_HUMI_LOW;
	param->temp_high 			= DEFALUT_TEMP_HIGH;
	param->temp_low 			= DEFALUT_TEMP_LOW;
	param->overcurrentTimes = 0;
	param->undervoltageTimes = 0;
	param->overvoltageTimes = 0;
	param->electricLeakageTimes = 0;
}

/************************************************************
*
* Function name	: save_read_threshold_parameter
* Description	: 
* Parameter		: 
* Return		: 
*	   20230720
************************************************************/
int8_t save_read_threshold_parameter(struct threshold_params *param)
{
	int8_t		ret      = 0;
	int 		err 	 = 0;
	lfs_file_t  lfs_fp   = {0};
	
	err = lfs_file_open(&g_lfs_t, &lfs_fp, SAVE_THRESHOLD_PARAMETER, LFS_O_RDWR);
	if(SAVE_DEBUG)  printf("save_read_threshold_parameter:%d\r\n",err);
	if(err == 0)
	{
		err = lfs_file_rewind(&g_lfs_t, &lfs_fp);
		err = lfs_file_read(&g_lfs_t, &lfs_fp, param,sizeof(struct threshold_params));
	}
	else
	{
		err = lfs_file_close(&g_lfs_t, &lfs_fp);
		save_read_default_threshold_parameter(param);
		save_stroage_threshold_parameter(param);
		ret = -1;
	}
	err = lfs_file_close(&g_lfs_t, &lfs_fp);
	
	return ret;
	
}


/************************************************************
*
* Function name	: save_stroage_backups_function
* Description	: 
* Parameter		: 
* Return		: 
*	  20231022
************************************************************/
int8_t save_stroage_backups_function(sys_backups_t *param)
{
	int8_t		ret      = 0;
 	int 		err 	 = 0;
	lfs_file_t  lfs_fp	 = {0};
	
	/* 数据保存 */
	err = lfs_file_open(&g_lfs_t, &lfs_fp, SAVE_REMOTE_BACKUPS_NAME, LFS_O_RDWR | LFS_O_CREAT);
	if(err == 0)
	{
		err = lfs_file_rewind(&g_lfs_t, &lfs_fp);
		err = lfs_file_write(&g_lfs_t, &lfs_fp, (uint8_t*)param, sizeof(sys_backups_t));
		if(err != sizeof(sys_backups_t)) {
			err = lfs_file_write(&g_lfs_t, &lfs_fp, (uint8_t*)param, sizeof(sys_backups_t));
		}
	}
	else
	{
		ret = -1;
	}
	err = lfs_file_close(&g_lfs_t, &lfs_fp);
	
	return ret;
}

/************************************************************
*
* Function name	: save_read_backups_function
* Description	: 
* Parameter		: 
* Return		: 
*	 20231022
************************************************************/
int8_t save_read_backups_function(sys_backups_t *param)
{
	int8_t		ret  = 0;
	int 		err 	 = 0;
	lfs_file_t  lfs_fp   = {0};
	
	err = lfs_file_open(&g_lfs_t, &lfs_fp, SAVE_REMOTE_BACKUPS_NAME, LFS_O_RDWR);
	if(SAVE_DEBUG)  printf("save_read_backups_function:%d\r\n",err);
	if(err == 0)
	{
		err = lfs_file_rewind(&g_lfs_t, &lfs_fp);
		err = lfs_file_read(&g_lfs_t, &lfs_fp, param,sizeof(sys_backups_t));
	}
	else
	{
		/* 读取默认值 */
		err = lfs_file_close(&g_lfs_t, &lfs_fp);
		
		save_read_default_backups(param);
		save_stroage_backups_function(param);
		
		ret = -1;
	}
	err = lfs_file_close(&g_lfs_t, &lfs_fp);
	
	return ret;
}

/************************************************************
*
* Function name	: save_read_default_backups
* Description	: 读取默认值
* Parameter		: 
* Return		: 
*	 20231022
************************************************************/
void save_read_default_backups(sys_backups_t *param)
{
	/* 远程服务器数据 */
	memset(param->remote.outside_iporname,0,sizeof(param->remote.outside_iporname));
	strcpy((char*)param->remote.outside_iporname,"test1.fnwlw.net");
	param->remote.outside_port  = 6102;
	
	param->config_flag =0;
}

