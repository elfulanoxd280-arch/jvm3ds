TARGET      := jvm3ds
DEVKITPRO   ?= /c/devkitPro
DEVKITARM   ?= $(DEVKITPRO)/devkitARM
CTRULIB     := $(DEVKITPRO)/libctru

PREFIX      := $(DEVKITARM)/bin/arm-none-eabi-
CC          := $(PREFIX)gcc
LD          := $(PREFIX)gcc

ARCH        := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS      := -g -Wall -O2 $(ARCH) -ffunction-sections \
               -I$(CTRULIB)/include
LDFLAGS     := -specs=3dsx.specs $(ARCH) -Wl,-Map,$(TARGET).map \
               -L$(CTRULIB)/lib -lctru -lm

SRCS        := main.c jvm.c
OBJS        := $(SRCS:.c=.o)

all: $(TARGET).3dsx

$(TARGET).elf: $(OBJS)
	$(LD) $(OBJS) $(LDFLAGS) -o $@

$(TARGET).3dsx: $(TARGET).elf
	3dsxtool $< $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET).elf $(TARGET).3dsx
