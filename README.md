# libargs

[![CI](https://github.com/quantung-libraries/libargs/actions/workflows/ci.yml/badge.svg)](https://github.com/quantung-libraries/libargs/actions/workflows/ci.yml)
[![PyPI](https://img.shields.io/pypi/v/libargs.svg)](https://pypi.org/project/libargs/)
[![Crates.io](https://img.shields.io/crates/v/libargs-rs.svg)](https://crates.io/crates/libargs-rs)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

Unix/GNU-style argument parsing library written in C, with native bindings for **Rust** and **Python**.

Supports every convention from POSIX and GNU:

| Syntax | Meaning |
|---|---|
| `-v` | short boolean flag |
| `-xvz` | combined short flags |
| `-fFILE` | short option, value attached |
| `-f FILE` | short option, value separated |
| `--verbose` | long boolean flag |
| `--file=FILE` | long option, `=` separator |
| `--file FILE` | long option, space separator |
| `--no-FLAG` | long boolean negation |
| `--` | end of options — everything after is positional |
| `-` | stdin placeholder — treated as positional |
| `POSIXLY_CORRECT` env | stop at first non-option argument |

---

## Build everything

```bash
git clone https://github.com/quantung-libraries/libargs.git
cd libargs
./build.sh all        # build + test C, Rust, Python
```

**Requirements:**

| Tool | Min version | Alpine | Ubuntu/Debian |
|---|---|---|---|
| GCC | 11+ | `apk add gcc musl-dev make` | `apt install build-essential` |
| Rust/Cargo | 1.65+ | `apk add cargo` | `apt install cargo` |
| Python | 3.8+ | `apk add python3-dev py3-pip` | `apt install python3-dev` |

---

## C library

```c
#include "args.h"

int main(int argc, char **argv) {
    args_opt_t opts[] = {
        ARGS_BOOL  ('v', "verbose", "enable verbose output"),
        ARGS_STRING('o', "output",  "FILE", "output file", "a.out"),
        ARGS_INT   ('n', "count",   "N",    "repeat N times", "1"),
        ARGS_FLOAT ('r', "ratio",   "F",    "scaling ratio",  "1.0"),
        ARGS_OPT_END
    };

    args_ctx_t ctx;
    args_ctx_init(&ctx);
    args_parse(argc, argv, opts, &ctx);

    if (args_find_long(opts, "verbose")->bval)
        printf("verbose mode\n");

    printf("output = %s\n", args_find_long(opts, "output")->value);
    printf("count  = %ld\n", args_find_long(opts, "count")->ival);

    for (int i = 0; i < ctx.npositionals; i++)
        printf("file: %s\n", ctx.positionals[i]);

    args_free(opts, &ctx);
    return 0;
}
```

**Link:**
```bash
# static
gcc myapp.c build/libargs.a -I. -o myapp

# shared
gcc myapp.c -Lbuild -largs -I. -o myapp
```

**API summary:**

```c
void       args_ctx_init   (args_ctx_t *ctx);
args_err_t args_parse      (int argc, char **argv, args_opt_t *opts, args_ctx_t *ctx);
args_opt_t *args_find_short(args_opt_t *opts, int short_name);
args_opt_t *args_find_long (args_opt_t *opts, const char *long_name);
void       args_print_help (FILE *fp, const args_ctx_t *ctx, const args_opt_t *opts);
void       args_free       (args_opt_t *opts, args_ctx_t *ctx);
void       args_error      (const args_ctx_t *ctx, const char *fmt, ...);
```

---

## Rust

Add to `Cargo.toml`:
```toml
[dependencies]
libargs = "1.0"
```

```rust
use libargs::{Parser, Opt};

fn main() {
    let args: Vec<String> = std::env::args().collect();

    let opts = vec![
        Opt::bool  ('v', "verbose", "enable verbose output"),
        Opt::string('o', "output",  "FILE", "output file",    Some("a.out")),
        Opt::int   ('n', "count",   "N",    "repeat N times", Some(1)),
        Opt::float ('r', "ratio",   "F",    "scaling ratio",  Some(1.0)),
    ];

    let r = Parser::new().parse(&args, opts).unwrap_or_else(|e| {
        eprintln!("error: {}", e);
        std::process::exit(2);
    });

    if r.flag("verbose") { println!("verbose mode"); }
    println!("output = {}", r.string("output").unwrap_or("(none)"));
    println!("count  = {}", r.int("count").unwrap_or(0));

    for f in r.positionals() { println!("file: {}", f); }
}
```

**ParseResult API:**
```rust
r.flag("name")        -> bool
r.string("name")      -> Option<&str>
r.int("name")         -> Option<i64>
r.float("name")       -> Option<f64>
r.count("name")       -> usize          // how many times seen
r.values("name")      -> &[String]      // all occurrences (repeated opts)
r.positionals()       -> &[String]
r.dashdash_seen()     -> bool
```

---

## Python

```bash
pip install libargs
```

```python
import sys
import args

opts = [
    args.opt_bool  ('v', 'verbose', 'enable verbose output'),
    args.opt_string('o', 'output',  'FILE', 'output file',    default='a.out'),
    args.opt_int   ('n', 'count',   'N',    'repeat N times'),
    args.opt_float ('r', 'ratio',   'F',    'scaling ratio',  default='1.0'),
]

r = args.Parser().parse(sys.argv, opts)

if r.flag('verbose'):
    print('verbose mode')

print(f"output = {r.string('output')}")
print(f"count  = {r.int('count')}")
print(f"files  = {r.positionals()}")
```

**ParseResult API:**
```python
r.flag('name')         # bool
r.string('name')       # str | None
r.int('name')          # int | None
r.float('name')        # float | None
r.count('name')        # int  — how many times seen
r.values('name')       # list[str] — all occurrences
r.positionals()        # list[str]
r.dashdash_seen()      # bool

args.Parser(
    exit_on_error=False,   # raise ValueError instead of exit(2)
    allow_unknown=True,    # collect unknown opts instead of failing
    posix_order=True,      # stop at first non-option (like POSIXLY_CORRECT)
)
```

---

## Test results

```
C      67/67  tests passed  (musl + glibc, ASAN where available)
Rust   22/22  tests passed  + 1 doc-test
Python 35/35  tests passed  (isolated venv)
```

---

## Build script reference

```bash
./build.sh all       # clean → build → test → ask about publish
./build.sh clean     # remove ALL build artefacts
./build.sh c         # C only
./build.sh rust      # Rust only
./build.sh python    # Python only (isolated venv)
./build.sh publish   # publish to PyPI + crates.io
```

---

## License

MIT — see [LICENSE](LICENSE).
