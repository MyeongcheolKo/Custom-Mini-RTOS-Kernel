/*
 * kernel.c
 *
 *  Created on: Dec 23, 2025
 *      Author: krisko
 */
#include <stdint.h>
#include <stdio.h>
#include "kortos.h"
#include "kernel_internal.h"
#include "port.h"

#define IDLE_TASK_IDX 0

static uint32_t scheduler_stack[OS_SCHEDULER_STACK_WORDS] __attribute__((aligned(8)));
static uint32_t idle_task_stack[OS_IDLE_STACK_WORDS] __attribute__((aligned(8)));
static TCB_t user_tasks[OS_MAX_TASKS];
static uint32_t task_count = 1; // idle task always exists
static uint32_t current_task = 0; // start with idle task
static uint32_t systick_count = 0;

/* -------------- os private function prototypes -------------- */
static void init_idle_task(void);
static __attribute__((used)) void update_tick_count(void);
static void unblock_tasks(void);
static void idle_task_handler(void);
static void waitlist_init(waitlist_t *waitlist, schedule_policy_t schedule_policy);
static os_err_t waitlist_block_current(waitlist_t *waitlist, uint32_t timeout, task_block_reason_t block_reason, uint32_t prev_int_state);
static TCB_t *waitlist_unblock(waitlist_t *waitlist);
static void waitlist_remove_task(waitlist_t *waitlist, uint8_t task_idx);

/*-------------- public APIs ---------------*/

/*
starts the kernel: sets up the scheduler stack, idle task, PendSV priority, and SysTick tick, switches to PSP, then dispatches the first task
call once from main after all os_task_create calls; never returns
*/
void os_kernel_start(void)
{
	uint32_t scheduler_stack_top = (uint32_t)(scheduler_stack + sizeof(scheduler_stack) / sizeof(scheduler_stack[0]));
	port_init_scheduler_stack(scheduler_stack_top);
	init_idle_task();
	port_set_pendSV_priority_lowest();
	port_init_systick(OS_TICK_HZ);
	os_schedule_next_task(); // dont call port_yield here bc we are manually starting the first task
	port_switch_to_psp();
	user_tasks[current_task].task_handler(); // never returns
}

/*
registers a task with its own private stack; lower priority value = higher priority, valid range OS_PRIORITY_HIGHEST..OS_PRIORITY_LOWEST
returns OS_OK, or OS_ERR_MAX_TASKS / OS_ERR_INVALID_PRIORITY / OS_ERR_NULL_PTR on failure
*/
os_err_t os_task_create(void (*task_handler)(void), uint8_t priority, uint32_t *task_stack_base, uint32_t task_stack_size)
{
	if (task_count >= OS_MAX_TASKS)
		return OS_ERR_MAX_TASKS;
	if (priority < OS_PRIORITY_HIGHEST || priority > OS_PRIORITY_LOWEST)
		return OS_ERR_INVALID_PRIORITY;
	if (task_stack_base == NULL)
		return OS_ERR_NULL_PTR;

		
	TCB_t *tcb = &user_tasks[task_count];
	tcb->stack_pointer = port_init_task_stack_frame(task_handler, task_stack_base, task_stack_size);
	tcb->task_handler = task_handler;
	tcb->current_state = TASK_READY;
	tcb->base_priority = priority;
	tcb->effective_priority = priority;
	tcb->wakeup_tick = 0; // value dont matter here since task is not blocked
	tcb->block_reason = BLOCKED_NONE;
	tcb->blocked_waitlist = NULL;
	tcb->timeout = false;
	
	task_count++;
	return OS_OK;
}

