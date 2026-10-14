/**
 * @file
 * Sequential API Internal module
 *
 */

/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
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

#include "lwip/opt.h"

#if LWIP_NETCONN /* don't build if not configured for use in lwipopts.h */

#include "lwip/api_msg.h"

#include "lwip/ip.h"
#include "lwip/udp.h"
#include "lwip/tcp.h"
#include "lwip/raw.h"

#include "lwip/memp.h"
#include "lwip/tcpip.h"
#include "lwip/igmp.h"
#include "lwip/dns.h"
#include "arch/sys_arch.h"   /* 调试计数 g_dbg_* 声明 */

#include <string.h>

/* forward declaration: netconn_free() 的兜底清理会用到 netconn_drain() */
static void netconn_drain(struct netconn *conn);

/* forward declaration: 本地诊断(定义在本文件后部, netconn_free() 会先用到) */
static void dbg_nc_freed(void *conn, u32_t lr);
static void dbg_nc_check(void *conn, u32_t lr);
static void dbg_nc_alloc_reg(void *conn, u32_t gen);
static void dbg_nc_free_unreg(void *conn, u32_t gen);
static void dbg_nc_check_live(void *conn, u32_t gen, u32_t lr);
/* 本地诊断: netconn 代数计数(定义在本文件后部, netconn_alloc 先用到) */
extern volatile u32_t g_dbg_conn_gen;

#define SET_NONBLOCKING_CONNECT(conn, val)  do { if(val) { \
  (conn)->flags |= NETCONN_FLAG_IN_NONBLOCKING_CONNECT; \
} else { \
  (conn)->flags &= ~ NETCONN_FLAG_IN_NONBLOCKING_CONNECT; }} while(0)
#define IN_NONBLOCKING_CONNECT(conn) (((conn)->flags & NETCONN_FLAG_IN_NONBLOCKING_CONNECT) != 0)

/* forward declarations */
#if LWIP_TCP
static err_t do_writemore(struct netconn *conn);
static void do_close_internal(struct netconn *conn);
#endif

#if LWIP_RAW
/**
 * Receive callback function for RAW netconns.
 * Doesn't 'eat' the packet, only references it and sends it to
 * conn->recvmbox
 *
 * @see raw.h (struct raw_pcb.recv) for parameters and return value
 */
static u8_t
recv_raw(void *arg, struct raw_pcb *pcb, struct pbuf *p,
    ip_addr_t *addr)
{
  struct pbuf *q;
  struct netbuf *buf;
  struct netconn *conn;

  LWIP_UNUSED_ARG(addr);
  conn = (struct netconn *)arg;

  if ((conn != NULL) && sys_mbox_valid(&conn->recvmbox)) {
#if LWIP_SO_RCVBUF
    int recv_avail;
    SYS_ARCH_GET(conn->recv_avail, recv_avail);
    if ((recv_avail + (int)(p->tot_len)) > conn->recv_bufsize) {
      return 0;
    }
#endif /* LWIP_SO_RCVBUF */
    /* copy the whole packet into new pbufs */
    q = pbuf_alloc(PBUF_RAW, p->tot_len, PBUF_RAM);
    if(q != NULL) {
      if (pbuf_copy(q, p) != ERR_OK) {
        pbuf_free(q);
        q = NULL;
      }
    }

    if (q != NULL) {
      u16_t len;
      buf = (struct netbuf *)memp_malloc(MEMP_NETBUF);
      if (buf == NULL) {
        pbuf_free(q);
        return 0;
      }

      buf->p = q;
      buf->ptr = q;
      ip_addr_copy(buf->addr, *ip_current_src_addr());
      buf->port = pcb->protocol;

      len = q->tot_len;
      if (sys_mbox_trypost(&conn->recvmbox, buf) != ERR_OK) {
        netbuf_delete(buf);
        return 0;
      } else {
#if LWIP_SO_RCVBUF
        SYS_ARCH_INC(conn->recv_avail, len);
#endif /* LWIP_SO_RCVBUF */
        /* Register event with callback */
        API_EVENT(conn, NETCONN_EVT_RCVPLUS, len);
      }
    }
  }

  return 0; /* do not eat the packet */
}
#endif /* LWIP_RAW*/

#if LWIP_UDP
/**
 * Receive callback function for UDP netconns.
 * Posts the packet to conn->recvmbox or deletes it on memory error.
 *
 * @see udp.h (struct udp_pcb.recv) for parameters
 */
