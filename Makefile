# Default to the documented DevKitC-1 N16R8 build, including flash and monitor.
IDF ?= idf.py
BUILD_DIR ?= build/esp32s3
IDF_ARGS = -B "$(BUILD_DIR)" -D IDF_TARGET=esp32s3 \
	-D SDKCONFIG="$(BUILD_DIR)/sdkconfig" \
	-D SDKCONFIG_DEFAULTS=sdkconfig.defaults.esp32s3

.PHONY: all build flash monitor clean

all: build

build:
	$(IDF) $(IDF_ARGS) build

flash:
	$(IDF) $(IDF_ARGS) flash

monitor:
	$(IDF) $(IDF_ARGS) monitor

clean:
	$(IDF) $(IDF_ARGS) fullclean
