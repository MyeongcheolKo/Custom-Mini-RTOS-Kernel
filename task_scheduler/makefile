CC = arm-none-eabi-gcc
MACH = cortex-m4
CFLAGS = -c -mcpu=$(MACH) -mthumb -std=gnu11 -O0 -I Inc/

main.o:Src/main.c
	$(CC) $(CFLAGS) $^ -o $@