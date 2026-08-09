"""
test_args.py — test suite for the libargs Python extension.

Run with:  python test_args.py
"""

import sys
import traceback
import args   # the C extension

# ─────────────────────────────────────────────────────────────────────────────
# Mini test framework
# ─────────────────────────────────────────────────────────────────────────────

_pass = 0
_fail = 0

def check(expr, label=""):
    global _pass, _fail
    caller = traceback.extract_stack()[-2]
    loc    = f"{caller.filename}:{caller.lineno}"
    if expr:
        _pass += 1
    else:
        _fail += 1
        print(f"FAIL  {loc}  {label or repr(expr)}")

def argv(*extra):
    return ["prog"] + list(extra)

def make_parser(**kw):
    return args.Parser(**kw)

# ─────────────────────────────────────────────────────────────────────────────
# Tests
# ─────────────────────────────────────────────────────────────────────────────

def test_short_bool():
    r = make_parser().parse(argv("-v"), [args.opt_bool("v","verbose","be verbose")])
    check(r.flag("verbose") == True)
    check(r.count("verbose") == 1)

def test_long_bool():
    r = make_parser().parse(argv("--verbose"), [args.opt_bool("v","verbose","be verbose")])
    check(r.flag("verbose") == True)

def test_short_value_space():
    r = make_parser().parse(argv("-f","out.txt"), [args.opt_string("f","file","FILE","output file")])
    check(r.string("file") == "out.txt")

def test_short_value_attached():
    r = make_parser().parse(argv("-fout.txt"), [args.opt_string("f","file","FILE","output")])
    check(r.string("file") == "out.txt")

def test_long_value_equals():
    r = make_parser().parse(argv("--file=data.csv"), [args.opt_string("f","file","FILE","file")])
    check(r.string("file") == "data.csv")

def test_long_value_space():
    r = make_parser().parse(argv("--file","data.csv"), [args.opt_string("f","file","FILE","file")])
    check(r.string("file") == "data.csv")

def test_int_decimal():
    r = make_parser().parse(argv("--count=42"), [args.opt_int("n","count","N","count")])
    check(r.int("count") == 42)

def test_int_hex():
    r = make_parser().parse(argv("--count=0xff"), [args.opt_int("n","count","N","count")])
    check(r.int("count") == 255)

def test_int_octal():
    r = make_parser().parse(argv("--count=0777"), [args.opt_int("n","count","N","count")])
    check(r.int("count") == 511)

def test_float():
    r = make_parser().parse(argv("--ratio=3.14"), [args.opt_float("r","ratio","F","ratio")])
    check(abs(r.float("ratio") - 3.14) < 1e-4)

def test_default_string():
    r = make_parser().parse(argv(), [args.opt_string("o","output","FILE","output",default="a.out")])
    check(r.string("output") == "a.out")

def test_default_int():
    r = make_parser().parse(argv(), [args.opt_int("n","count","N","count",default="7")])
    check(r.int("count") == 7)

def test_default_float():
    r = make_parser().parse(argv(), [args.opt_float("r","ratio","F","ratio",default="2.5")])
    check(abs(r.float("ratio") - 2.5) < 1e-4)

def test_combined_short():
    opts = [
        args.opt_bool("x","extract","extract"),
        args.opt_bool("v","verbose","verbose"),
        args.opt_bool("z","gzip","gzip"),
    ]
    r = make_parser().parse(argv("-xvz"), opts)
    check(r.flag("extract") == True)
    check(r.flag("verbose") == True)
    check(r.flag("gzip") == True)

def test_dashdash():
    opts = [args.opt_bool("v","verbose","verbose")]
    r = make_parser().parse(argv("-v","--","-f","notopt"), opts)
    check(r.dashdash_seen() == True)
    check(r.positionals() == ["-f","notopt"])
    check(r.flag("verbose") == True)

def test_stdin_dash():
    r = make_parser().parse(argv("-"), [])
    check(r.positionals() == ["-"])

def test_positionals():
    opts = [args.opt_bool("v","verbose","verbose")]
    r = make_parser().parse(argv("-v","a.txt","b.txt"), opts)
    check(r.positionals() == ["a.txt","b.txt"])

def test_repeated_multi_values():
    opts = [args.opt_string("I","include","DIR","include dir")]
    r = make_parser().parse(argv("-I","inc1","-I","inc2","-I","inc3"), opts)
    check(r.count("include") == 3)
    check(r.values("include") == ["inc1","inc2","inc3"])

def test_negation():
    opts = [args.opt_bool("v","verbose","verbose")]
    r = make_parser().parse(argv("--verbose","--no-verbose"), opts)
    check(r.flag("verbose") == False)

def test_posix_order():
    opts = [args.opt_bool("v","verbose","verbose")]
    r = args.Parser(posix_order=True).parse(argv("file.txt","-v"), opts)
    check(r.flag("verbose") == False)
    check(len(r.positionals()) == 2)

def test_no_args():
    r = make_parser().parse(argv(), [])
    check(r.positionals() == [])

def test_error_unknown_opt():
    try:
        make_parser().parse(argv("--no-such"), [])
        check(False, "expected ValueError")
    except ValueError:
        check(True)

def test_error_missing_value():
    try:
        make_parser().parse(argv("--file"), [args.opt_string("f","file","FILE","file")])
        check(False, "expected ValueError")
    except ValueError:
        check(True)

def test_error_bad_int():
    try:
        make_parser().parse(argv("--count=abc"), [args.opt_int("n","count","N","count")])
        check(False, "expected ValueError")
    except ValueError:
        check(True)

def test_tar_style():
    # tar -xvf archive.tar -- --not-an-option file.txt
    opts = [
        args.opt_bool("x","extract","extract"),
        args.opt_bool("v","verbose","verbose"),
        args.opt_string("f","file","FILE","archive file"),
    ]
    r = make_parser().parse(
        argv("-xvf","archive.tar","--","--not-an-option","file.txt"), opts)
    check(r.flag("extract") == True)
    check(r.flag("verbose") == True)
    check(r.string("file")  == "archive.tar")
    check(r.positionals()   == ["--not-an-option","file.txt"])

# ─────────────────────────────────────────────────────────────────────────────
# Run
# ─────────────────────────────────────────────────────────────────────────────

tests = [
    test_short_bool, test_long_bool,
    test_short_value_space, test_short_value_attached,
    test_long_value_equals, test_long_value_space,
    test_int_decimal, test_int_hex, test_int_octal,
    test_float,
    test_default_string, test_default_int, test_default_float,
    test_combined_short,
    test_dashdash, test_stdin_dash,
    test_positionals,
    test_repeated_multi_values,
    test_negation, test_posix_order,
    test_no_args,
    test_error_unknown_opt, test_error_missing_value, test_error_bad_int,
    test_tar_style,
]

for t in tests:
    try:
        t()
    except Exception as e:
        _fail += 1
        print(f"EXCEPTION in {t.__name__}: {e}")

total = _pass + _fail
print(f"\n{_pass}/{total} tests passed", end="")
if _fail:
    print(f"  ({_fail} FAILED)")
else:
    print()
sys.exit(1 if _fail else 0)
