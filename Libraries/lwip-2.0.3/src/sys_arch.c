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
 *******************************************************************************
 * @brief
 * 	Updated to support FreeRTOS 10.0.0 and lwip 2.0.3
 *
 * @author  JW, Neutron Controls
 * @version V1.2.0
 * @date    Apr2018
 * @brief   lwIP Options Configuration,  Version 2.0.3
 *
 ******************************************************************************
 */


/* C includes */
#include <stdint.h>
/* IFX includes */
#include "Cpu/Std/Platform_Types.h"
/* FreeRTOS includes */
#include "FreeRTOS.h"
#include "task.h"
/* lwIP includes. */
#include "lwip/debug.h"
#include "lwip/def.h"
#include "lwip/sys.h"
#include "lwip/mem.h"
#include "lwip/stats.h"
#include "lwipopts.h"
/* Application includes */


xTaskHandle xTaskGetCurrentTaskHandle( void ) PRIVILEGED_FUNCTION;

struct timeoutlist
{
	struct sys_timeo *timeouts;
	xTaskHandle pid;
};

/* This is the number of threads that can be started with sys_thread_new() */
#define kuLWIP_SYS_ARCH_THREAD_MAX	4u
#define kuLWIP_SYS_MAX_POST_DELAY 	portMAX_DELAY

#define SYS_ARCH_SHELL	0
#if (__RL_SHELL == 1) && (SYS_ARCH_SHELL == 1)
#define SYS_ARCH_SHELL_DBG(...) PRINTF("%sLWIP: ", shell_GetCpu()); PRINTF(__VA_ARGS__)
#else
#define SYS_ARCH_SHELL_DBG(...)
#endif

__far struct timeoutlist s_timeoutlist[kuLWIP_SYS_ARCH_THREAD_MAX];
__far uint32_t u32LwipThreads = 0u;

__far StaticTask_t __align(4) xTcpIpTaskTBC;
__far StackType_t __align(4) xTcpIpTaskStack[TCPIP_THREAD_STACKSIZE];

/* We are defining our on lwip mempool and placing it in DSPR0 */
__far uint8_t __align(4) lwipHeap[MEM_SIZE];


/*-----------------------------------------------------------------------------------*/
/*
 * Creates an empty mailbox for maximum "size" elements. Elements stored
 * in mailboxes are pointers. You have to define macros "_MBOX_SIZE"
 * in your lwipopts.h, or ignore this parameter in your implementation
 * and use a default size.
 * If the mailbox has been created, ERR_OK should be returned. Returning any
 * other error will provide a hint what went wrong, but except for assertions,
 * no real error handling is implemented.
 */
err_t sys_mbox_new(sys_mbox_t *mbox, int size)
{
	err_t ret = ERR_MEM;


	SYS_ARCH_SHELL_DBG("LWIP:  [SYS] New MAILBOX[0x%.8X]-->[0x%.8X] :: ", mbox, *mbox);

	if(*mbox == NULL)
	{
		*mbox = xQueueCreate(((UBaseType_t)size), sizeof(void*));

		if(*mbox != NULL)
		{
#if (LWIP_STATS == 1)
		++lwip_stats.sys.mbox.used;
#endif /* LWIP_STATS */
			ret = ERR_OK;
		}
	}

	SYS_ARCH_SHELL_DBG(" MAILBOX[0x%.8X] ELEMENTS[0x%.2X] ELEMENT SIZE[0x%.2X]\r\n", *mbox, size, sizeof(void*));

	return(ret);
}


/*-----------------------------------------------------------------------------------*/
/*
 * Deallocates a mailbox. If there are messages still present in the
 * mailbox when the mailbox is deallocated, it is an indication of a
 * programming error in lwIP and the developer should be notified.
 */
