// build.rs — compile the libargs C sources and tell Cargo to link them.
//
// This approach embeds the C compilation into the Rust build so that
// `cargo build` is fully self-contained.  No pre-built .a is required.

use std::path::PathBuf;

fn main() {
    // ---- locate the C source root (two dirs up from bindings/rust/) ----
    let manifest = PathBuf::from(std::env::var("CARGO_MANIFEST_DIR").unwrap());
    let c_root   = manifest.join("../..").canonicalize().unwrap();

    // ---- compile C sources via the `cc` crate emulation using raw commands ----
    // We use std::process::Command directly so we have zero build-dependencies.
    let sources = ["args.c", "args_help.c"];
    let out_dir = PathBuf::from(std::env::var("OUT_DIR").unwrap());

    let mut obj_files: Vec<PathBuf> = Vec::new();

    for src in &sources {
        let src_path = c_root.join(src);
        let obj_path = out_dir.join(src.replace(".c", ".o"));

        let status = std::process::Command::new("gcc")
            .args([
                "-std=gnu11",
                "-Wall",
                "-D_GNU_SOURCE",
                "-O2",
                "-fPIC",
                "-c",
            ])
            .arg(&src_path)
            .arg("-o")
            .arg(&obj_path)
            .status()
            .expect("gcc not found — install build-essential");

        assert!(status.success(), "Failed to compile {}", src);
        obj_files.push(obj_path);
    }

    // ---- archive into libargs_c.a ----
    let lib_path = out_dir.join("libargs_c.a");
    let mut ar = std::process::Command::new("ar");
    ar.arg("rcs").arg(&lib_path);
    for obj in &obj_files { ar.arg(obj); }
    let status = ar.status().expect("ar not found");
    assert!(status.success(), "ar failed");

    // ---- tell Cargo where to find the lib and what to link ----
    println!("cargo:rustc-link-search=native={}", out_dir.display());
    println!("cargo:rustc-link-lib=static=args_c");

    // ---- re-run if C sources change ----
    println!("cargo:rerun-if-changed=../../args.c");
    println!("cargo:rerun-if-changed=../../args_help.c");
    println!("cargo:rerun-if-changed=../../args.h");
}
