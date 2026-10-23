# =============================================================================
#  bfmax-leakage - build matrix
#
#    make VARIANT=<versione> PROFILE=<profilo> TOOL=<tool>
#
#  VARIANT : vedi variants.mk                 (default: ct_find)
#  PROFILE : release | debug | profile        (default: release)
#  TOOL    : correctness | ctgrind | ttest | bench | mastik | lib
#  CYCLES  : rdtscp | cpucycles               (default: rdtscp)
#
#  Output: build/<VARIANT>-<PROFILE>/
#
#    make matrix TOOL=...   compila il tool per tutte le versioni
#    make list              elenca le versioni
#    make profile           registra con perf il bench della VARIANT
#    make hotspot           apre la registrazione con hotspot
#    make clean
# =============================================================================

include variants.mk

# niente regole implicite di make: tutte le regole sono qui sotto (evita
# tentativi di rigenerare i .d con '%: %.c', es. con make -B)
MAKEFLAGS += --no-builtin-rules
.SUFFIXES:

VARIANT ?= ct_find
PROFILE ?= release
TOOL    ?= correctness
CYCLES  ?= rdtscp

ifeq ($(filter $(VARIANT),$(VARIANTS)),)
$(error VARIANT='$(VARIANT)' non valida. Scegliere tra: $(VARIANTS))
endif

CC ?= gcc

# -march fisso (non native): bench e ctgrind usano lo stesso binario e
# valgrind non supporta AVX-512. x86-64-v3 = AVX2 + BMI1/2 (come -mavx2 -mbmi).
MARCH ?= x86-64-v3

# -g in entrambi i profili: non cambia il codice generato.
ifeq ($(PROFILE),release)
OPT := -O3 -march=$(MARCH) -g
else ifeq ($(PROFILE),debug)
OPT := -O0 -march=$(MARCH) -g3 -fno-omit-frame-pointer -DDEBUG=1
else ifeq ($(PROFILE),profile)
# per perf/hotspot (da docs/profiling.txt): ottimizzato ma senza inline, cosi'
# ogni funzione resta un simbolo separato con il proprio costo, e con frame
# pointer e debug info completi per ricostruire le call chain
OPT := -O3 -march=$(MARCH) -g3 -fno-omit-frame-pointer -fno-inline \
       -fno-optimize-sibling-calls -DSKIP_INLINE=1
else
$(error PROFILE='$(PROFILE)' non valido. Scegliere tra: release debug profile)
endif

DEC := $($(VARIANT).dir)
ifeq ($(DEC),)
$(error VARIANT='$(VARIANT)' senza .dir in variants.mk)
endif
DEC_DIR := src/decoders/$(DEC)

CPPFLAGS := -Iinclude -Isrc/core -I$(DEC_DIR) -Iharness/common \
            -DPROFILE_NAME=\"$(PROFILE)\" -DVARIANT_NAME=$(VARIANT)
ifeq ($(CYCLES),cpucycles)
CPPFLAGS += -DUSE_CPUCYCLES
LDLIBS_CYC := -lcpucycles
endif
CFLAGS := -std=gnu11 -Wall -Wextra -Wno-unused-function $(OPT) -MMD -MP $(EXTRA_CFLAGS)
LDLIBS := -lm $(LDLIBS_CYC)

BUILD := build/$(VARIANT)-$(PROFILE)
OBJ   := $(BUILD)/obj

# --- libreria del decoder: identica per tutti i tool ------------------------
# file comuni del decoder + file della versione, tutti nella sua cartella
LIB_SRC := $(addprefix $(DEC_DIR)/,$($(DEC).common) $($(VARIANT).src))
LIB_OBJ := $(LIB_SRC:%.c=$(OBJ)/%.o)
LIB     := $(BUILD)/libdecoder.a
VFLAGS  := $($(VARIANT).flags)

COMMON_SRC := harness/common/instance.c
COMMON_OBJ := $(COMMON_SRC:%.c=$(OBJ)/%.o)

# --- controllo sorgenti: messaggio esplicito invece di "No rule to make target"
TOOL_SRC_correctness := harness/correctness/test_correctness.c
TOOL_SRC_ctgrind     := harness/ctgrind/test_ctgrind.c
TOOL_SRC_ttest       := harness/dudect/ttest.c
TOOL_SRC_bench       := harness/bench/bench_decoder.c
TOOL_SRC_mastik      := harness/mastik/victim.c
TOOL_SRC_lib         :=
NEEDED_SRC  := $(LIB_SRC) $(if $(filter lib,$(TOOL)),,$(COMMON_SRC)) $(TOOL_SRC_$(TOOL))
MISSING_SRC := $(filter-out $(wildcard $(NEEDED_SRC)),$(NEEDED_SRC))
NOBUILD_GOALS := clean list help matrix hotspot
ifneq ($(MISSING_SRC),)
ifneq ($(filter-out $(NOBUILD_GOALS),$(or $(MAKECMDGOALS),all)),)
$(error Sorgenti mancanti per VARIANT=$(VARIANT) TOOL=$(TOOL): $(MISSING_SRC))
endif
endif

