/**
 ******************************************************************************
 * @file    lwipopts.h
 * @author  MCD Application Team
 * @version V1.1.0
 * @date    07-October-2011
 * @brief   lwIP Options Configuration.
 *
 ******************************************************************************
 * @attention
 *
 * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
 * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
 * TIME. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY
 * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
 * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
 * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
 *
 ******************************************************************************
 ******************************************************************************
 * @brief
 * 	Updated to support FreeRTOS 10.0.0 and lwip 2.0.3
 *
 * @author  JW, Neutron Controls
 * @version V1.2.0
 * @date    Apr2018
 * @brief   lwIP Options Configuration,  Version 2.0.3
 * @author	CH
 * @version V1.2.1
 * @date    Nov2018
 * @brief	Changes for message header checksums.
 * 			Prevent lwip from adding header checksums into IP/TCP/UDP packets.
 * 			Done by hardware.
 *
 * @attention
 * <h2><center> OnzerOS&trade; SYSTEM LEVEL OPERATIONAL SOFTWARE</center></h2>
 * <h2><center>&copy; COPYRIGHT 2018 Neutron Controls</center></h2>
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * !DO NOT DISTRIBUTE!
 * For evaluation only.
 * Extended license agreement required for distribution and use!
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
#define SYS_LIGHTWEIGHT_PROT    			1

/**
 * NO_SYS==1: Provides VERY minimal functionality. Otherwise,
 * use lwIP facilities.
 */
#define NO_SYS                  			0

/**
 * LWIP_EVENT_API and LWIP_CALLBACK_API: Only one of these should be set to 1.
 *     LWIP_EVENT_API==1: The user defines lwip_tcp_event() to receive all
 *         events (accept, sent, etc) that happen in the system.
 *     LWIP_CALLBACK_API==1: The PCB callback function is called directly
 *         for the event. This is the default.
 */
#define LWIP_CALLBACK_API 					1
#define LWIP_EVENT_API   					0

/**
 * LWIP_TCPIP_CORE_LOCKING
 * Creates a global mutex that is held during TCPIP thread operations.
 * Can be locked by client code to perform lwIP operations without changing
 * into TCPIP thread using callbacks. See LOCK_TCPIP_CORE() and
 * UNLOCK_TCPIP_CORE().
 * Your system should provide mutexes supporting priority inversion to use this.
 */
#define LWIP_TCPIP_CORE_LOCKING         	1

/**
 * MEMP_NUM_NETCONN: the number of struct netconns.
 * (only needed if you use the sequential API, like api_lib.c)
 */
#define MEMP_NUM_NETCONN					8


/*
   ----------------------------------------------
   ---------------- IPV4 options ----------------
   ----------------------------------------------
*/

/***** IPV4 Options *****/
#define LWIP_IPV4   						1
#define IP_FORWARD   						0
#define IP_REASSEMBLY       				0
#define IP_FRAG             				0
#define IP_OPTIONS_ALLOWED					1
#define IP_REASS_MAXAGE   					3
#define IP_REASS_MAX_PBUFS  				10
#define IP_DEFAULT_TTL   					255
#define IP_SOF_BROADCAST   					0
#define IP_SOF_BROADCAST_RECV				0
#define IP_FORWARD_ALLOW_TX_ON_RX_NETIF		0
#define LWIP_RANDOMIZE_INITIAL_LOCAL_PORTS	0

/* ---------- ARP options ---------- */
#define LWIP_ARP   							1
#define ARP_TABLE_SIZE   					10
#define ARP_MAXAGE   						300
#define ARP_QUEUEING            			1//0
#define ARP_QUEUE_LEN   					3
#define ETHARP_SUPPORT_VLAN   				0
#define LWIP_ETHERNET   					LWIP_ARP
#define ETH_PAD_SIZE   						0
#define ETHARP_SUPPORT_STATIC_ENTRIES   	0
#define ETHARP_TABLE_MATCH_NETIF   			0

/* ---------- ICMP options --------- */
#define LWIP_ICMP                       	1
#define ICMP_TTL   							(IP_DEFAULT_TTL)
#define LWIP_BROADCAST_PING   				0
#define LWIP_MULTICAST_PING   				0

/* ---------- DHCP options --------- */
#define LWIP_DHCP               			1

/* -------- AUTOIP options --------- */
#define LWIP_AUTOIP   						0

/* ---------- IGMP options --------- */
#define LWIP_IGMP   						0



