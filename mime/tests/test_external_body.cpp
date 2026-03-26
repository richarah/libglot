#include <catch2/catch_test_macros.hpp>
#include "libglot/mime/complete_features.h"

using namespace libglot::mime;

TEST_CASE("External Body - FTP access type", "[mime][external_body]") {
    std::vector<std::pair<std::string_view, std::string_view>> params = {
        {"access-type", "ftp"},
        {"name", "document.pdf"},
        {"site", "ftp.example.com"},
        {"directory", "/pub/files"}
    };

    auto ref = ExternalBodyParser::parse(params);

    REQUIRE(ref.access_type == "ftp");
    REQUIRE(ref.name == "document.pdf");
    REQUIRE(ref.site == "ftp.example.com");
    REQUIRE(ref.directory == "/pub/files");
}

TEST_CASE("External Body - HTTP access type", "[mime][external_body]") {
    std::vector<std::pair<std::string_view, std::string_view>> params = {
        {"access-type", "http"},
        {"name", "image.jpg"},
        {"site", "www.example.com"}
    };

    auto ref = ExternalBodyParser::parse(params);

    REQUIRE(ref.access_type == "http");
    REQUIRE(ref.name == "image.jpg");
    REQUIRE(ref.site == "www.example.com");
}

TEST_CASE("External Body - Local file access", "[mime][external_body]") {
    std::vector<std::pair<std::string_view, std::string_view>> params = {
        {"access-type", "local-file"},
        {"name", "report.docx"},
        {"directory", "/home/user/documents"}
    };

    auto ref = ExternalBodyParser::parse(params);

    REQUIRE(ref.access_type == "local-file");
    REQUIRE(ref.name == "report.docx");
    REQUIRE(ref.directory == "/home/user/documents");
}

TEST_CASE("External Body - Mail server access", "[mime][external_body]") {
    std::vector<std::pair<std::string_view, std::string_view>> params = {
        {"access-type", "mail-server"},
        {"server", "mailserv@example.com"},
        {"subject", "send document"}
    };

    auto ref = ExternalBodyParser::parse(params);

    REQUIRE(ref.access_type == "mail-server");
    REQUIRE(ref.server == "mailserv@example.com");
    REQUIRE(ref.subject == "send document");
}

TEST_CASE("External Body - With size parameter", "[mime][external_body]") {
    std::vector<std::pair<std::string_view, std::string_view>> params = {
        {"access-type", "ftp"},
        {"name", "largefile.zip"},
        {"site", "ftp.example.com"},
        {"size", "1048576"}
    };

    auto ref = ExternalBodyParser::parse(params);

    REQUIRE(ref.size == 1048576);
}

TEST_CASE("External Body - With expiration date", "[mime][external_body]") {
    std::vector<std::pair<std::string_view, std::string_view>> params = {
        {"access-type", "http"},
        {"name", "temp.txt"},
        {"site", "www.example.com"},
        {"expiration", "2024-12-31"}
    };

    auto ref = ExternalBodyParser::parse(params);

    REQUIRE(ref.expiration == "2024-12-31");
}

TEST_CASE("External Body - Case insensitive parameter keys", "[mime][external_body]") {
    std::vector<std::pair<std::string_view, std::string_view>> params = {
        {"Access-Type", "FTP"},
        {"Name", "file.txt"},
        {"Site", "ftp.example.com"}
    };

    auto ref = ExternalBodyParser::parse(params);

    REQUIRE(ref.access_type == "FTP");
    REQUIRE(ref.name == "file.txt");
    REQUIRE(ref.site == "ftp.example.com");
}

TEST_CASE("External Body - All parameters", "[mime][external_body]") {
    std::vector<std::pair<std::string_view, std::string_view>> params = {
        {"access-type", "ftp"},
        {"name", "complete.pdf"},
        {"site", "ftp.example.com"},
        {"directory", "/pub/docs"},
        {"size", "2097152"},
        {"expiration", "2025-01-01"}
    };

    auto ref = ExternalBodyParser::parse(params);

    REQUIRE(ref.access_type == "ftp");
    REQUIRE(ref.name == "complete.pdf");
    REQUIRE(ref.site == "ftp.example.com");
    REQUIRE(ref.directory == "/pub/docs");
    REQUIRE(ref.size == 2097152);
    REQUIRE(ref.expiration == "2025-01-01");
}

