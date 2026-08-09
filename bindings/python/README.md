# libargs — Python bindings

Unix/GNU-style argument parsing as a Python C extension.

```python
import sys, args

opts = [
    args.opt_bool  ('v', 'verbose', 'enable verbose output'),
    args.opt_string('o', 'output',  'FILE', 'output file', default='a.out'),
    args.opt_int   ('n', 'count',   'N',    'repeat N times'),
    args.opt_float ('r', 'ratio',   'F',    'scaling ratio', default='1.0'),
]

r = args.Parser().parse(sys.argv, opts)

print(r.flag('verbose'))        # bool
print(r.string('output'))       # str | None
print(r.int('count'))           # int | None
print(r.positionals())          # list[str]
```

Supports: `-v`, `-xvz`, `-fFILE`, `--flag`, `--opt=VAL`, `--opt VAL`,
`--no-FLAG`, `--` end-of-args, `-` stdin, `POSIXLY_CORRECT`.
