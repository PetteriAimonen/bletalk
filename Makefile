PROJECT = bletalk

# Compilation options for EFM32 cpu
CC      = arm-none-eabi-gcc
AR		= arm-none-eabi-ar
OBJCOPY = arm-none-eabi-objcopy
CFLAGS ?= -O2 -g3 -Wall
CFLAGS += -std=gnu99
CFLAGS += -ffunction-sections -fdata-sections
CFLAGS += -DBGM113A256V2=1 -DHAL_CONFIG=1
CFLAGS += -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=softfp
LFLAGS  = -Wl,--gc-sections

# Path to SiliconLabs Gecko SDK
SDKDIR = $(HOME)/SimplicityStudio/developer/sdks/gecko_sdk_suite/v2.5

# SDK include directories
CFLAGS += -I$(SDKDIR)/platform/Device/SiliconLabs/BGM1/Include
CFLAGS += -I$(SDKDIR)/hardware/module/config
CFLAGS += -I$(SDKDIR)/platform/CMSIS/Include
CFLAGS += -I$(SDKDIR)/platform/emlib/inc
CFLAGS += -I$(SDKDIR)/platform/bootloader/api
CFLAGS += -I$(SDKDIR)/protocol/bluetooth/ble_stack/inc/soc
CFLAGS += -I$(SDKDIR)/protocol/bluetooth/ble_stack/inc/common
CFLAGS += -I$(SDKDIR)/app/bluetooth/common/util

# MCU linker script
LFLAGS += -L$(SDKDIR)/protocol/bluetooth/ble_stack/linker/GCC/ -Tbgm113a256v2.ld

