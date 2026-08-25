# ─── lucis Makefile ──────────────────────────────────────────────────────────
BUILD_DIR  := build
BINARY     := $(BUILD_DIR)/lucis
NPROC      := $(shell nproc 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)
BUILD_TYPE ?= Release
SANITIZERS ?= OFF
GENERATOR  ?=
CMAKE_FLAGS ?=
ANTLR_JAR ?= antlr-4.13.2-complete.jar
GRAMMAR_DIR ?= grammar
GENERATED_DIR ?= src/generated

# ccache (reuse of compiled objects) and lld (fast linking) when available
CCACHE_FLAG := $(shell command -v ccache >/dev/null 2>&1 && echo "-DCMAKE_CXX_COMPILER_LAUNCHER=ccache" || echo "")
LLD_FLAG := $(shell (command -v lld >/dev/null 2>&1 || command -v ld.lld >/dev/null 2>&1) && echo "-DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld" || echo "")

.PHONY: all build configure clean rebuild run grammar help install

# Default target
all: build

## configure  — roda cmake (só precisa rodar uma vez)
configure:
	@if [ -n "$(GENERATOR)" ]; then \
		cmake -S . -B $(BUILD_DIR) -G "$(GENERATOR)" \
			-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
			-DLUCIS_ENABLE_SANITIZERS=$(SANITIZERS) \
			$(CCACHE_FLAG) $(LLD_FLAG) \
			$(CMAKE_FLAGS); \
	else \
		cmake -S . -B $(BUILD_DIR) \
			-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
			-DLUCIS_ENABLE_SANITIZERS=$(SANITIZERS) \
			$(CCACHE_FLAG) $(LLD_FLAG) \
			$(CMAKE_FLAGS); \
	fi

## build  — compila o projeto (paralelo)
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

## fuzzer  — compila o fuzzer de mutação
fuzzer:
	@$(MAKE) -C fuzzer build

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

## test-framework  — valida o framework de testes (#[test]) positivo e negativo
test-framework: build
	@echo "== framework: positive suite (expect exit 0) =="
	@./build/lucis test test_framework/main.lc -q || exit 1
	@echo "== framework: negative cases (expect non-zero exit) =="
	@for f in test_framework/negative/*.lc; do \
		./build/lucis test $$f -q >/dev/null 2>&1; \
		if [ $$? -eq 0 ]; then \
			echo "FAIL: expected failure for $$f"; \
			exit 1; \
		fi; \
	done
	@echo "framework tests: OK"

## fuzz-example  — valida o lucis fuzz (deve encontrar o crash induzido)
fuzz-example: build
	@./build/lucis fuzz run fuzz_example/fuzz/example.lc --iters 2000 --seed 7 --timeout 500 -q; \
	code=$$?; \
	if [ $$code -eq 1 ]; then \
		echo "fuzz-example: crash found and minimized — OK"; \
	elif [ $$code -eq 0 ]; then \
		echo "fuzz-example: FAIL — no crash found"; exit 1; \
	else \
		echo "fuzz-example: FAIL — exit $$code"; exit 1; \
	fi

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
