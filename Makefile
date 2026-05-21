# ─── lux Makefile ──────────────────────────────────────────────────────────
BUILD_DIR  := build
BINARY     := $(BUILD_DIR)/lux
NPROC      := $(shell nproc 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)
BUILD_TYPE ?= Debug
SANITIZERS ?= ON
GENERATOR  ?=
CMAKE_FLAGS ?=

.PHONY: all build configure clean rebuild run help

# Default target
all: build

## configure  — roda cmake (só precisa rodar uma vez)
configure:
	@if [ -n "$(GENERATOR)" ]; then \
		cmake -S . -B $(BUILD_DIR) -G "$(GENERATOR)" \
			-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
			-DLUX_ENABLE_SANITIZERS=$(SANITIZERS) \
			$(CMAKE_FLAGS); \
	else \
		cmake -S . -B $(BUILD_DIR) \
			-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
			-DLUX_ENABLE_SANITIZERS=$(SANITIZERS) \
			$(CMAKE_FLAGS); \
	fi

## build  — compila o projeto
build:
	@if [ ! -f "$(BUILD_DIR)/Makefile" ] && [ ! -f "$(BUILD_DIR)/build.ninja" ]; then \
		$(MAKE) configure; \
	fi
	@cmake --build $(BUILD_DIR) --parallel $(NPROC)

## rebuild  — limpa e recompila tudo do zero
rebuild: clean configure build

## clean  — remove o diretório de build
clean:
	@rm -rf $(BUILD_DIR)
	@echo "Build directory removed."

## run FILE=<file.tm>  — compila e executa com um arquivo .tm
run: build
ifndef FILE
	@echo "Usage: make run FILE=examples/main.lx [ARGS='...']"
else
	@$(BINARY) $(FILE) $(ARGS)
endif

## help  — lista os targets disponíveis
help:
	@echo ""
	@echo "  lux — Makefile targets"
	@echo ""
	@grep -E '^##' Makefile | sed 's/## /  make /g'
	@echo "  make configure BUILD_TYPE=Release SANITIZERS=OFF"
	@echo "  make configure GENERATOR='Ninja' CMAKE_FLAGS='-DCMAKE_PREFIX_PATH=/opt/custom'"
	@echo ""
