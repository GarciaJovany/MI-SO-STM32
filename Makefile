PROJECT := mi_so_stm32
BUILD := build
PREFIX ?= arm-none-eabi-
CC := $(PREFIX)gcc
OBJCOPY := $(PREFIX)objcopy
SIZE := $(PREFIX)size

CFLAGS := -mcpu=cortex-m3 -mthumb -O2 -std=c11 -ffreestanding -fno-builtin \
  -fdata-sections -ffunction-sections -Wall -Wextra -Werror -Isrc
ASFLAGS := -mcpu=cortex-m3 -mthumb -x assembler-with-cpp
LDFLAGS := -mcpu=cortex-m3 -mthumb -nostartfiles -nostdlib -T linker/stm32f103c8.ld \
  -Wl,--gc-sections -Wl,-Map=$(BUILD)/$(PROJECT).map

CSRC := $(wildcard src/*/*.c)
ASRC := $(wildcard src/*/*.S)
OBJS := $(patsubst %.c,$(BUILD)/%.o,$(CSRC)) $(patsubst %.S,$(BUILD)/%.o,$(ASRC))

.PHONY: all clean flash test-host
all: $(BUILD)/$(PROJECT).bin

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD)/$(PROJECT).elf: $(OBJS) linker/stm32f103c8.ld
	$(CC) $(LDFLAGS) -o $@ $(OBJS) -lgcc
	$(SIZE) $@

$(BUILD)/$(PROJECT).bin: $(BUILD)/$(PROJECT).elf
	$(OBJCOPY) -O binary $< $@

flash: all
	openocd -f interface/stlink.cfg -f target/stm32f1x.cfg -c "program $(BUILD)/$(PROJECT).elf verify reset exit"

test-host:
	@mkdir -p $(BUILD)
	gcc -DOS_HOST_TEST -Isrc -std=c11 -Wall -Wextra -Werror \
      tests/test_scheduler.c src/sched/scheduler.c src/sched/pcb.c src/mem/heap.c \
      -o $(BUILD)/test_scheduler
	$(BUILD)/test_scheduler
	gcc -DOS_HOST_TEST -Isrc -std=gnu11 -g -Wall -Wextra -Werror \
      tests/test_sync.c src/kernel/task.c src/sched/scheduler.c src/sched/pcb.c \
      src/sync/sem.c src/sync/mutex.c src/sync/mq.c -o $(BUILD)/test_sync
	$(BUILD)/test_sync

clean:
	rm -rf $(BUILD)
