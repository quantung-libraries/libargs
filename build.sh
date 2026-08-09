#!/usr/bin/env bash
# =============================================================================
# build.sh — libargs master build, test and publish script
#
# Usage:
#   ./build.sh                    build + test everything
#   ./build.sh clean              remove ALL build artefacts
#   ./build.sh c                  build + test C only
#   ./build.sh rust               build + test Rust only
#   ./build.sh python             build + test Python only
#   ./build.sh version 1.2.3      bump version in all files
#   ./build.sh publish            interactive: GitHub tag + PyPI + crates.io
#   ./build.sh init               first-time git setup (remote, user, etc.)
# =============================================================================

set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD="$ROOT/build"
RUST_DIR="$ROOT/bindings/rust"
PY_DIR="$ROOT/bindings/python"
VENV="$BUILD/venv"

REPO_URL="https://github.com/quantung-libraries/libargs"
REPO_SSH="git@github.com:quantung-libraries/libargs.git"

# ── colours ──────────────────────────────────────────────────────────────────
if [ -t 1 ]; then
    RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
    CYAN='\033[0;36m'; BOLD='\033[1m'; RESET='\033[0m'
else
    RED=''; GREEN=''; YELLOW=''; CYAN=''; BOLD=''; RESET=''
fi

info()    { echo -e "${CYAN}[libargs]${RESET} $*"; }
success() { echo -e "${GREEN}[  OK  ]${RESET} $*"; }
warn()    { echo -e "${YELLOW}[ WARN ]${RESET} $*"; }
fail()    { echo -e "${RED}[ FAIL ]${RESET} $*"; exit 1; }
section() { echo -e "\n${BOLD}━━━ $* ━━━${RESET}"; }
ask()     { echo -en "${YELLOW}$* ${RESET}"; }

# ── read current version from Cargo.toml (single source of truth) ────────────
get_version() {
    sed -n 's/^version[[:space:]]*=[[:space:]]*"\([0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\)".*/\1/p' "$RUST_DIR/Cargo.toml" | head -1
}

# ── get current Rust edition year ─────────────────────────────────────────────
current_year() { date +%Y; }