// blocks the current task for tick_count ticks and yields to the next ready task
void os_task_delay(uint32_t tick_count)
{
	uint32_t prev_int_state = port_enter_critical();

	// only block the task if it not the idle task
	if (current_task != IDLE_TASK_IDX)
	{
		// set wakeup time for the task
		user_tasks[current_task].wakeup_tick = systick_count + tick_count;
		// change to blocked state and specify reason
		user_tasks[current_task].block_reason = BLOCKED_DELAY;
		user_tasks[current_task].current_state = TASK_BLOCKED;
		// pend pendSV exception
		port_yield(); // switches to another task to allow other tasks to run
	}

	// enable interrupt
	port_exit_critical(prev_int_state);
}

os_err_t os_sem_create(semaphore_t *sem, uint8_t initial_count, schedule_policy_t schedule_policy) 
{
	if (sem == NULL) return OS_ERR_NULL_PTR;
	if (initial_count > OS_MAX_TASKS) return OS_ERR_SEM_INVALID_INIT_COUNT;
	if (schedule_policy != FIFO && schedule_policy != PRIORITY) return OS_ERR_INVALID_SCEHDULE_POLICY;
	
	sem->count = initial_count;
	waitlist_init(&sem->waitlist, schedule_policy);
	return OS_OK;
}

os_err_t os_sem_wait(semaphore_t *sem, uint16_t timeout) 
{
	if (sem == NULL) return OS_ERR_NULL_PTR;

	uint32_t prev_int_state = port_enter_critical();

	// check if semaphore is availbe
	if (sem->count > 0)
	{
		// semaphore is available, decrement count and return (the task keeps running)
		sem->count--;
		port_exit_critical(prev_int_state);
		return OS_OK;
	}

	// semaphore is not available, check if the user wants to block the task
	if (timeout == 0)
	{
		// user dont want to block the task to wait for semaphore, return immediately 
		port_exit_critical(prev_int_state);
		return OS_ERR_UNAVAILABLE;
	}

	// block the current task and add it to the semaphore's wait list
	os_err_t block_result = waitlist_block_current(&sem->waitlist, timeout, BLOCKED_SEM, prev_int_state);
	if (block_result != OS_OK)
	{
		return block_result; // return the error that occured while trying to block the task
	}

	// reaches here when the task was blocked successfully and wakes up after being blocked, determine how it was unblocked
	if (user_tasks[current_task].timeout)
	{
		return OS_ERR_UNAVAILABLE; // the task was unblocked due to timeout
	}
	return OS_OK; // the task was unblocked due to semaphore being available
}

os_err_t os_sem_post(semaphore_t *sem) 
{
	if (sem == NULL) return OS_ERR_NULL_PTR;

	uint32_t prev_int_state = port_enter_critical();
	
	// attempt to unblock the next task in the waitlist based on the unblock policy
	if (waitlist_unblock(&sem->waitlist) != NULL) 
	{
		// a task is unblocked from waitlist, yield to allow the unblocked task to run if it has higher priority than the current task
		port_yield();
	}
	else // no tasks to unblock from waitlist, increment the semaphore count
	{
		sem->count++;
	}

	port_exit_critical(prev_int_state);
	return OS_OK;
}

os_err_t os_mutex_create(mutex_t *mtx)
{
	if (mtx == NULL) return OS_ERR_NULL_PTR;

	mtx->owner = NULL; // no task owns the mutex yet
	mtx->state = mutex_unlocked; // mutex is initially unlocked
	waitlist_init(&mtx->waitlist, PRIORITY); // mutexes always use priority scheduling for unblocking tasks
	return OS_OK;
}

