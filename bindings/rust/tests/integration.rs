use libargs::{ArgsError, Opt, Parser};

fn args(extra: &[&str]) -> Vec<String> {
    let mut v = vec!["prog".to_string()];
    v.extend(extra.iter().map(|s| s.to_string()));
    v
}

#[test]
fn test_short_bool() {
    let opts = vec![Opt::bool('v', "verbose", "be verbose")];
    let r = Parser::new().parse(&args(&["-v"]), opts).unwrap();
    assert!(r.flag("verbose"));
    assert_eq!(r.count("verbose"), 1);
}

#[test]
fn test_long_bool() {
    let opts = vec![Opt::bool('v', "verbose", "be verbose")];
    let r = Parser::new().parse(&args(&["--verbose"]), opts).unwrap();
    assert!(r.flag("verbose"));
}

#[test]
fn test_short_value_space() {
    let opts = vec![Opt::string('f', "file", "FILE", "file", None)];
    let r = Parser::new().parse(&args(&["-f", "out.txt"]), opts).unwrap();
    assert_eq!(r.string("file"), Some("out.txt"));
}

#[test]
fn test_long_value_equals() {
    let opts = vec![Opt::string('f', "file", "FILE", "file", None)];
    let r = Parser::new().parse(&args(&["--file=data.csv"]), opts).unwrap();
    assert_eq!(r.string("file"), Some("data.csv"));
}

#[test]
fn test_long_value_space() {
    let opts = vec![Opt::string('f', "file", "FILE", "file", None)];
    let r = Parser::new().parse(&args(&["--file", "data.csv"]), opts).unwrap();
    assert_eq!(r.string("file"), Some("data.csv"));
}

#[test]
fn test_int_decimal() {
    let opts = vec![Opt::int('n', "count", "N", "count", None)];
    let r = Parser::new().parse(&args(&["--count=42"]), opts).unwrap();
    assert_eq!(r.int("count"), Some(42));
}

#[test]
fn test_int_hex() {
    let opts = vec![Opt::int('n', "count", "N", "count", None)];
    let r = Parser::new().parse(&args(&["--count=0xff"]), opts).unwrap();
    assert_eq!(r.int("count"), Some(255));
}

#[test]
fn test_float() {
    let opts = vec![Opt::float('r', "ratio", "F", "ratio", None)];
    let r = Parser::new().parse(&args(&["--ratio=3.14"]), opts).unwrap();
    let v = r.float("ratio").unwrap();
    assert!((v - 3.14).abs() < 1e-4);
}

#[test]
fn test_default_string() {
    let opts = vec![Opt::string('o', "output", "FILE", "output", Some("a.out"))];
    let r = Parser::new().parse(&args(&[]), opts).unwrap();
    assert_eq!(r.string("output"), Some("a.out"));
}

#[test]
fn test_default_int() {
    let opts = vec![Opt::int('n', "count", "N", "count", Some(7))];
    let r = Parser::new().parse(&args(&[]), opts).unwrap();
    assert_eq!(r.int("count"), Some(7));
}

#[test]
fn test_positionals() {
    let opts = vec![Opt::bool('v', "verbose", "verbose")];
    let r = Parser::new().parse(&args(&["-v", "file1.txt", "file2.txt"]), opts).unwrap();
    assert!(r.flag("verbose"));
    assert_eq!(r.positionals(), &["file1.txt", "file2.txt"]);
}

#[test]
fn test_dashdash() {
    let opts = vec![Opt::bool('v', "verbose", "verbose")];
    let r = Parser::new().parse(&args(&["-v", "--", "-f", "notopt"]), opts).unwrap();
    assert!(r.dashdash_seen());
    assert_eq!(r.positionals(), &["-f", "notopt"]);
}

#[test]
fn test_stdin_dash_positional() {
    let opts: Vec<Opt> = vec![];
    let r = Parser::new().parse(&args(&["-"]), opts).unwrap();
    assert_eq!(r.positionals(), &["-"]);
}

#[test]
fn test_combined_short() {
    let opts = vec![
        Opt::bool('x', "extract", "extract"),
        Opt::bool('v', "verbose", "verbose"),
        Opt::bool('z', "gzip",    "gzip"),
    ];
    let r = Parser::new().parse(&args(&["-xvz"]), opts).unwrap();
    assert!(r.flag("extract"));
    assert!(r.flag("verbose"));
    assert!(r.flag("gzip"));
}

#[test]
fn test_repeated_option_count_and_values() {
    let opts = vec![Opt::string('I', "include", "DIR", "include dir", None)];
    let r = Parser::new()
        .parse(&args(&["-I", "inc1", "-I", "inc2", "-I", "inc3"]), opts)
        .unwrap();
    assert_eq!(r.count("include"), 3);
    assert_eq!(r.values("include"), &["inc1", "inc2", "inc3"]);
}

#[test]
fn test_negation() {
    let opts = vec![Opt::bool('v', "verbose", "verbose")];
    let r = Parser::new().parse(&args(&["--verbose", "--no-verbose"]), opts).unwrap();
    assert!(!r.flag("verbose"));
}

#[test]
fn test_posix_order() {
    let opts = vec![Opt::bool('v', "verbose", "verbose")];
    let r = Parser::new()
        .posix_order(true)
        .parse(&args(&["file.txt", "-v"]), opts)
        .unwrap();
    assert!(!r.flag("verbose"));
    assert_eq!(r.positionals().len(), 2);
}

#[test]
fn test_error_unknown_opt() {
    let opts: Vec<Opt> = vec![];
    let err = Parser::new().parse(&args(&["--no-such"]), opts).unwrap_err();
    assert!(matches!(err, ArgsError::UnknownOption(_)));
}

#[test]
fn test_error_missing_value() {
    let opts = vec![Opt::string('f', "file", "FILE", "file", None)];
    let err = Parser::new().parse(&args(&["--file"]), opts).unwrap_err();
    assert!(matches!(err, ArgsError::MissingValue(_)));
}

#[test]
fn test_error_bad_int() {
    let opts = vec![Opt::int('n', "count", "N", "count", None)];
    let err = Parser::new().parse(&args(&["--count=abc"]), opts).unwrap_err();
    assert!(matches!(err, ArgsError::BadInt(_)));
}

#[test]
fn test_short_value_attached() {
    let opts = vec![Opt::string('o', "output", "FILE", "output", None)];
    let r = Parser::new().parse(&args(&["-ofile.txt"]), opts).unwrap();
    assert_eq!(r.string("output"), Some("file.txt"));
}

#[test]
fn test_no_args() {
    let opts: Vec<Opt> = vec![];
    let r = Parser::new().parse(&args(&[]), opts).unwrap();
    assert_eq!(r.positionals().len(), 0);
}
