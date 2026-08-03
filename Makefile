ATMOSPHERE_LIBS ?= $(CURDIR)/Atmosphere-libs
SYS_NSP := sysmodule/out/nintendo_nx_arm64_armv8a/release/switch-ha-native.nsp
INSTALLER_NRO := installer/switch-ha-native.nro
DIST_NRO := dist/switch-ha-native.nro

.PHONY: all sysmodule installer clean
all: $(DIST_NRO)
sysmodule:
	$(MAKE) -C sysmodule ATMOSPHERE_LIBS="$(ATMOSPHERE_LIBS)"
$(SYS_NSP):
	$(MAKE) -C sysmodule ATMOSPHERE_LIBS="$(ATMOSPHERE_LIBS)"
installer: $(INSTALLER_NRO)
$(INSTALLER_NRO): $(SYS_NSP) installer/source/main.c installer/source/resources.s installer/romfs/titles.txt installer/assets/icon.jpg
	cp $(SYS_NSP) installer/romfs/switch-ha-native.nsp
	$(MAKE) -C installer
$(DIST_NRO): $(INSTALLER_NRO)
	mkdir -p dist
	cp $(INSTALLER_NRO) $(DIST_NRO)
clean:
	$(MAKE) -C sysmodule clean ATMOSPHERE_LIBS="$(ATMOSPHERE_LIBS)"
	$(MAKE) -C installer clean
	rm -f $(DIST_NRO)