os_err_t os_mutex_lock(mutex_t *mtx, uint16_t timeout)
{
	if (mtx == NULL) return OS_ERR_NULL_PTR;

	uint32_t prev_int_state = port_enter_critical();

	// mutex is available, lock it and return
	if (mtx->state == mutex_unlocked)
	{
		mtx->owner = &user_tasks[current_task];
		mtx->state = mutex_locked;
		port_exit_critical(prev_int_state);
		return OS_OK;
	}

	// mutex is not available, check if the user owns the mutex already
	if (mtx->owner == &user_tasks[current_task])
	{
		// the current task already owns the mutex, return error
		port_exit_critical(prev_int_state);
		return OS_ERR_MTX_RECURSIVE_LOCK; // cannot lock a mutex that is already owned by the same task
	}

	// mutex is not available, check if the user wants to block the task
	if (timeout == 0)
	{
		// user dont want to block the task to wait for mutex, return immediately 
		port_exit_critical(prev_int_state);
		return OS_ERR_UNAVAILABLE;
	}

	// mutex is not available and the current task does not own the mutex, block the current task
	os_err_t block_result = waitlist_block_current(&mtx->waitlist, timeout, BLOCKED_MUTEX, prev_int_state);
	if (block_result != OS_OK)
	{
		return block_result; // return the error that occured while trying to block the task
	}

	// reaches here when the task was blocked successfully and wakes up after being blocked, determine how it was unblocked
	if (user_tasks[current_task].timeout)
	{
		return OS_ERR_UNAVAILABLE; // the task was unblocked due to timeout so the mutex was unavailable
	}

	// the task was unblocked due to mutex being available
	return OS_OK;
}

os_err_t os_mutex_unlock(mutex_t *mtx)
{
	if (mtx == NULL) return OS_ERR_NULL_PTR;

	uint32_t prev_int_state = port_enter_critical();

	// check if the current task owns the mutex
	if (mtx->owner != &user_tasks[current_task])
	{
		// the current task does not own the mutex, cannot unlock a mutex that is not owned by the current task
		port_exit_critical(prev_int_state);
		return OS_ERR_MTX_NOT_OWNER;
	}

	// the task owns the mutex, remove it from waitlist
	TCB_t *next_owner = waitlist_unblock(&mtx->waitlist);
	if (next_owner != NULL) 
	{
		// a task is unblocked from waitlist, yield to allow the unblocked task to 
		// run if it has higher priority than the current and other tasks, not need to unlock 
		// the mutex since the unblocked task will now own it
		mtx->owner = next_owner; // transfer ownership to the unblocked task
		port_yield();
	}
	else // no tasks to unblock in waitlist, unlock the mutex
	{
		mtx->owner = NULL;
		mtx->state = mutex_unlocked;
	}

	port_exit_critical(prev_int_state);
	return OS_OK;
}

/*------------- internal kernel interface (core + port use, not application API) --------------*/

// called by SysTick_Handler to update tick count, unblock tasks, and pend PendSV
void os_tick(void)
{
	update_tick_count();

	unblock_tasks();

	port_yield();
}

// returns the saved PSP of the current task from its TCB
uint32_t os_get_sp_value(void)
{
	return user_tasks[current_task].stack_pointer;
}

// saves the current task's PSP into its TCB
void os_save_sp_value(uint32_t current_psp_val)
{
	user_tasks[current_task].stack_pointer = current_psp_val;
}

// schedules the next ready task in priority round-robin order, falls back to idle if all tasks are blocked
void os_schedule_next_task(void)
{
	// task that was running (and didn't block) goes back to TASK_READY
    if (user_tasks[current_task].current_state == TASK_RUNNING) 
		user_tasks[current_task].current_state = TASK_READY;
	
	uint8_t task_to_run = IDLE_TASK_IDX; // default to idle task
	uint8_t highest_priority = OS_PRIORITY_LOWEST + 1; // higher than the lowest priority, so any ready task(even same priority as idle) will be chosen over idle
	if (task_count <= 1) return;
	// finds the next task that is ready to run in round robin order with highest priority (lowest priority number)
	for (int i = current_task + 1; i < task_count; i++) // start after the current task
	{
		if (user_tasks[i].current_state == TASK_READY && user_tasks[i].effective_priority < highest_priority)
		{
			highest_priority = user_tasks[i].effective_priority;
			task_to_run = i;
		}
	}
	for (int i = 1; i <= current_task; i++) // reaches the current task last so it only runs if no other tasks with higher priority are ready, therefor round robin
	{
		if (user_tasks[i].current_state == TASK_READY && user_tasks[i].effective_priority < highest_priority)
		{
			highest_priority = user_tasks[i].effective_priority;
			task_to_run = i;
		}
	}

	// task_to_run = IDLE_TASK_IDX when no tasks are free, idle task was excluded from the comparisona above
	current_task = task_to_run;
	user_tasks[current_task].current_state = TASK_RUNNING;
}