# ── clean ─────────────────────────────────────────────────────────────────────
do_clean() {
    section "CLEAN"
    rm -rf "$BUILD"
    rm -f "$ROOT"/*.o "$ROOT"/*.a
    rm -rf "$RUST_DIR/target"
    rm -rf "$PY_DIR/build" "$PY_DIR/dist" "$PY_DIR"/*.so \
           "$PY_DIR"/__pycache__ "$PY_DIR"/*.egg-info "$PY_DIR"/*.pyc
    success "All clean."
}

# ── ASAN detection ────────────────────────────────────────────────────────────
detect_sanitizer() {
    if echo 'int main(){}' | gcc -fsanitize=address,undefined \
            -x c - -o /dev/null 2>/dev/null; then
        SANITIZE="-fsanitize=address,undefined"
        info "ASAN/UBSan: enabled"
    else
        SANITIZE=""
        warn "ASAN/UBSan: not available on this libc — skipped"
    fi
}

# ── C ─────────────────────────────────────────────────────────────────────────
do_c() {
    section "C LIBRARY + TESTS"
    mkdir -p "$BUILD"
    CFLAGS="-std=gnu11 -Wall -Wextra -Wformat=2 -D_GNU_SOURCE -O2 -fPIC"

    info "Compiling args.c + args_help.c ..."
    gcc $CFLAGS -c "$ROOT/args.c"      -o "$BUILD/args.o"
    gcc $CFLAGS -c "$ROOT/args_help.c" -o "$BUILD/args_help.o"

    info "Archiving → build/libargs.a ..."
    ar rcs "$BUILD/libargs.a" "$BUILD/args.o" "$BUILD/args_help.o"

    info "Linking shared → build/libargs.so ..."
    gcc -shared -Wl,-soname,libargs.so.1 \
        "$BUILD/args.o" "$BUILD/args_help.o" \
        -o "$BUILD/libargs.so.1.0.0"
    ln -sf libargs.so.1.0.0 "$BUILD/libargs.so.1"
    ln -sf libargs.so.1     "$BUILD/libargs.so"

    info "Compiling C test binary → build/test_c ..."
    detect_sanitizer
    gcc $CFLAGS -g $SANITIZE \
        "$ROOT/test_args.c" "$BUILD/libargs.a" \
        $SANITIZE -o "$BUILD/test_c"

    info "Running C tests ..."
    "$BUILD/test_c"
    success "C tests passed."
}

# ── Rust ──────────────────────────────────────────────────────────────────────
do_rust() {
    section "RUST LIBRARY + TESTS"
    command -v cargo >/dev/null 2>&1 || fail "cargo not found."

    info "Building Rust (release) ..."
    cd "$RUST_DIR"
    cargo build --release 2>&1

    info "Copying Rust demo binary → build/libargs-demo ..."
    cp "$RUST_DIR/target/release/libargs-demo" "$BUILD/libargs-demo"

    info "Running Rust tests ..."
    cargo test 2>&1
    success "Rust tests passed."

    info "Smoke-test Rust binary ..."
    "$BUILD/libargs-demo" -v --output=test.out --count=3 -- extra args
    success "Rust binary works."
    cd "$ROOT"
}

# ── Python ────────────────────────────────────────────────────────────────────
do_python() {
    section "PYTHON EXTENSION + TESTS"
    command -v python3 >/dev/null 2>&1 || fail "python3 not found."

    info "Creating isolated venv → build/venv/ ..."
    python3 -m venv "$VENV"
    PY="$VENV/bin/python"
    PIP="$VENV/bin/pip"

    info "Upgrading pip/wheel/setuptools ..."
    "$PIP" install --quiet --upgrade pip wheel setuptools

    info "Building + installing Python extension into venv ..."
    cd "$PY_DIR"
    rm -rf libargs.egg-info build/
    "$PIP" install --quiet .

    info "Running Python tests inside venv ..."
    "$PY" "$PY_DIR/test_args.py" 2>&1 | tee "$BUILD/test_python.log"
    grep -q "tests passed" "$BUILD/test_python.log" \
        || fail "Python tests FAILED — see build/test_python.log"
    success "Python tests passed."

    info "Smoke-test Python import ..."
    "$PY" -c "
import args
r = args.Parser().parse(['prog', '-v', '--output=hi.txt', '--', 'file.txt'],
    [args.opt_bool('v','verbose','verbose'),
     args.opt_string('o','output','F','file')])
assert r.flag('verbose')
assert r.string('output') == 'hi.txt'
assert r.positionals() == ['file.txt']
print('  import + smoke test: OK')
"
    success "Python extension works."
    cd "$ROOT"
}

# ── summary ───────────────────────────────────────────────────────────────────
print_summary() {
    local ver; ver=$(get_version)
    section "SUMMARY  (v${ver})"
    echo ""
    echo -e "  ${GREEN}✓${RESET} build/libargs.a        — C static library"
    echo -e "  ${GREEN}✓${RESET} build/libargs.so        — C shared library"
    echo -e "  ${GREEN}✓${RESET} build/test_c            — C test binary     (67 tests)"
    echo -e "  ${GREEN}✓${RESET} build/libargs-demo      — Rust demo binary  (22 tests)"
    echo -e "  ${GREEN}✓${RESET} build/venv/             — Python venv       (35 tests)"
    echo ""
    echo -e "  ${BOLD}Usage:${RESET}"
    echo -e "    C:      gcc myapp.c build/libargs.a -I. -o myapp"
    echo -e "    Rust:   cargo add libargs-rs"
    echo -e "    Python: pip install libargs"
    echo ""
}

# ── version bump ──────────────────────────────────────────────────────────────
do_version() {
    local new_ver="${1:-}"
    local old_ver; old_ver=$(get_version)
    local year; year=$(current_year)

    if [ -z "$new_ver" ]; then
        echo -e "  Current version: ${CYAN}${old_ver}${RESET}"
        ask "  New version (e.g. 1.2.0): "
        read -r new_ver
    fi

    # Validate semver
    if ! echo "$new_ver" | grep -qP '^\d+\.\d+\.\d+$'; then
        fail "Invalid version '$new_ver' — must be X.Y.Z"
    fi

    section "VERSION BUMP  ${old_ver} → ${new_ver}"

    # Cargo.toml — version + edition year
    info "Updating bindings/rust/Cargo.toml ..."
    sed -i "s/^version     *=.*/version     = \"${new_ver}\"/" "$RUST_DIR/Cargo.toml"
    sed -i "s/^edition     *=.*/edition     = \"${year}\"/"    "$RUST_DIR/Cargo.toml"

    # Regenerate Cargo.lock
    cd "$RUST_DIR" && cargo generate-lockfile 2>/dev/null; cd "$ROOT"

    # pyproject.toml
    info "Updating bindings/python/pyproject.toml ..."
    sed -i "s/^version     *=.*/version         = \"${new_ver}\"/" "$PY_DIR/pyproject.toml"

    # setup.py
    info "Updating bindings/python/setup.py ..."
    sed -i "s/version     *=.*/version     = \"${new_ver}\",/" "$PY_DIR/setup.py"

    # LICENSE — year
    info "Updating LICENSE year → ${year} ..."
    sed -i "s/Copyright (c) [0-9]*/Copyright (c) ${year}/" "$ROOT/LICENSE"

    # README badges version
    info "Updating README.md ..."
    sed -i "s/libargs-[0-9]\+\.[0-9]\+\.[0-9]\+/libargs-${new_ver}/g" "$ROOT/README.md"

    echo ""
    success "Version bumped to ${new_ver}  (edition: ${year})"
    echo ""
    echo -e "  Next steps:"
    echo -e "    ${CYAN}git add -A${RESET}"
    echo -e "    ${CYAN}git commit -m \"chore: bump version to ${new_ver}\"${RESET}"
    echo -e "    ${CYAN}./build.sh publish${RESET}"
}

