#include <catch2/catch_test_macros.hpp>
#include "libglot/mime/complete_features.h"

using namespace libglot::mime;

TEST_CASE("RFC2231 - Basic parameter continuation", "[mime][rfc2231]") {
    std::vector<std::pair<std::string_view, std::string_view>> params = {
        {"filename*0", "this_is_a_very_long_"},
        {"filename*1", "filename.txt"}
    };

    auto result = RFC2231Parser::parse_continued_parameters(params);

    REQUIRE(result.size() == 1);
    REQUIRE(result.count("filename") == 1);
    REQUIRE(result["filename"].value == "this_is_a_very_long_filename.txt");
    REQUIRE(result["filename"].encoded == false);
}

TEST_CASE("RFC2231 - Three part continuation", "[mime][rfc2231]") {
    std::vector<std::pair<std::string_view, std::string_view>> params = {
        {"title*0", "Introduction to "},
        {"title*1", "Programming in "},
        {"title*2", "C++.pdf"}
    };

    auto result = RFC2231Parser::parse_continued_parameters(params);

    REQUIRE(result.size() == 1);
    REQUIRE(result["title"].value == "Introduction to Programming in C++.pdf");
}

TEST_CASE("RFC2231 - Encoded continuation with charset", "[mime][rfc2231]") {
    std::vector<std::pair<std::string_view, std::string_view>> params = {
        {"filename*0*", "utf-8''Hello%20"},
        {"filename*1*", "World%21.txt"}
    };

    auto result = RFC2231Parser::parse_continued_parameters(params);

    REQUIRE(result.size() == 1);
    REQUIRE(result["filename"].encoded == true);
    REQUIRE(result["filename"].charset == "utf-8");
    REQUIRE(result["filename"].value == "Hello World!.txt");
}

TEST_CASE("RFC2231 - Charset and language in first fragment", "[mime][rfc2231]") {
    std::vector<std::pair<std::string_view, std::string_view>> params = {
        {"title*0*", "iso-8859-1'en'This%20is%20"},
        {"title*1*", "a%20test.doc"}
    };

    auto result = RFC2231Parser::parse_continued_parameters(params);

    REQUIRE(result.size() == 1);
    REQUIRE(result["title"].charset == "iso-8859-1");
    REQUIRE(result["title"].language == "en");
    REQUIRE(result["title"].value == "This is a test.doc");
}

TEST_CASE("RFC2231 - Mixed encoded and non-encoded fragments", "[mime][rfc2231]") {
    std::vector<std::pair<std::string_view, std::string_view>> params = {
        {"name*0*", "utf-8''Test%20"},
        {"name*1", "Document"},
        {"name*2*", "%20File.pdf"}
    };

    auto result = RFC2231Parser::parse_continued_parameters(params);

    REQUIRE(result.size() == 1);
    REQUIRE(result["name"].encoded == true);
}

TEST_CASE("RFC2231 - Out of order fragments", "[mime][rfc2231]") {
    std::vector<std::pair<std::string_view, std::string_view>> params = {
        {"file*2", "end"},
        {"file*0", "begin_"},
        {"file*1", "middle_"}
    };

    auto result = RFC2231Parser::parse_continued_parameters(params);

    REQUIRE(result.size() == 1);
    REQUIRE(result["file"].value == "begin_middle_end");
}

TEST_CASE("RFC2231 - Multiple different parameters", "[mime][rfc2231]") {
    std::vector<std::pair<std::string_view, std::string_view>> params = {
        {"filename*0", "report_"},
        {"filename*1", "2024.pdf"},
        {"title*0", "Annual "},
        {"title*1", "Report"}
    };

    auto result = RFC2231Parser::parse_continued_parameters(params);

    REQUIRE(result.size() == 2);
    REQUIRE(result["filename"].value == "report_2024.pdf");
    REQUIRE(result["title"].value == "Annual Report");
}

TEST_CASE("RFC2231 - Percent encoding special characters", "[mime][rfc2231]") {
    std::vector<std::pair<std::string_view, std::string_view>> params = {
        {"name*0*", "utf-8''%E2%98%85%20Star.txt"}
    };

    auto result = RFC2231Parser::parse_continued_parameters(params);

    REQUIRE(result.size() == 1);
    REQUIRE(result["name"].encoded == true);
    // U+2605 STAR is encoded as %E2%98%85
}

TEST_CASE("RFC2231 - Empty language field", "[mime][rfc2231]") {
    std::vector<std::pair<std::string_view, std::string_view>> params = {
        {"file*0*", "utf-8''document.pdf"}
    };

    auto result = RFC2231Parser::parse_continued_parameters(params);

    REQUIRE(result.size() == 1);
    REQUIRE(result["file"].charset == "utf-8");
    REQUIRE(result["file"].language == "");
}

TEST_CASE("RFC2231 - Single fragment with encoding", "[mime][rfc2231]") {
    std::vector<std::pair<std::string_view, std::string_view>> params = {
        {"filename*0*", "utf-8''test%2Efile.txt"}
    };

    auto result = RFC2231Parser::parse_continued_parameters(params);

    REQUIRE(result.size() == 1);
    REQUIRE(result["filename"].value == "test.file.txt");
}

TEST_CASE("RFC2231 - Long continuation chain", "[mime][rfc2231]") {
    std::vector<std::pair<std::string_view, std::string_view>> params = {
        {"data*0", "part0_"},
        {"data*1", "part1_"},
        {"data*2", "part2_"},
        {"data*3", "part3_"},
        {"data*4", "part4"}
    };

    auto result = RFC2231Parser::parse_continued_parameters(params);

    REQUIRE(result.size() == 1);
    REQUIRE(result["data"].value == "part0_part1_part2_part3_part4");
}

TEST_CASE("RFC2231 - Enron dataset pattern", "[mime][rfc2231][enron]") {
    // Real-world pattern from Enron emails
    std::vector<std::pair<std::string_view, std::string_view>> params = {
        {"name*0*", "us-ascii'en-us'This%20is%20even%20more%20"},
        {"name*1*", "***fun***%20isn't%20it!"}
    };

    auto result = RFC2231Parser::parse_continued_parameters(params);

    REQUIRE(result.size() == 1);
    REQUIRE(result["name"].charset == "us-ascii");
    REQUIRE(result["name"].language == "en-us");
}

TEST_CASE("RFC2231 - Hex encoding case insensitive", "[mime][rfc2231]") {
    std::vector<std::pair<std::string_view, std::string_view>> params = {
        {"file*0*", "utf-8''test%2Fpath%2Ffile.txt"}
    };

    auto result = RFC2231Parser::parse_continued_parameters(params);

    REQUIRE(result.size() == 1);
    REQUIRE(result["file"].value == "test/path/file.txt");
}

TEST_CASE("RFC2231 - No continuations present", "[mime][rfc2231]") {
    std::vector<std::pair<std::string_view, std::string_view>> params = {
        {"filename", "normal.txt"},
        {"charset", "utf-8"}
    };

    auto result = RFC2231Parser::parse_continued_parameters(params);

    REQUIRE(result.size() == 0); // No continued parameters
}

TEST_CASE("RFC2231 - Space encoding", "[mime][rfc2231]") {
    std::vector<std::pair<std::string_view, std::string_view>> params = {
        {"name*0*", "utf-8''My%20Document%20File.docx"}
    };

    auto result = RFC2231Parser::parse_continued_parameters(params);

    REQUIRE(result.size() == 1);
    REQUIRE(result["name"].value == "My Document File.docx");
}
