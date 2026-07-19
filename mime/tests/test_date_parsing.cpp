/// ============================================================================
/// Date: Parsing Tests (RFC 5322 Section 3.3, date-time)
/// ============================================================================
///
/// Table-driven coverage of DateTimeParser (complete_features.h) over the
/// standard date-time grammar and the obsolete forms permitted by RFC 5322
/// Section 4.3 (2/3-digit years, named/military time zones), plus its
/// wiring into the parse_message() pipeline via Message::date and the
/// InvalidDateFormat anomaly for values that don't parse.
/// ============================================================================

#include "../../core/include/libglot/util/arena.h"
#include "../include/libglot/mime/mime.h"
#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace libglot::mime;

TEST_CASE("Date: standard form with day-of-week and numeric zone", "[mime][date]") {
    auto d = DateTimeParser::parse("Fri, 21 Nov 1997 09:55:06 -0600");
    REQUIRE(d.valid);
    REQUIRE(d.year == 1997);
    REQUIRE(d.month == 11);
    REQUIRE(d.day == 21);
    REQUIRE(d.hour == 9);
    REQUIRE(d.minute == 55);
    REQUIRE(d.second == 6);
    REQUIRE(d.tz_offset_minutes == -360);
    REQUIRE(!d.tz_unknown);
}

TEST_CASE("Date: without optional day-of-week", "[mime][date]") {
    auto d = DateTimeParser::parse("21 Nov 1997 09:55:06 -0600");
    REQUIRE(d.valid);
    REQUIRE(d.year == 1997);
    REQUIRE(d.day == 21);
}

TEST_CASE("Date: without optional seconds", "[mime][date]") {
    auto d = DateTimeParser::parse("Fri, 21 Nov 1997 09:55 -0600");
    REQUIRE(d.valid);
    REQUIRE(d.hour == 9);
    REQUIRE(d.minute == 55);
    REQUIRE(d.second == 0);
}

TEST_CASE("Date: +0000 is UTC and known", "[mime][date]") {
    auto d = DateTimeParser::parse("Mon, 1 Jan 2001 00:00:00 +0000");
    REQUIRE(d.valid);
    REQUIRE(d.tz_offset_minutes == 0);
    REQUIRE(!d.tz_unknown);
}

TEST_CASE("Date: -0000 is UTC but marked as unknown-origin", "[mime][date]") {
    // RFC 5322 §3.3: "-0000" indicates the date-time was generated on a
    // system that may be in a local zone other than UTC -- i.e. offset info
    // is not reliable, unlike "+0000".
    auto d = DateTimeParser::parse("Mon, 1 Jan 2001 00:00:00 -0000");
    REQUIRE(d.valid);
    REQUIRE(d.tz_offset_minutes == 0);
    REQUIRE(d.tz_unknown);
}

TEST_CASE("Date: obsolete 2-digit year 00-49 maps to 20xx", "[mime][date]") {
    auto d = DateTimeParser::parse("Wed, 5 Jun 24 10:00:00 +0000");
    REQUIRE(d.valid);
    REQUIRE(d.year == 2024);
}

TEST_CASE("Date: obsolete 2-digit year 50-99 maps to 19xx", "[mime][date]") {
    auto d = DateTimeParser::parse("Thu, 5 Jun 85 10:00:00 +0000");
    REQUIRE(d.valid);
    REQUIRE(d.year == 1985);
}

TEST_CASE("Date: obsolete 3-digit year maps to 19xx", "[mime][date]") {
    // RFC 5322 §4.3: any 3-digit year is interpreted as 19xx.
    auto d = DateTimeParser::parse("Thu, 5 Jun 119 10:00:00 +0000");
    REQUIRE(d.valid);
    REQUIRE(d.year == 2019);
}

TEST_CASE("Date: obsolete named zones UT/GMT", "[mime][date]") {
    auto ut = DateTimeParser::parse("Mon, 1 Jan 2001 12:00:00 UT");
    REQUIRE(ut.valid);
    REQUIRE(ut.tz_offset_minutes == 0);
    REQUIRE(!ut.tz_unknown);

    auto gmt = DateTimeParser::parse("Mon, 1 Jan 2001 12:00:00 GMT");
    REQUIRE(gmt.valid);
    REQUIRE(gmt.tz_offset_minutes == 0);
    REQUIRE(!gmt.tz_unknown);
}

