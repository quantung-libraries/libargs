"""
setup.py — libargs Python C extension.

pip install creates an isolated build environment and copies the package
source there, but does NOT copy parent directories.  So we need the C
sources to live alongside this file (or be fetched in a custom command).

We copy args.c / args_help.c / args.h into this directory at import time
of setup.py itself — only if they're not already here.
"""
import os
import shutil
from setuptools import setup, Extension

here   = os.path.dirname(os.path.abspath(__file__))
c_root = os.path.normpath(os.path.join(here, "..", ".."))

# Copy C core sources into our directory so pip's isolated build can find them
for fname in ("args.c", "args_help.c", "args.h"):
    src = os.path.join(c_root, fname)
    dst = os.path.join(here, fname)
    if os.path.exists(src) and not os.path.exists(dst):
        shutil.copy2(src, dst)

args_ext = Extension(
    name    = "args",
    sources = [
        "args_module.c",
        "args.c",
        "args_help.c",
    ],
    include_dirs       = ["."],
    define_macros      = [("_GNU_SOURCE", "1")],
    extra_compile_args = ["-std=gnu11", "-Wall", "-O2"],
)

setup(
    name        = "libargs",
    version     = "1.0.1",
    ext_modules = [args_ext],
    package_data = {"": ["args.h"]},
)
