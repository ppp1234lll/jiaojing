#ifndef _ETH_H_
#define _ETH_H_

#include "sys.h"
#include "lwip/dhcp.h"
#include "lwip/timers.h"
#include "lwip/tcp_impl.h"
#include "lwip/ip_frag.h"
#include "lwip/tcpip.h" 

/* 参数 */
#define DUPLICATE_IP   (1)
#define DUPLICATE_MAC  (2)
extern uint8_t etharpError;

/* 函数声明 */
void eth_network_line_status_detection_function(void);
int8_t eth_network_line_status_check(void);
void eth_ping_timer_function(void);
void eth_ping_detection_function(void);
void dns_serverFound(const char *name, struct ip_addr *ipaddr, void *arg);
void eth_tcp_connect_control_function(void);
void eth_set_tcp_connect_reset(void);

uint8_t eth_get_network_cable_status(void);
uint8_t eth_get_tcp_status(void);

void eth_udp_connect_control_function(void);
void eth_tcp_server_control_function(void);
uint8_t eth_get_udp_status(void);
void eth_set_network_reset(void);
void eth_set_udp_connect_reset(void);
void eth_set_onvif_udp_connect_reset(void);
void eth_set_tcp_server_reset(void);
uint8_t eth_get_tcp_server_status(void);

#endif