.PHONY: all lib correctness ctgrind ttest bench mastik matrix list clean help profile hotspot
all: $(TOOL)
lib: $(LIB)

$(LIB): $(LIB_OBJ)
	@mkdir -p $(@D)
	$(AR) rcs $@ $^

# i flag della versione valgono solo per la libreria
$(OBJ)/src/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(VFLAGS) $(CFLAGS) -c $< -o $@

$(OBJ)/harness/%.o: harness/%.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# ctgrind: solo l'harness e' compilato con -DCTGRIND (attiva poison/unpoison)
$(OBJ)/ctgrind/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -DCTGRIND $(CFLAGS) -c $< -o $@

# --- tool --------------------------------------------------------------------
correctness: $(BUILD)/correctness
$(BUILD)/correctness: $(OBJ)/harness/correctness/test_correctness.o $(COMMON_OBJ) $(LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

ctgrind: $(BUILD)/ctgrind
$(BUILD)/ctgrind: $(OBJ)/ctgrind/harness/ctgrind/test_ctgrind.o $(COMMON_OBJ) $(LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

ttest: $(BUILD)/ttest
$(BUILD)/ttest: $(OBJ)/harness/dudect/ttest.o $(COMMON_OBJ) $(LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

# bench dei kernel: solo se il decoder ne ha uno (harness/bench/kernels/<dec>.c)
KBENCH_SRC := $(wildcard harness/bench/kernels/$(DEC).c)
bench: $(BUILD)/bench_decoder $(if $(KBENCH_SRC),$(BUILD)/bench_kernels)
$(BUILD)/bench_decoder: $(OBJ)/harness/bench/bench_decoder.o $(COMMON_OBJ) $(LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)
$(BUILD)/bench_kernels: $(OBJ)/harness/bench/kernels/$(DEC).o $(COMMON_OBJ) $(LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

MASTIK_TARGETS := $(BUILD)/victim
ifneq ($(MASTIK_DIR),)
MASTIK_TARGETS += $(BUILD)/spy_l1pp
endif
mastik: $(MASTIK_TARGETS)
ifeq ($(MASTIK_DIR),)
	@echo "Nota: spy_l1pp non compilato, impostare MASTIK_DIR=<prefix di Mastik>"
endif
$(BUILD)/victim: $(OBJ)/harness/mastik/victim.o $(COMMON_OBJ) $(LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)
$(BUILD)/spy_l1pp: harness/mastik/spy_l1pp.c
	@mkdir -p $(@D)
	$(CC) -O2 -I$(MASTIK_DIR)/include $< -o $@ -L$(MASTIK_DIR)/lib -lmastik -lm

# --- matrice completa ---------------------------------------------------------
matrix:
	@for v in $(VARIANTS); do \
	  $(MAKE) --no-print-directory VARIANT=$$v PROFILE=$(PROFILE) TOOL=$(TOOL) || exit 1; \
	done

list:
	@for v in $(VARIANTS); do echo $$v; done

# --- profiling con perf + hotspot -------------------------------------------
#   make VARIANT=plain profile                  compila (PROFILE=profile) e registra
#   make VARIANT=plain hotspot                  apre la registrazione
# Variabili: PROFILE_RUNS (decodifiche), PERF_CPU (core), PERF_PREFIX (es. sudo),
#            PERF, HOTSPOT, PERF_FREQ (default max), PERF_MMAP (buffer, default 16M).
# Senza sudo PERF_MMAP non puo' superare kernel.perf_event_mlock_kb:
#   sudo sysctl -w kernel.perf_event_mlock_kb=17000      (per 16M)
# oppure un buffer piu' piccolo: PERF_MMAP=256K (eventualmente PERF_FREQ piu' bassa)
PERF         ?= perf
HOTSPOT      ?= hotspot
PERF_PREFIX  ?=
PERF_CPU     ?= 0
PERF_FREQ    ?= max
PERF_MMAP    ?= 16M
PROFILE_RUNS ?= 1000
PROF_DIR     := results/profile
PROF_DATA    := $(PROF_DIR)/$(VARIANT).perf.data

profile:
	@$(MAKE) --no-print-directory VARIANT=$(VARIANT) PROFILE=profile TOOL=bench
	@mkdir -p $(PROF_DIR)
	$(PERF_PREFIX) taskset -c $(PERF_CPU) $(PERF) record -F $(PERF_FREQ) \
	    --call-graph dwarf,8192 --sample-cpu --mmap-pages $(PERF_MMAP) -o $(PROF_DATA) \
	    build/$(VARIANT)-profile/bench_decoder $(PROFILE_RUNS)
	@echo "registrazione: $(abspath $(PROF_DATA))  ->  make VARIANT=$(VARIANT) hotspot"

# percorso assoluto: le AppImage (hotspot) possono cambiare directory all'avvio
hotspot:
	@test -s $(PROF_DATA) || { echo "manca o e' vuoto $(PROF_DATA): prima make VARIANT=$(VARIANT) profile"; exit 1; }
	$(PERF_PREFIX) $(HOTSPOT) $(abspath $(PROF_DATA))

clean:
	rm -rf build

help:
	@sed -n '2,20p' Makefile

-include $(shell find build -name '*.d' 2>/dev/null)