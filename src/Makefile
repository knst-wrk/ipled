
TARGET = main
TARGET_MACH= -mthumb -mcpu=cortex-m3 -DSTM32F10X_MD -DHSE_VALUE=8000000UL
NEWLIB = --specs=nano.specs
#NEWLIB = --specs=nano.specs --specs=rdimon.specs -DDEBUG
OPTIMIZE = -O2

CSRC = \
	cmsis/system_stm32f10x.c \
	ff/ff.c \
	ff/ffunicode.c \
	ff/diskio.c \
	timeout.c \
	handler.c \
	analog.c \
	server.c \
	system.c \
	config.c \
	buffer.c \
	scene.c \
	rfio.c \
	leds.c \
	tpm2.c \
	dmx.c \
	tty.c \
	sd.c \
	ui.c \
	main.c

CXXSRC =

ASRC = \
	crt0.S

LIBS =




# Toolchain
TC = /home/kamajii/Projekte/stm32/gcc-arm-none-eabi-7-2017-q4-major/bin/arm-none-eabi-
OBJCOPY = $(TC)objcopy
OBJDUMP = $(TC)objdump
SIZE= $(TC)size
GDB = $(TC)gdb
RM = rm


# Tool arguments
CC = $(TC)gcc
CFLAGS = -Wall -Wextra -pedantic $(OPTIMIZE) -g --std=c99 -ffunction-sections -fdata-sections $(TARGET_MACH) $(NEWLIB) $(INCLUDE)

CXX = $(TC)g++
CXXFLAGS = -Wall -Wextra -pedantic $(OPTIMIZE) -g -std=c++11 -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti $(TARGET_MACH) $(NEWLIB) $(INCLUDE)

LD = $(TC)gcc
LDFLAGS = $(OPTIMIZE) -g -nostartfiles -Wl,--gc-sections -Wl,-Map=$(TARGET).mmap -Ldld -Wl,-Tlink.ld $(TARGET_MACH) $(NEWLIB) $(LIBS)

NM = $(TC)nm


# Default target
.PHONY: all
all: $(TARGET).elf $(TARGET).map $(TARGET).lss


# Suffix rules
.SUFFIXES:
.SUFFIXES: .c .cpp .S .o .lss .elf .hex .eep

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

%.S: %.c
	$(CC) $(CFLAGS) -o $@ -S $<

%.ii: %.c
	$(CC) $(CFLAGS) -o $@ -E $<


%.o: %.cpp
	$(CXX) $(CXXFLAGS) -o $@ -c $<

%.S: %.cpp
	$(CXX) $(CXXFLAGS) -o $@ -S $<

%.ii: %.cpp
	$(CXX) $(CXXFLAGS) -o $@ -E $<


%.o: %.S
	$(CC) $(CFLAGS) -o $@ -c $<


%.lss: %.elf
	$(OBJDUMP) -h -D -S $< > $@




# Automatically generate dependencies
c.dependencies: $(CSRC)
	if [ "$^" ]; then $(CC) $(CFLAGS) -MM $^ > c.dependencies; else touch c.dependencies; fi

cc.dependencies: $(CXXSRC)
	if [ "$^" ]; then $(CXX) $(CXXFLAGS) -MM $^ > cc.dependencies; else touch cc.dependencies; fi

include c.dependencies
include cc.dependencies


# Target
OBJS = \
	$(CSRC:.c=.o) \
	$(CXXSRC:.cpp=.o) \
	$(ASRC:.S=.o)

$(TARGET).elf: $(OBJS)
	$(LD) -o $@ $^ $(LDFLAGS)
	if [ -f $@ ]; then $(SIZE) -A $@; fi

$(TARGET).map: $(TARGET).elf
	$(NM) -C -l -S -fs -n $(TARGET).elf > $(TARGET).map

# Housekeeping
.PHONY: program
program:

.PHONY: clean
clean:
	-$(RM) c.dependencies
	-$(RM) cc.dependencies

	-$(RM) $(OBJS)
	-$(RM) $(TARGET).elf

	-$(RM) $(TARGET).lss
	-$(RM) $(TARGET).map

.PHONY: re
re:
	make clean
	make

.PHONY: tar
tar: clean
	-tar -cjv --exclude $(TARGET).tar.bz2 --exclude doc -f $(TARGET).tar.bz2 .

