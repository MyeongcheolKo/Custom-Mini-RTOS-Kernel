/*
 * scheduler.h
 *
 *  Created on: Dec 23, 2025
 *      Author: krisko
 */

#ifndef SCHEDULER_H_
#define SCHEDULER_H_

#include <stdio.h>

typedef struct
{
	uint32_t psp_val;
	uint32_t block_count;
	uint8_t current_state;
	void (*task_handler)(void);
}TCD_t;

extern TCD_t user_tasks[MAX_TASKS];
extern uint32_t current_task;

void init_systick_timer(uint32_t tick_hz);
__attribute__ ((naked)) void init_scheduler_stack(uint32_t schedu_top_of_stack);
void init_task_stack(void);

void save_psp_value(uint32_t current_psp_val);
uint32_t get_psp_value(void);
void switch_sp_to_psp(void);
void update_next_task(void);
void update_global_tick_count(void);

void enable_processor_faults(void);

void task_delay(uint32_t tick_count);
void schedule(void);
void unblock_tasks(void);


#endif /* SCHEDULER_H_ */
