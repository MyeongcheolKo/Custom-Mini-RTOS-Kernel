# Custom-Mini-RTOS
A mini RTOS i created from scratch including a task schedular, linker script, and drivers

Task Schedular
The scheduler manages 4 user tasks plus an idle task, using hardware features of the ARM Cortex-M architecture:

SysTick Timer — Generates periodic interrupts (1ms ticks) to drive scheduling
PendSV Exception — Performs the actual context switch at the lowest priority
Dual Stack Pointers — MSP for kernel/handlers, PSP for user tasks

┌─────────────────────────────────────────────────────────┐
│                      SRAM Layout                        │
├─────────────────────────────────────────────────────────┤
│  Scheduler Stack (MSP)    <- Used by exception handlers │
├─────────────────────────────────────────────────────────┤
│  Task 1 Private Stack (PSP)                             │
├─────────────────────────────────────────────────────────┤
│  Task 2 Private Stack (PSP)                             │
├─────────────────────────────────────────────────────────┤
│  Task 3 Private Stack (PSP)                             │
├─────────────────────────────────────────────────────────┤
│  Task 4 Private Stack (PSP)                             │
├─────────────────────────────────────────────────────────┤
│  Idle Task Stack (PSP)                                  │
└─────────────────────────────────────────────────────────┘
How It Works: 
    ┌──────────┐   task_delay()   ┌─────────┐
    │  READY   │ ───────────────> │ BLOCKED │
    └──────────┘                  └─────────┘
         ^                             │
         │      block_count expires    │
         └─────────────────────────────┘

All tasks start in READY state
When a task calls task_delay(ticks), it transitions to BLOCKED
The scheduler skips blocked tasks during round-robin selection
Glogal tick_count get updaated at every SysTick_handler
When glogal tick_count reaches the task's block_count, it becomes READY again

Context Switch Flow: 

SysTick fires (every 1ms)
           │
  ┌───────────────────┐
  │ Update tick count │
  │   Unblock tasks   │
  │   Pend PendSV     │ <- Doesn't switch here, just sets pending bit
  └───────────────────┘
           │      (after all higher-priority interrupts complete)
┌──────────────────────┐
│        PendSV        │
│ Save R4-R11 manually │ <- Hardware auto-saves R0-R3, R12, LR, PC, xPSR
│   Select next task   │ 
│    Restore R4-R11    │
└──────────────────────┘

Design Choices
- created a dummy stack frame so the first context switch works
- decided to add blocking state and systick timer because the delay time for software delay is actually (task delay + delays of other tasks). For example, if Task1 wants a 1000ms delay but Task2-4 also have delays of 500ms, 250ms, and 125ms: Task1's real wait time = 1000 + 500 + 250 + 125 = 1875ms. By adding a blocking state and using the SysTick timer, a blocked task is skipped during scheduling and the scheduler immediately moves to the next ready task. This way each task's delay is independent of what other tasks are doing
- added idle_task that is always READY and never blocks, so update_next_task() would always have a valid task to select all other tasks are blocked
- chose pendsv for context switching instead of in systick because pendsv has lower priority so it only get executed after all other interrupts. This way the we won't exit a interrupt handler due to a context switch, which causes a usage fault. We only pend the pendsv and do the context switch when all other interrupt and excpetions are dealt with
- used naked functions for switch_sp_to_psp, PendSV_Handler, and init_scheduler_stack to deal with the prologue and epilogue of a C functions corrupting LR
    - switch_sp_to_psp: prologue would push LR to the old stack (MSP), then epilogue would pop from the new stack (PSP) —> corruption
    - PendSV_Handler: need to manually control over what gets pushed/popped to the stack and where for context switching + compiler would corrupt the EXEC_RETURN value in LR
    - init_scheduler_stack: modifying MSP itself, prologue/epilogue would use old MSP inconsistently
  
Ideas for future improvements
- Add priority levels 
- Dynamic task creation/deletion
- Add stack canaries or MPU protection

