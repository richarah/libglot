// Head-to-head MIME parsing speed benchmark: libglot driver.
// Reads a filelist, and for each file: reads bytes, parses, walks every
// header and every part recursively, decodes text parts to UTF-8. Times
// the whole read+parse+walk loop. Same protocol as the Python/Java/Rust
// drivers in this directory, so results are comparable.
#include <libglot/mime/mime.h>
#include <libglot/util/arena.h>

#include <chrono>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace libglot::mime;

static volatile size_t g_sink = 0; // prevent the optimizer from eliding the walk

void walk(const Message& msg) {
    for (const auto* h : msg.headers) {
        g_sink += h->field.size() + h->value.size();
    }
    if (const auto* ct = find_header(msg, "Content-Type")) {
        if (ct->value.rfind("text/", 0) == 0) {
            if (auto text = decoded_body_utf8(msg)) {
                g_sink += text->size();
            }
        }
    }
    for (const auto* part : msg.parts) {
        if (part != nullptr) {
            walk(*part);
        }
    }
    if (msg.encapsulated != nullptr) {
        walk(*msg.encapsulated);
    }
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: bench_libglot <filelist>\n");
        return 2;
    }
    std::vector<std::string> paths;
    {
        std::ifstream list(argv[1]);
        std::string line;
        while (std::getline(list, line)) {
            if (!line.empty()) {
                paths.push_back(line);
            }
        }
    }

    size_t parsed = 0, failed = 0;
    auto start = std::chrono::steady_clock::now();
    for (const auto& path : paths) {
        std::ifstream f(path, std::ios::binary);
        std::ostringstream ss;
        ss << f.rdbuf();
        std::string raw = ss.str();

        libglot::Arena arena;
        try {
            auto result = parse_message(arena, raw);
            if (result.message != nullptr) {
                ++parsed;
                walk(*result.message);
            } else {
                ++failed;
            }
        } catch (const std::exception&) {
            ++failed;
        }
    }
    auto end = std::chrono::steady_clock::now();
    double secs = std::chrono::duration<double>(end - start).count();

    std::printf("libglot: %zu files, %zu parsed, %zu failed, %.3fs, %.0f msg/s (sink=%zu)\n",
                paths.size(), parsed, failed, secs, paths.size() / secs, g_sink);
    return 0;
}
