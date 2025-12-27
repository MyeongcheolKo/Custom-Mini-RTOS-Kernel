CC = arm-none-eabi-gcc
MACH = cortex-m4
CFLAGS = -c -mcpu=$(MACH) -mthumb -mfloat-abi=soft -std=gnu11 -Wall -O0 -g -Idrivers
LDFLAGS = -mcpu=$(MACH) -mthumb -mfloat-abi=soft --specs=nano.specs -T linker_script.ld -Wl,-Map=final.map
LDFLAGS_SH = -mcpu=$(MACH) -mthumb -mfloat-abi=soft --specs=rdimon.specs -T linker_script.ld -Wl,-Map=final.map

all:syscalls.o sysmem.o startup.o GPIO_driver.o LED_toggle.o final.elf

semi:sysmem.o startup.o final_sh.elf

sysmem.o:sysmem.c
	$(CC) $(CFLAGS) $^ -o $@

syscalls.o:syscalls.c
	$(CC) $(CFLAGS) $^ -o $@

startup.o:startup.c
	$(CC) $(CFLAGS) $^ -o $@
	
GPIO_driver.o:drivers/GPIO_driver.c
	$(CC) $(CFLAGS) $^ -o $@

LED_toggle.o:sample_applications/LED_toggle.c
	$(CC) $(CFLAGS) $^ -o $@

final.elf:startup.o syscalls.o sysmem.o GPIO_driver.o LED_toggle.o
	$(CC) $(LDFLAGS) $^ -o $@

final_sh.elf:startup.o sysmem.o
	$(CC) $(LDFLAGS_SH) $^ -o $@
	
clean:
	rm -rf *.o *.elf
	
flash: final.elf
	openocd -f board/st_nucleo_f4.cfg -c "program final.elf verify reset exit"

load:
	openocd -f board/st_nucleo_f4.cfg