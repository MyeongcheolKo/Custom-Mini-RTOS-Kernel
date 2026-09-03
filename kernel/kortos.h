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
	OS_ERR_INVALID_ARGUMENT,
	OS_ERR_INVALID_PRIORITY,
	OS_ERR_MAX_TASKS,
	OS_ERR_NULL_PTR,
	OS_ERR_UNAVAILABLE,
	OS_ERR_INVALID_SCEHDULE_POLICY,
	OS_ERR_WAITLIST_FULL,
	OS_ERR_SEM_INVALID_INIT_COUNT,
	OS_ERR_MTX_RECURSIVE_LOCK,
	OS_ERR_MTX_NOT_OWNER
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
	BLOCKED_MUTEX,
	BLOCKED_QUEUE_SEND,
	BLOCKED_QUEUE_RECV
} task_block_reason_t;

typedef enum {
	FIFO,
	PRIORITY // finds the first task with the highest priority in the wait list to unblock
} schedule_policy_t;

typedef enum {
	mutex_unlocked = 0,
	mutex_locked = 1
} mutex_state_t;

typedef struct {
	TCB_t *task_waitlist[OS_MAX_TASKS];
	uint8_t wait_count; // number of tasks in the wait list
	schedule_policy_t schedule_policy; // the policy used to schedule wait list tasks
} waitlist_t;

typedef struct {
	uint8_t count;
	waitlist_t waitlist;
} semaphore_t;

typedef struct {
	TCB_t *owner; // the task that currently owns the mutex, NULL if none
	mutex_state_t state; // the current state of the mutex(locked or unlocked)
	waitlist_t waitlist;
} mutex_t;

typedef struct {
	uint8_t *buffer; // ring buffer storage, allocated and owned by the application
					// uint8_t rather than void* so item N is accessible by buffer + N * item_size
	uint32_t item_size; // bytes per item, fixed at creation
	uint32_t max_items; // max number of items the buffer can hold, fixed at creation
	uint32_t count; // number of items in the buffer, 
	uint32_t head; // index to write
	uint32_t tail; // index to read
	waitlist_t send_waitlist; // tasks waiting to send to the queue, added to the waitlist when the queue is full
	waitlist_t recv_waitlist; // tasks waiting to receive from the queue, added to the waitlist when the queue is empty
	uint32_t dropped_count; // tracks failed send, doesnt reset, diagnosis tool for isr
} queue_t;

struct TCB_t {
	uint32_t stack_pointer;
	void (*task_handler)(void);
	task_state_t current_state;
	uint8_t base_priority; // lower value = higher priority, user defined and does not change after task creation
	uint8_t effective_priority; // lower value = higher priority, can be changed by mutex priority inheritance
	uint32_t wakeup_tick; // 0 means the task will wait forever
	task_block_reason_t block_reason;
	waitlist_t *blocked_waitlist; // the primitive's waitlist the task is blocked on, NULL if none
	mutex_t *owned_mutexes[OS_MAX_MTX_PER_TASK]; // array of mutexes owned by the task, NULL if none
	uint8_t owned_mutex_count; // number of mutexes owned by the task
	bool timeout; // true if the task was unblocked due to timeout
};

static const int OS_WAIT_FOREVER = -1;

/*---------- public APIs ----------*/

/*
@brief
	Starts the kernel: sets up the scheduler stack, idle task, PendSV priority, and SysTick
	tick, switches to PSP, then dispatches the first task; call once after all tasks and 
	primitive create calls, never returns
*/
void os_kernel_start(void);

/*
@brief
	Registers a task with its own private stack; lower priority value = higher priority

@param task_handler Function to run when the task is scheduled
@param priority Task priority, lower value = higher priority, valid range OS_PRIORITY_HIGHEST..OS_PRIORITY_LOWEST
@param task_stack_base Pointer to the base of the task's private stack
@param task_stack_size Size of task_stack_base in bytes

@retval OS_OK - task successfully created
@retval OS_ERR_MAX_TASKS - OS_MAX_TASKS have already been created
@retval OS_ERR_INVALID_PRIORITY - priority is outside OS_PRIORITY_HIGHEST..OS_PRIORITY_LOWEST
@retval OS_ERR_NULL_PTR - task_stack_base passed in is NULL
*/
os_err_t os_task_create(void (*task_handler)(void), uint8_t priority, uint32_t *task_stack_base, uint32_t task_stack_size);

/*
@brief
	Blocks the current task for tick_count ticks and yields to the next ready task

@param tick_count Number of ticks to block for
*/
void os_task_delay(uint32_t tick_count);

