# Default to the documented DevKitC-1 N16R8 build, including flash and monitor.
# EIM activation may expose idf.py only as a shell function, invisible to make.
# Prefer the exported SDK/venv paths; retain IDF overrides and standard PATH use.
IDF_PYTHON ?= $(if $(IDF_PYTHON_ENV_PATH),$(IDF_PYTHON_ENV_PATH)/bin/python3,python3)
IDF ?= $(if $(IDF_PATH),"$(IDF_PYTHON)" "$(IDF_PATH)/tools/idf.py",idf.py)
BUILD_DIR ?= build/esp32s3
PORT ?=
PORT_ARGS = $(if $(PORT),-p "$(PORT)",)
HARDWARE_BUILD_DIR ?= build/hardware-test
IDF_ARGS = -B "$(BUILD_DIR)" $(PORT_ARGS) -D IDF_TARGET=esp32s3 -D REACTOR_HARDWARE_TEST=OFF \
	-D SDKCONFIG="$(BUILD_DIR)/sdkconfig" \
	-D SDKCONFIG_DEFAULTS=sdkconfig.defaults.esp32s3
HARDWARE_ARGS = -B "$(HARDWARE_BUILD_DIR)" $(PORT_ARGS) -D IDF_TARGET=esp32s3 -D REACTOR_HARDWARE_TEST=ON \
	-D SDKCONFIG="$(HARDWARE_BUILD_DIR)/sdkconfig" \
	-D SDKCONFIG_DEFAULTS="sdkconfig.defaults.esp32s3;sdkconfig.defaults.hardware-test"

.PHONY: all build flash monitor clean hardware-build hardware-flash hardware-monitor check

all: build

build:
	$(IDF) $(IDF_ARGS) build

flash:
	$(IDF) $(IDF_ARGS) flash

monitor:
	$(IDF) $(IDF_ARGS) monitor

clean:
	$(IDF) $(IDF_ARGS) fullclean

hardware-build:
	$(IDF) $(HARDWARE_ARGS) build

hardware-flash:
	$(IDF) $(HARDWARE_ARGS) flash

hardware-monitor:
	$(IDF) $(HARDWARE_ARGS) monitor

check:
	cmake -S src/test -B build/host-tests
	cmake --build build/host-tests
	ctest --test-dir build/host-tests --output-on-failure
