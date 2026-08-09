# libargs-rs

[![Crates.io](https://img.shields.io/crates/v/libargs-rs.svg)](https://crates.io/crates/libargs-rs)
[![CI](https://github.com/quantung-libraries/libargs/actions/workflows/ci.yml/badge.svg)](https://github.com/quantung-libraries/libargs/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

Safe Rust bindings for **libargs** — a C library implementing Unix/GNU-style argument parsing.

Supports every convention: `-v`, `-xvz`, `-fFILE`, `--flag`, `--opt=VAL`,
`--opt VAL`, `--no-FLAG`, `--` end-of-args, `-` stdin, `POSIXLY_CORRECT`.

## Install

```toml
[dependencies]
libargs-rs = "1"
```

Requires GCC at build time (compiles the C core via `build.rs` — no pre-built binary needed).

## Quick start

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

## ParseResult API

```rust
r.flag("name")      -> bool           // was boolean flag set?
r.string("name")    -> Option<&str>   // last string value (or default)
r.int("name")       -> Option<i64>    // last integer value
r.float("name")     -> Option<f64>    // last float value
r.count("name")     -> usize          // how many times seen
r.values("name")    -> &[String]      // all occurrences (repeated opts like -I)
r.positionals()     -> &[String]      // non-option arguments
r.dashdash_seen()   -> bool           // was -- present?
```

## Parser options

```rust
Parser::new()
    .exit_on_error(false)  // return Err instead of calling exit(2)
    .allow_unknown(true)   // collect unknown opts instead of failing
    .posix_order(true)     // stop at first non-option (POSIXLY_CORRECT)
    .parse(&args, opts)
```

## Opt constructors

```rust
Opt::bool  (short, long, help)
Opt::bool_long(long, help)                        // no short name
Opt::string(short, long, metavar, help, default)
Opt::int   (short, long, metavar, help, default)
Opt::float (short, long, metavar, help, default)
```

## Supported conventions

| Syntax | Meaning |
|---|---|
| `-v` | short boolean flag |
| `-xvz` | combined short flags |
| `-fFILE` | short option, value attached |
| `-f FILE` | short option, space-separated value |
| `--verbose` | long boolean flag |
| `--file=FILE` | long option, `=` separator |
| `--file FILE` | long option, space-separated value |
| `--no-FLAG` | negate a boolean flag |
| `--` | end of options — rest are positional |
| `-` | stdin placeholder — treated as positional |
| `POSIXLY_CORRECT` env | stop at first non-option |

## License

MIT — see [LICENSE](LICENSE).
