#include <catch2/catch_test_macros.hpp>
#include "libglot/mime/complete_features.h"

using namespace libglot::mime;

TEST_CASE("Header Comments - Basic comment removal", "[mime][comments]") {
    std::string_view header = "From: John Doe (CEO) <john@example.com>";
    std::string result = HeaderCommentParser::remove_comments(header);

    REQUIRE(result == "From: John Doe  <john@example.com>");
}

TEST_CASE("Header Comments - Multiple comments", "[mime][comments]") {
    std::string_view header = "To: Jane (VP) <jane@example.com>, Bob (CTO) <bob@example.com>";
    std::string result = HeaderCommentParser::remove_comments(header);

    REQUIRE(result == "To: Jane  <jane@example.com>, Bob  <bob@example.com>");
}

TEST_CASE("Header Comments - Nested comments", "[mime][comments]") {
    std::string_view header = "Subject: Report (Q1 (January-March)) Results";
    std::string result = HeaderCommentParser::remove_comments(header);

    REQUIRE(result == "Subject: Report  Results");
}

TEST_CASE("Header Comments - Extract single comment", "[mime][comments]") {
    std::string_view header = "From: John Doe (CEO) <john@example.com>";
    auto comments = HeaderCommentParser::extract_comments(header);

    REQUIRE(comments.size() == 1);
    REQUIRE(comments[0] == "CEO");
}

TEST_CASE("Header Comments - Extract multiple comments", "[mime][comments]") {
    std::string_view header = "To: Jane (VP) <jane@example.com>, Bob (CTO) <bob@example.com>";
    auto comments = HeaderCommentParser::extract_comments(header);

    REQUIRE(comments.size() == 2);
    REQUIRE(comments[0] == "VP");
    REQUIRE(comments[1] == "CTO");
}

TEST_CASE("Header Comments - Comment with escaped parenthesis", "[mime][comments]") {
    std::string_view header = "Subject: Test (with \\(escaped\\) parens)";
    auto comments = HeaderCommentParser::extract_comments(header);

    REQUIRE(comments.size() == 1);
    REQUIRE(comments[0] == "with \\(escaped\\) parens");
}

TEST_CASE("Header Comments - No comments present", "[mime][comments]") {
    std::string_view header = "From: john@example.com";
    std::string result = HeaderCommentParser::remove_comments(header);

    REQUIRE(result == "From: john@example.com");

    auto comments = HeaderCommentParser::extract_comments(header);
    REQUIRE(comments.size() == 0);
}

TEST_CASE("Header Comments - Comment at beginning", "[mime][comments]") {
    std::string_view header = "(Important) From: sender@example.com";
    auto comments = HeaderCommentParser::extract_comments(header);

    REQUIRE(comments.size() == 1);
    REQUIRE(comments[0] == "Important");
}

TEST_CASE("Header Comments - Comment at end", "[mime][comments]") {
    std::string_view header = "To: recipient@example.com (urgent)";
    auto comments = HeaderCommentParser::extract_comments(header);

    REQUIRE(comments.size() == 1);
    REQUIRE(comments[0] == "urgent");
}

TEST_CASE("Header Comments - Quoted string with parenthesis", "[mime][comments]") {
    std::string_view header = "Subject: \"This (is) quoted\" (this is comment)";
    std::string result = HeaderCommentParser::remove_comments(header);

    REQUIRE(result == "Subject: \"This (is) quoted\" ");

    auto comments = HeaderCommentParser::extract_comments(header);
    REQUIRE(comments.size() == 1);
    REQUIRE(comments[0] == "this is comment");
}

TEST_CASE("Header Comments - Escaped quote inside comment", "[mime][comments]") {
    std::string_view header = "From: sender@example.com (Note: \\\"quoted\\\")";
    auto comments = HeaderCommentParser::extract_comments(header);

    REQUIRE(comments.size() == 1);
    REQUIRE(comments[0] == "Note: \\\"quoted\\\"");
}

TEST_CASE("Header Comments - Empty comment", "[mime][comments]") {
    std::string_view header = "From: sender@example.com ()";
    auto comments = HeaderCommentParser::extract_comments(header);

    REQUIRE(comments.size() == 1);
    REQUIRE(comments[0] == "");
}

TEST_CASE("Header Comments - Deep nesting", "[mime][comments]") {
    std::string_view header = "Subject: Test (level1 (level2 (level3)))";
    std::string result = HeaderCommentParser::remove_comments(header);

    REQUIRE(result == "Subject: Test ");
}

TEST_CASE("Header Comments - RFC 5322 example", "[mime][comments]") {
    std::string_view header = "From: Pete(A nice \\) chap) <pete(his account)@silly.test(his host)>";
    auto comments = HeaderCommentParser::extract_comments(header);

    REQUIRE(comments.size() == 3);
    REQUIRE(comments[0] == "A nice \\) chap");
    REQUIRE(comments[1] == "his account");
    REQUIRE(comments[2] == "his host");
}

TEST_CASE("Header Comments - Enron dataset pattern", "[mime][comments][enron]") {
    // Common pattern in Enron emails
    std::string_view header = "From: Smith, John (Legal) <john.smith@enron.com>";
    std::string result = HeaderCommentParser::remove_comments(header);

    REQUIRE(result == "From: Smith, John  <john.smith@enron.com>");

    auto comments = HeaderCommentParser::extract_comments(header);
    REQUIRE(comments.size() == 1);
    REQUIRE(comments[0] == "Legal");
}

TEST_CASE("Header Comments - Multiple lines with comments", "[mime][comments]") {
    std::string_view header = "Received: from mail.example.com (mail.example.com [192.0.2.1])\n"
                              "    by server.test.com (Postfix) with ESMTP";
    auto comments = HeaderCommentParser::extract_comments(header);

    REQUIRE(comments.size() == 2);
    REQUIRE(comments[0] == "mail.example.com [192.0.2.1]");
    REQUIRE(comments[1] == "Postfix");
}

TEST_CASE("Header Comments - Whitespace in comments", "[mime][comments]") {
    std::string_view header = "From: sender@example.com (  lots   of   spaces  )";
    auto comments = HeaderCommentParser::extract_comments(header);

    REQUIRE(comments.size() == 1);
    REQUIRE(comments[0] == "  lots   of   spaces  ");
}

TEST_CASE("Header Comments - Special characters in comment", "[mime][comments]") {
    std::string_view header = "Subject: Test (Contains: @#$%^&*)";
    auto comments = HeaderCommentParser::extract_comments(header);

    REQUIRE(comments.size() == 1);
    REQUIRE(comments[0] == "Contains: @#$%^&*");
}

TEST_CASE("Header Comments - Backslash at end of comment", "[mime][comments]") {
    std::string_view header = "From: sender@example.com (ends with \\\\)";
    auto comments = HeaderCommentParser::extract_comments(header);

    REQUIRE(comments.size() == 1);
    REQUIRE(comments[0] == "ends with \\\\");
}

TEST_CASE("Header Comments - Adjacent comments", "[mime][comments]") {
    std::string_view header = "From: sender(comment1)(comment2)@example.com";
    auto comments = HeaderCommentParser::extract_comments(header);

    REQUIRE(comments.size() == 2);
    REQUIRE(comments[0] == "comment1");
    REQUIRE(comments[1] == "comment2");
}