# ── init — first-time git setup ───────────────────────────────────────────────
do_init() {
    section "GIT INIT"

    # Check if already a git repo
    if git rev-parse --git-dir >/dev/null 2>&1; then
        info "Git repo already exists."
    else
        info "Initialising git repo ..."
        git init
    fi

    # Set remote
    local current_remote; current_remote=$(git remote get-url origin 2>/dev/null || echo "")
    if [ -z "$current_remote" ]; then
        info "Adding remote origin → ${REPO_SSH} ..."
        git remote add origin "$REPO_SSH"
    elif [ "$current_remote" != "$REPO_SSH" ]; then
        warn "Remote is '${current_remote}' — updating to '${REPO_SSH}' ..."
        git remote set-url origin "$REPO_SSH"
    else
        info "Remote already correct."
    fi

    # Git user (only if not set)
    if [ -z "$(git config user.name 2>/dev/null || true)" ]; then
        ask "  Git user name: "
        read -r git_name
        git config user.name "$git_name"
    fi
    if [ -z "$(git config user.email 2>/dev/null || true)" ]; then
        ask "  Git email: "
        read -r git_email
        git config user.email "$git_email"
    fi

    # .gitignore already in repo — skip

    info "Staging all files ..."
    git add -A

    local ver; ver=$(get_version)
    if git diff --cached --quiet; then
        info "Nothing to commit."
    else
        git commit -m "chore: initial commit v${ver}"
    fi

    info "Pushing to origin/main ..."
    git branch -M main
    git push -u origin main

    success "Repo initialised and pushed to ${REPO_URL}"
    echo ""
    echo -e "  ${BOLD}Next:${RESET} run ${CYAN}./build.sh publish${RESET} to tag and release."
}

