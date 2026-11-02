#---------------------------------------------------------------------------------
# GBA-Craft Makefile
# Copyright (c) 2026 Khamerron Kizer / ConsciousNode SoftWorks
# SPDX-License-Identifier: MIT
#---------------------------------------------------------------------------------
.SUFFIXES:

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

include $(DEVKITARM)/gba_rules

#---------------------------------------------------------------------------------
# Project configuration (all custom variables explicitly defined here)
#---------------------------------------------------------------------------------
TARGET		:=	gba-craft
BUILD		:=	build
SOURCES		:=	source
INCLUDES	:=	include

LIBTONC		:=	$(DEVKITPRO)/libtonc
LIBDIRS		:=	$(LIBTONC)

#---------------------------------------------------------------------------------
# Architecture and compiler/linker flags
#---------------------------------------------------------------------------------
ARCH		:=	-mthumb -mthumb-interwork

CFLAGS		:=	-Wall -O2 -std=c99 \
			-mcpu=arm7tdmi -mtune=arm7tdmi \
			-fno-strict-aliasing \
			$(ARCH)
CFLAGS		+=	$(INCLUDE)

ASFLAGS		:=	$(ARCH)
LDFLAGS		=	$(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS		:=	-ltonc

#---------------------------------------------------------------------------------
# Build directory recursion (standard devkitPro template layout)
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))

export OFILES	:=	$(CFILES:.c=.o) $(SFILES:.s=.o)

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).elf $(TARGET).gba $(TARGET).map

else

DEPENDS	:=	$(OFILES:.o=.d)

$(OUTPUT).gba	:	$(OUTPUT).elf
$(OUTPUT).elf	:	$(OFILES)

-include $(DEPENDS)

endif
