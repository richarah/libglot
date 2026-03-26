#include <catch2/catch_test_macros.hpp>
#include "libglot/mime/complete_features.h"

using namespace libglot::mime;

TEST_CASE("Boundary Recovery - Detect simple boundary", "[mime][boundary_recovery]") {
    std::string_view body = R"(--boundary123
Content-Type: text/plain

Part 1
--boundary123
Content-Type: text/html

Part 2
--boundary123--)";

    std::string detected = BoundaryRecovery::detect_boundary(body);

    REQUIRE(detected == "boundary123");
}

TEST_CASE("Boundary Recovery - Detect boundary with equals prefix", "[mime][boundary_recovery]") {
    std::string_view body = R"(--=_Part_123456
Content-Type: text/plain

Content here
--=_Part_123456
Content-Type: text/html

More content
--=_Part_123456--)";

    std::string detected = BoundaryRecovery::detect_boundary(body);

    REQUIRE(detected == "=_Part_123456");
}

TEST_CASE("Boundary Recovery - Multiple candidate boundaries", "[mime][boundary_recovery]") {
    std::string_view body = R"(--wrong
--boundary_actual
Part 1
--boundary_actual
Part 2
--boundary_actual--)";

    std::string detected = BoundaryRecovery::detect_boundary(body);

    // Should detect the most common one (appears 3 times vs 1)
    REQUIRE(detected == "boundary_actual");
}

TEST_CASE("Boundary Recovery - Split with recovery on missing final boundary", "[mime][boundary_recovery]") {
    std::string_view body = R"(--boundary
Content-Type: text/plain

Part 1
--boundary
Content-Type: text/html

Part 2 without final boundary)";

    auto parts = BoundaryRecovery::split_with_recovery(body, "boundary");

    REQUIRE(parts.size() == 2);
}

TEST_CASE("Boundary Recovery - Split with proper final boundary", "[mime][boundary_recovery]") {
    std::string_view body = R"(--boundary
Content-Type: text/plain

Part 1
--boundary
Content-Type: text/html

Part 2
--boundary--)";

    auto parts = BoundaryRecovery::split_with_recovery(body, "boundary");

    REQUIRE(parts.size() == 2);
}

TEST_CASE("Boundary Recovery - Boundary with special characters", "[mime][boundary_recovery]") {
    std::string_view body = R"(--_boundary-with_special.chars
Part 1
--_boundary-with_special.chars
Part 2
--_boundary-with_special.chars--)";

    std::string detected = BoundaryRecovery::detect_boundary(body);

    REQUIRE(detected == "_boundary-with_special.chars");
}

TEST_CASE("Boundary Recovery - No boundary present", "[mime][boundary_recovery]") {
    std::string_view body = "Plain text without any boundaries";

    std::string detected = BoundaryRecovery::detect_boundary(body);

    REQUIRE(detected.empty());
}

TEST_CASE("Boundary Recovery - Single boundary occurrence", "[mime][boundary_recovery]") {
    std::string_view body = "--onlyonce\nSome content";

    std::string detected = BoundaryRecovery::detect_boundary(body);

    // Should not detect with only one occurrence
    REQUIRE(detected.empty());
}

TEST_CASE("Boundary Recovery - CRLF line endings", "[mime][boundary_recovery]") {
    std::string_view body = "--boundary\r\nPart 1\r\n--boundary\r\nPart 2\r\n--boundary--";

    auto parts = BoundaryRecovery::split_with_recovery(body, "boundary");

    REQUIRE(parts.size() == 2);
}

TEST_CASE("Boundary Recovery - LF line endings", "[mime][boundary_recovery]") {
    std::string_view body = "--boundary\nPart 1\n--boundary\nPart 2\n--boundary--";

    auto parts = BoundaryRecovery::split_with_recovery(body, "boundary");

    REQUIRE(parts.size() == 2);
}

TEST_CASE("Boundary Recovery - Enron dataset pattern", "[mime][boundary_recovery][enron]") {
    // Common pattern in Enron emails
    std::string_view body = R"(--=_NextPart_000_0001_01C123456.789ABCDE
Content-Type: text/plain

Email content
--=_NextPart_000_0001_01C123456.789ABCDE
Content-Type: text/html

HTML version
--=_NextPart_000_0001_01C123456.789ABCDE--)";

    std::string detected = BoundaryRecovery::detect_boundary(body);

    REQUIRE(detected == "=_NextPart_000_0001_01C123456.789ABCDE");
}

TEST_CASE("Boundary Recovery - Three parts", "[mime][boundary_recovery]") {
    std::string_view body = R"(--multi
Part 1
--multi
Part 2
--multi
Part 3
--multi--)";

    auto parts = BoundaryRecovery::split_with_recovery(body, "multi");

    REQUIRE(parts.size() == 3);
}

TEST_CASE("Boundary Recovery - Empty parts", "[mime][boundary_recovery]") {
    std::string_view body = R"(--boundary

--boundary

--boundary--)";

    auto parts = BoundaryRecovery::split_with_recovery(body, "boundary");

    REQUIRE(parts.size() == 2);
}

TEST_CASE("Boundary Recovery - Boundary in content", "[mime][boundary_recovery]") {
    // Boundary delimiter should only match at line start with --
    std::string_view body = R"(--boundary
Content mentioning boundary in text
--boundary
More content
--boundary--)";

    auto parts = BoundaryRecovery::split_with_recovery(body, "boundary");

    REQUIRE(parts.size() == 2);
}

TEST_CASE("Boundary Recovery - Long boundary string", "[mime][boundary_recovery]") {
    std::string boundary = "verylongboundarystringwithlotsofcharacters1234567890";
    std::string body = "--" + boundary + "\nPart 1\n--" + boundary + "\nPart 2\n--" + boundary + "--";

    std::string detected = BoundaryRecovery::detect_boundary(body);

    REQUIRE(detected == boundary);
}

TEST_CASE("Boundary Recovery - Boundary with digits", "[mime][boundary_recovery]") {
    std::string_view body = R"(--boundary_123_456
Part 1
--boundary_123_456
Part 2
--boundary_123_456--)";

    std::string detected = BoundaryRecovery::detect_boundary(body);

    REQUIRE(detected == "boundary_123_456");
}

TEST_CASE("Boundary Recovery - Whitespace after boundary", "[mime][boundary_recovery]") {
    std::string_view body = "--boundary   \nPart 1\n--boundary   \nPart 2\n--boundary--";

    auto parts = BoundaryRecovery::split_with_recovery(body, "boundary");

    REQUIRE(parts.size() == 2);
}

TEST_CASE("Boundary Recovery - Missing opening boundary", "[mime][boundary_recovery]") {
    std::string_view body = "Part without opening\n--boundary\nPart 2\n--boundary--";

    auto parts = BoundaryRecovery::split_with_recovery(body, "boundary");

    // Should handle gracefully
    REQUIRE(parts.size() >= 1);
}

TEST_CASE("Boundary Recovery - Nested multipart", "[mime][boundary_recovery]") {
    std::string_view body = R"(--outer
--inner
Nested part
--inner--
--outer--)";

    std::string detected = BoundaryRecovery::detect_boundary(body);

    // Both boundaries appear, but outer appears more
    REQUIRE(!detected.empty());
}

TEST_CASE("Boundary Recovery - Malformed final delimiter", "[mime][boundary_recovery]") {
    std::string_view body = R"(--boundary
Part 1
--boundary
Part 2
--boundary-)";  // Only one dash instead of two

    auto parts = BoundaryRecovery::split_with_recovery(body, "boundary");

    REQUIRE(parts.size() == 2);
}
