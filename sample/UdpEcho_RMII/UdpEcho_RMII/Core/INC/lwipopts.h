/**
  ******************************************************************************
  * @file    lwipopts.h
  * @author  MCU Application Team
  * @version V1.1.0
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2023 Puya Semiconductor Co.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by Puya under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2016 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */

#ifndef __LWIPOPTS_H__
#define __LWIPOPTS_H__
/**
 * SYS_LIGHTWEIGHT_PROT==1: if you want inter-task protection for certain
 * critical regions during buffer allocation, deallocation and memory
 * allocation and deallocation.
 */
#define SYS_LIGHTWEIGHT_PROT    0

/**
* NO_SYS==1: Provides VERY minimal functionality. Otherwise,
* use lwIP facilities.
*/
#define NO_SYS                  1
/**
 * NO_SYS_NO_TIMERS==1: Drop support for sys_timeout when NO_SYS==1
 * Mainly for compatibility to old versions.
 */
#define NO_SYS_NO_TIMERS        0

/* ---------- Memory options ---------- */
/* MEM_ALIGNMENT: should be set to the alignment of the CPU for which
   lwIP is compiled. 4 byte alignment -> define MEM_ALIGNMENT to 4, 2
   byte alignment -> define MEM_ALIGNMENT to 2. */
#define MEM_ALIGNMENT           4

/* MEM_SIZE: the size of the heap memory. If the application will send
a lot of data that needs to be copied, this should be set high. */
#define MEM_SIZE                (16*1024)

/* MEMP_NUM_PBUF: the number of memp struct pbufs. If the application
   sends a lot of data out of ROM (or other static memory), this
   should be set high. */
#define MEMP_NUM_PBUF           10
/* MEMP_NUM_RAW_PCB: the number of raw protocol control blocks. One
   per active RAW "connection". */
#define MEMP_NUM_RAW_PCB        4
/* MEMP_NUM_UDP_PCB: the number of UDP protocol control blocks. One
   per active UDP "connection". */
#define MEMP_NUM_UDP_PCB        4
/* MEMP_NUM_TCP_PCB: the number of simulatenously active TCP
   connections. */
#define MEMP_NUM_TCP_PCB        6
/* MEMP_NUM_TCP_PCB_LISTEN: the number of listening TCP
   connections. */
#define MEMP_NUM_TCP_PCB_LISTEN 6
/* MEMP_NUM_TCP_SEG: the number of simultaneously queued TCP
   segments. */
#define MEMP_NUM_TCP_SEG        16
/* MEMP_NUM_SYS_TIMEOUT: the number of simulateously active
   timeouts. */
#define MEMP_NUM_SYS_TIMEOUT    6

/* ---------- TCP options ---------- */
#define LWIP_TCP                1
#define TCP_TTL                 255

/* Controls if TCP should queue segments that arrive out of
   order. Define to 0 if your device is low on memory. */
#define TCP_QUEUE_OOSEQ         0

/* TCP Maximum segment size. */
#define TCP_MSS                 (1500-40)	  /* TCP_MSS = (Ethernet MTU - IP header size - TCP header size) */

/* TCP sender buffer space (bytes). This must be at least as much
   as (2 * TCP_MSS) for things to work. */
#define TCP_SND_BUF             (4*TCP_MSS)

/*  TCP_SND_QUEUELEN: TCP sender buffer space (pbufs). This must be at least
  as much as (2 * TCP_SND_BUF/TCP_MSS) for things to work. */

#define TCP_SND_QUEUELEN        (2*TCP_SND_BUF/TCP_MSS)

/* TCP receive window. This must be at least (2 * TCP_MSS) for things to work well. */
#define TCP_WND                 (2*TCP_MSS)

/* TCP selective acknowledgements. */
#define LWIP_TCP_SACK_OUT       0

/* TCP sanity checks (disable to save some code size). */
#define LWIP_DISABLE_TCP_SANITY_CHECKS  0

/* ---------- Pbuf options ---------- */
/* PBUF_POOL_SIZE: the number of buffers in the pbuf pool. */
#define PBUF_POOL_SIZE          12
/* PBUF_POOL_BUFSIZE: the size of each pbuf in the pbuf pool. */
#define PBUF_POOL_BUFSIZE       592

/* ---------- IPv4 options ---------- */
/* LWIP_ARP==1: Enable ARP functionality. */
#define LWIP_ARP                1

/* ---------- ICMP options ---------- */
#define LWIP_ICMP                       1

/* ---------- IGMP options ---------- */
#define LWIP_IGMP                       0

/* ---------- DNS options ---------- */
#define LWIP_DNS                        0

/* ---------- DHCP options ---------- */
/* Define LWIP_DHCP to 1 if you want DHCP configuration of
   interfaces. DHCP is not implemented in lwIP 0.5.1, however, so
   turning this on does currently not work. */
#define LWIP_DHCP               0

/* ---------- UDP options ---------- */
#define LWIP_UDP                1
#define UDP_TTL                 255

