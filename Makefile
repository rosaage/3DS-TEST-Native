.SUFFIXES:
ifeq ($(strip $(DEVKITARM)),)
$(error Set DEVKITARM, or build from the devkitPro MSYS2 terminal)
endif
include $(DEVKITARM)/3ds_rules

TARGET := 3DS-TEST-Native
BUILD := build
ARCH := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS := -g -Wall -Wextra -O2 -mword-relocations -ffunction-sections $(ARCH)
CFLAGS += $(INCLUDE) -D__3DS__
LDFLAGS = -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)
LIBS := -lcitro2d -lcitro3d -lctru -lm
APP_TITLE := 3DS TEST Native
APP_DESCRIPTION := Buttons, touch and raw stick X/Y tester
APP_AUTHOR := 3DS-TEST contributors
MAKEROM ?= makerom
BANNERTOOL ?= bannertool
BSDTAR ?= bsdtar
APP_UNIQUE_ID ?= 0xC64B

ifneq ($(BUILD),$(notdir $(CURDIR)))
export OUTPUT := $(CURDIR)/$(TARGET)
export TOPDIR := $(CURDIR)
export DEPSDIR := $(CURDIR)/$(BUILD)
export VPATH := $(CURDIR)/source
export INCLUDE := -I$(CTRULIB)/include
export LIBPATHS := -L$(CTRULIB)/lib
export OFILES := main.o cpp.o
export LD := $(CC)
export APP_ICON := $(CURDIR)/icon.png
export _3DSXFLAGS := --smdh=$(OUTPUT).smdh
.PHONY: all cia dist clean
all:
	@mkdir -p $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	rm -rf $(BUILD)

dist: all
	rm -rf $(BUILD)/dist
	mkdir -p $(BUILD)/dist/3ds/$(TARGET)
	cp -R licenses LICENSE $(TARGET).3dsx $(TARGET).smdh $(BUILD)/dist/3ds/$(TARGET)/
	$(BSDTAR) --format zip -cf $(TARGET).zip -C $(BUILD)/dist 3ds

cia: all $(BUILD)/banner.bin
	$(MAKEROM) -f cia -target t -o $(TARGET).cia -rsf meta/application.rsf \
		-DAPP_UNIQUE_ID=$(APP_UNIQUE_ID) -elf $(TARGET).elf \
		-icon $(TARGET).smdh -banner $(BUILD)/banner.bin

$(BUILD)/banner.bin: banner.png meta/banner.wav
	@mkdir -p $(BUILD)
	$(BANNERTOOL) makebanner -i banner.png -a meta/banner.wav -o $@
else
$(OUTPUT).3dsx: $(OUTPUT).elf $(OUTPUT).smdh
$(OUTPUT).elf: $(OFILES)
-include $(DEPSDIR)/*.d
endif