TEST_CASE("Date: obsolete North American named zones", "[mime][date]") {
    struct Case {
        const char* zone;
        int expected_offset;
    };
    static constexpr Case cases[] = {
        {"EST", -5 * 60}, {"EDT", -4 * 60}, {"CST", -6 * 60}, {"CDT", -5 * 60},
        {"MST", -7 * 60}, {"MDT", -6 * 60}, {"PST", -8 * 60}, {"PDT", -7 * 60},
    };
    for (const auto& c : cases) {
        std::string value = std::string("Mon, 1 Jan 2001 12:00:00 ") + c.zone;
        auto d = DateTimeParser::parse(value);
        REQUIRE(d.valid);
        REQUIRE(d.tz_offset_minutes == c.expected_offset);
        REQUIRE(!d.tz_unknown);
    }
}

TEST_CASE("Date: military single-letter zones are treated as -0000 (unknown)",
          "[mime][date]") {
    // RFC 5322 §4.3: the 1-character military zones were mis-defined by
    // RFC 822 and are unpredictable, so they SHOULD be treated as "-0000".
    for (char zone : {'A', 'N', 'Y', 'Z'}) {
        std::string value = std::string("Mon, 1 Jan 2001 12:00:00 ") + zone;
        auto d = DateTimeParser::parse(value);
        REQUIRE(d.valid);
        REQUIRE(d.tz_offset_minutes == 0);
        REQUIRE(d.tz_unknown);
    }
}

TEST_CASE("Date: leap second (sec=60) is accepted", "[mime][date]") {
    auto d = DateTimeParser::parse("Tue, 30 Jun 2015 23:59:60 +0000");
    REQUIRE(d.valid);
    REQUIRE(d.second == 60);
}

TEST_CASE("Date: folded date-time (already unfolded upstream) parses",
          "[mime][date][pipeline]") {
    libglot::Arena arena;
    std::string_view source = "Date: Fri, 21 Nov 1997\n"
                              " 09:55:06 -0600\n"
                              "\n"
                              "body\n";

    auto result = parse_message(arena, source);
    REQUIRE(result.message != nullptr);
    REQUIRE(result.message->date != nullptr);
    REQUIRE(result.message->date->year == 1997);
    REQUIRE(result.message->date->tz_offset_minutes == -360);
    REQUIRE(!result.has_anomaly(AnomalyKind::InvalidDateFormat));
}

// ============================================================================
// Invalid dates: never throw, always recorded as an anomaly
// ============================================================================

TEST_CASE("Date: invalid forms do not throw and leave the date unparsed",
          "[mime][date]") {
    static constexpr const char* kInvalid[] = {
        "",
        "not a date at all",
        "32 Jan 2001 10:00:00 +0000",     // day out of range
        "30 Feb 2001 10:00:00 +0000",     // Feb never has 30 days
        "29 Feb 2001 10:00:00 +0000",     // 2001 is not a leap year
        "5 Foo 2001 10:00:00 +0000",      // bad month name
        "5 Jun 2001 25:00:00 +0000",      // hour out of range
        "5 Jun 2001 10:61:00 +0000",      // minute out of range
        "5 Jun 2001 10:00:00",            // missing zone
        "5 Jun 2001 10:00:00 +9999",      // zone minutes out of range
    };
    for (const char* value : kInvalid) {
        auto d = DateTimeParser::parse(value);
        REQUIRE(!d.valid);
    }
}

TEST_CASE("Date: 29 Feb on a leap year is valid", "[mime][date]") {
    auto d = DateTimeParser::parse("29 Feb 2000 10:00:00 +0000");
    REQUIRE(d.valid);
    REQUIRE(d.day == 29);
}

TEST_CASE("Date: invalid Date header records an anomaly, never throws",
          "[mime][date][pipeline]") {
    libglot::Arena arena;
    std::string_view source = "Date: this is not a date\n\nbody\n";

    auto result = parse_message(arena, source);
    REQUIRE(result.message != nullptr);
    REQUIRE(result.message->date == nullptr);
    REQUIRE(result.has_anomaly(AnomalyKind::InvalidDateFormat));
}
