# Custom-Mini-RTOS

A mini RTOS kernel for ARM Cortex-M built from scratch, including the supporting bare-metal infrastructure (linker script, startup code) and peripheral drivers.

## Task Scheduler(Kernel)

The scheduler manages 4 user tasks plus an idle task, using hardware features of the ARM Cortex-M architecture:

- **SysTick Timer** — Generates periodic interrupts (1ms ticks) to drive scheduling
- **PendSV Exception** — Performs the actual context switch at the lowest priority
- **Dual Stack Pointers** — MSP for kernel/handlers, PSP for user tasks

<img width="445" height="452" alt="Screenshot 2025-12-23 at 3 49 44 PM" src="https://github.com/user-attachments/assets/5aaaae75-c4e4-42ba-8412-b99382260066" />

### Task Lifecycle

- All tasks start in `READY` state
- When a task calls `task_delay(ticks)`, it transitions to `BLOCKED`
- The scheduler skips blocked tasks during round-robin selection
- Global `tick_count` gets updated at every `SysTick_Handler`
- When global `tick_count` reaches the task's `block_count`, it becomes `READY` again

### Context Switch Flow

<img width="532" height="275" alt="Screenshot 2025-12-23 at 3 50 46 PM" src="https://github.com/user-attachments/assets/4bd41f5a-3ef7-4740-9594-103fbbb59d41" />

## Design Choices

- **Dummy stack frame** — Created so the first context switch works. When a task runs for the first time, there's no "previous context" to retrieve, so we initialize the stack with a fake frame.

- **Blocking state + SysTick timer** — The delay time for software delay is actually (task delay + delays of other tasks). For example, if Task1 wants a 1000ms delay but Task2-4 also have delays of
  500ms, 250ms, and 2000ms: Task1's real wait time = 1000 + 500 + 250 + 2000 = 3750ms. By adding a blocking state and using the SysTick timer, a blocked task is skipped during scheduling and the scheduler
  immediately moves to the next ready task. This way each task's delay is independent of what other tasks are doing.

- **Idle task** — Always `READY` and never blocks, so `update_next_task()` always has a valid task to select when all other tasks are blocked.

- **PendSV for context switching** — Chose PendSV instead of doing it in SysTick because PendSV has lower priority, so it only gets executed after all other interrupts. This way we won't exit an interrupt handler due to a context switch, which causes a usage fault. We only pend the PendSV and do the context switch when all other interrupts and exceptions are dealt with.

- **Naked functions** — Used for `switch_sp_to_psp`, `PendSV_Handler`, and `init_scheduler_stack` to deal with the prologue and epilogue of C functions corrupting LR:
  - `switch_sp_to_psp`: Prologue would push LR to the old stack (MSP), then epilogue would pop from the new stack (PSP) -> corruption
  - `PendSV_Handler`: Need manual control over what gets pushed/popped to the stack and where for context switching + function calls corrupt the EXC_RETURN value in LR
  - `init_scheduler_stack`: Modifying MSP itself, prologue/epilogue would use old/new MSP inconsistently
- **Race condition in `task_delay`** - Disabled interrupts while setting block_count and current_state to prevent a race condition with SysTick_Handler. Without this, SysTick could increment g_tick_count between reading the value and setting the blocked state, causing the == check in unblock_tasks to miss and leave the task blocked forever.

## See it Working!
![task scheduler demo](task_scheduler_demo.gif)
## Ideas for Future Improvements

- Add priority levels
- Dynamic task creation/deletion
- Add stack canaries or MPU protection
