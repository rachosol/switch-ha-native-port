ATMOSPHERE_BUILD_CONFIGS :=
all: nx_release

THIS_MAKEFILE := $(abspath $(lastword $(MAKEFILE_LIST)))
CURRENT_DIRECTORY := $(abspath $(dir $(THIS_MAKEFILE)))

define ATMOSPHERE_ADD_TARGET
ATMOSPHERE_BUILD_CONFIGS += $(strip $1)
$(strip $1):
	@$$(MAKE) -f $(CURRENT_DIRECTORY)/system_module.mk ATMOSPHERE_MAKEFILE_TARGET="$(strip $1)" ATMOSPHERE_BUILD_NAME="$(strip $2)" ATMOSPHERE_BOARD="$(strip $3)" ATMOSPHERE_CPU="$(strip $4)" $(strip $5)
endef

$(eval $(call ATMOSPHERE_ADD_TARGET,nx_release,release,nx-hac-001,arm-cortex-a57,))

clean:
	@$(MAKE) -f $(CURRENT_DIRECTORY)/system_module.mk clean ATMOSPHERE_MAKEFILE_TARGET="nx_release" ATMOSPHERE_BUILD_NAME="release" ATMOSPHERE_BOARD="nx-hac-001" ATMOSPHERE_CPU="arm-cortex-a57"

.PHONY: all clean $(ATMOSPHERE_BUILD_CONFIGS)
