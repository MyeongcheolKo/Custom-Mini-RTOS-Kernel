CC = arm-none-eabi-gcc
MACH = cortex-m4
CFLAGS = -c -mcpu=$(MACH) -mthumb -std=gnu11 -Wall -O0
LDFLAGS = -mcpu=$(MACH) -mthumb --specs=nano.specs -T linker_script.ld -Wl,-Map=final.map

all:main.o scheduler.o syscalls.o sysmem.o startup.o final.elf

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

final.elf:main.o scheduler.o startup.o syscalls.o sysmem.o
	$(CC) $(LDFLAGS) $^ -o $@

clean:
	rm -rf *.o *.elf

load:
	openocd -f board/st_nucleo_f4.cfg