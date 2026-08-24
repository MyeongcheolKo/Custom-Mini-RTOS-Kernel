#ifndef OS_PORT_H_
#define OS_PORT_H_
#include <stdint.h>

#define PORT_WAIT_FOR_INTERRUPT() do{ __asm volatile("WFI"); } while(0)

/* arch services the kernel core calls */

// Enter critical section, disable interrupts and return the previous interrupt state (0 = enabled, 1 = disabled)
uint32_t port_enter_critical(void);

// Exit critical section, restore interrupts to the previous state (0 = enabled, 1 = disabled)
void port_exit_critical(uint32_t prev_primask);

// sets MSP to the top of the scheduler stack where all the handlers run
__attribute__((naked)) void port_init_scheduler_stack(uint32_t scheduler_top_of_stack);

// configure PendSV to lowest priority so it is only triggered when no other exceptions are active
void port_set_pendSV_priority_lowest(void);

// configures SysTick to fire at tick_hz interrupts per second using the processor clock
void port_init_systick(uint32_t tick_hz);

// switches the active stack pointer from MSP to PSP so tasks run on their own private stacks
__attribute__((naked)) void port_switch_to_psp(void);

// builds the initial dummy stack frame at the top of a stack for the task, returns the PSP
uint32_t port_init_task_stack_frame(void (*handler)(void), uint32_t *base, uint32_t size);

// triggers a PendSV exception to perform a context switch at the lowest exception priority
void port_yield(void);

#endif 