TARGET := switch-ha-native
BUILD := build
SOURCES := source
INCLUDES := include
APP_TITLE := Switch HA Native
APP_AUTHOR := rachosol
APP_VERSION := 1.0.0
APP_ICON := $(CURDIR)/assets/icon.jpg

ifeq ($(strip $(DEVKITPRO)),)
$(error Please set DEVKITPRO)
endif

ARCH := -march=armv8-a -mtune=cortex-a57 -fPIE
CFLAGS := -g -Wall -O2 -ffunction-sections $(ARCH)
CFLAGS += -D__SWITCH__
ASFLAGS := -g $(ARCH)
LDFLAGS := -specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)
LIBS := -lnx
LD := $(DEVKITPRO)/devkitA64/bin/aarch64-none-elf-gcc

ifneq ($(BUILD),$(notdir $(CURDIR)))
export OUTPUT := $(CURDIR)/$(TARGET)
export TOPDIR := $(CURDIR)
export VPATH := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)
CFILENAMES := $(notdir $(wildcard $(SOURCES)/*.c))
SFILENAMES := $(notdir $(wildcard $(SOURCES)/*.s))
export OFILES := $(CFILENAMES:.c=.o) $(SFILENAMES:.s=.o)
export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) -I$(DEVKITPRO)/libnx/include
export CPPFLAGS := $(INCLUDE)
export LIBPATHS := -L$(DEVKITPRO)/libnx/lib
export NROFLAGS := --nacp=$(OUTPUT).nacp --icon=$(CURDIR)/assets/icon.jpg
export ARCH CFLAGS ASFLAGS LDFLAGS LIBS LD APP_TITLE APP_AUTHOR APP_VERSION
.PHONY: all clean
all:
	@mkdir -p $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile all
clean:
	@rm -rf $(BUILD) $(TARGET).nro $(TARGET).nacp $(TARGET).elf $(TARGET).map
else
include $(DEVKITPRO)/libnx/switch_rules
DEPENDS := $(OFILES:.o=.d)
all: $(OUTPUT).nro
$(OUTPUT).nro: $(OUTPUT).nacp
$(OUTPUT).elf: $(OFILES)
-include $(DEPENDS)
endif
