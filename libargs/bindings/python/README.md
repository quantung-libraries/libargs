# libargs

[![PyPI](https://img.shields.io/pypi/v/libargs.svg)](https://pypi.org/project/libargs/)
[![CI](https://github.com/quantung-libraries/libargs/actions/workflows/ci.yml/badge.svg)](https://github.com/quantung-libraries/libargs/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](https://github.com/quantung-libraries/libargs/blob/main/LICENSE)

Python bindings for **libargs** — Unix/GNU-style argument parsing as a fast C extension.

Supports: `-v`, `-xvz`, `-fFILE`, `--flag`, `--opt=VAL`, `--opt VAL`,
`--no-FLAG`, `--` end-of-args, `-` stdin, `POSIXLY_CORRECT`.

## Install

```bash
pip install libargs
```

Requires a C compiler (`gcc`) at install time.

## Quick start

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

for f in r.positionals():
    print(f"file: {f}")
```

## ParseResult API

```python
r.flag('name')        # bool         — was flag set?
r.string('name')      # str | None   — last value (or default)
r.int('name')         # int | None   — parsed integer
r.float('name')       # float | None — parsed float
r.count('name')       # int          — how many times seen
r.values('name')      # list[str]    — all occurrences (repeated opts)
r.positionals()       # list[str]    — non-option arguments
r.dashdash_seen()     # bool         — was -- present?
```

## Option constructors

```python
args.opt_bool  (short, long, help)
args.opt_string(short, long, metavar, help, default=None)
args.opt_int   (short, long, metavar, help, default=None)
args.opt_float (short, long, metavar, help, default=None)
```

## Parser options

```python
args.Parser(
    exit_on_error=False,  # raise ValueError instead of exit(2)
    allow_unknown=True,   # collect unknown opts instead of failing
    posix_order=True,     # stop at first non-option (POSIXLY_CORRECT)
)
```

## Supported conventions

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
| `--` | end of options |
| `-` | stdin placeholder |

## License

MIT — see [LICENSE](https://github.com/quantung-libraries/libargs/blob/main/LICENSE).
