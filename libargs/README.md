# libargs

[![CI](https://github.com/quantung-libraries/libargs/actions/workflows/ci.yml/badge.svg)](https://github.com/quantung-libraries/libargs/actions/workflows/ci.yml)
[![PyPI](https://img.shields.io/pypi/v/libargs.svg)](https://pypi.org/project/libargs/)
[![Crates.io](https://img.shields.io/crates/v/libargs-rs.svg)](https://crates.io/crates/libargs-rs)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

Unix/GNU-style argument parsing — C library with native bindings for **Rust** and **Python**.

| Syntax | Meaning |
|---|---|
| `-v` | short boolean flag |
| `-xvz` | combined short flags |
| `-fFILE` | short option, value attached |
| `-f FILE` | short option, space-separated |
| `--verbose` | long boolean flag |
| `--file=FILE` | long option with `=` |
| `--file FILE` | long option, space-separated |
| `--no-FLAG` | negate a boolean flag |
| `--` | end of options — rest are positional |
| `-` | stdin placeholder — treated as positional |
| `POSIXLY_CORRECT` env | stop at first non-option |

---

## Build

```bash
git clone https://github.com/quantung-libraries/libargs.git
cd libargs
./build.sh all
```

**Requirements:** `gcc`, `cargo`, `python3-dev`

---

## C

```c
#include "args.h"

int main(int argc, char **argv) {
    args_opt_t opts[] = {
        ARGS_BOOL  ('v', "verbose", "enable verbose output"),
        ARGS_STRING('o', "output",  "FILE", "output file", "a.out"),
        ARGS_INT   ('n', "count",   "N",    "repeat N times", "1"),
        ARGS_OPT_END
    };

    args_ctx_t ctx;
    args_ctx_init(&ctx);
    args_parse(argc, argv, opts, &ctx);

    if (args_find_long(opts, "verbose")->bval)
        printf("verbose mode\n");
    printf("output = %s\n", args_find_long(opts, "output")->value);

    for (int i = 0; i < ctx.npositionals; i++)
        printf("file: %s\n", ctx.positionals[i]);

    args_free(opts, &ctx);
    return 0;
}
```

```bash
gcc myapp.c build/libargs.a -I. -o myapp
```

---

## Rust

```toml
[dependencies]
libargs-rs = "1"
```

```rust
use libargs::{Parser, Opt};

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let opts = vec![
        Opt::bool  ('v', "verbose", "enable verbose output"),
        Opt::string('o', "output",  "FILE", "output file", Some("a.out")),
        Opt::int   ('n', "count",   "N",    "repeat N times", Some(1)),
    ];
    let r = Parser::new().parse(&args, opts).unwrap_or_else(|e| {
        eprintln!("error: {}", e);
        std::process::exit(2);
    });

    if r.flag("verbose") { println!("verbose mode"); }
    println!("output = {}", r.string("output").unwrap_or("(none)"));
    for f in r.positionals() { println!("file: {}", f); }
}
```

See [bindings/rust/README.md](bindings/rust/README.md) for full API.

---

## Python

```bash
pip install libargs
```

```python
import sys, args

opts = [
    args.opt_bool  ('v', 'verbose', 'enable verbose output'),
    args.opt_string('o', 'output',  'FILE', 'output file', default='a.out'),
    args.opt_int   ('n', 'count',   'N',    'repeat N times'),
]

r = args.Parser().parse(sys.argv, opts)

if r.flag('verbose'): print('verbose mode')
print(f"output = {r.string('output')}")
for f in r.positionals(): print(f"file: {f}")
```

See [bindings/python/README.md](bindings/python/README.md) for full API.

---

## Test results

```
C      67/67  (Linux glibc · Linux musl · macOS)
Rust   22/22  + 1 doc-test
Python 35/35  (isolated venv)
```

## build.sh reference

```bash
./build.sh all              # clean → build → test → ask about publish
./build.sh clean            # remove all build artefacts
./build.sh c / rust / python
./build.sh version 1.2.0   # bump version in all files
./build.sh edition 2024     # change Rust edition
./build.sh init             # first-time git setup
./build.sh publish          # tag + push + PyPI + crates.io
```

## License

MIT — see [LICENSE](LICENSE).
