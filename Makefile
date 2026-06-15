# ─── lucis Makefile ──────────────────────────────────────────────────────────
BUILD_DIR  := build
BINARY     := $(BUILD_DIR)/lucis
NPROC      := $(shell nproc 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)
BUILD_TYPE ?= Debug
SANITIZERS ?= ON
GENERATOR  ?=
CMAKE_FLAGS ?=
ANTLR_JAR ?= antlr-4.13.2-complete.jar
GRAMMAR_DIR ?= grammar
GENERATED_DIR ?= src/generated

.PHONY: all build configure clean rebuild run grammar help install

# Default target
all: build

## configure  — roda cmake (só precisa rodar uma vez)
configure:
	@if [ -n "$(GENERATOR)" ]; then \
		cmake -S . -B $(BUILD_DIR) -G "$(GENERATOR)" \
			-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
			-DLUCIS_ENABLE_SANITIZERS=$(SANITIZERS) \
			$(CMAKE_FLAGS); \
	else \
		cmake -S . -B $(BUILD_DIR) \
			-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
			-DLUCIS_ENABLE_SANITIZERS=$(SANITIZERS) \
			$(CMAKE_FLAGS); \
	fi

## build  — compila o projeto
build:
	@if [ ! -f "$(BUILD_DIR)/Makefile" ] && [ ! -f "$(BUILD_DIR)/build.ninja" ]; then \
		$(MAKE) configure; \
	fi
	@cmake --build $(BUILD_DIR) --parallel $(NPROC)

## install  — instala o compilador e a stdlib no sistema
install: build
	@cd $(BUILD_DIR) && sudo make install

## rebuild  — limpa e recompila tudo do zero
rebuild: clean configure build

## clean  — remove o diretório de build
clean:
	@rm -rf $(BUILD_DIR) .lucis/build
	@echo "Build directory removed."

## run FILE=<file.tm>  — compila e executa com um arquivo .tm
run: build
ifndef FILE
	@echo "Usage: make run FILE=examples/main.lc [ARGS='...']"
else
	@$(BINARY) $(FILE) $(ARGS)
endif

## grammar  — regenera lexer/parser ANTLR manualmente (não faz parte do build)
grammar:
	@if [ ! -f "$(ANTLR_JAR)" ]; then \
		echo "Missing $(ANTLR_JAR)."; \
		echo "Set ANTLR_JAR=/path/to/antlr-4.13.2-complete.jar or place the jar at project root."; \
		exit 1; \
	fi
	@# Remove stale nested outputs produced by some ANTLR invocations.
	@rm -rf "$(GENERATED_DIR)/grammar" "$(GENERATED_DIR)/Lucis"
	@mkdir -p $(GENERATED_DIR)
	@cd "$(GRAMMAR_DIR)" && \
		java -jar "../$(ANTLR_JAR)" -Dlanguage=Cpp -visitor -no-listener \
			-Xexact-output-dir \
			-o "../$(GENERATED_DIR)" \
			LucisLexer.g4 LucisParser.g4
	@echo "ANTLR generated sources updated in $(GENERATED_DIR)."

test:
	./build/lucis run tests/main.lc -q

## help  — lista os targets disponíveis
help:
	@echo ""
	@echo "  lucis — Makefile targets"
	@echo ""
	@grep -E '^##' Makefile | sed 's/## /  make /g'
	@echo "  make configure BUILD_TYPE=Release SANITIZERS=OFF"
	@echo "  make configure GENERATOR='Ninja' CMAKE_FLAGS='-DCMAKE_PREFIX_PATH=/opt/custom'"
	@echo "  make grammar ANTLR_JAR=antlr-4.13.2-complete.jar"
	@echo "  make test"
	@echo ""