void sys_mbox_free(sys_mbox_t *mbox)
{
	if(*mbox != NULL)
	{
		if(uxQueueMessagesWaiting(*mbox) != 0)
		{
			SYS_ARCH_SHELL_DBG("LWIP:  [SYS] Free MAILBOX[0x%.8X]-->[0x%.8X] FAILED!\r\n", mbox, *mbox);
#if (LWIP_STATS == 1)
			lwip_stats.sys.mbox.err++;
#endif /* SYS_STATS */
		}
		vQueueDelete(*mbox);

		SYS_ARCH_SHELL_DBG("LWIP:  [SYS] Free MAILBOX[0x%.8X]-->[0x%.8X] OK!\r\n", mbox, *mbox);

#if LWIP_STATS
			--lwip_stats.sys.mbox.used;
#endif /* LWIP_STATS */
	}
}


/*-----------------------------------------------------------------------------------*/
/*
 * Posts the "msg" to the mailbox. This function has to block until
 * the "msg" is really posted.
 */
void sys_mbox_post(sys_mbox_t* mbox, void* msg)
{
	while(xQueueSend(*mbox, ((const void*)msg), kuLWIP_SYS_MAX_POST_DELAY) != pdPASS){}

	SYS_ARCH_SHELL_DBG("LWIP:  [SYS] Post MAIL[0x%.8X]-->[0x%.8X] MSG[0x%.8X]\r\n", mbox, *mbox, msg);
}


/*-----------------------------------------------------------------------------------*/
/*
 * Try to post the "msg" to the mailbox. Returns ERR_MEM if this one
 * is full, else, ERR_OK if the "msg" is posted.
 */
err_t sys_mbox_trypost(sys_mbox_t* mbox, void* msg)
{
	err_t rtn = ERR_OK;

	/* xTicksToWait = immediately */
   if(xQueueSendToBack(*mbox, &msg, 0) != pdPASS)
   {
#if (LWIP_STATS == 1)
	    lwip_stats.sys.mbox.err++;
#endif /* SYS_STATS */

	   rtn = ERR_MEM;
   }

   SYS_ARCH_SHELL_DBG("LWIP:  [SYS] TryPost STAT[%d] MAIL[0x%.8X]-->[0x%.8X] MSG[0x%.8X]\r\n", rtn, mbox, *mbox, msg);

   return(rtn);
}


/*-----------------------------------------------------------------------------------*/
/*
 * Blocks the thread until a message arrives in the mailbox, but does
 * not block the thread longer than "timeout" milliseconds (similar to
 * the sys_arch_sem_wait() function). If "timeout" is 0, the thread should
 * be blocked until a message arrives. The "msg" argument is a result
 * parameter that is set by the function (i.e., by doing "*msg =
 * ptr"). The "msg" parameter maybe NULL to indicate that the message
 * should be dropped.
 *
 * The return values are the same as for the sys_arch_sem_wait() function:
 * Number of milliseconds spent waiting or SYS_ARCH_TIMEOUT if there was a
 * timeout.
 *
 * Note that a function with a similar name, sys_mbox_fetch(), is
 * implemented by lwIP.
 */
u32_t sys_arch_mbox_fetch(sys_mbox_t* mbox, void** msg, u32_t timeout)
{
	void* dummyptr;
	portTickType StartTime;
	portTickType EndTime;
	portTickType Elapsed = 0;


	StartTime = xTaskGetTickCount();

	if(*msg == NULL)
	{
		*msg = &dummyptr;
	}

	if(timeout != 0)
	{
		if(pdTRUE == xQueueReceive(*mbox, &(*msg), timeout))
		{
			EndTime = xTaskGetTickCount();
			Elapsed = (EndTime - StartTime);
		}
		else // timed out blocking for message
		{
			*msg = NULL;
			Elapsed = SYS_ARCH_TIMEOUT;
		}
	}
	else // block forever for a message.
	{
		while(pdTRUE != xQueueReceive(*mbox, &(*msg), portMAX_DELAY)){}
		EndTime = xTaskGetTickCount();
		Elapsed = (EndTime - StartTime) * portTICK_RATE_MS;
	}

	if(*msg != NULL)
	{
		SYS_ARCH_SHELL_DBG("LWIP:  [SYS] TryFetch MAIL[0x%.8X]-->[0x%.8X] MSG[0x%.8X] TO[%.d] TE[0x%.8X]\r\n", mbox, *mbox, *msg, timeout, Elapsed);
	}

	return(Elapsed);
}