/*
@brief
	Initializes semaphore with an initial count and the schedule policy used to pick which
	blocked task to wake on each os_sem_post() (FIFO = longest waiting, PRIORITY = highest priority)

@param sem Pointer to the semaphore to initialize
@param initial_count The initial count of the semaphore
@param schedule_policy The policy used to schedule wait list tasks

@retval OS_OK - semaphore initialized
@retval OS_ERR_NULL_PTR - sem passed in is NULL
@retval OS_ERR_SEM_INVALID_INIT_COUNT - initial_count is greater than OS_MAX_TASKS
@retval OS_ERR_INVALID_SCEHDULE_POLICY - schedule_policy is neither FIFO nor PRIORITY
*/
os_err_t os_sem_create(semaphore_t *sem, uint8_t initial_count, schedule_policy_t schedule_policy);

/*
@brief
	Waits on semaphore: decrements count and returns immediately if available, otherwise
	blocks the calling task for up to timeout ticks

@param sem Pointer to the semaphore to wait on
@param timeout Number of ticks to block for if sem is unavailable, 0 to not block

@retval OS_OK - semaphore acquired, either immediately or after a post arrived while waiting
@retval OS_ERR_UNAVAILABLE - sem was unavailable and timeout was 0, or timeout ticks elapsed before it was posted
@retval OS_ERR_NULL_PTR - sem passed in is NULL
@retval OS_ERR_FULL - sem's wait list is already full
*/
os_err_t os_sem_wait(semaphore_t *sem, uint32_t timeout);

/*
@brief
	Posts to semaphore: wakes the next blocked task according to sem schedule policy 
	if any are waiting, otherwise increments count

@param sem Pointer to the semaphore to post to

@retval OS_OK - successfully posted
@retval OS_ERR_NULL_PTR - sem passed in is NULL
*/
os_err_t os_sem_post(semaphore_t *sem);

/*
@brief
	Initializes mtx to the unlocked state with no owner, always uses priority scheduling
	for its wait list

@param mtx Pointer to the mutex to initialize

@retval OS_OK - mutex initialized
@retval OS_ERR_NULL_PTR - mtx passed in is NULL
*/
os_err_t os_mutex_create(mutex_t *mtx);

/*
@brief
	Locks mtx and returns immediately if mutex is available, otherwise blocks the calling
	task for up to timeout ticks. Does not support recursive locks. If mtx is held
	by a lower priority task, that owner's effective priority is boosted to the
	caller's to prevent priority inversion. If the owner is itself blocked waiting
	on another mutex, the boost propagates to that mutex's owner as well, and so on
	up the chain until it reaches a task that isn't blocked on a mutex. If the call
	times out instead of acquiring mtx, any boost donated is released the same way. 

@param mtx Pointer to the mutex to lock
@param timeout Number of ticks to block for if mtx is unavailable, 0 to not block

@retval OS_OK - mutex locked, either immediately or after it became available while waiting
@retval OS_ERR_MTX_RECURSIVE_LOCK - calling task already owns mtx
@retval OS_ERR_UNAVAILABLE - mtx was unavailable and timeout was 0, or timeout ticks elapsed before it unlocked
@retval OS_ERR_NULL_PTR - mtx passed in is NULL
@retval OS_ERR_FULL - mtx's wait list is already full
*/
os_err_t os_mutex_lock(mutex_t *mtx, uint32_t timeout);

/*
@brief
	Unlocks mtx. If a task is waiting on mtx, ownership transfers directly to it (mtx
	stays locked) and recomputes the new owner's effective priority as it may no longer 
	be boosted by waiters on mtx. If that owner is itself blocked waiting on
	another mutex, the same recomputation propagates to that mutex's owner as well, and
	so on up the chain.

@param mtx Pointer to the mutex to unlock

@retval OS_OK - unlocked, or ownership handed off to a waiting task
@retval OS_ERR_MTX_NOT_OWNER - calling task does not own mtx
@retval OS_ERR_NULL_PTR - mtx passed in is NULL
*/
os_err_t os_mutex_unlock(mutex_t *mtx);

os_err_t os_queue_create(queue_t *queue, void *buffer, uint32_t item_size, uint32_t max_items, schedule_policy_t schedule_policy);

os_err_t os_queue_send_from_task(queue_t *queue, const void *item, uint32_t timeout);

os_err_t os_queue_send_from_isr(queue_t *queue, const void *item);

os_err_t os_queue_recv_from_task(queue_t *queue, void *item, uint32_t timeout);

os_err_t os_queue_recv_from_isr(queue_t *queue, void *item);

// optional user hook for the idle task, called once per idle loop iteration
__attribute__((weak)) void os_idle_task_hook(void) { /*default is empty, user can override this function*/ }


#endif /* KORTOS_H_ */