# MCU support library
EMLIB_CSRC = $(wildcard $(SDKDIR)/platform/emlib/src/*.c)
EMLIB_OBJS = $(patsubst %.c,build/emlib/%.o,$(notdir $(EMLIB_CSRC)))
LIBS += build/emdrv/sleep.o
LIBS += build/emlib.a

# Bluetooth stack libraries
LIBS += build/bgm1/startup_bgm1.o build/bgm1/system_bgm1.o
LIBS += build/bootloader.o
LIBS += $(SDKDIR)/protocol/bluetooth/lib/EFR32BG1B/GCC/binapploader.o
LIBS += $(SDKDIR)/protocol/bluetooth/lib/EFR32BG1B/GCC/libbluetooth.a
LIBS += $(SDKDIR)/protocol/bluetooth/lib/EFR32BG1B/GCC/libmbedtls.a
LIBS += $(SDKDIR)/protocol/bluetooth/lib/EFR32BG1B/GCC/libpsstore.a
LIBS += $(SDKDIR)/platform/radio/rail_lib/autogen/librail_release/librail_config_bgm113a256v2_gcc.a
LIBS += $(SDKDIR)/platform/radio/rail_lib/autogen/librail_release/librail_module_efr32xg1_gcc_release.a

# Standard system libraries
SYSLIBS += -lm -lgcc -lc -lnosys

# Source code files in this project
CSRCS = main.c bluetooth.c board.c application_properties.c
CFLAGS += -I src

OBJS = $(CSRCS:%.c=build/%.o)
DEPS := $(OBJS:%.o=%.d)

GDB = arm-none-eabi-gdb
OOCD = openocd
OOCDFLAGS = -f interface/stlink-v2.cfg -f target/efm32.cfg

all: build build/$(PROJECT).elf

clean:
	rm -rf build

build:
	mkdir -p build
	mkdir -p build/bgm1
	mkdir -p build/emlib
	mkdir -p build/emdrv

start_openocd:
	pidof openocd || $(OOCD) $(OOCDFLAGS)

program: build/$(PROJECT).elf
	( echo "reset halt; program $< verify reset exit" | nc localhost 4444 ) || \
		 $(OOCD) $(OOCDFLAGS) -c "init; reset halt; program $< verify reset exit"

debug:
	$(GDB) -iex 'target extended :3333' build/$(PROJECT).elf

build/$(PROJECT).elf: $(OBJS) $(LIBS)
	$(CC) $(CFLAGS) $(LFLAGS) -o $@ -Wl,--start-group $(OBJS) $(LIBS) -Wl,--end-group \
			-Wl,--start-group $(SYSLIBS) -Wl,--end-group
	
	@echo
	@arm-none-eabi-size -t $@

-include $(DEPS)

build/%.o: src/%.c
	$(CC) $(CFLAGS) -MMD -c -o $@ $<

# Build rules for SDK libraries
build/emlib/%.o: $(SDKDIR)/platform/emlib/src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

build/emdrv/%.o: $(SDKDIR)/platform/emdrv/sleep/src/%.c
	$(CC) $(CFLAGS) -c -I $(SDKDIR)/platform/emdrv/sleep/inc -o $@ $<

build/bgm1/%.o: $(SDKDIR)/platform/Device/SiliconLabs/BGM1/Source/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

build/bgm1/%.o: $(SDKDIR)/platform/Device/SiliconLabs/BGM1/Source/GCC/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

build/emlib.a: $(EMLIB_OBJS)
	$(AR) cr $@ $^

build/bootloader.o: $(SDKDIR)/platform/bootloader/sample-apps/bootloader-uart-bgapi/bgm113a256v2-brd4301a/bootloader-uart-bgapi-combined.s37
	$(OBJCOPY) -I srec -O elf32-littlearm -B arm --prefix-sections=.binbootloader \
		--gap-fill 0 --pad-to 0x4000 $< $@



# arm-none-eabi-gcc -g -gdwarf-2 -mcpu=cortex-m4 -mthumb -T "/home/petteri/SimplicityStudio/v4_workspace/soc-thermometer/bgm113a256v2.ld" -Xlinker --gc-sections -Xlinker -Map="soc-thermometer.map" -mfpu=fpv4-sp-d16 -mfloat-abi=softfp --specs=nano.specs -o soc-thermometer.axf -Wl,--start-group "./app/bluetooth/common/util/infrastructure.o" "./application_properties.o" "./gatt_db.o" "./init_app.o" "./init_board_efr32xg1.o" "./init_mcu_efr32xg1.o" "./main.o" "./pti.o" "./hardware/kit/common/bsp/bsp_stk.o" "./hardware/kit/common/drivers/i2cspm.o" "./hardware/kit/common/drivers/mx25flash_spi.o" "./hardware/kit/common/drivers/si7013.o" "./hardware/kit/common/drivers/tempsens.o" "./hardware/kit/common/drivers/udelay.o" "./platform/Device/SiliconLabs/BGM1/Source/GCC/startup_bgm1.o" "./platform/Device/SiliconLabs/BGM1/Source/system_bgm1.o" "./platform/emdrv/sleep/src/sleep.o" "./platform/emdrv/tempdrv/src/tempdrv.o" "./platform/emlib/src/em_assert.o" "./platform/emlib/src/em_burtc.o" "./platform/emlib/src/em_cmu.o" "./platform/emlib/src/em_core.o" "./platform/emlib/src/em_cryotimer.o" "./platform/emlib/src/em_crypto.o" "./platform/emlib/src/em_emu.o" "./platform/emlib/src/em_gpio.o" "./platform/emlib/src/em_i2c.o" "./platform/emlib/src/em_msc.o" "./platform/emlib/src/em_rmu.o" "./platform/emlib/src/em_rtcc.o" "./platform/emlib/src/em_se.o" "./platform/emlib/src/em_system.o" "./platform/emlib/src/em_timer.o" "./platform/emlib/src/em_usart.o" "/home/petteri/SimplicityStudio/v4_workspace/soc-thermometer/platform/radio/rail_lib/autogen/librail_release/librail_module_efr32xg1_gcc_release.a" "/home/petteri/SimplicityStudio/v4_workspace/soc-thermometer/platform/radio/rail_lib/autogen/librail_release/librail_config_bgm113a256v2_gcc.a" "/home/petteri/SimplicityStudio/v4_workspace/soc-thermometer/protocol/bluetooth/lib/EFR32BG1B/GCC/binapploader.o" "/home/petteri/SimplicityStudio/v4_workspace/soc-thermometer/protocol/bluetooth/lib/EFR32BG1B/GCC/libmbedtls.a" "/home/petteri/SimplicityStudio/v4_workspace/soc-thermometer/protocol/bluetooth/lib/EFR32BG1B/GCC/libbluetooth.a" "/home/petteri/SimplicityStudio/v4_workspace/soc-thermometer/protocol/bluetooth/lib/EFR32BG1B/GCC/libpsstore.a" -lm -Wl,--end-group -Wl,--start-group -lgcc -lc -lnosys -Wl,--end-group