/*-----------------------------------------------------------------------------------*/
/*
 * This is similar to sys_arch_mbox_fetch, however if a message is not
 * present in the mailbox, it immediately returns with the code
 * SYS_MBOX_EMPTY. On success 0 is returned.
 *
 * To allow for efficient implementations, this can be defined as a
 * function-like macro in sys_arch.h instead of a normal function. For
 * example, a naive implementation could be:
 *   #define sys_arch_mbox_tryfetch(mbox,msg) \
 *     sys_arch_mbox_fetch(mbox,msg,1)
 * although this would introduce unnecessary delays.
 */
u32_t sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg)
{
	void* dummyptr;


	if(*msg == NULL)
	{
		*msg = &dummyptr;
	}

	if(pdTRUE == xQueueReceive(*mbox, &(*msg), 0))
	{
		return(ERR_OK);
	}
	else
	{
		return(SYS_MBOX_EMPTY);
	}
}


/*-----------------------------------------------------------------------------------*/
/*
 * Returns 1 if the mailbox is valid, 0 if it is not valid.
 * When using pointers, a simple way is to check the pointer for != NULL.
 * When directly using OS structures, implementing this may be more complex.
 * This may also be a define, in which case the function is not prototyped.
 */
int sys_mbox_valid(sys_mbox_t *mbox)
{
	int iRtn = 0;

#if defined(_RL__SHELL__) && defined(SYS_DEBUG_PRINT)
	SYS_ARCH_SHELL_DBG("LWIP:  [SYS] Set Valid MAIL[0x%.8X]-->[0x%.8X]", mbox, *mbox);
#endif /* (_RL_SHELL__) && (SYS_DEBUG_PRINT) */
	if(*mbox != NULL)
	{
		iRtn = 1;
	}

	SYS_ARCH_SHELL_DBG(" STAT[%d]\r\n", iRtn);
	return(iRtn);
}


/*-----------------------------------------------------------------------------------*/
/*
 * Invalidate a mailbox so that sys_mbox_valid() returns 0.
 * ATTENTION: This does NOT mean that the mailbox shall be deallocated:
 * sys_mbox_free() is always called before calling this function!
 * This may also be a define, in which case the function is not prototyped.
 *
 * If threads are supported by the underlying operating system and if
 * such functionality is needed in lwIP, the following function will have
 * to be implemented as well:
 */
void sys_mbox_set_invalid(sys_mbox_t *mbox)
{
	SYS_ARCH_SHELL_DBG("LWIP:  [SYS] Set Invalid MAIL[0x%.8X]-->[0x%.8X]\r\n", mbox, *mbox);

	if(*mbox != NULL)
	{
		*mbox = NULL;
	}
}


/*-----------------------------------------------------------------------------------*/
/*
 * Creates a new semaphore. The semaphore is allocated to the memory that 'sem'
 * points to (which can be both a pointer or the actual OS structure).
 * The "count" argument specifies the initial state of the semaphore (which is
 * either 0 or 1).
 * If the semaphore has been created, ERR_OK should be returned. Returning any
 * other error will provide a hint what went wrong, but except for assertions,
 * no real error handling is implemented.
 */
err_t sys_sem_new(sys_sem_t* sem, u8_t count)
{
	err_t rtn = ERR_OK;


	vSemaphoreCreateBinary(*sem);

	if(*sem == NULL)
	{
#if (LWIP_STATS == 1)
		++lwip_stats.sys.sem.err;
#endif /* SYS_STATS */
	   rtn = ERR_BUF;
	}
	else
	{
		if(count == 0)	// Means it can't be taken
		{
			xSemaphoreTake(*sem, 0);
		}

#if (LWIP_STATS == 1)
		++lwip_stats.sys.sem.used;
		if(lwip_stats.sys.sem.max < lwip_stats.sys.sem.used)
		{
			lwip_stats.sys.sem.max = lwip_stats.sys.sem.used;
		}
#endif /* SYS_STATS */
	}

#if defined(_RL__SHELL__) && defined(SYS_DEBUG_PRINT)
//	   PRINT("LWIP:  [SYS] New SEM[0x%.8X] COUNT[0x%.8X] status[%d]\r\n", *sem, count, rtn);
#endif /* (_RL_SHELL__) && (SYS_DEBUG_PRINT) */

	return(rtn);
}


