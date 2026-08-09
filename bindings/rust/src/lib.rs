//! libargs — Safe Rust bindings for the libargs C library.
//!
//! # Quick start
//!
//! ```rust
//! use libargs::{Parser, Opt, OptType};
//!
//! fn main() {
//!     let opts = vec![
//!         Opt::bool('v', "verbose", "be verbose"),
//!         Opt::string('o', "output", "FILE", "output file", Some("a.out")),
//!         Opt::int('n', "count",  "N",    "repeat N times", None),
//!     ];
//!
//!     let args: Vec<String> = std::env::args().collect();
//!     let result = Parser::new().parse(&args, opts).unwrap();
//!
//!     if result.flag("verbose") { println!("verbose mode"); }
//!     println!("output = {}", result.string("output").unwrap_or("(none)"));
//!     println!("count  = {}", result.int("count").unwrap_or(0));
//!     for pos in result.positionals() { println!("file: {}", pos); }
//! }
//! ```

use std::collections::HashMap;
use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_double, c_int, c_long};

// ─────────────────────────────────────────────────────────────────────────────
// FFI declarations — mirror args.h exactly
// ─────────────────────────────────────────────────────────────────────────────

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum CArgsType {
    Bool   = 0,
    String = 1,
    Int    = 2,
    Float  = 3,
}

/// Mirror of `args_opt_t` in args.h.
/// All pointer fields are raw C pointers; we manage lifetime carefully.
#[repr(C)]
struct CArgsOpt {
    short_name:    c_int,
    long_name:     *const c_char,
    opt_type:      CArgsType,
    metavar:       *const c_char,
    help:          *const c_char,
    default_value: *const c_char,
    // results
    count:         c_int,
    value:         *const c_char,
    ival:          c_long,
    fval:          c_double,
    bval:          c_int,
    values:        *mut *const c_char,
    nvalues:       usize,
}

/// Mirror of `args_ctx_t` in args.h.
#[repr(C)]
struct CArgsCtx {
    prog:          *const c_char,
    usage_line:    *const c_char,
    description:   *const c_char,
    epilog:        *const c_char,
    exit_on_error: c_int,
    allow_unknown: c_int,
    posix_order:   c_int,
    // results
    positionals:   *mut *mut c_char,
    npositionals:  c_int,
    unknown_opts:  *mut *mut c_char,
    nunknown:      c_int,
    dashdash_seen: c_int,
    // internal
    _opts:         *mut CArgsOpt,
    _nopts:        usize,
}

extern "C" {
    fn args_ctx_init(ctx: *mut CArgsCtx);
    fn args_parse(
        argc: c_int,
        argv: *mut *mut c_char,
        opts: *mut CArgsOpt,
        ctx:  *mut CArgsCtx,
    ) -> c_int;
    fn args_free(opts: *mut CArgsOpt, ctx: *mut CArgsCtx);
}

// ─────────────────────────────────────────────────────────────────────────────
// Safe public types
// ─────────────────────────────────────────────────────────────────────────────

/// Option type for the safe API.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum OptType {
    Bool,
    String,
    Int,
    Float,
}

/// One option definition — passed to [`Parser::parse`].
#[derive(Debug, Clone)]
pub struct Opt {
    pub short_name:    Option<char>,
    pub long_name:     Option<String>,
    pub opt_type:      OptType,
    pub metavar:       Option<String>,
    pub help:          Option<String>,
    pub default_value: Option<String>,
}

impl Opt {
    pub fn bool(short: char, long: &str, help: &str) -> Self {
        Self {
            short_name:    Some(short),
            long_name:     Some(long.to_owned()),
            opt_type:      OptType::Bool,
            metavar:       None,
            help:          Some(help.to_owned()),
            default_value: None,
        }
    }

    pub fn bool_long(long: &str, help: &str) -> Self {
        Self {
            short_name:    None,
            long_name:     Some(long.to_owned()),
            opt_type:      OptType::Bool,
            metavar:       None,
            help:          Some(help.to_owned()),
            default_value: None,
        }
    }

