/*
 * scheduler.c
 *
 *  Created on: Dec 23, 2025
 *      Author: krisko
 */
#include <stdint.h>
#include <stdio.h>
#include "main.h"
#include "scheduler.h"
#include "tasks.h"

uint32_t current_task = 1; //start with TASK1
TCD_t user_tasks[MAX_TASKS];
uint32_t g_tick_count = 0;

void save_psp_value(uint32_t current_psp_val)
{
	user_tasks[current_task].psp_val = current_psp_val;
}

uint32_t get_psp_value(void)
{
	return user_tasks[current_task].psp_val;
}

__attribute__((naked)) void switch_sp_to_psp(void) // need naked bc in c fcn prologue LR is pushed onto stack at MSP
{													// in epilogue, it POP whatever on the stack at PSP to PC

	/* CUATION! here the BL corrupts the LR value as it stores the address of this fcn to get back from get_psp_value
	 * so we need to save it first
	 */
	__asm volatile("PUSH {LR}");
	//get value of PSP of the current task
	__asm volatile("BL get_psp_value"); // calls get_PSP_value and returns with R0 containing the value
	//initialize PSP
	__asm volatile("MSR PSP,R0");
	//POP the original value of LR from stack back to LR
	__asm volatile("POP {LR}");

	//change SP to PSP
	__asm volatile("MOV R0,#0x02"); //store the value to config into R0 first, R0 is spare now bc the PSP value is already loaded to PSP
	__asm volatile("MSR CONTROL,R0 ");
	__asm volatile("BX LR"); // return
}


void init_systick_timer(uint32_t tick_hz)
{
	uint32_t count_val = (SYSTIC_TIMER_CLOCK / tick_hz) - 1;
	uint32_t *p_SRVR = (uint32_t*)0xE000E014;

	//clear the value of SRVR
	*p_SRVR &= ~(0x00FFFFFF);

	//load the value into SRVR
	*p_SRVR |= count_val;

	//do some settings
	uint32_t *p_SCSR = (uint32_t*)0xE000E010;
	*p_SCSR |= (1 << 1); // enable the systick exception request
	*p_SCSR |= (1 << 2); // use processor clock

	*p_SCSR |= 1; // enable the counter


}

__attribute__ ((naked)) void init_scheduler_stack(uint32_t schedu_top_of_stack)
{
	//change msp (handler sp) to the correct address of the scheduler stack
	__asm volatile("MSR MSP,%0" : : "r"(schedu_top_of_stack));
	__asm volatile("BX LR"); // get back to caller
}

void init_task_stack(void)
{
	//when it is the first time the tasks are run, there were no past status/context
	//so can't really retrieve the contaxt as there are nothing on the stack
	//to solve this, create some dummy variables on the stack:
	// general registers -> all set to 0
	// xPSR -> only need t bit to be 1
	// PC -> the corresponding task handler
	// LR -> EXC_RETURN, should be 0xFFFFFFFD as we need return to thread with PSP
	user_tasks[0].current_state = TASK_READY_STATE;
	user_tasks[1].current_state = TASK_READY_STATE;
	user_tasks[2].current_state = TASK_READY_STATE;
	user_tasks[3].current_state = TASK_READY_STATE;
	user_tasks[4].current_state = TASK_READY_STATE;


	user_tasks[0].psp_val = IDLE_STACK_START;
	user_tasks[1].psp_val = T1_STACK_START;
	user_tasks[2].psp_val = T2_STACK_START;
	user_tasks[3].psp_val = T3_STACK_START;
	user_tasks[4].psp_val = T4_STACK_START;

	user_tasks[0].task_handler = idle_task;
	user_tasks[1].task_handler = task1_handler;
	user_tasks[2].task_handler = task2_handler;
	user_tasks[3].task_handler = task3_handler;
	user_tasks[4].task_handler = task4_handler;




	uint32_t *p_PSP;
	for(int i = 0; i < MAX_TASKS; i++)
	{
		p_PSP = (uint32_t*) user_tasks[i].psp_val;

		//stack model is full descending so decrement first, then store the value
		p_PSP--;
		*p_PSP = DUMMY_XPSR;

		//PC, need to point to the task_handler
		p_PSP--;
		*p_PSP = (uint32_t) user_tasks[i].task_handler;

		//LR
		p_PSP--;
		*p_PSP = 0xFFFFFFFD;

		//general registers
		for(int j = 0; j < 13; j++)
		{
			p_PSP--;
			*p_PSP = 0;

		}
		//preserve the value of PSP, as it is modified during the process
		user_tasks[i].psp_val = (uint32_t)p_PSP;

	}
}

