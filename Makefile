ifeq ($(strip $(DEVKITPPC)),)
$(error Please set DEVKITPPC in your environment)
endif
include $(DEVKITPPC)/wii_rules
TARGET := boot
BUILD := build
SOURCES := source
INCLUDES :=
CFLAGS := -O2 -Wall -g $(MACHDEP) $(INCLUDE)
LDFLAGS := -g $(MACHDEP) -Wl,-Map,$(notdir $@).map
LIBS := -lwiiuse -lbte -logc -lm
LIBDIRS :=
ifneq ($(BUILD),$(notdir $(CURDIR)))
export OUTPUT := $(CURDIR)/$(TARGET)
export VPATH := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)
export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) -I$(CURDIR)/$(BUILD) -I$(LIBOGC_INC)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib) -L$(LIBOGC_LIB)
CFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
export OFILES := $(CFILES:.c=.o)
export LD := $(CC)
.PHONY: $(BUILD) clean
$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
clean:
	@rm -rf $(BUILD) $(TARGET).elf $(TARGET).dol $(TARGET).map
else
DEPENDS := $(OFILES:.o=.d)
$(OUTPUT).dol: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)
-include $(DEPENDS)
endif
