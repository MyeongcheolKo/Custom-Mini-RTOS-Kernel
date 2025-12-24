CC = arm-none-eabi-gcc
MACH = cortex-m4
CFLAGS = -c -mcpu=$(MACH) -mthumb -std=gnu11 -Wall -O0

all:main.o scheduler.o sysmem.o syscalls.o startup.o

main.o:main.c
	$(CC) $(CFLAGS) $^ -o $@

scheduler.o:scheduler.c
	$(CC) $(CFLAGS) $^ -o $@

sysmem.o:sysmem.c
	$(CC) $(CFLAGS) $^ -o $@

syscalls.o:syscalls.c
	$(CC) $(CFLAGS) $^ -o $@

startup.o:startup.c
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -rf *.o *.elf
