CC = arm-none-eabi-gcc
MACH = cortex-m4
CFLAGS = -c -mcpu=$(MACH) -mthumb -mfloat-abi=soft -std=gnu11 -Wall -O0 -g -Idrivers
LDFLAGS = -mcpu=$(MACH) -mthumb -mfloat-abi=soft --specs=nano.specs -T linker_script.ld -Wl,-Map=final.map
LDFLAGS_SH = -mcpu=$(MACH) -mthumb -mfloat-abi=soft --specs=rdimon.specs -T linker_script.ld -Wl,-Map=final.map

all:syscalls.o sysmem.o startup.o GPIO_driver.o SPI_driver.o I2C_driver.o I2C_interrupt_send_receive.o final.elf

semi:sysmem.o startup.o final_sh.elf

sysmem.o:sysmem.c
	$(CC) $(CFLAGS) $^ -o $@

syscalls.o:syscalls.c
	$(CC) $(CFLAGS) $^ -o $@

startup.o:startup.c
	$(CC) $(CFLAGS) $^ -o $@
	
GPIO_driver.o:drivers/GPIO_driver.c
	$(CC) $(CFLAGS) $^ -o $@

SPI_driver.o:drivers/SPI_driver.c
	$(CC) $(CFLAGS) $^ -o $@
	
I2C_driver.o:drivers/I2C_driver.c
	$(CC) $(CFLAGS) $^ -o $@
	
LED_toggle.o:sample_applications/LED_toggle.c
	$(CC) $(CFLAGS) $^ -o $@

button_LED.o:sample_applications/button_LED.c
	$(CC) $(CFLAGS) $^ -o $@
	
external_button_LED.o:sample_applications/external_button_LED.c
	$(CC) $(CFLAGS) $^ -o $@

interrupt_button_LED.o:sample_applications/interrupt_button_LED.c
	$(CC) $(CFLAGS) $^ -o $@

SPI_testing.o:sample_applications/SPI_testing.c
	$(CC) $(CFLAGS) $^ -o $@

SPI_transmit_arduino.o:sample_applications/SPI_transmit_arduino.c
	$(CC) $(CFLAGS) $^ -o $@
	
SPI_send_recieve_arduino.o:sample_applications/SPI_send_recieve_arduino.c
	$(CC) $(CFLAGS) $^ -o $@

I2C_controller_send_arduino.o:sample_applications/I2C_controller_send_arduino.c
	$(CC) $(CFLAGS) $^ -o $@

I2C_interrupt_send_receive.o:sample_applications/I2C_interrupt_send_receive.c
	$(CC) $(CFLAGS) $^ -o $@
	
final.elf:startup.o syscalls.o sysmem.o GPIO_driver.o SPI_driver.o I2C_driver.o I2C_interrupt_send_receive.o
	$(CC) $(LDFLAGS) $^ -o $@

final_sh.elf:startup.o sysmem.o
	$(CC) $(LDFLAGS_SH) $^ -o $@
	
clean:
	rm -rf *.o *.elf

load:
	openocd -f board/st_nucleo_f4.cfg