/*-----------------------------------------------------------------------------------*/
/*
 * Blocks the thread while waiting for the semaphore to be
 * signaled. If the "timeout" argument is non-zero, the thread should
 * only be blocked for the specified time (measured in
 * milliseconds). If the "timeout" argument is zero, the thread should be
 * blocked until the semaphore is signalled.
 *
 * If the timeout argument is non-zero, the return value is the number of
 * milliseconds spent waiting for the semaphore to be signaled. If the
 * semaphore wasn't signaled within the specified time, the return value is
 * SYS_ARCH_TIMEOUT. If the thread didn't have to wait for the semaphore
 * (i.e., it was already signaled), the function may return zero.
 *
 * Notice that lwIP implements a function with a similar name,
 * sys_sem_wait(), that uses the sys_arch_sem_wait() function.
 */
u32_t sys_arch_sem_wait(sys_sem_t* sem, u32_t timeout)
{
	portTickType StartTime;
	portTickType EndTime;
	portTickType Elapsed = 0;


	StartTime = xTaskGetTickCount();
	if(timeout != 0)
	{
		if(xSemaphoreTake(*sem, (timeout/portTICK_RATE_MS)) == pdTRUE)
		{
			EndTime = xTaskGetTickCount();
			Elapsed = (EndTime - StartTime) * portTICK_RATE_MS;
		}
		else
		{
			Elapsed = SYS_ARCH_TIMEOUT;
		}
	}
	else // must block without a timeout
	{
		while(xSemaphoreTake(*sem, portMAX_DELAY) != pdTRUE ){}
		EndTime = xTaskGetTickCount();
		Elapsed = (EndTime - StartTime) * portTICK_RATE_MS;
	}
#if defined(_RL__SHELL__) && defined(SYS_DEBUG_PRINT)
//	   PRINT("LWIP:  [SYS] arch SEMWait[0x%.8X] timeout[%d] Elapsed[0x%.8X]\r\n", *sem,timeout,Elapsed);
#endif /* (_RL_SHELL__) && (SYS_DEBUG_PRINT) */
	return(Elapsed);
}


/*-----------------------------------------------------------------------------------*/
/*
 * Signals a semaphore
 */
void sys_sem_signal(sys_sem_t* sem)
{
	xSemaphoreGive(*sem);
}


/*-----------------------------------------------------------------------------------*/
/*
 * Deallocates a semaphore
 */
void sys_sem_free(sys_sem_t* sem)
{
	vQueueDelete(*sem);

#if (LWIP_STATS == 1)
      --lwip_stats.sys.sem.used;
#endif /* SYS_STATS */
#if defined(_RL__SHELL__) && defined(SYS_DEBUG_PRINT)
//	   PRINT("LWIP:  [SYS] Free SEM[0x%.8X]\r\n", *sem);
#endif /* (_RL_SHELL__) && (SYS_DEBUG_PRINT) */
}


/*-----------------------------------------------------------------------------------*/
/*
 * Returns 1 if the semaphore is valid, 0 if it is not valid.
 * When using pointers, a simple way is to check the pointer for != NULL.
 * When directly using OS structures, implementing this may be more complex.
 * This may also be a define, in which case the function is not prototyped.
 */
int sys_sem_valid(sys_sem_t* sem)
{
	int iRtn = 0;


	if(*sem != NULL)
	{
		iRtn = 1;
	}

	return(iRtn);
}


/*-----------------------------------------------------------------------------------*/
/*
 * Invalidate a semaphore so that sys_sem_valid() returns 0.
 * ATTENTION: This does NOT mean that the semaphore shall be deallocated:
 * sys_sem_free() is always called before calling this function!
 * This may also be a define, in which case the function is not prototyped.
 */
void sys_sem_set_invalid(sys_sem_t* sem)
{
	*sem = NULL;
}


