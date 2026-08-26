/*
 * kortos.h
 *
 *  Created on: Dec 23, 2025
 *      Author: krisko
 */

#ifndef KORTOS_H_
#define KORTOS_H_

#include <stdint.h>
#include <stdbool.h>
#include "kortos_config.h"

typedef struct TCB_t TCB_t; // forward declaration of TCB_t so it can be used in semaphore_t

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
} schedule_policy_t;

typedef struct {
	TCB_t *task_waitlist[OS_MAX_TASKS];
	uint8_t wait_count; // number of tasks in the wait list
	schedule_policy_t schedule_policy; // the policy used to schedule wait list tasks
} waitlist_t;

typedef struct {
	uint8_t count;
	waitlist_t waitlist;
} semaphore_t;

struct TCB_t {
	uint32_t stack_pointer;
	void (*task_handler)(void);
	uint8_t base_priority; // lower value = higher priority, user defined and does not change after task creation
	uint8_t effective_priority; // lower value = higher priority, can be changed by mutex priority inheritance
	uint32_t wakeup_tick; // 0 means the task will wait forever
	task_state_t current_state;
	task_block_reason_t block_reason;
	waitlist_t *blocked_waitlist; // the primitive the task is waiting on, NULL if none
	bool timeout; // true if the task was unblocked due to timeout
};

static const int OS_WAIT_FOREVER = -1;

/*---------- public APIs ----------*/

// sets scheduler up and starts the kernel, does not return
void os_kernel_start(void);
// registers a task with its own private stack; lower priority value = higher priority, valid range OS_PRIORITY_HIGHEST..OS_PRIORITY_LOWEST
// returns OS_OK, or OS_ERR_MAX_TASKS / OS_ERR_INVALID_PRIORITY / OS_ERR_NULL_PTR on failure
os_err_t os_task_create(void (*task_handler)(void), uint8_t priority, uint32_t *task_stack_base, uint32_t task_stack_size);

// blocks the calling task for tick_count SysTick ticks and yields to the next ready task (no-op when called from the idle task)
void os_task_delay(uint32_t tick_count);

// initializes the semaphore passed in with initial_count
os_err_t os_sem_create(semaphore_t *sem, uint8_t initial_count, schedule_policy_t schedule_policy);

// returns OS_OK when the semaphore is available
os_err_t os_sem_wait(semaphore_t *sem, uint16_t timeout);

os_err_t os_sem_post(semaphore_t *sem);

// optional user hook for the idle task, called once per idle loop iteration
__attribute__((weak)) void os_idle_task_hook(void) { /*default is empty, user can override this function*/ }


#endif /* KORTOS_H_ */