TEST_CASE("External Body - Empty parameters", "[mime][external_body]") {
    std::vector<std::pair<std::string_view, std::string_view>> params = {};

    auto ref = ExternalBodyParser::parse(params);

    REQUIRE(ref.access_type.empty());
    REQUIRE(ref.name.empty());
    REQUIRE(ref.size == 0);
}

TEST_CASE("External Body - RFC 2046 FTP example", "[mime][external_body]") {
    std::vector<std::pair<std::string_view, std::string_view>> params = {
        {"access-type", "FTP"},
        {"name", "ietf-spec.txt"},
        {"site", "ftp.ietf.org"},
        {"directory", "rfc"}
    };

    auto ref = ExternalBodyParser::parse(params);

    REQUIRE(ref.access_type == "FTP");
    REQUIRE(ref.name == "ietf-spec.txt");
    REQUIRE(ref.site == "ftp.ietf.org");
    REQUIRE(ref.directory == "rfc");
}

TEST_CASE("External Body - Anon FTP pattern", "[mime][external_body]") {
    std::vector<std::pair<std::string_view, std::string_view>> params = {
        {"access-type", "anon-ftp"},
        {"name", "public.tar.gz"},
        {"site", "ftp.gnu.org"},
        {"directory", "/pub/gnu"}
    };

    auto ref = ExternalBodyParser::parse(params);

    REQUIRE(ref.access_type == "anon-ftp");
}

TEST_CASE("External Body - URL in name", "[mime][external_body]") {
    std::vector<std::pair<std::string_view, std::string_view>> params = {
        {"access-type", "http"},
        {"name", "https://example.com/file.pdf"}
    };

    auto ref = ExternalBodyParser::parse(params);

    REQUIRE(ref.name == "https://example.com/file.pdf");
}

TEST_CASE("External Body - Large file size", "[mime][external_body]") {
    std::vector<std::pair<std::string_view, std::string_view>> params = {
        {"access-type", "ftp"},
        {"name", "bigdata.iso"},
        {"size", "4294967296"}  // 4 GB
    };

    auto ref = ExternalBodyParser::parse(params);

    REQUIRE(ref.size == 4294967296ULL);
}

TEST_CASE("External Body - TFTP access", "[mime][external_body]") {
    std::vector<std::pair<std::string_view, std::string_view>> params = {
        {"access-type", "tftp"},
        {"name", "bootimage.bin"},
        {"site", "tftp.local"}
    };

    auto ref = ExternalBodyParser::parse(params);

    REQUIRE(ref.access_type == "tftp");
}

TEST_CASE("External Body - Unknown parameters ignored", "[mime][external_body]") {
    std::vector<std::pair<std::string_view, std::string_view>> params = {
        {"access-type", "ftp"},
        {"name", "file.txt"},
        {"unknown-param", "value"},
        {"another-unknown", "data"}
    };

    auto ref = ExternalBodyParser::parse(params);

    REQUIRE(ref.access_type == "ftp");
    REQUIRE(ref.name == "file.txt");
}

TEST_CASE("External Body - Directory with spaces", "[mime][external_body]") {
    std::vector<std::pair<std::string_view, std::string_view>> params = {
        {"access-type", "local-file"},
        {"name", "document.pdf"},
        {"directory", "/home/user/My Documents"}
    };

    auto ref = ExternalBodyParser::parse(params);

    REQUIRE(ref.directory == "/home/user/My Documents");
}

TEST_CASE("External Body - Subject with special characters", "[mime][external_body]") {
    std::vector<std::pair<std::string_view, std::string_view>> params = {
        {"access-type", "mail-server"},
        {"server", "archive@example.com"},
        {"subject", "GET /archive/file-2024.txt"}
    };

    auto ref = ExternalBodyParser::parse(params);

    REQUIRE(ref.subject == "GET /archive/file-2024.txt");
}

TEST_CASE("External Body - Zero size file", "[mime][external_body]") {
    std::vector<std::pair<std::string_view, std::string_view>> params = {
        {"access-type", "ftp"},
        {"name", "empty.txt"},
        {"size", "0"}
    };

    auto ref = ExternalBodyParser::parse(params);

    REQUIRE(ref.size == 0);
}

TEST_CASE("External Body - Windows path directory", "[mime][external_body]") {
    std::vector<std::pair<std::string_view, std::string_view>> params = {
        {"access-type", "local-file"},
        {"name", "data.csv"},
        {"directory", "C:\\Users\\Public\\Documents"}
    };

    auto ref = ExternalBodyParser::parse(params);

    REQUIRE(ref.directory == "C:\\Users\\Public\\Documents");
}
