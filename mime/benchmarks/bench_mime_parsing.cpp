#include <benchmark/benchmark.h>
#include <libglot/mime/parser_extended.h>
#include <libglot/mime/encoding.h>
#include <libglot/mime/charset.h>
#include <libglot/util/arena.h>

using namespace libglot::mime;

// ============================================================================
// Basic MIME Parsing Benchmarks
// ============================================================================

static void BM_ParseSimpleMessage(benchmark::State& state) {
    std::string_view message = R"(Content-Type: text/plain
Subject: Test Message
From: sender@example.com

This is a simple message body.)";

    for (auto _ : state) {
        libglot::Arena arena;
        MimeParserExtended parser(arena, message);
        benchmark::DoNotOptimize(parser.parse_message_multipart());
    }
}
BENCHMARK(BM_ParseSimpleMessage);

// ============================================================================
// Multipart Parsing Benchmarks
// ============================================================================

static void BM_ParseMultipartMessage(benchmark::State& state) {
    std::string_view message = R"(Content-Type: multipart/mixed; boundary="boundary123"

--boundary123
Content-Type: text/plain

Part 1 content
--boundary123
Content-Type: text/html

<p>Part 2 content</p>
--boundary123--
)";

    for (auto _ : state) {
        libglot::Arena arena;
        MimeParserExtended parser(arena, message);
        benchmark::DoNotOptimize(parser.parse_message_multipart());
    }
}
BENCHMARK(BM_ParseMultipartMessage);

static void BM_ParseNestedMultipart(benchmark::State& state) {
    std::string_view message = R"(Content-Type: multipart/mixed; boundary="outer"

--outer
Content-Type: multipart/alternative; boundary="inner"

--inner
Content-Type: text/plain

Plain text
--inner
Content-Type: text/html

<p>HTML</p>
--inner--
--outer
Content-Type: application/pdf

PDF content
--outer--
)";

    for (auto _ : state) {
        libglot::Arena arena;
        MimeParserExtended parser(arena, message);
        benchmark::DoNotOptimize(parser.parse_message_multipart());
    }
}
BENCHMARK(BM_ParseNestedMultipart);

// ============================================================================
// Transfer Encoding Benchmarks
// ============================================================================

static void BM_DecodeBase64_Small(benchmark::State& state) {
    std::string_view encoded = "SGVsbG8gV29ybGQ=";  // "Hello World"

    for (auto _ : state) {
        benchmark::DoNotOptimize(TransferEncoding::decode_base64(encoded));
    }
}
BENCHMARK(BM_DecodeBase64_Small);

static void BM_DecodeBase64_Large(benchmark::State& state) {
    // 1KB of base64-encoded data
    std::string large_encoded;
    for (int i = 0; i < 100; i++) {
        large_encoded += "VGhpcyBpcyBhIGxvbmcgc3RyaW5nIHRoYXQgd2lsbCBiZSBlbmNvZGVkIGluIGJhc2U2NA==";
    }
    std::string_view encoded = large_encoded;

    for (auto _ : state) {
        benchmark::DoNotOptimize(TransferEncoding::decode_base64(encoded));
    }
}
BENCHMARK(BM_DecodeBase64_Large);

static void BM_DecodeQuotedPrintable(benchmark::State& state) {
    std::string_view encoded = "Hello=20World=21=0ANew=20line";

    for (auto _ : state) {
        benchmark::DoNotOptimize(TransferEncoding::decode_quoted_printable(encoded));
    }
}
BENCHMARK(BM_DecodeQuotedPrintable);

static void BM_DecodeRFC2047(benchmark::State& state) {
    std::string_view encoded = "=?UTF-8?B?SGVsbG8gV29ybGQ=?=";

    for (auto _ : state) {
        benchmark::DoNotOptimize(EncodedWordDecoder::decode(encoded));
    }
}
BENCHMARK(BM_DecodeRFC2047);

// ============================================================================
// Charset Conversion Benchmarks
// ============================================================================

static void BM_CharsetConversion_ISO88591(benchmark::State& state) {
    std::string input;
    for (int i = 0; i < 100; i++) {
        input += "Café résumé naïve";  // Characters in extended ASCII range
    }
    std::string_view input_view = input;

    for (auto _ : state) {
        benchmark::DoNotOptimize(
            CharsetConverter::iso88591_to_utf8(input_view)
        );
    }
}
BENCHMARK(BM_CharsetConversion_ISO88591);

static void BM_CharsetConversion_Windows1252(benchmark::State& state) {
    std::string input;
    for (int i = 0; i < 100; i++) {
        input += "Smart quotes — and dashes";
    }
    std::string_view input_view = input;

    for (auto _ : state) {
        benchmark::DoNotOptimize(
            CharsetConverter::windows1252_to_utf8(input_view)
        );
    }
}
BENCHMARK(BM_CharsetConversion_Windows1252);

static void BM_UTF8Validation(benchmark::State& state) {
    std::string utf8_text = "Hello 世界 こんにちは मनोविज्ञान";

    for (auto _ : state) {
        benchmark::DoNotOptimize(
            CharsetConverter::is_valid_utf8(utf8_text)
        );
    }
}
BENCHMARK(BM_UTF8Validation);

// ============================================================================
// Main
// ============================================================================

BENCHMARK_MAIN();
