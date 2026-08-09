/// Demo binary for libargs Rust bindings.
/// Run:  cargo run -- -v --output=foo.txt -- file1 file2
use libargs::{Opt, Parser};

fn main() {
    let raw: Vec<String> = std::env::args().collect();

    let opts = vec![
        Opt::bool  ('v', "verbose", "enable verbose output"),
        Opt::bool  ('q', "quiet",   "suppress all output"),
        Opt::string('o', "output",  "FILE", "write output to FILE", Some("a.out")),
        Opt::int   ('n', "count",   "N",    "repeat N times",        Some(1)),
        Opt::float ('r', "ratio",   "F",    "scaling ratio",         Some(1.0)),
    ];

    let result = Parser::new()
        .description("libargs Rust demo — exercises every option type")
        .epilog("Source: https://github.com/you/libargs")
        .parse(&raw, opts)
        .unwrap_or_else(|e| {
            eprintln!("error: {}", e);
            std::process::exit(2);
        });

    println!("=== parsed options ===");
    println!("  verbose  : {}", result.flag("verbose"));
    println!("  quiet    : {}", result.flag("quiet"));
    println!("  output   : {}", result.string("output").unwrap_or("(none)"));
    println!("  count    : {}", result.int("count").unwrap_or(0));
    println!("  ratio    : {}", result.float("ratio").unwrap_or(0.0));

    let pos = result.positionals();
    if pos.is_empty() {
        println!("  positionals: (none)");
    } else {
        println!("  positionals:");
        for p in pos { println!("    {}", p); }
    }

    if result.dashdash_seen() {
        println!("  (-- was present)");
    }
}
