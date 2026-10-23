# Minimal wrapper for ESP-IDF builds.

.PHONY: all build flash monitor clean

all: build

build:
	idf.py build

flash:
	idf.py flash

monitor:
	idf.py monitor

clean:
	idf.py fullclean