    pub fn string(short: char, long: &str, meta: &str,
                  help: &str, default: Option<&str>) -> Self {
        Self {
            short_name:    Some(short),
            long_name:     Some(long.to_owned()),
            opt_type:      OptType::String,
            metavar:       Some(meta.to_owned()),
            help:          Some(help.to_owned()),
            default_value: default.map(|s| s.to_owned()),
        }
    }

    pub fn int(short: char, long: &str, meta: &str,
               help: &str, default: Option<i64>) -> Self {
        Self {
            short_name:    Some(short),
            long_name:     Some(long.to_owned()),
            opt_type:      OptType::Int,
            metavar:       Some(meta.to_owned()),
            help:          Some(help.to_owned()),
            default_value: default.map(|v| v.to_string()),
        }
    }

    pub fn float(short: char, long: &str, meta: &str,
                 help: &str, default: Option<f64>) -> Self {
        Self {
            short_name:    Some(short),
            long_name:     Some(long.to_owned()),
            opt_type:      OptType::Float,
            metavar:       Some(meta.to_owned()),
            help:          Some(help.to_owned()),
            default_value: default.map(|v| v.to_string()),
        }
    }
}

/// Errors returned by the parser.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum ArgsError {
    UnknownOption(String),
    MissingValue(String),
    BadInt(String),
    BadFloat(String),
    OutOfMemory,
    NulByte(std::ffi::NulError),
}

impl std::fmt::Display for ArgsError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            ArgsError::UnknownOption(s) => write!(f, "unknown option: {}", s),
            ArgsError::MissingValue(s)  => write!(f, "missing value for: {}", s),
            ArgsError::BadInt(s)        => write!(f, "invalid integer: {}", s),
            ArgsError::BadFloat(s)      => write!(f, "invalid float: {}", s),
            ArgsError::OutOfMemory      => write!(f, "out of memory"),
            ArgsError::NulByte(e)       => write!(f, "interior nul byte: {}", e),
        }
    }
}
impl std::error::Error for ArgsError {}

impl From<std::ffi::NulError> for ArgsError {
    fn from(e: std::ffi::NulError) -> Self { ArgsError::NulByte(e) }
}

// ─────────────────────────────────────────────────────────────────────────────
// Parsed result
// ─────────────────────────────────────────────────────────────────────────────

/// The result of a successful parse.  Look up options by long name.
#[derive(Debug)]
pub struct ParseResult {
    flags:       HashMap<String, bool>,
    strings:     HashMap<String, Option<String>>,
    ints:        HashMap<String, Option<i64>>,
    floats:      HashMap<String, Option<f64>>,
    counts:      HashMap<String, usize>,
    all_values:  HashMap<String, Vec<String>>,
    positionals: Vec<String>,
    dashdash:    bool,
}

impl ParseResult {
    /// Was the boolean flag `name` set?
    pub fn flag(&self, name: &str) -> bool {
        *self.flags.get(name).unwrap_or(&false)
    }

    /// How many times was option `name` seen?
    pub fn count(&self, name: &str) -> usize {
        *self.counts.get(name).unwrap_or(&0)
    }

    /// Last string value for `name` (or default).
    pub fn string(&self, name: &str) -> Option<&str> {
        self.strings.get(name)?.as_deref()
    }

    /// Last integer value for `name` (or default).
    pub fn int(&self, name: &str) -> Option<i64> {
        *self.ints.get(name)?
    }

    /// Last float value for `name` (or default).
    pub fn float(&self, name: &str) -> Option<f64> {
        *self.floats.get(name)?
    }

    /// All values seen for `name` (useful for repeated options like `-I`).
    pub fn values(&self, name: &str) -> &[String] {
        self.all_values.get(name).map(|v| v.as_slice()).unwrap_or(&[])
    }

    /// Non-option arguments (after `--` or interspersed).
    pub fn positionals(&self) -> &[String] {
        &self.positionals
    }

