#include <catch2/catch_test_macros.hpp>
#include "libglot/mime/complete_features.h"

using namespace libglot::mime;

TEST_CASE("Address Groups - Single group with one address", "[mime][address_groups]") {
    std::string_view header = "Executives: john@example.com;";
    auto groups = AddressGroupParser::parse(header);

    REQUIRE(groups.size() == 1);
    REQUIRE(groups[0].group_name == "Executives");
    REQUIRE(groups[0].addresses.size() == 1);
    REQUIRE(groups[0].addresses[0] == "john@example.com");
}

TEST_CASE("Address Groups - Single group with multiple addresses", "[mime][address_groups]") {
    std::string_view header = "Team: alice@example.com, bob@example.com, charlie@example.com;";
    auto groups = AddressGroupParser::parse(header);

    REQUIRE(groups.size() == 1);
    REQUIRE(groups[0].group_name == "Team");
    REQUIRE(groups[0].addresses.size() == 3);
    REQUIRE(groups[0].addresses[0] == "alice@example.com");
    REQUIRE(groups[0].addresses[1] == "bob@example.com");
    REQUIRE(groups[0].addresses[2] == "charlie@example.com");
}

TEST_CASE("Address Groups - Multiple groups", "[mime][address_groups]") {
    std::string_view header = "Executives: john@example.com; Staff: jane@example.com, bob@example.com;";
    auto groups = AddressGroupParser::parse(header);

    REQUIRE(groups.size() == 2);
    REQUIRE(groups[0].group_name == "Executives");
    REQUIRE(groups[0].addresses.size() == 1);
    REQUIRE(groups[1].group_name == "Staff");
    REQUIRE(groups[1].addresses.size() == 2);
}

TEST_CASE("Address Groups - Empty group", "[mime][address_groups]") {
    std::string_view header = "EmptyList: ;";
    auto groups = AddressGroupParser::parse(header);

    REQUIRE(groups.size() == 1);
    REQUIRE(groups[0].group_name == "EmptyList");
    REQUIRE(groups[0].addresses.size() == 0);
}

TEST_CASE("Address Groups - Group with display names", "[mime][address_groups]") {
    std::string_view header = "Management: \"John Doe\" <john@example.com>, \"Jane Smith\" <jane@example.com>;";
    auto groups = AddressGroupParser::parse(header);

    REQUIRE(groups.size() == 1);
    REQUIRE(groups[0].group_name == "Management");
    REQUIRE(groups[0].addresses.size() == 2);
}

TEST_CASE("Address Groups - Whitespace handling", "[mime][address_groups]") {
    std::string_view header = "  Team  :  alice@example.com  ,  bob@example.com  ;  ";
    auto groups = AddressGroupParser::parse(header);

    REQUIRE(groups.size() == 1);
    REQUIRE(groups[0].group_name == "Team");
    REQUIRE(groups[0].addresses.size() == 2);
}

TEST_CASE("Address Groups - Group name with spaces", "[mime][address_groups]") {
    std::string_view header = "Executive Team: ceo@example.com, cto@example.com;";
    auto groups = AddressGroupParser::parse(header);

    REQUIRE(groups.size() == 1);
    REQUIRE(groups[0].group_name == "Executive Team");
}

TEST_CASE("Address Groups - No groups present", "[mime][address_groups]") {
    std::string_view header = "john@example.com, jane@example.com";
    auto groups = AddressGroupParser::parse(header);

    REQUIRE(groups.size() == 0);
}

TEST_CASE("Address Groups - RFC 5322 example", "[mime][address_groups]") {
    std::string_view header = "A Group:Ed Jones <c@a.test>,joe@where.test,John <jdoe@one.test>;";
    auto groups = AddressGroupParser::parse(header);

    REQUIRE(groups.size() == 1);
    REQUIRE(groups[0].group_name == "A Group");
    REQUIRE(groups[0].addresses.size() == 3);
}