/* ---------- Statistics options ---------- */
#define LWIP_STATS              0
#define LWIP_PROVIDE_ERRNO 1

/* ---------- link callback options ---------- */
/* LWIP_NETIF_LINK_CALLBACK==1: Support a callback function from an interface
 * whenever the link changes (i.e., link down)
 */
#define LWIP_NETIF_LINK_CALLBACK        0

/* LWIP_NETIF_STATUS_CALLBACK==1: Support a callback function whenever an
 * interface changes its up/down status.
 */
#define LWIP_NETIF_STATUS_CALLBACK      0

/* LWIP_NETIF_EXT_STATUS_CALLBACK==1: Support an extended callback function
 * for several netif related event types.
 */
#define LWIP_NETIF_EXT_STATUS_CALLBACK  0

/* LWIP_NETIF_LOOPBACK==1: Support sending packets with a destination IP
 * address equal to the netif IP address.
 */
#define LWIP_NETIF_LOOPBACK     0
/*
   --------------------------------------
   ---------- Checksum options ----------
   --------------------------------------
*/

/*
The STM32F4x7 allows computing and verifying the IP, UDP, TCP and ICMP checksums by hardware:
 - To use this feature let the following define uncommented.
 - To disable it and process by CPU comment the  the checksum.
*/

/* Per-netif checksum control (LWIP_CHECKSUM_CTRL_PER_NETIF==1): when enabled the netif
   driver configures checksum per interface. The software GEN/CHECK macros below follow
   the GUI (Checksum group, aligned with CubeMX LWIP XML derived defaults):
   value = (CHECKSUM_BY_HARDWARE || !LWIP_CHECKSUM_CTRL_PER_NETIF) ? 0 : GUI value.
   GUI keeps those switches disabled in the hardware-checksum / per-netif-off states. */
#define LWIP_CHECKSUM_CTRL_PER_NETIF    0
/* CHECKSUM_GEN_IP==0: Generate checksums by hardware/disabled; ==1 software for outgoing IP packets.*/
#define CHECKSUM_GEN_IP                 0/* CHECKSUM_GEN_UDP==0: disabled; ==1 software for outgoing UDP packets.*/
#define CHECKSUM_GEN_UDP                0/* CHECKSUM_GEN_TCP==0: disabled; ==1 software for outgoing TCP packets.*/
#define CHECKSUM_GEN_TCP                0/* CHECKSUM_GEN_ICMP==0: disabled; ==1 software for outgoing ICMP packets.*/
#define CHECKSUM_GEN_ICMP               0/* CHECKSUM_GEN_ICMP6==0: disabled; ==1 software for outgoing ICMP6 packets.*/
#define CHECKSUM_GEN_ICMP6              0/* CHECKSUM_CHECK_IP==0: disabled; ==1 software check for incoming IP packets.*/
#define CHECKSUM_CHECK_IP               0/* CHECKSUM_CHECK_UDP==0: disabled; ==1 software check for incoming UDP packets.*/
#define CHECKSUM_CHECK_UDP              0/* CHECKSUM_CHECK_TCP==0: disabled; ==1 software check for incoming TCP packets.*/
#define CHECKSUM_CHECK_TCP              0/* CHECKSUM_CHECK_ICMP==0: disabled; ==1 software check for incoming ICMP packets.*/
#define CHECKSUM_CHECK_ICMP             0/* CHECKSUM_CHECK_ICMP6==0: disabled; ==1 software check for incoming ICMP6 packets.*/
#define CHECKSUM_CHECK_ICMP6            0

/* ---------- IPv6 options ---------- */
/* LWIP_IPV6==1: enable IPv6. Requires the core/ipv6 sources, which code/2.1/pre.cjs
   adds to the compile group only when this switch is 1. */
#define LWIP_IPV6                       0

/* ---------- IPv6 advanced options (emitted when LWIP_IPV6==1) ---------- */

/* ---------- Applications / optional features ---------- */
/* Whether the sources under apps/ are compiled is driven by the same switches below
   (FEATURE_APPS in code/2.1/pre.cjs): apps/http, apps/sntp, apps/smtp, apps/mdns, apps/tftp. */
#define LWIP_HTTPD                      0
#define LWIP_SNTP                       0
#define LWIP_SMTP                       0
#define LWIP_MDNS                       0
#define LWIP_TFTP                       0

/* ---------- SNTP detailed options ---------- */
/* Only emitted when LWIP_SNTP==1 (SNTP Options group in the GUI). These mirror the
   CubeMX LWIP XML defaults and override the SDK sntp_opts.h defaults (#if !defined). */