/*-----------------------------------------------------------------------------------*/
err_t sys_mutex_new(sys_mutex_t* mutex)
{
	err_t rtn = ERR_OK;


	*mutex = xSemaphoreCreateMutex();
	if(*mutex == NULL)
	{
#if (LWIP_STATS == 1)
      --lwip_stats.sys.mutex.err;
#endif /* SYS_STATS */
		rtn = ERR_MEM;
	}

#if (LWIP_STATS == 1)
      --lwip_stats.sys.mutex.used;
#endif /* SYS_STATS */
    SYS_ARCH_SHELL_DBG("LWIP:  [SYS] New MUTEX[0x%.8X]\r\n", *mutex);

	return(rtn);
}


/*-----------------------------------------------------------------------------------*/
/*
 * Blocks the thread until the mutex can be grabbed.
 */
void sys_mutex_lock(sys_mutex_t *mutex)
{
	xSemaphoreTake(*mutex, 1);
}


/*-----------------------------------------------------------------------------------*/
/*
 * Releases the mutex previously locked through 'sys_mutex_lock()'.
 */
void sys_mutex_unlock(sys_mutex_t *mutex)
{
	xSemaphoreGive(*mutex);
}


/*-----------------------------------------------------------------------------------*/
/*
 * Is called to initialize the sys_arch layer.
 */
void sys_init(void)
{
	// Initialize the the per-thread sys_timeouts structures
	// make sure there are no valid PIDs in the list
	for(uint32_t u32Index = 0u; u32Index < kuLWIP_SYS_ARCH_THREAD_MAX; u32Index++)
	{
		s_timeoutlist[u32Index].pid = 0;
		s_timeoutlist[u32Index].timeouts = NULL;
	}

	// keep track of how many threads have been created
	u32LwipThreads = 0;
}

/*-----------------------------------------------------------------------------------*/
/*
 * Returns a pointer to the per-thread sys_timeouts structure. In lwIP,
 * each thread has a list of timeouts which is represented as a linked
 * list of sys_timeout structures. The sys_timeouts structure holds a
 * pointer to a linked list of timeouts. This function is called by
 * the lwIP timeout scheduler and must not return a NULL value.
 *
 * In a single threaded sys_arch implementation, this function will
 * simply return a pointer to a global sys_timeouts variable stored in
 * the sys_arch module.
 */
struct sys_timeo* sys_arch_timeouts(void)
{
	xTaskHandle pid;
	struct timeoutlist *tl;


	pid =  xTaskGetCurrentTaskHandle();

	for(uint32_t u32Index = 0u; u32Index < kuLWIP_SYS_ARCH_THREAD_MAX; u32Index++)
	{
		tl = &(s_timeoutlist[u32Index]);
		if(tl->pid == pid)
		{
			return(tl->timeouts);
		}
	}

	// Error
	return NULL;
}


/*-----------------------------------------------------------------------------------*/
/* Starts a new thread named "name" with priority "prio" that will begin its
 * execution in the function "thread()". The "arg" argument will be passed as an
 * argument to the thread() function. The stack size to used for this thread is
 * the "stacksize" parameter. The id of the new thread is returned. Both the id
 * and the priority are system dependent.
 *
 * When lwIP is used from more than one context (e.g. from multiple threads OR from
 * main-loop and from interrupts), the SYS_LIGHTWEIGHT_PROT protection SHOULD be enabled!
 */
