// Head-to-head MIME parsing speed benchmark: Rust `mail-parser` driver.
// Same protocol as bench_libglot.cpp / bench_python.py: read file, parse,
// walk every header and every part, decode text/html bodies.
use mail_parser::MessageParser;
use std::env;
use std::fs;
use std::io::{BufRead, BufReader};
use std::time::Instant;

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() != 2 {
        eprintln!("usage: mimebench <filelist>");
        std::process::exit(2);
    }
    let file = fs::File::open(&args[1]).expect("open filelist");
    let paths: Vec<String> = BufReader::new(file)
        .lines()
        .map(|l| l.expect("read line"))
        .filter(|l| !l.is_empty())
        .collect();

    let parser = MessageParser::new();
    let mut parsed: u64 = 0;
    let mut failed: u64 = 0;
    let mut sink: u64 = 0;

    let start = Instant::now();
    for path in &paths {
        let raw = match fs::read(path) {
            Ok(b) => b,
            Err(_) => {
                failed += 1;
                continue;
            }
        };
        match parser.parse(&raw) {
            Some(msg) => {
                parsed += 1;
                for h in msg.headers() {
                    sink += h.name.as_str().len() as u64;
                    sink += format!("{:?}", h.value).len() as u64;
                }
                for part in msg.parts.iter() {
                    for h in &part.headers {
                        sink += h.name.as_str().len() as u64;
                        sink += format!("{:?}", h.value).len() as u64;
                    }
                }
                for t in msg.text_bodies() {
                    sink += format!("{:?}", t.body).len() as u64;
                }
                for t in msg.html_bodies() {
                    sink += format!("{:?}", t.body).len() as u64;
                }
            }
            None => failed += 1,
        }
    }
    let elapsed = start.elapsed().as_secs_f64();

    println!(
        "rust mail-parser: {} files, {} parsed, {} failed, {:.3}s, {:.0} msg/s (sink={})",
        paths.len(),
        parsed,
        failed,
        elapsed,
        paths.len() as f64 / elapsed,
        sink
    );
}