    /// Was `--` encountered?
    pub fn dashdash_seen(&self) -> bool {
        self.dashdash
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Parser builder
// ─────────────────────────────────────────────────────────────────────────────

/// Configures and runs the argument parser.
pub struct Parser {
    exit_on_error: bool,
    allow_unknown: bool,
    posix_order:   bool,
    description:   Option<String>,
    epilog:        Option<String>,
    usage_line:    Option<String>,
}

impl Default for Parser {
    fn default() -> Self { Self::new() }
}

impl Parser {
    pub fn new() -> Self {
        Self {
            exit_on_error: false,  // Rust callers get Result, not exit(2)
            allow_unknown: false,
            posix_order:   false,
            description:   None,
            epilog:        None,
            usage_line:    None,
        }
    }

    pub fn exit_on_error(mut self, v: bool) -> Self { self.exit_on_error = v; self }
    pub fn allow_unknown(mut self, v: bool) -> Self { self.allow_unknown = v; self }
    pub fn posix_order  (mut self, v: bool) -> Self { self.posix_order   = v; self }
    pub fn description  (mut self, s: &str) -> Self { self.description = Some(s.to_owned()); self }
    pub fn epilog       (mut self, s: &str) -> Self { self.epilog      = Some(s.to_owned()); self }
    pub fn usage_line   (mut self, s: &str) -> Self { self.usage_line  = Some(s.to_owned()); self }

    /// Parse `args` (typically `std::env::args().collect()`) against `opts`.
    ///
    /// `args[0]` is the program name (as per POSIX convention).
    pub fn parse(
        &self,
        args: &[String],
        opts: Vec<Opt>,
    ) -> Result<ParseResult, ArgsError> {
        // ---- build CString storage (must outlive the C call) ----
        struct CStorage {
            long_names:     Vec<Option<CString>>,
            metavars:       Vec<Option<CString>>,
            helps:          Vec<Option<CString>>,
            defaults:       Vec<Option<CString>>,
            argv_cstrings:  Vec<CString>,
            argv_ptrs:      Vec<*mut c_char>,
        }

        let n = opts.len();
        let mut store = CStorage {
            long_names:    Vec::with_capacity(n),
            metavars:      Vec::with_capacity(n),
            helps:         Vec::with_capacity(n),
            defaults:      Vec::with_capacity(n),
            argv_cstrings: Vec::with_capacity(args.len()),
            argv_ptrs:     Vec::with_capacity(args.len()),
        };

        for arg in args {
            let cs = CString::new(arg.as_str())?;
            store.argv_cstrings.push(cs);
        }
        for cs in &store.argv_cstrings {
            store.argv_ptrs.push(cs.as_ptr() as *mut c_char);
        }

        // ---- build C option table ----
        // Sentinel at end
        let mut c_opts: Vec<CArgsOpt> = Vec::with_capacity(n + 1);

        for opt in &opts {
            let ln = match &opt.long_name {
                Some(s) => { let cs = CString::new(s.as_str())?; store.long_names.push(Some(cs)); store.long_names.last().unwrap().as_ref().unwrap().as_ptr() }
                None    => { store.long_names.push(None); std::ptr::null() }
            };
            let mv = match &opt.metavar {
                Some(s) => { let cs = CString::new(s.as_str())?; store.metavars.push(Some(cs)); store.metavars.last().unwrap().as_ref().unwrap().as_ptr() }
                None    => { store.metavars.push(None); std::ptr::null() }
            };
            let hp = match &opt.help {
                Some(s) => { let cs = CString::new(s.as_str())?; store.helps.push(Some(cs)); store.helps.last().unwrap().as_ref().unwrap().as_ptr() }
                None    => { store.helps.push(None); std::ptr::null() }
            };
            let dv = match &opt.default_value {
                Some(s) => { let cs = CString::new(s.as_str())?; store.defaults.push(Some(cs)); store.defaults.last().unwrap().as_ref().unwrap().as_ptr() }
                None    => { store.defaults.push(None); std::ptr::null() }
            };

            let ctype = match opt.opt_type {
                OptType::Bool   => CArgsType::Bool,
                OptType::String => CArgsType::String,
                OptType::Int    => CArgsType::Int,
                OptType::Float  => CArgsType::Float,
            };
            let sn = opt.short_name.map(|c| c as c_int).unwrap_or(0);

            c_opts.push(CArgsOpt {
                short_name:    sn,
                long_name:     ln,
                opt_type:      ctype,
                metavar:       mv,
                help:          hp,
                default_value: dv,
                count: 0, value: std::ptr::null(), ival: 0,
                fval: 0.0, bval: 0,
                values: std::ptr::null_mut(), nvalues: 0,
            });
        }
        // sentinel
        c_opts.push(CArgsOpt {
            short_name: 0, long_name: std::ptr::null(),
            opt_type: CArgsType::Bool,
            metavar: std::ptr::null(), help: std::ptr::null(),
            default_value: std::ptr::null(),
            count: 0, value: std::ptr::null(), ival: 0, fval: 0.0, bval: 0,
            values: std::ptr::null_mut(), nvalues: 0,
        });

        // ---- build context ----
        let mut ctx: CArgsCtx = unsafe { std::mem::zeroed() };
        unsafe { args_ctx_init(&mut ctx) };
        ctx.exit_on_error = if self.exit_on_error { 1 } else { 0 };
        ctx.allow_unknown = if self.allow_unknown  { 1 } else { 0 };
        ctx.posix_order   = if self.posix_order    { 1 } else { 0 };

        // ---- call the C parser ----
        let rc = unsafe {
            args_parse(
                store.argv_ptrs.len() as c_int,
                store.argv_ptrs.as_mut_ptr(),
                c_opts.as_mut_ptr(),
                &mut ctx,
            )
        };

        // ---- map error codes ----
        // ARGS_OK=0, ERR_UNKNOWN_OPT=-1, ERR_MISSING_VAL=-2,
        // ERR_BAD_INT=-3, ERR_BAD_FLOAT=-4, ERR_OOM=-5
        if rc != 0 {
            unsafe { args_free(c_opts.as_mut_ptr(), &mut ctx) };
            return Err(match rc {
                -1 => ArgsError::UnknownOption(String::new()),
                -2 => ArgsError::MissingValue(String::new()),
                -3 => ArgsError::BadInt(String::new()),
                -4 => ArgsError::BadFloat(String::new()),
                -5 => ArgsError::OutOfMemory,
                _  => ArgsError::UnknownOption(format!("code {}", rc)),
            });
        }

        // ---- harvest results into ParseResult ----
        let mut result = ParseResult {
            flags:       HashMap::new(),
            strings:     HashMap::new(),
            ints:        HashMap::new(),
            floats:      HashMap::new(),
            counts:      HashMap::new(),
            all_values:  HashMap::new(),
            positionals: Vec::new(),
            dashdash:    ctx.dashdash_seen != 0,
        };

        for (i, opt) in opts.iter().enumerate() {
            let co   = &c_opts[i];
            let key  = opt.long_name.clone().unwrap_or_else(|| {
                opt.short_name.map(|c| c.to_string()).unwrap_or_default()
            });

            result.counts.insert(key.clone(), co.count as usize);

            match opt.opt_type {
                OptType::Bool => {
                    result.flags.insert(key, co.bval != 0);
                }
                OptType::String => {
                    let v = if co.value.is_null() {
                        None
                    } else {
                        unsafe { Some(CStr::from_ptr(co.value).to_string_lossy().into_owned()) }
                    };
                    result.strings.insert(key.clone(), v);
                    // multi-values
                    let mut vals = Vec::new();
                    for j in 0..co.nvalues {
                        let p = unsafe { *co.values.add(j) };
                        if !p.is_null() {
                            vals.push(unsafe { CStr::from_ptr(p).to_string_lossy().into_owned() });
                        }
                    }
                    result.all_values.insert(key, vals);
                }
                OptType::Int => {
                    let v = if co.value.is_null() { None } else { Some(co.ival as i64) };
                    result.ints.insert(key, v);
                }
                OptType::Float => {
                    let v = if co.value.is_null() { None } else { Some(co.fval) };
                    result.floats.insert(key, v);
                }
            }
        }

        // positionals
        for i in 0..ctx.npositionals as usize {
            let p = unsafe { *ctx.positionals.add(i) };
            if !p.is_null() {
                result.positionals.push(
                    unsafe { CStr::from_ptr(p).to_string_lossy().into_owned() }
                );
            }
        }

        unsafe { args_free(c_opts.as_mut_ptr(), &mut ctx) };
        Ok(result)
    }
}