static void
recv_udp(void *arg, struct udp_pcb *pcb, struct pbuf *p,
   ip_addr_t *addr, u16_t port)
{
  struct netbuf *buf;
  struct netconn *conn;
  u16_t len;
#if LWIP_SO_RCVBUF
  int recv_avail;
#endif /* LWIP_SO_RCVBUF */

  LWIP_UNUSED_ARG(pcb); /* only used for asserts... */
  LWIP_ASSERT("recv_udp must have a pcb argument", pcb != NULL);
  LWIP_ASSERT("recv_udp must have an argument", arg != NULL);
  conn = (struct netconn *)arg;
  LWIP_ASSERT("recv_udp: recv for wrong pcb!", conn->pcb.udp == pcb);

#if LWIP_SO_RCVBUF
  SYS_ARCH_GET(conn->recv_avail, recv_avail);
  if ((conn == NULL) || !sys_mbox_valid(&conn->recvmbox) ||
      ((recv_avail + (int)(p->tot_len)) > conn->recv_bufsize)) {
#else  /* LWIP_SO_RCVBUF */
  if ((conn == NULL) || !sys_mbox_valid(&conn->recvmbox)) {
#endif /* LWIP_SO_RCVBUF */
    pbuf_free(p);
    return;
  }

  buf = (struct netbuf *)memp_malloc(MEMP_NETBUF);
  if (buf == NULL) {
    pbuf_free(p);
    return;
  } else {
    buf->p = p;
    buf->ptr = p;
    ip_addr_set(&buf->addr, addr);
    buf->port = port;
#if LWIP_NETBUF_RECVINFO
    {
      const struct ip_hdr* iphdr = ip_current_header();
      /* get the UDP header - always in the first pbuf, ensured by udp_input */
      const struct udp_hdr* udphdr = (void*)(((char*)iphdr) + IPH_LEN(iphdr));
#if LWIP_CHECKSUM_ON_COPY
      buf->flags = NETBUF_FLAG_DESTADDR;
#endif /* LWIP_CHECKSUM_ON_COPY */
      ip_addr_set(&buf->toaddr, ip_current_dest_addr());
      buf->toport_chksum = udphdr->dest;
    }
#endif /* LWIP_NETBUF_RECVINFO */
  }

  len = p->tot_len;
  if (sys_mbox_trypost(&conn->recvmbox, buf) != ERR_OK) {
    netbuf_delete(buf);
    return;
  } else {
#if LWIP_SO_RCVBUF
    SYS_ARCH_INC(conn->recv_avail, len);
#endif /* LWIP_SO_RCVBUF */
    /* Register event with callback */
    API_EVENT(conn, NETCONN_EVT_RCVPLUS, len);
  }
}
#endif /* LWIP_UDP */

#if LWIP_TCP
/**
 * Receive callback function for TCP netconns.
 * Posts the packet to conn->recvmbox, but doesn't delete it on errors.
 *
 * @see tcp.h (struct tcp_pcb.recv) for parameters and return value
 */
static err_t
recv_tcp(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
  struct netconn *conn;
  u16_t len;

  LWIP_UNUSED_ARG(pcb);
  LWIP_ASSERT("recv_tcp must have a pcb argument", pcb != NULL);
  LWIP_ASSERT("recv_tcp must have an argument", arg != NULL);
  conn = (struct netconn *)arg;
  LWIP_ASSERT("recv_tcp: recv for wrong pcb!", conn->pcb.tcp == pcb);

  if (conn == NULL) {
    return ERR_VAL;
  }
  if (!sys_mbox_valid(&conn->recvmbox)) {
    /* recvmbox already deleted */
    if (p != NULL) {
      tcp_recved(pcb, p->tot_len);
      pbuf_free(p);
    }
    return ERR_OK;
  }
  /* Unlike for UDP or RAW pcbs, don't check for available space
     using recv_avail since that could break the connection
     (data is already ACKed) */

  /* don't overwrite fatal errors! */
  NETCONN_SET_SAFE_ERR(conn, err);

  if (p != NULL) {
    len = p->tot_len;
  } else {
    len = 0;
  }

  if (sys_mbox_trypost(&conn->recvmbox, p) != ERR_OK) {
    /* don't deallocate p: it is presented to us later again from tcp_fasttmr! */
    return ERR_MEM;
  } else {
#if LWIP_SO_RCVBUF
    SYS_ARCH_INC(conn->recv_avail, len);
#endif /* LWIP_SO_RCVBUF */
    /* Register event with callback */
    API_EVENT(conn, NETCONN_EVT_RCVPLUS, len);
  }

  return ERR_OK;
}

/**
 * Poll callback function for TCP netconns.
 * Wakes up an application thread that waits for a connection to close
 * or data to be sent. The application thread then takes the
 * appropriate action to go on.
 *
 * Signals the conn->sem.
 * netconn_close waits for conn->sem if closing failed.
 *
 * @see tcp.h (struct tcp_pcb.poll) for parameters and return value
 */
static err_t
poll_tcp(void *arg, struct tcp_pcb *pcb)
{
  struct netconn *conn = (struct netconn *)arg;

  LWIP_UNUSED_ARG(pcb);
  LWIP_ASSERT("conn != NULL", (conn != NULL));

  if (conn->state == NETCONN_WRITE) {
    do_writemore(conn);
  } else if (conn->state == NETCONN_CLOSE) {
    do_close_internal(conn);
  }
  /* @todo: implement connect timeout here? */

  /* Did a nonblocking write fail before? Then check available write-space. */
  if (conn->flags & NETCONN_FLAG_CHECK_WRITESPACE) {
    /* If the queued byte- or pbuf-count drops below the configured low-water limit,
       let select mark this pcb as writable again. */
    if ((conn->pcb.tcp != NULL) && (tcp_sndbuf(conn->pcb.tcp) > TCP_SNDLOWAT) &&
      (tcp_sndqueuelen(conn->pcb.tcp) < TCP_SNDQUEUELOWAT)) {
      conn->flags &= ~NETCONN_FLAG_CHECK_WRITESPACE;
      API_EVENT(conn, NETCONN_EVT_SENDPLUS, 0);
    }
  }

  return ERR_OK;
}

/**
 * Sent callback function for TCP netconns.
 * Signals the conn->sem and calls API_EVENT.
 * netconn_write waits for conn->sem if send buffer is low.
 *
 * @see tcp.h (struct tcp_pcb.sent) for parameters and return value
 */
static err_t
sent_tcp(void *arg, struct tcp_pcb *pcb, u16_t len)
{
  struct netconn *conn = (struct netconn *)arg;

  LWIP_UNUSED_ARG(pcb);
  LWIP_ASSERT("conn != NULL", (conn != NULL));

  if (conn->state == NETCONN_WRITE) {
    do_writemore(conn);
  } else if (conn->state == NETCONN_CLOSE) {
    do_close_internal(conn);
  }

  if (conn) {
    /* If the queued byte- or pbuf-count drops below the configured low-water limit,
       let select mark this pcb as writable again. */
    if ((conn->pcb.tcp != NULL) && (tcp_sndbuf(conn->pcb.tcp) > TCP_SNDLOWAT) &&
      (tcp_sndqueuelen(conn->pcb.tcp) < TCP_SNDQUEUELOWAT)) {
      conn->flags &= ~NETCONN_FLAG_CHECK_WRITESPACE;
      API_EVENT(conn, NETCONN_EVT_SENDPLUS, len);
    }
  }
  
  return ERR_OK;
}

/**
 * Error callback function for TCP netconns.
 * Signals conn->sem, posts to all conn mboxes and calls API_EVENT.
 * The application thread has then to decide what to do.
 *
 * @see tcp.h (struct tcp_pcb.err) for parameters
 */
static void
err_tcp(void *arg, err_t err)
{
  struct netconn *conn;
  enum netconn_state old_state;
  SYS_ARCH_DECL_PROTECT(lev);

  conn = (struct netconn *)arg;
  LWIP_ASSERT("conn != NULL", (conn != NULL));

  conn->pcb.tcp = NULL;
  g_dbg_err_tcp++;                                     /* 调试计数: 连接被 abort/出错 */

  /* no check since this is always fatal! */
  SYS_ARCH_PROTECT(lev);
  conn->last_err = err;
  SYS_ARCH_UNPROTECT(lev);

  /* reset conn->state now before waking up other threads */
  old_state = conn->state;
  conn->state = NETCONN_NONE;

  /* Notify the user layer about a connection error. Used to signal
     select. */
  API_EVENT(conn, NETCONN_EVT_ERROR, 0);
  /* Try to release selects pending on 'read' or 'write', too.
     They will get an error if they actually try to read or write. */
  API_EVENT(conn, NETCONN_EVT_RCVPLUS, 0);
  API_EVENT(conn, NETCONN_EVT_SENDPLUS, 0);

  /* pass NULL-message to recvmbox to wake up pending recv */
  if (sys_mbox_valid(&conn->recvmbox)) {
    /* use trypost to prevent deadlock */
    sys_mbox_trypost(&conn->recvmbox, NULL);
  }
  /* pass NULL-message to acceptmbox to wake up pending accept */
  if (sys_mbox_valid(&conn->acceptmbox)) {
    /* use trypost to preven deadlock */
    sys_mbox_trypost(&conn->acceptmbox, NULL);
  }

  if ((old_state == NETCONN_WRITE) || (old_state == NETCONN_CLOSE) ||
      (old_state == NETCONN_CONNECT)) {
    /* calling do_writemore/do_close_internal is not necessary
       since the pcb has already been deleted! */
    int was_nonblocking_connect = IN_NONBLOCKING_CONNECT(conn);
    SET_NONBLOCKING_CONNECT(conn, 0);

    if (!was_nonblocking_connect) {
      /* set error return code */
      LWIP_ASSERT("conn->current_msg != NULL", conn->current_msg != NULL);
      conn->current_msg->err = err;
      conn->current_msg = NULL;
      /* wake up the waiting task */
      sys_sem_signal(&conn->op_completed);
    }
  } else {
    LWIP_ASSERT("conn->current_msg == NULL", conn->current_msg == NULL);
  }
}

/**
 * Setup a tcp_pcb with the correct callback function pointers
 * and their arguments.
 *
 * @param conn the TCP netconn to setup
 */
static void
setup_tcp(struct netconn *conn)
{
  struct tcp_pcb *pcb;

  pcb = conn->pcb.tcp;
  tcp_arg(pcb, conn);
  tcp_recv(pcb, recv_tcp);
  tcp_sent(pcb, sent_tcp);
  tcp_poll(pcb, poll_tcp, 4);
  tcp_err(pcb, err_tcp);
}

/**
 * Accept callback function for TCP netconns.
 * Allocates a new netconn and posts that to conn->acceptmbox.
 *
 * @see tcp.h (struct tcp_pcb_listen.accept) for parameters and return value
 */
static err_t
accept_function(void *arg, struct tcp_pcb *newpcb, err_t err)
{
  struct netconn *newconn;
  struct netconn *conn = (struct netconn *)arg;

  LWIP_DEBUGF(API_MSG_DEBUG, ("accept_function: newpcb->tate: %s\n", tcp_debug_state_str(newpcb->state)));

  if (!sys_mbox_valid(&conn->acceptmbox)) {
    LWIP_DEBUGF(API_MSG_DEBUG, ("accept_function: acceptmbox already deleted\n"));
    return ERR_VAL;
  }

  /* We have to set the callback here even though
   * the new socket is unknown. conn->socket is marked as -1. */
  newconn = netconn_alloc(conn->type, conn->callback);
  if (newconn == NULL) {
    return ERR_MEM;
  }
  newconn->pcb.tcp = newpcb;
  setup_tcp(newconn);
  /* no protection: when creating the pcb, the netconn is not yet known
     to the application thread */
  newconn->last_err = err;

  if (sys_mbox_trypost(&conn->acceptmbox, newconn) != ERR_OK) {
    g_dbg_accept_postfail++;                           /* 调试计数 */
    /* When returning != ERR_OK, the pcb is aborted in tcp_process(),
       so do nothing here! */
    /* remove all references to this netconn from the pcb */
    struct tcp_pcb* pcb = newconn->pcb.tcp;
    tcp_arg(pcb, NULL);
    tcp_recv(pcb, NULL);
    tcp_sent(pcb, NULL);
    tcp_poll(pcb, NULL, 4);
    tcp_err(pcb, NULL);
    /* remove reference from to the pcb from this netconn */
    newconn->pcb.tcp = NULL;
    /* no need to drain since we know the recvmbox is empty. */
    sys_mbox_free(&newconn->recvmbox);
    sys_mbox_set_invalid(&newconn->recvmbox);
    netconn_free(newconn);
    return ERR_MEM;
  } else {
    /* Register event with callback */
    API_EVENT(conn, NETCONN_EVT_RCVPLUS, 0);
  }

  return ERR_OK;
}
#endif /* LWIP_TCP */

/**
 * Create a new pcb of a specific type.
 * Called from do_newconn().
 *
 * @param msg the api_msg_msg describing the connection type
 * @return msg->conn->err, but the return value is currently ignored
 */
static void
pcb_new(struct api_msg_msg *msg)
{
  LWIP_ASSERT("pcb_new: pcb already allocated", msg->conn->pcb.tcp == NULL);

  /* Allocate a PCB for this connection */
  switch(NETCONNTYPE_GROUP(msg->conn->type)) {
#if LWIP_RAW
  case NETCONN_RAW:
    msg->conn->pcb.raw = raw_new(msg->msg.n.proto);
    if(msg->conn->pcb.raw == NULL) {
      msg->err = ERR_MEM;
      break;
    }
    raw_recv(msg->conn->pcb.raw, recv_raw, msg->conn);
    break;
#endif /* LWIP_RAW */
#if LWIP_UDP
  case NETCONN_UDP:
    msg->conn->pcb.udp = udp_new();
    if(msg->conn->pcb.udp == NULL) {
      msg->err = ERR_MEM;
      break;
    }
#if LWIP_UDPLITE
    if (msg->conn->type==NETCONN_UDPLITE) {
      udp_setflags(msg->conn->pcb.udp, UDP_FLAGS_UDPLITE);
    }
#endif /* LWIP_UDPLITE */
    if (msg->conn->type==NETCONN_UDPNOCHKSUM) {
      udp_setflags(msg->conn->pcb.udp, UDP_FLAGS_NOCHKSUM);
    }
    udp_recv(msg->conn->pcb.udp, recv_udp, msg->conn);
    break;
#endif /* LWIP_UDP */
#if LWIP_TCP
  case NETCONN_TCP:
    msg->conn->pcb.tcp = tcp_new();
    if(msg->conn->pcb.tcp == NULL) {
      msg->err = ERR_MEM;
      break;
    }
    setup_tcp(msg->conn);
    break;
#endif /* LWIP_TCP */
  default:
    /* Unsupported netconn type, e.g. protocol disabled */
    msg->err = ERR_VAL;
    break;
  }
}

/**
 * Create a new pcb of a specific type inside a netconn.
 * Called from netconn_new_with_proto_and_callback.
 *
 * @param msg the api_msg_msg describing the connection type
 */
void
do_newconn(struct api_msg_msg *msg)
{
  msg->err = ERR_OK;
  if(msg->conn->pcb.tcp == NULL) {
    pcb_new(msg);
  }
  /* Else? This "new" connection already has a PCB allocated. */
  /* Is this an error condition? Should it be deleted? */
  /* We currently just are happy and return. */

  TCPIP_APIMSG_ACK(msg);
}

/**
 * Create a new netconn (of a specific type) that has a callback function.
 * The corresponding pcb is NOT created!
 *
 * @param t the type of 'connection' to create (@see enum netconn_type)
 * @param proto the IP protocol for RAW IP pcbs
 * @param callback a function to call on status changes (RX available, TX'ed)
 * @return a newly allocated struct netconn or
 *         NULL on memory error
 */
struct netconn*
netconn_alloc(enum netconn_type t, netconn_callback callback)
{
  struct netconn *conn;
  int size;

  conn = (struct netconn *)memp_malloc(MEMP_NETCONN);
  if (conn == NULL) {
    return NULL;
  }

  conn->last_err = ERR_OK;
  conn->type = t;
  conn->pcb.tcp = NULL;

#if (DEFAULT_RAW_RECVMBOX_SIZE == DEFAULT_UDP_RECVMBOX_SIZE) && \
    (DEFAULT_RAW_RECVMBOX_SIZE == DEFAULT_TCP_RECVMBOX_SIZE)
  size = DEFAULT_RAW_RECVMBOX_SIZE;
#else
  switch(NETCONNTYPE_GROUP(t)) {
#if LWIP_RAW
  case NETCONN_RAW:
    size = DEFAULT_RAW_RECVMBOX_SIZE;
    break;
#endif /* LWIP_RAW */
#if LWIP_UDP
  case NETCONN_UDP:
    size = DEFAULT_UDP_RECVMBOX_SIZE;
    break;
#endif /* LWIP_UDP */
#if LWIP_TCP
  case NETCONN_TCP:
    size = DEFAULT_TCP_RECVMBOX_SIZE;
    break;
#endif /* LWIP_TCP */
  default:
    LWIP_ASSERT("netconn_alloc: undefined netconn_type", 0);
    goto free_and_return;
  }
#endif

  if (sys_sem_new(&conn->op_completed, 0) != ERR_OK) {
    goto free_and_return;
  }
  if (sys_mbox_new(&conn->recvmbox, size) != ERR_OK) {
    sys_sem_free(&conn->op_completed);
    goto free_and_return;
  }

#if LWIP_TCP
  sys_mbox_set_invalid(&conn->acceptmbox);
#endif
  conn->state        = NETCONN_NONE;
#if LWIP_SOCKET
  /* initialize socket to -1 since 0 is a valid socket */
  conn->socket       = -1;
#endif /* LWIP_SOCKET */
  conn->callback     = callback;
#if LWIP_TCP
  conn->current_msg  = NULL;
  conn->write_offset = 0;
#endif /* LWIP_TCP */
#if LWIP_SO_SNDTIMEO
  conn->send_timeout = 0;
  conn->dbg_gen = ++g_dbg_conn_gen;   /* 本地诊断: 分配时写入代数标记 */
  dbg_nc_alloc_reg(conn, conn->dbg_gen);   /* 本地诊断: 存活表登记 */
#endif /* LWIP_SO_SNDTIMEO */
#if LWIP_SO_RCVTIMEO
  conn->recv_timeout = 0;
#endif /* LWIP_SO_RCVTIMEO */
#if LWIP_SO_RCVBUF
  conn->recv_bufsize = RECV_BUFSIZE_DEFAULT;
  conn->recv_avail   = 0;
#endif /* LWIP_SO_RCVBUF */
  conn->flags = 0;
  g_dbg_nc_alloc++;                                    /* 调试计数 */
  return conn;
free_and_return:
  memp_free(MEMP_NETCONN, conn);
  return NULL;
}

/**
 * Delete a netconn and all its resources.
 * The pcb is NOT freed (since we might not be in the right thread context do this).
 *
 * @param conn the netconn to free
 */
void
netconn_free(struct netconn *conn)
{
  LWIP_ASSERT("PCB must be deallocated outside this function", conn->pcb.tcp == NULL);
  LWIP_ASSERT("recvmbox must be deallocated before calling this function",
    !sys_mbox_valid(&conn->recvmbox));
#if LWIP_TCP
  LWIP_ASSERT("acceptmbox must be deallocated before calling this function",
    !sys_mbox_valid(&conn->acceptmbox));
#endif /* LWIP_TCP */

  g_dbg_nc_free++;                                     /* 调试计数 */

  /* 调试计数: 若此处邮箱指针仍非空, 说明调用者漏了 netconn_drain()(用于发现泄漏路径)。
     这里**不再**补做取空/释放: 对已释放或已被复用的 netconn 来说邮箱指针可能是脏值,
     贸然 OSQDel/myfree 会破坏 uCOS 队列池; 且用脏指针取空会死循环。
     邮箱的释放统一由 netconn_drain() 负责(它只在邮箱有效时取空, 但只要分配过就一定释放)。 */
  if (conn->recvmbox != NULL) {
    g_dbg_nc_free_leak_rcv++;                          /* 调试计数: 命中 recvmbox 未释放 */
  }
#if LWIP_TCP
  if (conn->acceptmbox != NULL) {
    g_dbg_nc_free_leak_acc++;                          /* 调试计数: 命中 acceptmbox 未释放 */
  }
#endif

  sys_sem_free(&conn->op_completed);
  sys_sem_set_invalid(&conn->op_completed);

  dbg_nc_freed(conn, (u32_t)__builtin_return_address(0));  /* 诊断: 登记"已释放的 netconn" */
  memp_free(MEMP_NETCONN, conn);
}

/* ================= 本地诊断: netconn_drain 重复调用追踪 =================
   用于定位"同一 netconn 被重复 drain(重复释放)"——它会让 uCOS OS_Q/OS_EVENT 成对丢失,
   最终 netconn_alloc 失败、设备对新连接一律回 RST。查询见 g_dbg_drain_* 符号。 */
#define DREN_HIST   4
volatile u32_t g_dbg_drain_cnt = 0;         /* netconn_drain 调用总次数 */
volatile u32_t g_dbg_drain_dup = 0;         /* 同一 conn 在最近 DREN_HIST 次中被重复 drain 的次数 */
volatile u32_t g_dbg_drain_dup_conn = 0;    /* 重复的那个 netconn 指针 */
volatile u32_t g_dbg_drain_dup_lr = 0;      /* 后一次 netconn_drain 的调用者返回地址 */
volatile u32_t g_dbg_drain_dup_prev = 0;    /* 首次 netconn_drain 的调用者返回地址 */
volatile u32_t g_dbg_drain_conn[DREN_HIST]; /* 最近 drain 的 conn(环形) */
volatile u32_t g_dbg_drain_lr[DREN_HIST];   /* 对应调用者返回地址 */
volatile u32_t g_dbg_drain_idx = 0;
volatile u32_t g_dbg_drain_tcb[DREN_HIST];  /* 对应执行任务的 OSTCBCur(判定是否跨线程) */
volatile u32_t g_dbg_drain_ts[DREN_HIST];   /* 对应时间戳 OSTimeGet(判定两次相隔多久) */
volatile u32_t g_dbg_drain_dup_tcb = 0;     /* 后一次 drain 的任务 */
volatile u32_t g_dbg_drain_dup_prev_tcb = 0;/* 首次 drain 的任务 */
volatile u32_t g_dbg_drain_dup_ts = 0;      /* 后一次 drain 的时间戳 */
volatile u32_t g_dbg_drain_dup_prev_ts = 0; /* 首次 drain 的时间戳 */

/* netconn 代数计数: netconn_alloc 时递增并写入 conn->dbg_gen。
   只有"同地址+同代数"再次被 do_delconn/drain 时, 才是真实的"对已释放句柄的重复操作"。 */
volatile u32_t g_dbg_conn_gen = 0;
volatile u32_t g_dbg_ddel_real = 0;         /* 真·重复 delete 次数(同地址+同代数) */
volatile u32_t g_dbg_ddel_real_conn = 0;    /* 真重复的那个 conn 地址 */
volatile u32_t g_dbg_ddel_real_gen = 0;     /* 其代数 */
volatile u32_t g_dbg_ddel_real_lr = 0;      /* 后一次 delete 的调用者返回地址 */
volatile u32_t g_dbg_ddel_real_ts = 0;      /* 后一次 delete 的时间戳 */
volatile u32_t g_dbg_ddel_real_prev_lr = 0; /* 首次 delete 的调用者返回地址 */
volatile u32_t g_dbg_ddel_real_prev_ts = 0; /* 首次 delete 的时间戳 */
#define DDEL_HIST   8
static u32_t s_ddel_conn[DDEL_HIST];
static u32_t s_ddel_gen[DDEL_HIST];
static u32_t s_ddel_lr[DDEL_HIST];
static u32_t s_ddel_ts[DDEL_HIST];
static u32_t s_ddel_idx = 0;

/* ================= 本地诊断: "使用已释放 netconn"追踪 =================
   原理: netconn_free() 时把该 netconn 指针与"释放者返回地址"记入环形表;
        之后在 do_close/do_delconn/netconn_drain/netconn_close 入口检查入参是否命中该表。
        命中即说明有人拿着"已经被 memp_free 掉的 netconn 指针"(可能已被新 netconn 复用)
        继续 close/delete/drain → 会释放别人的邮箱, 最终耗尽 uCOS 队列/事件池。
   查询: g_dbg_stale_cnt(次数)、g_dbg_stale_conn(指针)、g_dbg_stale_lr(本次使用者)、
        g_dbg_stale_free_lr(最初释放者)。 */
#define NCFREE_HIST   16
volatile u32_t g_dbg_stale_cnt = 0;
volatile u32_t g_dbg_stale_conn = 0;
volatile u32_t g_dbg_stale_lr = 0;
volatile u32_t g_dbg_stale_free_lr = 0;
static void *s_ncfree[NCFREE_HIST];
static u32_t s_ncfree_lr[NCFREE_HIST];
static u32_t s_ncfree_idx = 0;

static void dbg_nc_freed(void *conn, u32_t lr)
{
  s_ncfree[s_ncfree_idx]    = conn;
  s_ncfree_lr[s_ncfree_idx] = lr;
  s_ncfree_idx = (s_ncfree_idx + 1u) % NCFREE_HIST;
  dbg_nc_free_unreg(conn, ((struct netconn *)conn)->dbg_gen);   /* 从存活表注销 */
}

/* ===== 存活登记表: netconn_alloc 登记(地址+代数), netconn_free 注销 =====
   用途: 抓"用陈旧句柄操作新连接"——若 do_delconn 收到的地址在表中存在、但代数与
   表中不一致, 说明这个句柄属于一个【已被释放的旧 netconn】, 而该地址现在属于
   一个【更新的 netconn】: 继续操作会释放新属主的邮箱(盗走 OS_Q/OS_EVENT)。 */
#define LIVE_HIST   32
static u32_t s_live_conn[LIVE_HIST];
static u32_t s_live_gen[LIVE_HIST];
static u32_t s_live_idx = 0;
volatile u32_t g_dbg_stale_gen_cnt = 0;      /* 命中"陈旧代数句柄"操作次数 */
volatile u32_t g_dbg_stale_gen_addr = 0;     /* 该地址 */
volatile u32_t g_dbg_stale_gen_lr = 0;       /* 本次操作的调用者返回地址(元凶) */
volatile u32_t g_dbg_stale_gen_usedgen = 0;  /* 陈旧句柄携带的代数 */
volatile u32_t g_dbg_stale_gen_livegen = 0;  /* 表中登记(新属主)的代数 */

static void dbg_nc_alloc_reg(void *conn, u32_t gen)
{
  u32_t k;
  u32_t slot = LIVE_HIST;
  for (k = 0; k < LIVE_HIST; k++) {
    if (s_live_conn[k] == (u32_t)conn) { slot = k; break; }   /* 地址复用: 原位更新 */
    if ((slot == LIVE_HIST) && (s_live_conn[k] == 0)) { slot = k; }
  }
  if (slot == LIVE_HIST) { slot = s_live_idx; s_live_idx = (s_live_idx + 1u) % LIVE_HIST; }
  s_live_conn[slot] = (u32_t)conn;
  s_live_gen[slot]  = gen;
}

static void dbg_nc_free_unreg(void *conn, u32_t gen)
{
  u32_t k;
  for (k = 0; k < LIVE_HIST; k++) {
    if ((s_live_conn[k] == (u32_t)conn) && (s_live_gen[k] == gen)) {
      s_live_conn[k] = 0;
      s_live_gen[k]  = 0;
      break;
    }
  }
}

static void dbg_nc_check_live(void *conn, u32_t gen, u32_t lr)
{
  u32_t k;
  if (conn == NULL) { return; }
  for (k = 0; k < LIVE_HIST; k++) {
    if (s_live_conn[k] == (u32_t)conn) {
      if (s_live_gen[k] != gen) {          /* 地址存活但代数不同: 陈旧句柄! */
        g_dbg_stale_gen_cnt++;
        g_dbg_stale_gen_addr    = (u32_t)conn;
        g_dbg_stale_gen_lr      = lr;      /* 本次操作者(元凶路径) */
        g_dbg_stale_gen_usedgen = gen;     /* 陈旧句柄的代数 */
        g_dbg_stale_gen_livegen = s_live_gen[k];  /* 现属主的代数 */
      }
      return;
    }
  }
}

static void dbg_nc_check(void *conn, u32_t lr)
{
  u32_t k;
  if (conn == NULL) {
    return;
  }
  for (k = 0; k < NCFREE_HIST; k++) {
    if (s_ncfree[k] == conn) {
      g_dbg_stale_cnt++;
      g_dbg_stale_conn    = (u32_t)conn;
      g_dbg_stale_lr      = lr;                 /* 本次使用者(元凶路径) */
      g_dbg_stale_free_lr = s_ncfree_lr[k];     /* 最初释放者 */
      break;
    }
  }
}

/**
 * Delete rcvmbox and acceptmbox of a netconn and free the left-over data in
 * these mboxes
 *
 * @param conn the netconn to free
 * @bytes_drained bytes drained from recvmbox
 * @accepts_drained pending connections drained from acceptmbox
 */
static void
netconn_drain(struct netconn *conn)
{
  void *mem;
  u32_t guard;                 /* 本地修补: 取空循环硬上限, 防止脏邮箱导致死循环 */
#if LWIP_TCP
  struct pbuf *p;
#endif /* LWIP_TCP */

  dbg_nc_check(conn, (u32_t)__builtin_return_address(0));  /* 诊断: 是否在 drain 已释放的 netconn */

  /* 诊断(本地): 记录 netconn_drain 的入参 conn 与调用者返回地址, 用于定位"同一 netconn 被
     重复 drain/释放"。若同一 conn 在最近 DREN_HIST 次中出现两次, 说明存在重复释放路径。 */
  {
    u32_t dlr = (u32_t)__builtin_return_address(0);
    u32_t dts = (u32_t)OSTimeGet();
    u32_t dtcb = (u32_t)OSTCBCur;
    u32_t k;
    g_dbg_drain_cnt++;
    for (k = 0; k < DREN_HIST; k++) {
      if (g_dbg_drain_conn[k] == (u32_t)conn) {
        g_dbg_drain_dup++;
        g_dbg_drain_dup_conn = (u32_t)conn;
        g_dbg_drain_dup_lr   = dlr;                  /* 后一次调用者 */
        g_dbg_drain_dup_prev = g_dbg_drain_lr[k];    /* 首次调用者 */
        g_dbg_drain_dup_tcb = dtcb;
        g_dbg_drain_dup_prev_tcb = g_dbg_drain_tcb[k];
        g_dbg_drain_dup_ts = dts;
        g_dbg_drain_dup_prev_ts = g_dbg_drain_ts[k];
        break;
      }
    }
    g_dbg_drain_conn[g_dbg_drain_idx] = (u32_t)conn;
    g_dbg_drain_lr[g_dbg_drain_idx]   = dlr;
    g_dbg_drain_tcb[g_dbg_drain_idx]  = dtcb;
    g_dbg_drain_ts[g_dbg_drain_idx]   = dts;
    g_dbg_drain_idx = (g_dbg_drain_idx + 1u) % DREN_HIST;
  }

  /* This runs in tcpip_thread, so we don't need to lock against rx packets */

  /* Delete and drain the recvmbox. */
  /* 修补(本地): 拆开"取空"与"释放"两件事——
     - 取空: 仅当邮箱确实有效(sys_mbox_valid)时才做, 避免用脏/已释放指针取空而死循环;
     - 释放: 只要指针非空(说明分配过)就一定 sys_mbox_free, 避免 uCOS 队列(OS_Q)泄漏
       (原实现两者都依赖 sys_mbox_valid, 而本移植的该函数在"队列已满"时返回0,
        于是既不取空也不释放 → 队列池被耗尽 → 新连接被 RST)。
     取空循环另加 64 次上限, 作为最后一道保险。 */
  if (conn->recvmbox != NULL) {
    if (sys_mbox_valid(&conn->recvmbox)) {
      guard = 0;
      while ((sys_mbox_tryfetch(&conn->recvmbox, &mem) != SYS_MBOX_EMPTY) && (++guard < 64u)) {
#if LWIP_TCP
        if (conn->type == NETCONN_TCP) {
          if(mem != NULL) {
            p = (struct pbuf*)mem;
            /* pcb might be set to NULL already by err_tcp() */
            if (conn->pcb.tcp != NULL) {
              tcp_recved(conn->pcb.tcp, p->tot_len);
            }
            pbuf_free(p);
          }
        } else
#endif /* LWIP_TCP */
        {
          netbuf_delete((struct netbuf *)mem);
        }
      }
    }
    sys_mbox_free(&conn->recvmbox);
    sys_mbox_set_invalid(&conn->recvmbox);
  }

  /* Delete and drain the acceptmbox. */
#if LWIP_TCP
  if (conn->acceptmbox != NULL) {
    if (sys_mbox_valid(&conn->acceptmbox)) {
      guard = 0;
      while ((sys_mbox_tryfetch(&conn->acceptmbox, &mem) != SYS_MBOX_EMPTY) && (++guard < 64u)) {
        struct netconn *newconn = (struct netconn *)mem;
        if (newconn == NULL) {
          continue;
        }
        /* Only tcp pcbs have an acceptmbox, so no need to check conn->type */
        /* pcb might be set to NULL already by err_tcp() */
        if (conn->pcb.tcp != NULL) {
          tcp_accepted(conn->pcb.tcp);
        }
        /* drain recvmbox */
        netconn_drain(newconn);
        if (newconn->pcb.tcp != NULL) {
          tcp_abort(newconn->pcb.tcp);
          newconn->pcb.tcp = NULL;
        }
        netconn_free(newconn);
      }
    }
    sys_mbox_free(&conn->acceptmbox);
    sys_mbox_set_invalid(&conn->acceptmbox);
  }
#endif /* LWIP_TCP */
}

#if LWIP_TCP
/**
 * Internal helper function to close a TCP netconn: since this sometimes
 * doesn't work at the first attempt, this function is called from multiple
 * places.
 *
 * @param conn the TCP netconn to close
 */
static void
do_close_internal(struct netconn *conn)
{
  err_t err;
  u8_t shut, shut_rx, shut_tx, close;

  LWIP_ASSERT("invalid conn", (conn != NULL));
  LWIP_ASSERT("this is for tcp netconns only", (conn->type == NETCONN_TCP));
  LWIP_ASSERT("conn must be in state NETCONN_CLOSE", (conn->state == NETCONN_CLOSE));
  LWIP_ASSERT("pcb already closed", (conn->pcb.tcp != NULL));
  LWIP_ASSERT("conn->current_msg != NULL", conn->current_msg != NULL);

  shut = conn->current_msg->msg.sd.shut;
  shut_rx = shut & NETCONN_SHUT_RD;
  shut_tx = shut & NETCONN_SHUT_WR;
  /* shutting down both ends is the same as closing */
  close = shut == NETCONN_SHUT_RDWR;

  /* Set back some callback pointers */
  if (close) {
    tcp_arg(conn->pcb.tcp, NULL);
  }
  if (conn->pcb.tcp->state == LISTEN) {
    tcp_accept(conn->pcb.tcp, NULL);
  } else {
    /* some callbacks have to be reset if tcp_close is not successful */
    if (shut_rx) {
      tcp_recv(conn->pcb.tcp, NULL);
      tcp_accept(conn->pcb.tcp, NULL);
    }
    if (shut_tx) {
      tcp_sent(conn->pcb.tcp, NULL);
    }
    if (close) {
      tcp_poll(conn->pcb.tcp, NULL, 4);
      tcp_err(conn->pcb.tcp, NULL);
    }
  }
  /* Try to close the connection */
  if (close) {
    err = tcp_close(conn->pcb.tcp);
  } else {
    err = tcp_shutdown(conn->pcb.tcp, shut_rx, shut_tx);
  }
  if (err == ERR_OK) {
    /* Closing succeeded */
    conn->current_msg->err = ERR_OK;
    conn->current_msg = NULL;
    conn->state = NETCONN_NONE;
    if (close) {
		/* ADD by zl */
//		 tcp_pcb_remove(&tcp_active_pcbs, pcb);
//		 memp_free(MEMP_TCP_PCB, conn->pcb.tcp);
		
      /* Set back some callback pointers as conn is going away */
      conn->pcb.tcp = NULL;
      /* Trigger select() in socket layer. Make sure everybody notices activity
       on the connection, error first! */
      API_EVENT(conn, NETCONN_EVT_ERROR, 0);
    }
    if (shut_rx) {
      API_EVENT(conn, NETCONN_EVT_RCVPLUS, 0);
    }
    if (shut_tx) {
      API_EVENT(conn, NETCONN_EVT_SENDPLUS, 0);
    }
    /* wake up the application task */
    sys_sem_signal(&conn->op_completed);
  } else {
    g_dbg_close_fail++;                                /* 调试计数: tcp_close 未成功(常为 ERR_MEM) */
    /* Closing failed, restore some of the callbacks */
    /* Closing of listen pcb will never fail! */
    LWIP_ASSERT("Closing a listen pcb may not fail!", (conn->pcb.tcp->state != LISTEN));
    tcp_sent(conn->pcb.tcp, sent_tcp);
    tcp_poll(conn->pcb.tcp, poll_tcp, 4);
    tcp_err(conn->pcb.tcp, err_tcp);
    tcp_arg(conn->pcb.tcp, conn);
    /* don't restore recv callback: we don't want to receive any more data */
  }
  /* If closing didn't succeed, we get called again either
     from poll_tcp or from sent_tcp */
}
#endif /* LWIP_TCP */

/**
 * Delete the pcb inside a netconn.
 * Called from netconn_delete.
 *
 * @param msg the api_msg_msg pointing to the connection
 */
void
do_delconn(struct api_msg_msg *msg)
{
  dbg_nc_check(msg->conn, (u32_t)__builtin_return_address(0));  /* 诊断: 是否在操作已释放的 netconn */
  {   /* 本地诊断: 真·重复 delete 检测——只有"同地址+同代数"再次被 delete 才算真实重复,
         彻底排除 memp 块地址复用造成的误报 */
    u32_t dlr = (u32_t)__builtin_return_address(0);
    u32_t dts = (u32_t)OSTimeGet();
    u32_t k;
    if (msg->conn != NULL) {
      dbg_nc_check_live(msg->conn, msg->conn->dbg_gen, dlr);  /* 本地诊断: 陈旧代数句柄检测 */
      for (k = 0; k < DDEL_HIST; k++) {
        if ((s_ddel_conn[k] == (u32_t)msg->conn) && (s_ddel_gen[k] == msg->conn->dbg_gen)) {
          g_dbg_ddel_real++;
          g_dbg_ddel_real_conn = (u32_t)msg->conn;
          g_dbg_ddel_real_gen  = msg->conn->dbg_gen;
          g_dbg_ddel_real_lr   = dlr;
          g_dbg_ddel_real_ts   = dts;
          g_dbg_ddel_real_prev_lr  = s_ddel_lr[k];
          g_dbg_ddel_real_prev_ts  = s_ddel_ts[k];
          break;
        }
      }
      s_ddel_conn[s_ddel_idx] = (u32_t)msg->conn;
      s_ddel_gen[s_ddel_idx]  = msg->conn->dbg_gen;
      s_ddel_lr[s_ddel_idx]   = dlr;
      s_ddel_ts[s_ddel_idx]   = dts;
      s_ddel_idx = (s_ddel_idx + 1u) % DDEL_HIST;
    }
  }
  /* @todo TCP: abort running write/connect? */
 if ((msg->conn->state != NETCONN_NONE) &&
     (msg->conn->state != NETCONN_LISTEN) &&
     (msg->conn->state != NETCONN_CONNECT)) {
    /* this only happens for TCP netconns */
    LWIP_ASSERT("msg->conn->type == NETCONN_TCP", msg->conn->type == NETCONN_TCP);
    msg->err = ERR_INPROGRESS;
    g_dbg_delconn_inprog++;                            /* 调试计数 */
    /* 修补(本地): 该 netconn 即将由 netconn_free() 删除, 但本分支原实现不释放邮箱。
       当连接处于 NETCONN_CLOSE(上一次 close 因 tcp_close() 返回 ERR_MEM 未完成,
       例如对端不读取导致发送缓冲未清空)时, recvmbox 会被泄漏 —— 在 uCOS 下 recvmbox
       是一个 OS_Q 队列(OS_MAX_QS 仅 10 个), 长期反复会耗尽队列池, 使 accept 分配的
       netconn 失败、新连接被 RST(表现为"设备不再接受连接")。
       此处补做 drain + 释放邮箱, 使 netconn_free() 不再泄漏队列/信号量资源。 */
    netconn_drain(msg->conn);
  } else {
    LWIP_ASSERT("blocking connect in progress",
      (msg->conn->state != NETCONN_CONNECT) || IN_NONBLOCKING_CONNECT(msg->conn));
    /* Drain and delete mboxes */
    netconn_drain(msg->conn);

    if (msg->conn->pcb.tcp != NULL) {

      switch (NETCONNTYPE_GROUP(msg->conn->type)) {
#if LWIP_RAW
      case NETCONN_RAW:
        raw_remove(msg->conn->pcb.raw);
        break;
#endif /* LWIP_RAW */
#if LWIP_UDP
      case NETCONN_UDP:
        msg->conn->pcb.udp->recv_arg = NULL;
        udp_remove(msg->conn->pcb.udp);
        break;
#endif /* LWIP_UDP */
#if LWIP_TCP
      case NETCONN_TCP:
        LWIP_ASSERT("already writing or closing", msg->conn->current_msg == NULL &&
          msg->conn->write_offset == 0);
        msg->conn->state = NETCONN_CLOSE;
        msg->msg.sd.shut = NETCONN_SHUT_RDWR;
        msg->conn->current_msg = msg;
        do_close_internal(msg->conn);
        /* API_EVENT is called inside do_close_internal, before releasing
           the application thread, so we can return at this point! */
        return;
#endif /* LWIP_TCP */
      default:
        break;
      }
      msg->conn->pcb.tcp = NULL;
    }
    /* tcp netconns don't come here! */

    /* @todo: this lets select make the socket readable and writable,
       which is wrong! errfd instead? */
    API_EVENT(msg->conn, NETCONN_EVT_RCVPLUS, 0);
    API_EVENT(msg->conn, NETCONN_EVT_SENDPLUS, 0);
  }
  if (sys_sem_valid(&msg->conn->op_completed)) {
    sys_sem_signal(&msg->conn->op_completed);
  }
}

/**
 * Bind a pcb contained in a netconn
 * Called from netconn_bind.
 *
 * @param msg the api_msg_msg pointing to the connection and containing
 *            the IP address and port to bind to
 */
void
do_bind(struct api_msg_msg *msg)
{
  if (ERR_IS_FATAL(msg->conn->last_err)) {
    msg->err = msg->conn->last_err;
  } else {
    msg->err = ERR_VAL;
    if (msg->conn->pcb.tcp != NULL) {
      switch (NETCONNTYPE_GROUP(msg->conn->type)) {
#if LWIP_RAW
      case NETCONN_RAW:
        msg->err = raw_bind(msg->conn->pcb.raw, msg->msg.bc.ipaddr);
        break;
#endif /* LWIP_RAW */
#if LWIP_UDP
      case NETCONN_UDP:
        msg->err = udp_bind(msg->conn->pcb.udp, msg->msg.bc.ipaddr, msg->msg.bc.port);
        break;
#endif /* LWIP_UDP */
#if LWIP_TCP
      case NETCONN_TCP:
        msg->err = tcp_bind(msg->conn->pcb.tcp, msg->msg.bc.ipaddr, msg->msg.bc.port);
        break;
#endif /* LWIP_TCP */
      default:
        break;
      }
    }
  }
  TCPIP_APIMSG_ACK(msg);
}

#if LWIP_TCP
/**
 * TCP callback function if a connection (opened by tcp_connect/do_connect) has
 * been established (or reset by the remote host).
 *
 * @see tcp.h (struct tcp_pcb.connected) for parameters and return values
 */
static err_t
do_connected(void *arg, struct tcp_pcb *pcb, err_t err)
{
  struct netconn *conn;
  int was_blocking;

  LWIP_UNUSED_ARG(pcb);

  conn = (struct netconn *)arg;

  if (conn == NULL) {
    return ERR_VAL;
  }

  LWIP_ASSERT("conn->state == NETCONN_CONNECT", conn->state == NETCONN_CONNECT);
  LWIP_ASSERT("(conn->current_msg != NULL) || conn->in_non_blocking_connect",
    (conn->current_msg != NULL) || IN_NONBLOCKING_CONNECT(conn));

  if (conn->current_msg != NULL) {
    conn->current_msg->err = err;
  }
  if ((conn->type == NETCONN_TCP) && (err == ERR_OK)) {
    setup_tcp(conn);
  }
  was_blocking = !IN_NONBLOCKING_CONNECT(conn);
  SET_NONBLOCKING_CONNECT(conn, 0);
  conn->current_msg = NULL;
  conn->state = NETCONN_NONE;
  if (!was_blocking) {
    NETCONN_SET_SAFE_ERR(conn, ERR_OK);
  }
  API_EVENT(conn, NETCONN_EVT_SENDPLUS, 0);

  if (was_blocking) {
    sys_sem_signal(&conn->op_completed);
  }
  return ERR_OK;
}
#endif /* LWIP_TCP */

/**
 * Connect a pcb contained inside a netconn
 * Called from netconn_connect.
 *
 * @param msg the api_msg_msg pointing to the connection and containing
 *            the IP address and port to connect to
 */
void
do_connect(struct api_msg_msg *msg)
{
  if (msg->conn->pcb.tcp == NULL) {
    /* This may happen when calling netconn_connect() a second time */
    msg->err = ERR_CLSD;
  } else {
    switch (NETCONNTYPE_GROUP(msg->conn->type)) {
#if LWIP_RAW
  case NETCONN_RAW:
    msg->err = raw_connect(msg->conn->pcb.raw, msg->msg.bc.ipaddr);
    break;
#endif /* LWIP_RAW */
#if LWIP_UDP
  case NETCONN_UDP:
    msg->err = udp_connect(msg->conn->pcb.udp, msg->msg.bc.ipaddr, msg->msg.bc.port);
    break;
#endif /* LWIP_UDP */
#if LWIP_TCP
  case NETCONN_TCP:
    /* Prevent connect while doing any other action. */
    if (msg->conn->state != NETCONN_NONE) {
      msg->err = ERR_ISCONN;
    } else {
      setup_tcp(msg->conn);
      msg->err = tcp_connect(msg->conn->pcb.tcp, msg->msg.bc.ipaddr,
        msg->msg.bc.port, do_connected);
      if (msg->err == ERR_OK) {
        u8_t non_blocking = netconn_is_nonblocking(msg->conn);
        msg->conn->state = NETCONN_CONNECT;
        SET_NONBLOCKING_CONNECT(msg->conn, non_blocking);
        if (non_blocking) {
          msg->err = ERR_INPROGRESS;
        } else {
          msg->conn->current_msg = msg;
          /* sys_sem_signal() is called from do_connected (or err_tcp()),
          * when the connection is established! */
          return;
        }
      }
    }
    break;
#endif /* LWIP_TCP */
  default:
    LWIP_ERROR("Invalid netconn type", 0, do{ msg->err = ERR_VAL; }while(0));
    break;
    }
  }
  sys_sem_signal(&msg->conn->op_completed);
}

/**
 * Connect a pcb contained inside a netconn
 * Only used for UDP netconns.
 * Called from netconn_disconnect.
 *
 * @param msg the api_msg_msg pointing to the connection to disconnect
 */
void
do_disconnect(struct api_msg_msg *msg)
{
#if LWIP_UDP
  if (NETCONNTYPE_GROUP(msg->conn->type) == NETCONN_UDP) {
    udp_disconnect(msg->conn->pcb.udp);
    msg->err = ERR_OK;
  } else
#endif /* LWIP_UDP */
  {
    msg->err = ERR_VAL;
  }
  TCPIP_APIMSG_ACK(msg);
}

#if LWIP_TCP
/**
 * Set a TCP pcb contained in a netconn into listen mode
 * Called from netconn_listen.
 *
 * @param msg the api_msg_msg pointing to the connection
 */
void
do_listen(struct api_msg_msg *msg)
{
  if (ERR_IS_FATAL(msg->conn->last_err)) {
    msg->err = msg->conn->last_err;
  } else {
    msg->err = ERR_CONN;
    if (msg->conn->pcb.tcp != NULL) {
      if (msg->conn->type == NETCONN_TCP) {
        if (msg->conn->state == NETCONN_NONE) {
#if TCP_LISTEN_BACKLOG
          struct tcp_pcb* lpcb = tcp_listen_with_backlog(msg->conn->pcb.tcp, msg->msg.lb.backlog);
#else  /* TCP_LISTEN_BACKLOG */
          struct tcp_pcb* lpcb = tcp_listen(msg->conn->pcb.tcp);
#endif /* TCP_LISTEN_BACKLOG */
          if (lpcb == NULL) {
            /* in this case, the old pcb is still allocated */
            msg->err = ERR_MEM;
          } else {
            /* delete the recvmbox and allocate the acceptmbox */
            if (sys_mbox_valid(&msg->conn->recvmbox)) {
              /** @todo: should we drain the recvmbox here? */
              sys_mbox_free(&msg->conn->recvmbox);
              sys_mbox_set_invalid(&msg->conn->recvmbox);
            }
            msg->err = ERR_OK;
            if (!sys_mbox_valid(&msg->conn->acceptmbox)) {
              msg->err = sys_mbox_new(&msg->conn->acceptmbox, DEFAULT_ACCEPTMBOX_SIZE);
            }
            if (msg->err == ERR_OK) {
              msg->conn->state = NETCONN_LISTEN;
              msg->conn->pcb.tcp = lpcb;
              tcp_arg(msg->conn->pcb.tcp, msg->conn);
              tcp_accept(msg->conn->pcb.tcp, accept_function);
            } else {
              /* since the old pcb is already deallocated, free lpcb now */
              tcp_close(lpcb);
              msg->conn->pcb.tcp = NULL;
            }
          }
        }
      } else {
        msg->err = ERR_ARG;
      }
    }
  }
  TCPIP_APIMSG_ACK(msg);
}
#endif /* LWIP_TCP */

/**
 * Send some data on a RAW or UDP pcb contained in a netconn
 * Called from netconn_send
 *
 * @param msg the api_msg_msg pointing to the connection
 */
void
do_send(struct api_msg_msg *msg)
{
  if (ERR_IS_FATAL(msg->conn->last_err)) {
    msg->err = msg->conn->last_err;
  } else {
    msg->err = ERR_CONN;
    if (msg->conn->pcb.tcp != NULL) {
      switch (NETCONNTYPE_GROUP(msg->conn->type)) {
#if LWIP_RAW
      case NETCONN_RAW:
        if (ip_addr_isany(&msg->msg.b->addr)) {
          msg->err = raw_send(msg->conn->pcb.raw, msg->msg.b->p);
        } else {
          msg->err = raw_sendto(msg->conn->pcb.raw, msg->msg.b->p, &msg->msg.b->addr);
        }
        break;
#endif
#if LWIP_UDP
      case NETCONN_UDP:
#if LWIP_CHECKSUM_ON_COPY
        if (ip_addr_isany(&msg->msg.b->addr)) {
          msg->err = udp_send_chksum(msg->conn->pcb.udp, msg->msg.b->p,
            msg->msg.b->flags & NETBUF_FLAG_CHKSUM, msg->msg.b->toport_chksum);
        } else {
          msg->err = udp_sendto_chksum(msg->conn->pcb.udp, msg->msg.b->p,
            &msg->msg.b->addr, msg->msg.b->port,
            msg->msg.b->flags & NETBUF_FLAG_CHKSUM, msg->msg.b->toport_chksum);
        }
#else /* LWIP_CHECKSUM_ON_COPY */
        if (ip_addr_isany(&msg->msg.b->addr)) {
          msg->err = udp_send(msg->conn->pcb.udp, msg->msg.b->p);
        } else {
          msg->err = udp_sendto(msg->conn->pcb.udp, msg->msg.b->p, &msg->msg.b->addr, msg->msg.b->port);
        }
#endif /* LWIP_CHECKSUM_ON_COPY */
        break;
#endif /* LWIP_UDP */
      default:
        break;
      }
    }
  }
  TCPIP_APIMSG_ACK(msg);
}

#if LWIP_TCP
/**
 * Indicate data has been received from a TCP pcb contained in a netconn
 * Called from netconn_recv
 *
 * @param msg the api_msg_msg pointing to the connection
 */
void
do_recv(struct api_msg_msg *msg)
{
  msg->err = ERR_OK;
  if (msg->conn->pcb.tcp != NULL) {
    if (msg->conn->type == NETCONN_TCP) {
#if TCP_LISTEN_BACKLOG
      if (msg->conn->pcb.tcp->state == LISTEN) {
        tcp_accepted(msg->conn->pcb.tcp);
      } else
#endif /* TCP_LISTEN_BACKLOG */
      {
        u32_t remaining = msg->msg.r.len;
        do {
          u16_t recved = (remaining > 0xffff) ? 0xffff : (u16_t)remaining;
          tcp_recved(msg->conn->pcb.tcp, recved);
          remaining -= recved;
        }while(remaining != 0);
      }
    }
  }
  TCPIP_APIMSG_ACK(msg);
}

/**
 * See if more data needs to be written from a previous call to netconn_write.
 * Called initially from do_write. If the first call can't send all data
 * (because of low memory or empty send-buffer), this function is called again
 * from sent_tcp() or poll_tcp() to send more data. If all data is sent, the
 * blocking application thread (waiting in netconn_write) is released.
 *
 * @param conn netconn (that is currently in state NETCONN_WRITE) to process
 * @return ERR_OK
 *         ERR_MEM if LWIP_TCPIP_CORE_LOCKING=1 and sending hasn't yet finished
 */
static err_t
do_writemore(struct netconn *conn)
{
  err_t err;
  void *dataptr;
  u16_t len, available;
  u8_t write_finished = 0;
  size_t diff;
  u8_t dontblock = netconn_is_nonblocking(conn) ||
       (conn->current_msg->msg.w.apiflags & NETCONN_DONTBLOCK);
  u8_t apiflags = conn->current_msg->msg.w.apiflags;

  LWIP_ASSERT("conn != NULL", conn != NULL);
  LWIP_ASSERT("conn->state == NETCONN_WRITE", (conn->state == NETCONN_WRITE));
  LWIP_ASSERT("conn->current_msg != NULL", conn->current_msg != NULL);
  LWIP_ASSERT("conn->pcb.tcp != NULL", conn->pcb.tcp != NULL);
  LWIP_ASSERT("conn->write_offset < conn->current_msg->msg.w.len",
    conn->write_offset < conn->current_msg->msg.w.len);

#if LWIP_SO_SNDTIMEO
  if ((conn->send_timeout != 0) &&
      ((s32_t)(sys_now() - conn->current_msg->msg.w.time_started) >= conn->send_timeout)) {
    write_finished = 1;
    if (conn->write_offset == 0) {
      /* nothing has been written */
      err = ERR_WOULDBLOCK;
      conn->current_msg->msg.w.len = 0;
    } else {
      /* partial write */
      err = ERR_OK;
      conn->current_msg->msg.w.len = conn->write_offset;
    }
  } else
#endif /* LWIP_SO_SNDTIMEO */
  {
    dataptr = (u8_t*)conn->current_msg->msg.w.dataptr + conn->write_offset;
    diff = conn->current_msg->msg.w.len - conn->write_offset;
    if (diff > 0xffffUL) { /* max_u16_t */
      len = 0xffff;
#if LWIP_TCPIP_CORE_LOCKING
      conn->flags |= NETCONN_FLAG_WRITE_DELAYED;
#endif
      apiflags |= TCP_WRITE_FLAG_MORE;
    } else {
      len = (u16_t)diff;
    }
    available = tcp_sndbuf(conn->pcb.tcp);
    if (available < len) {
      /* don't try to write more than sendbuf */
      len = available;
      if (dontblock){ 
        if (!len) {
          err = ERR_WOULDBLOCK;
          goto err_mem;
        }
      } else {
#if LWIP_TCPIP_CORE_LOCKING
        conn->flags |= NETCONN_FLAG_WRITE_DELAYED;
#endif
        apiflags |= TCP_WRITE_FLAG_MORE;
      }
    }
    LWIP_ASSERT("do_writemore: invalid length!", ((conn->write_offset + len) <= conn->current_msg->msg.w.len));
    err = tcp_write(conn->pcb.tcp, dataptr, len, apiflags);
    /* if OK or memory error, check available space */
    if ((err == ERR_OK) || (err == ERR_MEM)) {
err_mem:
      if (dontblock && (len < conn->current_msg->msg.w.len)) {
        /* non-blocking write did not write everything: mark the pcb non-writable
           and let poll_tcp check writable space to mark the pcb writable again */
        API_EVENT(conn, NETCONN_EVT_SENDMINUS, len);
        conn->flags |= NETCONN_FLAG_CHECK_WRITESPACE;
      } else if ((tcp_sndbuf(conn->pcb.tcp) <= TCP_SNDLOWAT) ||
                 (tcp_sndqueuelen(conn->pcb.tcp) >= TCP_SNDQUEUELOWAT)) {
        /* The queued byte- or pbuf-count exceeds the configured low-water limit,
           let select mark this pcb as non-writable. */
        API_EVENT(conn, NETCONN_EVT_SENDMINUS, len);
      }
    }

    if (err == ERR_OK) {
      conn->write_offset += len;
      if ((conn->write_offset == conn->current_msg->msg.w.len) || dontblock) {
        /* return sent length */
        conn->current_msg->msg.w.len = conn->write_offset;
        /* everything was written */
        write_finished = 1;
        conn->write_offset = 0;
      }
      tcp_output(conn->pcb.tcp);
    } else if ((err == ERR_MEM) && !dontblock) {
      /* If ERR_MEM, we wait for sent_tcp or poll_tcp to be called
         we do NOT return to the application thread, since ERR_MEM is
         only a temporary error! */

      /* tcp_write returned ERR_MEM, try tcp_output anyway */
      tcp_output(conn->pcb.tcp);

#if LWIP_TCPIP_CORE_LOCKING
      conn->flags |= NETCONN_FLAG_WRITE_DELAYED;
#endif
    } else {
      /* On errors != ERR_MEM, we don't try writing any more but return
         the error to the application thread. */
      write_finished = 1;
      conn->current_msg->msg.w.len = 0;
    }
  }
  if (write_finished) {
    /* everything was written: set back connection state
       and back to application task */
    conn->current_msg->err = err;
    conn->current_msg = NULL;
    conn->state = NETCONN_NONE;
#if LWIP_TCPIP_CORE_LOCKING
    if ((conn->flags & NETCONN_FLAG_WRITE_DELAYED) != 0)
#endif
    {
      sys_sem_signal(&conn->op_completed);
    }
  }
#if LWIP_TCPIP_CORE_LOCKING
  else
    return ERR_MEM;
#endif
  return ERR_OK;
}
#endif /* LWIP_TCP */

/**
 * Send some data on a TCP pcb contained in a netconn
 * Called from netconn_write
 *
 * @param msg the api_msg_msg pointing to the connection
 */
void
do_write(struct api_msg_msg *msg)
{
  if (ERR_IS_FATAL(msg->conn->last_err)) {
    msg->err = msg->conn->last_err;
  } else {
    if (msg->conn->type == NETCONN_TCP) {
#if LWIP_TCP
      if (msg->conn->state != NETCONN_NONE) {
        /* netconn is connecting, closing or in blocking write */
        msg->err = ERR_INPROGRESS;
      } else if (msg->conn->pcb.tcp != NULL) {
        msg->conn->state = NETCONN_WRITE;
        /* set all the variables used by do_writemore */
        LWIP_ASSERT("already writing or closing", msg->conn->current_msg == NULL &&
          msg->conn->write_offset == 0);
        LWIP_ASSERT("msg->msg.w.len != 0", msg->msg.w.len != 0);
        msg->conn->current_msg = msg;
        msg->conn->write_offset = 0;
#if LWIP_TCPIP_CORE_LOCKING
        msg->conn->flags &= ~NETCONN_FLAG_WRITE_DELAYED;
        if (do_writemore(msg->conn) != ERR_OK) {
          LWIP_ASSERT("state!", msg->conn->state == NETCONN_WRITE);
          UNLOCK_TCPIP_CORE();
          sys_arch_sem_wait(&msg->conn->op_completed, 0);
          LOCK_TCPIP_CORE();
          LWIP_ASSERT("state!", msg->conn->state == NETCONN_NONE);
        }
#else /* LWIP_TCPIP_CORE_LOCKING */
        do_writemore(msg->conn);
#endif /* LWIP_TCPIP_CORE_LOCKING */
        /* for both cases: if do_writemore was called, don't ACK the APIMSG
           since do_writemore ACKs it! */
        return;
      } else {
        msg->err = ERR_CONN;
      }
#else /* LWIP_TCP */
      msg->err = ERR_VAL;
#endif /* LWIP_TCP */
#if (LWIP_UDP || LWIP_RAW)
    } else {
      msg->err = ERR_VAL;
#endif /* (LWIP_UDP || LWIP_RAW) */
    }
  }
  TCPIP_APIMSG_ACK(msg);
}

/**
 * Return a connection's local or remote address
 * Called from netconn_getaddr
 *
 * @param msg the api_msg_msg pointing to the connection
 */
void
do_getaddr(struct api_msg_msg *msg)
{
  if (msg->conn->pcb.ip != NULL) {
    *(msg->msg.ad.ipaddr) = (msg->msg.ad.local ? msg->conn->pcb.ip->local_ip :
                             msg->conn->pcb.ip->remote_ip);

    msg->err = ERR_OK;
    switch (NETCONNTYPE_GROUP(msg->conn->type)) {
#if LWIP_RAW
    case NETCONN_RAW:
      if (msg->msg.ad.local) {
        *(msg->msg.ad.port) = msg->conn->pcb.raw->protocol;
      } else {
        /* return an error as connecting is only a helper for upper layers */
        msg->err = ERR_CONN;
      }
      break;
#endif /* LWIP_RAW */
#if LWIP_UDP
    case NETCONN_UDP:
      if (msg->msg.ad.local) {
        *(msg->msg.ad.port) = msg->conn->pcb.udp->local_port;
      } else {
        if ((msg->conn->pcb.udp->flags & UDP_FLAGS_CONNECTED) == 0) {
          msg->err = ERR_CONN;
        } else {
          *(msg->msg.ad.port) = msg->conn->pcb.udp->remote_port;
        }
      }
      break;
#endif /* LWIP_UDP */
#if LWIP_TCP
    case NETCONN_TCP:
      *(msg->msg.ad.port) = (msg->msg.ad.local?msg->conn->pcb.tcp->local_port:msg->conn->pcb.tcp->remote_port);
      break;
#endif /* LWIP_TCP */
    default:
      LWIP_ASSERT("invalid netconn_type", 0);
      break;
    }
  } else {
    msg->err = ERR_CONN;
  }
  TCPIP_APIMSG_ACK(msg);
}

/**
 * Close a TCP pcb contained in a netconn
 * Called from netconn_close
 *
 * @param msg the api_msg_msg pointing to the connection
 */
void
do_close(struct api_msg_msg *msg)
{
  dbg_nc_check(msg->conn, (u32_t)__builtin_return_address(0));  /* 诊断: 是否在操作已释放的 netconn */
#if LWIP_TCP
  /* @todo: abort running write/connect? */
  if ((msg->conn->state != NETCONN_NONE) && (msg->conn->state != NETCONN_LISTEN)) {
    /* this only happens for TCP netconns */
    LWIP_ASSERT("msg->conn->type == NETCONN_TCP", msg->conn->type == NETCONN_TCP);
    msg->err = ERR_INPROGRESS;
    g_dbg_close_inprog++;                              /* 调试计数 */
  } else if ((msg->conn->pcb.tcp != NULL) && (msg->conn->type == NETCONN_TCP)) {
    if ((msg->msg.sd.shut != NETCONN_SHUT_RDWR) && (msg->conn->state == NETCONN_LISTEN)) {
      /* LISTEN doesn't support half shutdown */
      msg->err = ERR_CONN;
    } else {
      if (msg->msg.sd.shut & NETCONN_SHUT_RD) {
        /* Drain and delete mboxes */
        netconn_drain(msg->conn);
      }
      LWIP_ASSERT("already writing or closing", msg->conn->current_msg == NULL &&
        msg->conn->write_offset == 0);
      msg->conn->state = NETCONN_CLOSE;
      msg->conn->current_msg = msg;
      do_close_internal(msg->conn);
      /* for tcp netconns, do_close_internal ACKs the message */
      return;
    }
  } else
#endif /* LWIP_TCP */
  {
    msg->err = ERR_VAL;
  }
  sys_sem_signal(&msg->conn->op_completed);
}

#if LWIP_IGMP
/**
 * Join multicast groups for UDP netconns.
 * Called from netconn_join_leave_group
 *
 * @param msg the api_msg_msg pointing to the connection
 */
void
do_join_leave_group(struct api_msg_msg *msg)
{ 
  if (ERR_IS_FATAL(msg->conn->last_err)) {
    msg->err = msg->conn->last_err;
  } else {
    if (msg->conn->pcb.tcp != NULL) {
      if (NETCONNTYPE_GROUP(msg->conn->type) == NETCONN_UDP) {
#if LWIP_UDP
        if (msg->msg.jl.join_or_leave == NETCONN_JOIN) {
          msg->err = igmp_joingroup(msg->msg.jl.netif_addr, msg->msg.jl.multiaddr);
        } else {
          msg->err = igmp_leavegroup(msg->msg.jl.netif_addr, msg->msg.jl.multiaddr);
        }
#endif /* LWIP_UDP */
#if (LWIP_TCP || LWIP_RAW)
      } else {
        msg->err = ERR_VAL;
#endif /* (LWIP_TCP || LWIP_RAW) */
      }
    } else {
      msg->err = ERR_CONN;
    }
  }
  TCPIP_APIMSG_ACK(msg);
}
#endif /* LWIP_IGMP */

#if LWIP_DNS
/**
 * Callback function that is called when DNS name is resolved
 * (or on timeout). A waiting application thread is waked up by
 * signaling the semaphore.
 */
static void
do_dns_found(const char *name, ip_addr_t *ipaddr, void *arg)
{
  struct dns_api_msg *msg = (struct dns_api_msg*)arg;

  LWIP_ASSERT("DNS response for wrong host name", strcmp(msg->name, name) == 0);
  LWIP_UNUSED_ARG(name);

  if (ipaddr == NULL) {
    /* timeout or memory error */
    *msg->err = ERR_VAL;
  } else {
    /* address was resolved */
    *msg->err = ERR_OK;
    *msg->addr = *ipaddr;
  }
  /* wake up the application task waiting in netconn_gethostbyname */
  sys_sem_signal(msg->sem);
}

/**
 * Execute a DNS query
 * Called from netconn_gethostbyname
 *
 * @param arg the dns_api_msg pointing to the query
 */
void
do_gethostbyname(void *arg)
{
  struct dns_api_msg *msg = (struct dns_api_msg*)arg;

  *msg->err = dns_gethostbyname(msg->name, msg->addr, do_dns_found, msg);
  if (*msg->err != ERR_INPROGRESS) {
    /* on error or immediate success, wake up the application
     * task waiting in netconn_gethostbyname */
    sys_sem_signal(msg->sem);
  }
}
#endif /* LWIP_DNS */

#endif /* LWIP_NETCONN */