void task_delay(uint32_t tick_count)
{
	//disable interrupt
	INTERRUPT_DISABLE();
	//only block the task if it not the idle task
	if(current_task)
	{
		//add block count to the task
		user_tasks[current_task].block_count = g_tick_count + tick_count;
		//change to blocked state
		user_tasks[current_task].current_state = TASK_BlOCKED_STATE;
		//pend pendSV exception
		schedule(); // switches to another task to allow other tasks to run
	}

	//enable interrupt
	INTERRUPT_ENABLE();
}

void update_global_tick_count(void)
{
	g_tick_count++;
}

void schedule(void)
{
	uint32_t *p_ICSR = (uint32_t*) 0xE000ED04;
	//pend the pendSV exception
	*p_ICSR |= (1 << 28);
}

void unblock_tasks(void)
{
	//unblock any tasks that are qualified for running
	for(int i = 1; i < MAX_TASKS; i++)//ignores the idle task
	{
		if(user_tasks[i].current_state != TASK_READY_STATE)
		{
			//blocking period has elapsed
			if(user_tasks[i].block_count == g_tick_count)
			{
				user_tasks[i].current_state = TASK_READY_STATE;
			}
		}
	}
}

void update_next_task(void)
{
	//finds the next task that is ready to run
	int state = TASK_BlOCKED_STATE;
	for (int i = 0; i < MAX_TASKS; i++)
	{
		current_task++;
		current_task %= MAX_TASKS; // current task will always be 0-3 inclusive and gets back to 0 when it is 4 -> round-robin
		state = user_tasks[current_task].current_state;
		//only break if the task is not the idle_task, b/c idle_task's state is always TASK_READY_STATE
		if( (state == TASK_READY_STATE) && (current_task != 0))
		{
			break;
		}
	}

	if(state == TASK_BlOCKED_STATE)
	{
		current_task = 0;
	}
}

void enable_processor_faults(void)
{
	uint32_t *p_SHCSR = (uint32_t*)0xE000ED24;
	*p_SHCSR |= (1 << 18); // usage fault
	*p_SHCSR |= (1 << 17); // bus fault
	*p_SHCSR |= (1 << 16); // mem fault
}

__attribute__((naked)) void PendSV_Handler(void)
{
	//do context switching to switch to the next ready to run task

	/*save the context of current task*/
	//1. get current running task's PSP value
	__asm volatile("MRS R0, PSP"); // store the PSP value to R0

	//2. using the PSP value, store SF2( R4 to R11)
	/* caution:
	 * - can't just use push b/c the handler always uses MSP, which will only push the values to the MSP stack,
	 * 	 not the task private stack
	 * 		-> use STMDB to save the values at the PSP address extracted into R0
	 * 				->stores into multiple registers and "decrements before" (decrement first then store)
	 */
	__asm volatile("STMDB R0!, {R4-R11}"); // ! means that the final address that is stored will be loaded back to R0

	//3. save the current value of PSP
	__asm volatile("PUSH {LR}"); // push LR onto the stack first, because c fcn calls will corrupt LR
	__asm volatile("BL save_psp_value");// R0 is passed as parameter by default

	/*retrieve the context of next task*/
	//1. decide next task to run
	__asm volatile("BL update_next_task");

	//2. get the task's past PSP value
	__asm volatile("BL get_psp_value");

	//3. using that PSP value retrieve SF2( R4 to R11), SF1 will be automatically retrieved when exiting handler
	 __asm volatile("LDMIA R0!, {R4-R11}");//load multiple from memory to register, increment address after each access

	//4. update PSP and exit
	 __asm volatile("MSR PSP, R0");

	//pop back the LR pushed to stack
	 __asm volatile("POP {LR}");

	//return
	 __asm volatile("BX LR");
}

void SysTick_Handler(void)
{

	update_global_tick_count();
	//unblock qualified tasks
	unblock_tasks();
	//pendSV
	schedule();

}

void HardFault_Handler(void)
{
	// printf("hard fault\n");
	while(1);
}

void MemManage_Handler(void)
{
	// printf("mem fault\n");
	while(1);
}

void BusFault_Handler(void)
{
	// printf("bus fault\n");
	while(1);
}

void UsageFault_Handler(void)
{
	// printf("usage fault\n");
	while(1);
}

