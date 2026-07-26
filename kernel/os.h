/*
 * os.h
 *
 *  Created on: Dec 23, 2025
 *      Author: krisko
 */

#ifndef OS_H_
#define OS_H_

#include <stdint.h>
#include <stdbool.h>
#include "osConfig.h"

typedef struct TCB_t TCB_t; // forward declaration of TCB_t so it can be used in semephore_t

typedef enum {
	OS_OK = 0,
	OS_ERR_INVALID_PRIORITY,
	OS_ERR_MAX_TASKS,
	OS_ERR_NULL_PTR,
	OS_ERR_FULL,
	OS_SEM_UNAVAILABLE,
	OS_ERR_INVALID_SEM_UNBLOCK_METHOD,
	OS_ERR_INVALID_SEM_INIT_COUNT
} os_err_t;

typedef enum {
	TASK_READY,
	TASK_RUNNING,
	TASK_BLOCKED
} task_state_t;

typedef enum {
	BLOCKED_NONE = 0, // task is not blocked
	BLOCKED_DELAY,
	BLOCKED_SEM,
	BLOCKED_MUTEX
} task_block_reason_t;

typedef enum {
	FIFO,
	PRIORITY // finds the first task with the highest priority in the wait list to unblock
} unblock_method_t;

typedef struct {
	uint8_t count;
	TCB_t *task_wait_list[OS_MAX_TASKS];
	uint8_t wait_count; // number of tasks in the wait list
	unblock_method_t unblock_method; // the way wait list tasks are scheduled when the semahpore is availabe
} semephore_t; 

struct TCB_t {
	uint32_t stack_pointer;
	void (*task_handler)(void);
	uint8_t priority_level; // lower value = higher priority, 0 is reserved for idle task
	uint32_t wakeup_tick; // 0 means the task will wait forever
	task_state_t current_state;
	task_block_reason_t block_reason;
	semephore_t *blocked_sem; // which sem this task is waiting on, NULL if none
	bool timeout; // true if the task was unblocked due to timeout
};

static const int OS_WAIT_FOREVER = -1;

/*---------- public APIs ----------*/

// sets scheduler up and starts the kernel, does not return
void os_kernel_start(void);

// registers a task with its own private stack; lower priority value = higher priority, valid range 1..OS_PRIORITY_LOWEST (0 is reserved for the idle task)
// returns OS_OK, or OS_ERR_MAX_TASKS / OS_ERR_INVALID_PRIORITY / OS_ERR_NULL_PTR on failure
os_err_t os_task_create(void (*task_handler)(void), uint8_t priority, uint32_t *task_stack_base, uint32_t task_stack_size);

// blocks the calling task for tick_count SysTick ticks and yields to the next ready task (no-op when called from the idle task)
void os_task_delay(uint32_t tick_count);

// initializes the semaphore passed in with initial_count
os_err_t os_sem_create(semephore_t *sem, uint8_t initial_count, unblock_method_t unblock_method);

// returns OS_OK when the semaphore is available
os_err_t os_sem_wait(semephore_t *sem, uint16_t timeout);

os_err_t os_sem_post(semephore_t *sem);

// optional user hook for the idle task, called once per idle loop iteration
__attribute__((weak)) void os_idle_task_hook(void) { /*default is empty, user can override this function*/ }


#endif /* OS_H_ */
