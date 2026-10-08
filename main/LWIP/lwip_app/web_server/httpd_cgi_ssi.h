#ifndef _HTTPD_CGI_SSI_H_
#define _HTTPD_CGI_SSI_H_

#include "sys.h"

#define HTTP_DEBUG  0  // 打印调试
#define CODE_MAX_NUM (12)

void set_return_status_function(uint16_t flag,uint8_t *buff);

/* httpd_cgi */
int8_t httpd_cgi_login_function(int iNumParams, char *pcParam[], char *pcValue[]);         // 网页登录
int8_t httpd_cgi_select_function(char *pcValue[]);									       // 下拉框状态
int8_t httpd_cgi_switch_function(int iNumParams, char *pcParam[], char *pcValue[]);        // 开关状态
int8_t httpd_cgi_set_system_function(int iNumParams, char *pcParam[], char *pcValue[]);    // 系统设置
int8_t httpd_cgi_set_network_function(int iNumParams, char *pcParam[], char *pcValue[]);   // 设置网络
int8_t httpd_cgi_set_camera_ip_function(int iNumParams, char *pcParam[], char *pcValue[]); // 摄像机IP
int8_t httpd_cgi_set_remote_ip_function(int iNumParams, char *pcParam[], char *pcValue[]); // 远端服务器设置
int8_t httpd_cgi_update_function(int iNumParams, char *pcParam[], char *pcValue[]);		   // 系统更新
int8_t httpd_cgi_system_function(int iNumParams, char *pcParam[], char *pcValue[]);		   // 系统设置
int8_t httpd_cgi_show_function(char *pcValue[], uint16_t *data, uint8_t *buff);			   // 显示更新
int8_t httpd_cgi_set_threshold_function(int iNumParams, char *pcParam[], char *pcValue[]);  // 阈值 20230720
int8_t httpd_cgi_login_mod_function(int iNumParams, char *pcParam[], char *pcValue[]);

/* httpd_ssi */
void httpd_ssi_data_collection_function(char *pcInsert);  // 采集数据显示
void httpd_ssi_switch_status_function(char *pcInsert);	  // 开关状态
void httpd_ssi_system_status_function(char *pcInsert);    // 系统状态
void httpd_ssi_system_seting_function(char *pcInsert);	  // 系统设置
void httpd_ssi_nework_gprs_show_function(char *pcInsert); // 无线网络信息
void httpd_ssi_network_setting_function(char* pcInsert);  // 网络信息
void httpd_ssi_other_setting_function(char *pcInsert);	  // 其他信息-摄像头ip
void http_ssi_server_setting_function(char *pcInsert);	  // 远端服务器信息
void httpd_ssi_threshold_seting_function(char *pcInsert); // 阈值 20230720
void httpd_ssi_power_data_collection_function(char *pcInsert);
void httpd_ssi_other_data_collection_function(char *pcInsert);
void httpd_ssi_volt_cur_data_collection_function(char *pcInsert);


void Vin220_Power_Handler(char *pcInsert, uint8_t num);
void Vin220_Handler(char *pcInsert, uint8_t num);
void lighting_protector_detection(char *pcInsert); // 防雷开关检测
void open_door_status_Handler(char *pcInsert); // 箱门状态
void cabinet_posture_Handler(char *pcInsert);  // 箱体姿态
void device_parameter_handler(char *pcInsert,uint8_t num);
void local_network_Handler(char *pcInsert, uint8_t mode);
void camera_ip_get_Handler(char *pcInsert, uint8_t num);
#endif