#if 0
sys_thread_t sys_thread_new( const char* name, lwip_thread_fn thread, void* arg, int stacksize, int prio )
{
	xTaskHandle createdTask = NULL;


	if(u32LwipThreads < kuLWIP_SYS_ARCH_THREAD_MAX)
	{
		/* Let's look at the TASK name to ensure TASK is supported statically */
#if (configSUPPORT_STATIC_ALLOCATION == 1)
		if(!shellhelper_ParseMatchOnly(TCPIP_THREAD_NAME, ((char*)name)))
		{/* Called form ./lwip-2.0.2/src/api/tcpip.c */
			createdTask = ostask_Create(((TaskFunction_t)thread), ((const char* const)name), ((uint32_t)stacksize), arg, ((UBaseType_t)prio), xTcpIpTaskStack, &xTcpIpTaskTBC);
		}
#if (LWIP_SOCKET == 1)
		else if(!shellhelper_ParseMatchOnly(kBSD_TASK_NAME, ((char*)name)))
		{/* Called form ./bsdsocket/src/bsdsocket.c */
			createdTask = ostask_Create(((TaskFunction_t)thread), ((const char* const)name), ((uint32_t)stacksize), arg, ((UBaseType_t)prio), xBsdTaskStack, &xBsdTaskTBC);
		}
#endif /* LWIP_SOCKET */
#else	/* Dynamic memory allocation */
		xTaskCreate(thread, ((const char* const)name), ((unsigned short)stacksize), arg, ((UBaseType_t)prio), &createdTask);
#endif /* configSUPPORT_STATIC_ALLOCATION */

		/* Was the TASK created */
		if(createdTask != NULL)
		{/* YES */
			for(uint32_t u32Index = 0u; u32Index < kuLWIP_SYS_ARCH_THREAD_MAX; u32Index++)
			{
			   if(s_timeoutlist[u32Index].pid == 0)
			   {
				   s_timeoutlist[u32Index].pid = createdTask;
				   SYS_ARCH_SHELL_DBG("LWIP:  [SYS] New TASK PID[0x%.8X] added TIMER[0x%.2X] \r\n", createdTask, u32Index);
				   break;
			   }
			}

			u32LwipThreads++;
		}
	}
	return(createdTask);
}
#else
sys_thread_t sys_thread_new( const char* name, lwip_thread_fn thread, void* arg, int stacksize, int prio )
{
	xTaskHandle xCreatedTask;
	uint8 xResult;
	sys_thread_t xReturn;

	xResult = xTaskCreate( thread, ( signed char * ) name, stacksize, arg, prio, &xCreatedTask );

	if( xResult == pdPASS )
	{
		xReturn = xCreatedTask;
	}
	else
	{
		xReturn = NULL;
	}

	return xReturn;
}

#endif

void sys_delete_thread_timer(xTaskHandle pid)
{
	struct timeoutlist *tl;


	for(uint32_t u32Index = 0u; u32Index < kuLWIP_SYS_ARCH_THREAD_MAX; u32Index++)
	{
		tl = &(s_timeoutlist[u32Index]);
		if(tl->pid == pid)
		{
			SYS_ARCH_SHELL_DBG("LWIP:  [SYS] REMOVED TIMER[0x%.2X] TID[0x%.8X]\r\n", u32Index, pid);
			/* We need to remove all timers form the list */
			s_timeoutlist[u32Index].pid = NULL;

			break;
		}
	}

	if(u32LwipThreads != 0)
	{/* remove on form the list */
		u32LwipThreads--;
	}
}


/*-----------------------------------------------------------------------------------*/
/*
 * This optional function does a "fast" critical region protection and returns
 * the previous protection level. This function is only called during very short
 * critical regions. An embedded system which supports ISR-based drivers might
 * want to implement this function by disabling interrupts. Task-based systems
 * might want to implement this by using a mutex or disabling tasking. This
 * function should support recursive calls from the same task or interrupt. In
 * other words, sys_arch_protect() could be called while already protected. In
 * that case the return value indicates that it is already protected.
 *
 * sys_arch_protect() is only required if your port is supporting an operating
 * system.
 */
sys_prot_t sys_arch_protect(void)
{
	portENTER_CRITICAL();
	return 1;
}


/*-----------------------------------------------------------------------------------*/
/*
 * This optional function does a "fast" set of critical region protection to the
 * value specified by pval. See the documentation for sys_arch_protect() for
 * more information. This function is only required if your port is supporting
 * an operating system.
 */
void sys_arch_unprotect(sys_prot_t pval)
{
	( void ) pval;
	portEXIT_CRITICAL();
}


/*-----------------------------------------------------------------------------------*/
/*
 * This optional function returns the current time in milliseconds (don't care
 * for wraparound, this is only used for time diffs).
 * Not implementing this function means you cannot use some modules (e.g. TCP
 * timestamps, internal timeouts for NO_SYS==1).
 */
u32_t sys_now(void)
{
	portTickType rtn = xTaskGetTickCount();
	return(rtn);
}

