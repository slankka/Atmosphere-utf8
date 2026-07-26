.SUFFIXES:

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

TARGET		:=	fs_codecvt
BUILD		:=	build
SOURCES		:=	source/nx source source/utils
DATA		:=	data
INCLUDES	:=	include

ifneq ($(BUILD),$(notdir $(CURDIR)))
FS_CODECVT_DIR ?= $(CURDIR)
else
FS_CODECVT_DIR ?= $(CURDIR)/../
endif

include $(DEVKITPRO)/libnx/switch_rules

ARCH	:=	-march=armv8-a -mtune=cortex-a57 -mtp=soft -fPIE

DEFINES := -DINNER_HEAP_SIZE=0x4000

CFLAGS	:=	-Wall -O2 -ffunction-sections -fdata-sections -Wno-unused-function \
			$(ARCH) $(DEFINES)

CFLAGS	+=	$(INCLUDE) -D__SWITCH__

CXXFLAGS	:= $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++17

ASFLAGS	:=	-g $(ARCH)
LDFLAGS	=	-specs=$(FS_CODECVT_DIR)/fs_codecvt.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

# LD must match what the Atmosphere build system expects
LD := $(CXX)

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export TOPDIR	:=	$(FS_CODECVT_DIR)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
			$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

export OFILES_BIN	:=	$(addsuffix .o,$(BINFILES))
export OFILES_SRC	:=	$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES 	:=	$(OFILES_BIN) $(OFILES_SRC)
export HFILES_BIN	:=	$(addsuffix .h,$(subst .,_,$(BINFILES)))

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(SOURCES),-I$(CURDIR)/$(dir)) \
			-I$(CURDIR)/$(BUILD)

.PHONY: $(BUILD) clean all

all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo clean ...
	@rm -rf $(BUILD) $(OUTPUT).elf $(OUTPUT).kip $(OUTPUT)_unpacked.kip

else
DEPENDS	:=	$(OFILES:.o=.d)

all	:	$(OUTPUT)_unpacked.kip

$(OUTPUT)_unpacked.kip	:	$(OUTPUT).kip
	@hactool -t kip --uncompressed=$(OUTPUT)_unpacked.kip $(OUTPUT).kip
	@echo "built ... $(notdir $(OUTPUT))_unpacked.kip"

$(OUTPUT).kip	:	$(OUTPUT).elf
	@elf2kip $(OUTPUT).elf $(FS_CODECVT_DIR)/fs_codecvt.json $(OUTPUT).kip
	@echo "built ... $(notdir $(OUTPUT)).kip"

$(OUTPUT).elf	:	$(OFILES)

-include $(DEPENDS)

endif