/*------------ core private functions -------------*/

// builds a dummy exception stack frame for idle task
static void init_idle_task(void)
{
	TCB_t *tcb = &user_tasks[IDLE_TASK_IDX];
	tcb->stack_pointer = port_init_task_stack_frame(idle_task_handler, idle_task_stack, sizeof(idle_task_stack));
	tcb->task_handler = idle_task_handler;
	tcb->current_state = TASK_READY;
	tcb->base_priority = OS_PRIORITY_LOWEST + 1; // set to lowest priority(lower than user defined lowest priority), so idle task only runs when no other tasks are ready
	tcb->effective_priority = OS_PRIORITY_LOWEST + 1;
	tcb->wakeup_tick = 0; // value dont matter here since task is not blocked
	tcb->block_reason = BLOCKED_NONE;
	tcb->blocked_waitlist = NULL;
	tcb->timeout = false;
}

// increments the tick counter on every SysTick interrupt
static void update_tick_count(void)
{
	systick_count++;
}

// transitions TASK_BLOCKED tasks to TASK_READY if their wakeup tick has been reached
static void unblock_tasks(void)
{
	if (task_count <= 1) return;
	// unblock any tasks that are qualified for running
	for (int i = 1; i < task_count; i++) // ignores the idle task
	{
		// wake up any task that has a wakeup deadline(task_delay wake tick or semaphore and mutex timeout) 
		// if it is blocked and the wakeup tick has been reached
		if (user_tasks[i].current_state == TASK_BLOCKED && 
			user_tasks[i].wakeup_tick != 0 && // wakeup_tick == 0 means no wakeup tick, wait forever, don't unblock
			(int32_t)(systick_count - user_tasks[i].wakeup_tick) >= 0) // Compare the difference so that it works when systick_count wraps around
		{
			// check if the block was blocked on a primitive
			if (user_tasks[i].blocked_waitlist != NULL)
			{
				user_tasks[i].timeout = true;
				
				// find the task in the waitlist and remove it from the waitlist
				waitlist_t *waitlist = user_tasks[i].blocked_waitlist;
				for (int remove_idx = 0; remove_idx < waitlist->wait_count; remove_idx++)
				{
					if (waitlist->task_waitlist[remove_idx] == &user_tasks[i])
					{
						waitlist_remove_task(waitlist, remove_idx);
						user_tasks[i].blocked_waitlist = NULL;
						break;
					}
				}
			}
			else // task wasblocked by task_delay
			{
				user_tasks[i].timeout = false;
			}

			user_tasks[i].current_state = TASK_READY;
			user_tasks[i].block_reason = BLOCKED_NONE;
			user_tasks[i].wakeup_tick = 0; // reset wakeup tick
		}
	}
}

// initializes the waitlist with the specified scheduling policy
static void waitlist_init(waitlist_t *waitlist, schedule_policy_t schedule_policy)
{
	for (int i = 0; i < OS_MAX_TASKS; i++)
	{
		waitlist->task_waitlist[i] = NULL;
	}
	waitlist->wait_count = 0;
	waitlist->schedule_policy = schedule_policy;
}