# ── publish ───────────────────────────────────────────────────────────────────
do_publish() {
    section "PUBLISH"
    local ver; ver=$(get_version)
    local tag="v${ver}"
    echo -e "\n  ${BOLD}Version: ${CYAN}${tag}${RESET}\n"

    # ── 1. Commit + tag ────────────────────────────────────────────────────
    echo -e "${BOLD}① Git tag${RESET}"
    git add -A
    if git diff --cached --quiet; then
        info "Nothing to commit."
    else
        git commit -m "release: ${tag}"
    fi

    if git rev-parse "$tag" >/dev/null 2>&1; then
        warn "Tag ${tag} already exists locally."
        ask "  Delete and recreate? [y/N]"
        read -r choice
        if [[ "$choice" =~ ^[Yy]$ ]]; then
            git tag -d "$tag"
            git push origin ":refs/tags/${tag}" 2>/dev/null || true
        else
            info "Keeping existing tag."
        fi
    fi

    if ! git rev-parse "$tag" >/dev/null 2>&1; then
        git tag -a "$tag" -m "Release ${tag}"
        success "Created tag ${tag}"
    fi

    ask "  Push commit + tag to origin? [y/N]"
    read -r choice
    if [[ "$choice" =~ ^[Yy]$ ]]; then
        git push origin main
        git push origin "$tag"
        success "Pushed ${tag} → CI will run tests and publish automatically."
        echo ""
        echo -e "  Watch CI: ${CYAN}${REPO_URL}/actions${RESET}"
        echo ""
        echo -e "  CI will automatically:"
        echo -e "    ${GREEN}✓${RESET} Test on Ubuntu (glibc) + Alpine (musl)"
        echo -e "    ${GREEN}✓${RESET} Publish to PyPI:      https://pypi.org/project/libargs/"
        echo -e "    ${GREEN}✓${RESET} Publish to crates.io: https://crates.io/crates/libargs-rs"
        echo -e "    ${GREEN}✓${RESET} Create GitHub Release: ${REPO_URL}/releases"
    else
        info "Skipped push. Run manually:"
        echo "     git push origin main && git push origin ${tag}"
    fi

    # ── 2. Manual PyPI (optional fallback) ─────────────────────────────────
    echo ""
    echo -e "${BOLD}② Manual PyPI upload (optional — CI does this automatically)${RESET}"
    ask "  Build + upload to PyPI manually now? [y/N]"
    read -r choice
    if [[ "$choice" =~ ^[Yy]$ ]]; then
        [ -x "$VENV/bin/python" ] || fail "Run './build.sh python' first."
        "$VENV/bin/pip" install --quiet build twine
        cd "$PY_DIR"
        rm -rf dist/ libargs.egg-info build/
        "$VENV/bin/python" -m build --sdist
        "$VENV/bin/twine" upload dist/*
        success "Published to PyPI: https://pypi.org/project/libargs/${ver}/"
        cd "$ROOT"
    fi

    # ── 3. Manual crates.io (optional fallback) ────────────────────────────
    echo ""
    echo -e "${BOLD}③ Manual crates.io publish (optional — CI does this automatically)${RESET}"
    ask "  Publish to crates.io manually now? [y/N]"
    read -r choice
    if [[ "$choice" =~ ^[Yy]$ ]]; then
        command -v cargo >/dev/null 2>&1 || fail "cargo not found."
        cd "$RUST_DIR"
        if [ -z "${CARGO_REGISTRY_TOKEN:-}" ]; then
            cargo login
        fi
        cargo publish --no-verify
        success "Published: https://crates.io/crates/libargs-rs"
        cd "$ROOT"
    fi

    section "DONE"
    echo ""
    echo -e "  ${GREEN}libargs ${tag} released!${RESET}"
    echo -e "    GitHub:    ${REPO_URL}/releases/tag/${tag}"
    echo -e "    PyPI:      https://pypi.org/project/libargs/${ver}/"
    echo -e "    crates.io: https://crates.io/crates/libargs-rs/${ver}"
    echo ""
}

# ── dispatch ──────────────────────────────────────────────────────────────────
CMD="${1:-all}"

case "$CMD" in
    clean)        do_clean ;;
    c)            do_c ;;
    rust)         do_rust ;;
    python)       do_python ;;
    version)      do_version "${2:-}" ;;
    init)         do_init ;;
    publish)      do_publish ;;
    all)
        do_clean
        do_c
        do_rust
        do_python
        print_summary
        echo ""
        ask "All tests passed! Publish now? [y/N]"
        read -r pub
        if [[ "$pub" =~ ^[Yy]$ ]]; then
            do_publish
        fi
        ;;
    *)
        echo "Usage: $0 [all|clean|c|rust|python|version X.Y.Z|init|publish]"
        exit 1
        ;;
esac