TEST_CASE("Address Groups - Three groups", "[mime][address_groups]") {
    std::string_view header = "Sales: sales@example.com; Support: support@example.com; Dev: dev@example.com;";
    auto groups = AddressGroupParser::parse(header);

    REQUIRE(groups.size() == 3);
    REQUIRE(groups[0].group_name == "Sales");
    REQUIRE(groups[1].group_name == "Support");
    REQUIRE(groups[2].group_name == "Dev");
}

TEST_CASE("Address Groups - Missing semicolon at end", "[mime][address_groups]") {
    std::string_view header = "Team: alice@example.com, bob@example.com";
    auto groups = AddressGroupParser::parse(header);

    REQUIRE(groups.size() == 1);
    REQUIRE(groups[0].group_name == "Team");
    REQUIRE(groups[0].addresses.size() == 2);
}

TEST_CASE("Address Groups - Single address in group", "[mime][address_groups]") {
    std::string_view header = "CEO: ceo@example.com;";
    auto groups = AddressGroupParser::parse(header);

    REQUIRE(groups.size() == 1);
    REQUIRE(groups[0].group_name == "CEO");
    REQUIRE(groups[0].addresses.size() == 1);
}

TEST_CASE("Address Groups - Complex mixed list", "[mime][address_groups]") {
    std::string_view header = "Recipients: user1@example.com; CC: user2@example.com, user3@example.com;";
    auto groups = AddressGroupParser::parse(header);

    REQUIRE(groups.size() == 2);
    REQUIRE(groups[0].addresses.size() == 1);
    REQUIRE(groups[1].addresses.size() == 2);
}

TEST_CASE("Address Groups - Enron dataset pattern", "[mime][address_groups][enron]") {
    // Common pattern in Enron emails
    std::string_view header = "Legal Department: john.doe@enron.com, jane.smith@enron.com;";
    auto groups = AddressGroupParser::parse(header);

    REQUIRE(groups.size() == 1);
    REQUIRE(groups[0].group_name == "Legal Department");
    REQUIRE(groups[0].addresses.size() == 2);
}

TEST_CASE("Address Groups - Nested angle brackets", "[mime][address_groups]") {
    std::string_view header = "Team: John Doe <john@example.com>, Jane Smith <jane@example.com>;";
    auto groups = AddressGroupParser::parse(header);

    REQUIRE(groups.size() == 1);
    REQUIRE(groups[0].addresses.size() == 2);
}

TEST_CASE("Address Groups - Group with trailing comma", "[mime][address_groups]") {
    std::string_view header = "Team: alice@example.com, bob@example.com,;";
    auto groups = AddressGroupParser::parse(header);

    REQUIRE(groups.size() == 1);
    REQUIRE(groups[0].addresses.size() == 2);
}

TEST_CASE("Address Groups - Long group name", "[mime][address_groups]") {
    std::string_view header = "Very Long Department Name With Spaces: admin@example.com;";
    auto groups = AddressGroupParser::parse(header);

    REQUIRE(groups.size() == 1);
    REQUIRE(groups[0].group_name == "Very Long Department Name With Spaces");
}

TEST_CASE("Address Groups - Group with quoted addresses", "[mime][address_groups]") {
    std::string_view header = "Team: \"user@domain\" <user@example.com>;";
    auto groups = AddressGroupParser::parse(header);

    REQUIRE(groups.size() == 1);
    REQUIRE(groups[0].addresses.size() == 1);
}

TEST_CASE("Address Groups - International group name", "[mime][address_groups]") {
    std::string_view header = "Équipe: member1@example.com, member2@example.com;";
    auto groups = AddressGroupParser::parse(header);

    REQUIRE(groups.size() == 1);
    REQUIRE(groups[0].group_name == "Équipe");
}

TEST_CASE("Address Groups - Consecutive semicolons", "[mime][address_groups]") {
    std::string_view header = "Group1: addr1@example.com;; Group2: addr2@example.com;";
    auto groups = AddressGroupParser::parse(header);

    // Should handle gracefully
    REQUIRE(groups.size() >= 1);
}
