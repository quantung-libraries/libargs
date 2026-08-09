# ============================================================================
# libargs — top-level Makefile
# Compatible with glibc AND musl (Alpine, static builds).
#
# Targets:
#   make            — build everything
#   make test       — build + run all 3 test suites
#   make release    — copy artifacts to build/
#   make clean      — remove all build products
# ============================================================================

CC      = gcc
CFLAGS  = -std=gnu11 -Wall -Wextra -Wformat=2 -D_GNU_SOURCE -O2 -fPIC
ARFLAGS = rcs
SRC     = args.c args_help.c
OBJ     = $(SRC:.c=.o)
BUILD   = build

# ── Auto-detect ASAN support (not available on musl/Alpine) ────────────────
# Try to compile a trivial program with -fsanitize=address.
# If it fails, fall back to plain -g (no sanitizers).
ASAN_TEST := $(shell echo 'int main(){}' | \
    $(CC) -fsanitize=address,undefined -x c - -o /dev/null 2>/dev/null \
    && echo yes || echo no)

ifeq ($(ASAN_TEST),yes)
    SANITIZE = -fsanitize=address,undefined
    $(info [libargs] ASAN/UBSan: enabled)
else
    SANITIZE =
    $(info [libargs] ASAN/UBSan: not available on this libc — skipped)
endif

.PHONY: all test release c-lib c-test rust-test python-test clean

all: c-lib rust-build python-build

# ── C library ──────────────────────────────────────────────────────────────

c-lib: $(BUILD)/libargs.a $(BUILD)/libargs.so

$(BUILD)/libargs.a: $(OBJ)
	@mkdir -p $(BUILD)
	$(AR) $(ARFLAGS) $@ $^

$(BUILD)/libargs.so: $(OBJ)
	@mkdir -p $(BUILD)
	$(CC) -shared -Wl,-soname,libargs.so.1 $^ -o $(BUILD)/libargs.so.1.0.0
	ln -sf libargs.so.1.0.0 $(BUILD)/libargs.so.1
	ln -sf libargs.so.1     $(BUILD)/libargs.so

$(OBJ): %.o: %.c args.h
	$(CC) $(CFLAGS) -c $< -o $@

c-test: c-lib
	$(CC) $(CFLAGS) -g $(SANITIZE) test_args.c \
	    $(BUILD)/libargs.a $(SANITIZE) -o $(BUILD)/test_args
	$(BUILD)/test_args

# ── Rust ───────────────────────────────────────────────────────────────────

rust-build:
	cd bindings/rust && cargo build --release

rust-test:
	cd bindings/rust && cargo test

$(BUILD)/libargs.rlib: rust-build
	cp bindings/rust/target/release/liblibargs.rlib $(BUILD)/libargs.rlib

$(BUILD)/libargs_rust.a: rust-build
	cp bindings/rust/target/release/liblibargs.a $(BUILD)/libargs_rust.a

# ── Python ─────────────────────────────────────────────────────────────────

python-build:
	cd bindings/python && python3 setup.py build_ext --inplace -q

python-test: python-build
	cd bindings/python && python3 test_args.py

# ── Run all tests ──────────────────────────────────────────────────────────

test: c-test rust-test python-test
	@echo ""
	@echo "════════════════════════════════════"
	@echo "  All test suites passed."
	@echo "════════════════════════════════════"

# ── Collect release artifacts ──────────────────────────────────────────────

release: c-lib rust-build python-build \
         $(BUILD)/libargs.rlib $(BUILD)/libargs_rust.a
	cp bindings/python/args.cpython-*.so $(BUILD)/_args_python.so 2>/dev/null || true
	@echo ""
	@echo "Release artifacts in $(BUILD)/:"
	@ls -lh $(BUILD)/

# ── Clean ──────────────────────────────────────────────────────────────────

clean:
	rm -f $(OBJ) $(BUILD)/test_args
	rm -f $(BUILD)/libargs.a $(BUILD)/libargs.so* $(BUILD)/libargs.so.1*
	rm -f $(BUILD)/libargs.rlib $(BUILD)/libargs_rust.a $(BUILD)/_args_python.so
	cd bindings/rust   && cargo clean
	cd bindings/python && rm -rf build *.so __pycache__