/* ---------- TCP options ---------- */
#define LWIP_TCP   							1
#define TCP_TTL   							(IP_DEFAULT_TTL)
#define TCP_MSS								512 //Raymond(2024-02-04) (1500 - 40)	  /* TCP_MSS = (Ethernet MTU - IP header size - TCP header size) */
#define TCP_WND 							(2 * TCP_MSS)
#define TCP_MAXRTX							12
#define TCP_SYNMAXRTX						6
#define TCP_QUEUE_OOSEQ						0
#define TCP_CALCULATE_EFF_SEND_MSS			1
#define TCP_SND_BUF             			(2 * TCP_MSS)
//#define TCP_SND_QUEUELEN        			(2* TCP_SND_BUF/TCP_MSS)
#define TCP_SND_QUEUELEN   					((4 * (TCP_SND_BUF) + (TCP_MSS - 1))/(TCP_MSS))
#define TCP_SNDLOWAT   						LWIP_MIN(LWIP_MAX(((TCP_SND_BUF)/2), (2 * TCP_MSS) + 1), (TCP_SND_BUF) - 1)
#define TCP_SNDQUEUELOWAT   				LWIP_MAX(((TCP_SND_QUEUELEN)/2), 5)
#define TCP_OOSEQ_MAX_BYTES   				0
#define TCP_OOSEQ_MAX_PBUFS   				0
#define TCP_LISTEN_BACKLOG      			1
#define TCP_DEFAULT_LISTEN_BACKLOG   		0xff
#define TCP_OVERSIZE   						TCP_MSS
#define LWIP_TCP_TIMESTAMPS   				0
#define TCP_WND_UPDATE_THRESHOLD   			LWIP_MIN((TCP_WND / 4), (TCP_MSS * 4))
#define LWIP_WND_SCALE   					0

/* ---------- UDP options ---------- */
#define LWIP_UDP   							1
#define LWIP_UDPLITE   						0
#define UDP_TTL   							(IP_DEFAULT_TTL)
#define LWIP_NETBUF_RECVINFO				0

/* ---------- DNS options ---------- */
#define  LWIP_DNS   						0

/* ---------- RAW options ---------- */
#define  LWIP_RAW   						1//0
#define  RAW_TTL   							(IP_DEFAULT_TTL)

/* ---------- Memory options ---------- */
/* MEM_ALIGNMENT: should be set to the alignment of the CPU for which
   lwIP is compiled. 4 byte alignment -> define MEM_ALIGNMENT to 4, 2
   byte alignment -> define MEM_ALIGNMENT to 2. */
#define MEM_ALIGNMENT           			4

/* MEM_SIZE: the size of the heap memory. If the application will send
a lot of data that needs to be copied, this should be set high. */
#define MEM_SIZE                			(20 * 1536)//(10 * 1536)

/* MEMP_NUM_PBUF: the number of memp struct pbufs. If the application
   sends a lot of data out of ROM (or other static memory), this
   should be set high. */
#define MEMP_NUM_PBUF           			20

/* MEMP_NUM_SYS_TIMEOUT: the number of simulateously active
   timeouts. */
#define MEMP_NUM_SYS_TIMEOUT    			10

/* MEMP_NUM_UDP_PCB: the number of UDP protocol control blocks. One
   per active UDP "connection". */
#define MEMP_NUM_UDP_PCB        			6

/*
 * NOTE: These _TCP_ numbers will require adjustment based on the
 * applicaion's requirements.
 * All TCP based connections require PCBs
 */
/* MEMP_NUM_TCP_PCB: the number of simulatenously active TCP
   connections. */
#define MEMP_NUM_TCP_PCB        			8
/* MEMP_NUM_TCP_PCB_LISTEN: the number of listening TCP
   connections. */
#define MEMP_NUM_TCP_PCB_LISTEN 			4
/* MEMP_NUM_TCP_SEG: the number of simultaneously queued TCP
   segments. */
#define MEMP_NUM_TCP_SEG        			20



/* ---------- Pbuf options ---------- */
/* PBUF_POOL_SIZE: the number of buffers in the pbuf pool. */
#define PBUF_POOL_SIZE          			10

/* PBUF_POOL_BUFSIZE: the size of each pbuf in the pbuf pool. */
#define PBUF_POOL_BUFSIZE       			1536


/* ---------- Statistics options ---------- */
#define LWIP_STATS 							1
#define LWIP_PROVIDE_ERRNO 					1


/*
   --------------------------------------
   ---------- Checksum options ----------
   --------------------------------------
*/

#define CHECKSUM_BY_HARDWARE /* Hardware has the ability to insert header checksums for IP/UDP/TCP/ICMP.
                                Refer to CIC: Checksum Insertion Control bits in Ethernet MAC */

//#undef CHECKSUM_BY_HARDWARE

#ifdef CHECKSUM_BY_HARDWARE
  /* CHECKSUM_GEN_IP==0: don't generate checksums by stack for outgoing IP packets.*/
  #define CHECKSUM_GEN_IP                 0
  /* CHECKSUM_GEN_UDP==0: don't generate checksums by stack for outgoing UDP packets.*/
  #define CHECKSUM_GEN_UDP                0
  /* CHECKSUM_GEN_TCP==0: don't generate checksums by stack for outgoing TCP packets.*/
  #define CHECKSUM_GEN_TCP                0
  /* CHECKSUM_GEN_ICMP==0: don't generate checksums by stack for outgoing ICMP packets.*/
  #define CHECKSUM_GEN_ICMP               0