/* ---------- Misc options ---------- */
/* LWIP_TIMERS==1: support for sys_timeout (used by tcpip/arp/dhcp timers). */
#define LWIP_TIMERS                     1
/* LWIP_DISABLE_MEMP_SANITY_CHECKS==1: disable MEMP sanity checks. */
#define LWIP_DISABLE_MEMP_SANITY_CHECKS 0
/* MEMP_NUM_LOCALHOSTLIST: number of host entries in the local host list (DNS). */
#define MEMP_NUM_LOCALHOSTLIST          5
/* LWIP_MULTICAST_TX_OPTIONS: enable multicast TX; follows IGMP/IPv6 (CubeXML linkage),
   required for IGMP reports / IPv6 MLD to actually leave the host. */
#define LWIP_MULTICAST_TX_OPTIONS       0

/* ---------- HTTPD advanced options (emitted when LWIP_HTTPD==1) ---------- */

/* ---------- SNMP options ---------- */
/* LWIP_SNMP==1: enable the SNMP agent. Its sources (apps/snmp + MIB2) are added by pre.cjs.
   Agent transport: GUI SNMP_USE_NETCONN wins; when both are 0 (legacy data) it falls back to
   netconn in RTOS mode, raw otherwise. When enabled, LWIP_STATS above is forced to 1. */
#define LWIP_SNMP                       0
#define SNMP_USE_NETCONN                0
#define SNMP_USE_RAW                    0

/* ---------- PPP options ---------- */
/* PPP_SUPPORT==1: enable PPP. Sub-protocol switches come from the GUI (PPPOS defaults on);
   the concrete session is created by the application (pppos_create/pppoe_create/pppol2tp_create).
   Sources under netif/ppp/ are added by pre.cjs when PPP_SUPPORT=1; advanced macros follow
   CubeMX XML defaults. */
#define PPP_SUPPORT                     0
#define PPPOS_SUPPORT                   1
#define PPPOE_SUPPORT                   0
#define PPPOL2TP_SUPPORT                0

/* ---------- LwIP Debug / Perf options ---------- */
/* LWIP_DBG_MIN_LEVEL: minimum debug level (macro text mapped in pre.cjs). */
#define LWIP_DBG_MIN_LEVEL              LWIP_DBG_LEVEL_ALL
/* LWIP_PERF==1: enable performance hooks (PERF_START/PERF_STOP are empty in arch/perf.h). */
#define LWIP_PERF                       0

/*
   ----------------------------------------------
   ---------- Sequential layer options ----------
   ----------------------------------------------
*/
/**
 * LWIP_NETCONN==1: Enable Netconn API (require to use api_lib.c)
 * socket API is built on top of the netconn API (sockets.c is implemented via netconn_*),
 * so whenever LWIP_SOCKET is enabled, LWIP_NETCONN must be 1.
 * With NO_SYS=1 (no OS), the Sequential API is unavailable (init.c #error), forced to 0.
 */
#define LWIP_NETCONN                    0

/*
   ------------------------------------
   ---------- Socket options ----------
   ------------------------------------
*/
/**
 * LWIP_SOCKET==1: Enable Socket API (require to use sockets.c)
 * With NO_SYS=1 (no OS), the Sequential API is unavailable (init.c #error), forced to 0.
 */
#define LWIP_SOCKET                     0

/**
 * LWIP_COMPAT_SOCKETS==1: Enable BSD-style sockets functions names through defines.
 * LWIP_COMPAT_SOCKETS==2: Same as ==1 but correctly named functions are created.
 */
#define LWIP_COMPAT_SOCKETS             1

/**
 * LWIP_POSIX_SOCKETS_IO_NAMES==1: Enable POSIX-style sockets functions names.
 */
#define LWIP_POSIX_SOCKETS_IO_NAMES     1

/**
 * LWIP_SOCKET_OFFSET==n: Increases the file descriptor number created by LwIP with n.
 */
#define LWIP_SOCKET_OFFSET              0

/**
 * LWIP_SOCKET_SELECT==1: enable select() for sockets.
 */
#define LWIP_SOCKET_SELECT              1

/**
 * LWIP_SOCKET_POLL==1: enable poll() for sockets.
 */
#define LWIP_SOCKET_POLL                1

/*
   ---------------------------------
   ---------- OS options ----------
   ---------------------------------
*/

#define DEFAULT_UDP_RECVMBOX_SIZE       10
#define DEFAULT_RAW_RECVMBOX_SIZE       0
#define DEFAULT_TCP_RECVMBOX_SIZE       6
#define DEFAULT_ACCEPTMBOX_SIZE         6
#define DEFAULT_THREAD_STACKSIZE        1024

#define DEFAULT_THREAD_NAME             "lwip"
#define DEFAULT_THREAD_PRIO             3

#define TCPIP_THREAD_NAME               "tcpip_thread"
#define TCPIP_THREAD_STACKSIZE          1024
#define TCPIP_MBOX_SIZE                 6
#define TCPIP_THREAD_PRIO               3

/*
   ----------------------------------------
   ---------- Lwip Debug options ----------
   ----------------------------------------
*/
//#define LWIP_DEBUG                      1
#endif /* __LWIPOPTS_H__ */

/************************ (C) COPYRIGHT Puya *****END OF FILE******************/
