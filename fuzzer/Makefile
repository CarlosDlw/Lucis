BUILD_DIR := build-fuzzer
FUZZER_BIN := $(BUILD_DIR)/lucis-fuzzer
NPROC := $(shell nproc 2>/dev/null || echo 4)

.PHONY: all build clean

all: build

build:
	@cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release
	@cmake --build $(BUILD_DIR) -j$(NPROC)

clean:
	@rm -rf $(BUILD_DIR)
