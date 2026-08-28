ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITARM)/3ds_rules
include $(TOPDIR)/config/server.mk

TARGET := rebank
BUILD := build
PKSM_CORE := vendor/PKSM-Core
SOURCES := source/app \
	source/core \
	source/save \
	source/network \
	source/bank \
	source/audio \
	source/i18n \
	source/gui \
	source/gui/elements \
	source/gui/welcomescreen \
	source/gui/loginscreen \
	source/gui/registerscreen \
	source/gui/resetpasswordscreen \
	source/gui/gameselectscreen \
	source/gui/bankscreen \
	source/gui/logsscreen \
	source/gui/loadingscreen \
	vendor/PKSM-SPI \
	$(PKSM_CORE)/memecrypto \
	$(PKSM_CORE)/source/i18n \
	$(PKSM_CORE)/source/personal \
	$(PKSM_CORE)/source/pkx \
	$(PKSM_CORE)/source/sav \
	$(PKSM_CORE)/source/utils \
	$(PKSM_CORE)/source/wcx
DATA := data
INCLUDES := include
ROMFS := romfs
APP_TITLE := ReBank
APP_DESCRIPTION := Secure Pokemon storage client
APP_AUTHOR := Jxstn
APP_ICON := $(TOPDIR)/packaging/icon/icon48.png

ARCH := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS := -g -Wall -Wextra -O2 -mword-relocations -ffunction-sections $(ARCH)
CFLAGS += $(INCLUDE) -D__3DS__ \
		  -DREBANK_SERVER_SCHEME=\"$(REBANK_SERVER_SCHEME)\" \
		  -DREBANK_SERVER_HOST=\"$(REBANK_SERVER_HOST)\" \
		  -DREBANK_SERVER_PORT=$(REBANK_SERVER_PORT)
CXXFLAGS := $(CFLAGS) -fno-rtti -std=gnu++20
ASFLAGS := -g $(ARCH)
LDFLAGS := -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)
LIBS := -lcitro2d -lcitro3d -lvorbisidec -logg -ljansson -lctru -lm
LIBDIRS := $(CTRULIB) $(PORTLIBS)

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT := $(CURDIR)/$(TARGET)
export TOPDIR := $(CURDIR)
export VPATH := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
                $(foreach dir,$(DATA),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)

CFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES := $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

export LD := $(CXX)
export OFILES_BIN := $(addsuffix .o,$(BINFILES))
export OFILES_SRC := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES := $(OFILES_BIN) $(OFILES_SRC)
export HFILES_BIN := $(addsuffix .h,$(subst .,_,$(BINFILES)))
export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
				  -I$(CURDIR)/vendor/PKSM-SPI \
				  -I$(CURDIR)/$(PKSM_CORE)/include \
				  -I$(CURDIR)/$(PKSM_CORE)/include/enums \
				  -I$(CURDIR)/$(PKSM_CORE)/include/personal \
				  -I$(CURDIR)/$(PKSM_CORE)/include/pkx \
				  -I$(CURDIR)/$(PKSM_CORE)/include/sav \
				  -I$(CURDIR)/$(PKSM_CORE)/include/utils \
				  -I$(CURDIR)/$(PKSM_CORE)/include/wcx \
				  -I$(CURDIR)/$(PKSM_CORE)/memecrypto \
				  -I$(CURDIR)/$(PKSM_CORE)/pcg-cpp/include \
                  $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                  -I$(CURDIR)/$(BUILD)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)
export _3DSXDEPS := $(if $(NO_SMDH),,$(OUTPUT).smdh)
export _3DSXFLAGS += --smdh=$(CURDIR)/$(TARGET).smdh
export _3DSXFLAGS += --romfs=$(CURDIR)/$(ROMFS)

.PHONY: all clean

all: $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

$(BUILD):
	@mkdir -p $@

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).3dsx $(TARGET).elf $(TARGET).smdh

else

$(OUTPUT).3dsx: $(OUTPUT).elf $(_3DSXDEPS)
$(OUTPUT).elf: $(OFILES)
$(OFILES_SRC): $(HFILES_BIN)

-include $(DEPSDIR)/*.d

endif