#else
  /* CHECKSUM_GEN_IP==1: generate checksums by stack for outgoing IP packets.*/
  #define CHECKSUM_GEN_IP                 1
  /* CHECKSUM_GEN_UDP==1:  generate checksums by stack for outgoing UDP packets.*/
  #define CHECKSUM_GEN_UDP                1
  /* CHECKSUM_GEN_TCP==1:  generate checksums by stack for outgoing TCP packets.*/
  #define CHECKSUM_GEN_TCP                1
  /* CHECKSUM_GEN_ICMP==1: generate checksums by stack for outgoing ICMP packets.*/
  #define CHECKSUM_GEN_ICMP               1
#endif

/* CHECKSUM_CHECK_UDP==1: Check checksums by hardware for incoming IP packets.*/
#define CHECKSUM_CHECK_IP               0		//1
/* CHECKSUM_CHECK_UDP==1: Check checksums by hardware for incoming UDP packets.*/
#define CHECKSUM_CHECK_UDP              0
/* CHECKSUM_CHECK_TCP==1: Check checksums by hardware for incoming TCP packets.*/
#define CHECKSUM_CHECK_TCP              0
/* CHECKSUM_CHECK_ICMP==1: Check checksums by hardware for incoming ICMP packets.*/
#define CHECKSUM_CHECK_ICMP             0


/*
   ----------------------------------------------
   ---------- Sequential layer options ----------
   ----------------------------------------------
*/
/**
 * LWIP_NETCONN==1: Enable Netconn API (require to use api_lib.c)
 */
#define LWIP_NETCONN                    1

/*
   ------------------------------------
   ---------- Socket options ----------
   ------------------------------------
*/
/**
 * LWIP_SOCKET==1: Enable Socket API (require to use sockets.c)
 */
#define LWIP_SOCKET                     0
#define LWIP_POSIX_SOCKETS_IO_NAMES		1
#define LWIP_NETCONN_SEM_PER_THREAD		0

/*
   -----------------------------------
   ---------- DEBUG options ----------
   -----------------------------------
*/

#define LWIP_DEBUG                      	LWIP_DBG_ON
//#define TCP_INPUT_DEBUG                 	LWIP_DBG_ON
//#define TCP_OUTPUT_DEBUG                	LWIP_DBG_ON
//#define TCP_QLEN_DEBUG                  	LWIP_DBG_ON
//#define ETHARP_DEBUG                    	LWIP_DBG_ON
//#define TCP_DEBUG							LWIP_DBG_ON
//#define TCPIP_DEBUG						LWIP_DBG_ON
//#define SOCKETS_DEBUG 					LWIP_DBG_ON
//#define TCP_RST_DEBUG                   	LWIP_DBG_ON
//#define TCP_CWND_DEBUG                  	LWIP_DBG_ON
//#define IP_DEBUG                        	LWIP_DBG_ON
//#define UDP_DEBUG							LWIP_DBG_ON
//#define PBUF_DEBUG						LWIP_DBG_ON
//#define TIMERS_DEBUG						LWIP_DBG_ON
//#define MEMP_DEBUG						LWIP_DBG_ON


/*
   ---------------------------------
   ---------- OS options ----------
   ---------------------------------
*/
/**
 * TCPIP_THREAD_NAME: The name assigned to the main tcpip thread.
 */
#define TCPIP_THREAD_NAME 				"LWIPTCP"
/**
 * TCPIP_THREAD_STACKSIZE: The stack size used by the main tcpip thread.
 * The stack size value itself is platform-dependent, but is passed to
 * sys_thread_new() when the thread is created.
 */
#define TCPIP_THREAD_STACKSIZE          1024
/**
 * TCPIP_THREAD_PRIO: The priority assigned to the main tcpip thread.
 * The priority value itself is platform-dependent, but is passed to
 * sys_thread_new() when the thread is created.
 */
#define TCPIP_THREAD_PRIO               4
/**
 * TCPIP_MBOX_SIZE: The mailbox size for the tcpip thread messages
 * The queue size value itself is platform-dependent, but is passed to
 * sys_mbox_new() when tcpip_init is called.
 */
#define TCPIP_MBOX_SIZE                 16
/**
 * DEFAULT_UDP_RECVMBOX_SIZE: The mailbox size for the incoming packets on a
 * NETCONN_UDP. The queue size value itself is platform-dependent, but is passed
 * to sys_mbox_new() when the recvmbox is created.
 */
#define DEFAULT_UDP_RECVMBOX_SIZE       1536
/**
 * DEFAULT_TCP_RECVMBOX_SIZE: The mailbox size for the incoming packets on a
 * NETCONN_TCP. The queue size value itself is platform-dependent, but is passed
 * to sys_mbox_new() when the recvmbox is created.
 */
#define DEFAULT_TCP_RECVMBOX_SIZE       16
/**
 * DEFAULT_ACCEPTMBOX_SIZE: The mailbox size for the incoming connections.
 * The queue size value itself is platform-dependent, but is passed to
 * sys_mbox_new() when the acceptmbox is created.
 */
#define DEFAULT_ACCEPTMBOX_SIZE         16
/**
 * DEFAULT_THREAD_STACKSIZE: The stack size used by any other lwIP thread.
 * The stack size value itself is platform-dependent, but is passed to
 * sys_thread_new() when the thread is created.
 */
#define DEFAULT_THREAD_STACKSIZE        128


#endif /* __LWIPOPTS_H__ */

