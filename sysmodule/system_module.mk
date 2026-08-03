THIS_MAKEFILE := $(abspath $(lastword $(MAKEFILE_LIST)))
CURRENT_DIRECTORY := $(abspath $(dir $(THIS_MAKEFILE)))
# Clone this dependency as the repository submodule, or pass
# ATMOSPHERE_LIBS=/path/to/Atmosphere-libs when building.
ATMOSPHERE_LIBS ?= $(CURRENT_DIRECTORY)/../Atmosphere-libs
include $(ATMOSPHERE_LIBS)/config/templates/stratosphere.mk

# Do not derive the executable name from this generic "sysmodule" directory.
TARGET := switch-ha-native

ATMOSPHERE_SYSTEM_MODULE_TARGETS := nsp

ifneq ($(__RECURSIVE__),1)
export TOPDIR := $(CURDIR)
export VPATH := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
CFILES := $(call FIND_SOURCE_FILES,$(SOURCES),c)
CPPFILES := $(call FIND_SOURCE_FILES,$(SOURCES),cpp)
SFILES := $(call FIND_SOURCE_FILES,$(SOURCES),s)
export LD := $(CXX)
export OFILES := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) $(foreach dir,$(LIBDIRS),-I$(dir)/include) $(foreach dir,$(AMS_LIBDIRS),-I$(dir)/include) -I$(CURDIR)/$(ATMOSPHERE_BUILD_DIR)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib) $(foreach dir,$(AMS_LIBDIRS),-L$(dir)/$(ATMOSPHERE_LIBRARY_DIR))
export APP_JSON := $(TOPDIR)/switch_ha_native.json

.PHONY: clean all check_lib
all: $(ATMOSPHERE_OUT_DIR) $(ATMOSPHERE_BUILD_DIR) $(ATMOSPHERE_LIBRARIES_DIR)/libstratosphere/$(ATMOSPHERE_LIBRARY_DIR)/libstratosphere.a
	@$(MAKE) __RECURSIVE__=1 OUTPUT=$(CURDIR)/$(ATMOSPHERE_OUT_DIR)/$(TARGET) DEPSDIR=$(CURDIR)/$(ATMOSPHERE_BUILD_DIR) --no-print-directory -C $(ATMOSPHERE_BUILD_DIR) -f $(THIS_MAKEFILE)
$(ATMOSPHERE_LIBRARIES_DIR)/libstratosphere/$(ATMOSPHERE_LIBRARY_DIR)/libstratosphere.a:
	@$(MAKE) --no-print-directory -C $(ATMOSPHERE_LIBRARIES_DIR)/libstratosphere -f $(ATMOSPHERE_LIBRARIES_DIR)/libstratosphere/libstratosphere.mk
$(ATMOSPHERE_OUT_DIR) $(ATMOSPHERE_BUILD_DIR):
	@[ -d $@ ] || mkdir -p $@
clean:
	@rm -fr $(ATMOSPHERE_OUT_DIR) $(ATMOSPHERE_BUILD_DIR)
else
DEPENDS := $(OFILES:.o=.d)
all: $(foreach target,$(ATMOSPHERE_SYSTEM_MODULE_TARGETS),$(OUTPUT).$(target))
$(OUTPUT).nsp: $(OUTPUT).nso $(OUTPUT).npdm
$(OUTPUT).nso: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)
$(OFILES): $(ATMOSPHERE_LIBRARIES_DIR)/libstratosphere/$(ATMOSPHERE_LIBRARY_DIR)/libstratosphere.a
%.npdm: %.npdm.json
	@npdmtool $< $@
-include $(DEPENDS)
endif