// blocks the current task for timeout ticks and add to waitlist, exits critical
// section with prev_int_state and yield to allow other tasks to run
static os_err_t waitlist_block_current(waitlist_t *waitlist, uint32_t timeout, task_block_reason_t block_reason, uint32_t prev_int_state)
{
	// check if the waitlist is full
	if (waitlist->wait_count >= OS_MAX_TASKS)
	{
		port_exit_critical(prev_int_state);
		return OS_ERR_FULL;
	}

	// block the task
	user_tasks[current_task].current_state = TASK_BLOCKED;
	user_tasks[current_task].block_reason = block_reason;
	user_tasks[current_task].blocked_waitlist = waitlist; // save the waitlist pointer in the task's TCB
	user_tasks[current_task].wakeup_tick = (timeout == OS_WAIT_FOREVER) ? 0 : systick_count + timeout;

	// add the task to the wait list
	waitlist->task_waitlist[waitlist->wait_count] = &user_tasks[current_task];
	waitlist->wait_count++; 

	port_exit_critical(prev_int_state);
	
	// schedule for other tasks to run since the current task is blocked
	port_yield();

	// resumes here when wakes up after being blocked, caller should determine how 
	// it was unblocked by checking user_tasks[current_task].timeout
	return OS_OK;
}

// wakes the next task from waitlist based on its schedule policy, clears its blocked
// state, and removes it from the waitlist; returns NULL if waitlist is empty
static TCB_t *waitlist_unblock(waitlist_t *waitlist)
{
	if (waitlist->wait_count == 0)
	{
		return NULL; // no tasks to unblock
	}

	// find the index of the task to unblock in the wait list depending on the unblock method
	int unblock_idx = -1;
	if (waitlist->schedule_policy == FIFO)
	{
		// FIFO: remove the first task in the wait list
		unblock_idx = 0;
	} 
	else if (waitlist->schedule_policy == PRIORITY)
	{
		// PRIORITY: find the first task with the highest priority 
		uint8_t highest_priority = OS_PRIORITY_LOWEST + 1;
		for (int i = 0; i < waitlist->wait_count; i++)
		{
			if (waitlist->task_waitlist[i] != NULL && waitlist->task_waitlist[i]->effective_priority < highest_priority)
			{
				highest_priority = waitlist->task_waitlist[i]->effective_priority;
				unblock_idx = i;
			}
		}
	}
	
	// set the task state to ready and clear its block reason and wakeup tick
	TCB_t *task_to_unblock = waitlist->task_waitlist[unblock_idx];
	task_to_unblock->wakeup_tick = 0;
	task_to_unblock->current_state = TASK_READY;
	task_to_unblock->block_reason = BLOCKED_NONE;
	task_to_unblock->blocked_waitlist = NULL; 
	task_to_unblock->timeout = false; // reset timeout flag, so wont incorrectly return OS_SEM_UNAVAILABLE if the task is blocked again

	// remove the task from the wait list
	waitlist_remove_task(waitlist, unblock_idx);

	return task_to_unblock;
}


// removes the task at task_idx from waitlist, shifting the remaining entries down to fill the gap
static void waitlist_remove_task(waitlist_t *waitlist, uint8_t task_idx)
{
	// remove the task from the wait list (not strictly necessary to set to NULL since we will shift the remaining tasks down, but included to show intent)
	waitlist->task_waitlist[task_idx] = NULL;

	// shift the remaining tasks in the wait list to fill the gap
	for (int i = task_idx; i < waitlist->wait_count - 1; i++)
	{
		waitlist->task_waitlist[i] = waitlist->task_waitlist[i + 1];
	}

	// set the last task in the wait list to NULL
	waitlist->task_waitlist[waitlist->wait_count - 1] = NULL;

	// decrement wait_task count
	waitlist->wait_count--; 
}

// the internal used idle task handler called by the scheduler, users should override os_idle_task_hook() instead of this
static void idle_task_handler(void)
{
	while(1)
	{
		os_idle_task_hook(); // user optional work
		PORT_WAIT_FOR_INTERRUPT(); // sleep core until next interrupt (SysTick wakes it)
	}
}
