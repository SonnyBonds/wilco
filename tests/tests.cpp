#define CATCH_CONFIG_MAIN
#define CUSTOM_BUILD_H_MAIN
#include "wilco.h"
#undef INPUT
#include "catch2/catch.hpp"

#include "src/dependencyparser.h"

// Needed since we link with wilco, even if this isn't really used
void configure(Environment& env)
{ }

TEST_CASE( "String utils" ) {
    CHECK(str::padLeft("test", 4) == "    test");
    CHECK(str::padLeft("test", 4, '#') == "####test");
    CHECK(str::padRight("test", 4) == "test    ");
    CHECK(str::padRight("test", 4, '#') == "test####");

    CHECK(str::padLeftToSize("test", 10) == "      test");
    CHECK(str::padRightToSize("test", 10) == "test      ");
    CHECK(str::padLeftToSize("test", 2) == "test");
    CHECK(str::padRightToSize("test", 2) == "test");

    CHECK(str::wrap("abcd efgh ijkl mnop qrst", 12, 5) == "abcd efgh\n     ijkl mnop\n     qrst");
    //CHECK(str::wrap("abcd efgh ijkl mnop qrst", 9, 5) == "abcd efgh\n     ijkl mnop\n     qrst");
    //CHECK(str::wrap("abcd efgh ijkl mnop qrst", 10, 5) == "abcd efgh\n     ijkl mnop\n     qrst");
}

TEST_CASE( "Dependency Parser" ) {
    SECTION("gcc style") {
        std::string dependencyData = R"--( c:\asdf:
        some\path\with\ spaces \
        another\without \

        trailing\space\  
        \leading \\backslash
        path"with"quotes
        m\ u\ l\ tiple\ s\ p\ aces
        endoffile)--";

        std::vector<std::string> result;
        REQUIRE(!parseDependencyData(dependencyData, [&result](std::string_view path){
            result.push_back(std::string(path));
            return false;
        }));
        REQUIRE(result == std::vector<std::string>{
            R"--(some\path\with spaces)--",
            R"--(another\without)--",
            R"--(trailing\space )--",
            R"--(\leading)--",
            R"--(\\backslash)--",
            R"--(path"with"quotes)--",
            R"--(m u l tiple s p aces)--",
            R"--(endoffile)--",
        });
    }

    SECTION("cl style") {
        std::string dependencyData = R"--({
    "Version": "1.1",
    "Data": {
        "Source": "c:\\some\\file\\build.cpp",
        "ProvidedModule": "",
        "Includes": [
            "some\\path\\with spaces",
            "another\\without",
            "trailing\\space ",
            "\\leading",
            "\\\\backslash",
            "path\"with\"quotes",
            "m u l tiple s p aces",
            "endoffile"
        ],
        "ImportedModules": [],
        "ImportedHeaderUnits": []
    }
}           
        )--";

        std::vector<std::string> result;
        REQUIRE(!parseDependencyData(dependencyData, [&result](std::string_view path){
            result.push_back(std::string(path));
            return false;
        }));
        REQUIRE(result == std::vector<std::string>{
            R"--(some\path\with spaces)--",
            R"--(another\without)--",
            R"--(trailing\space )--",
            R"--(\leading)--",
            R"--(\\backslash)--",
            R"--(path"with"quotes)--",
            R"--(m u l tiple s p aces)--",
            R"--(endoffile)--",
        });
    }
}

TEST_CASE( "Hash" ) {
    CHECK(hash::md5String("asdfasdfasdfasdf") == "08afd6f9ae0c6017d105b4ce580de885");
    CHECK(hash::md5String("Hello world") == "3e25960a79dbc69b674cd4ec67a72c62");
    CHECK(hash::md5String("md5") == "1bc29b36f623ba82aaf6724fd3b16718");
    CHECK(hash::md5String("A slightly longer text string of text to hash.") == "69f519d9eca214b238de1f92e52e9e1d");
}

namespace Catch {
    template<>
    struct StringMaker<uuid::uuid> {
        static std::string convert( uuid::uuid const& value ) {
            return std::string(value);
        }
    };
}

TEST_CASE( "UUID" ) {
    CHECK(uuid::uuid("90bffb75-6d1b-4608-874c-e97cb403ab94") == uuid::uuid(0x90bffb75, 0x6d1b4608, 0x874ce97c, 0xb403ab94));
    CHECK(std::string(uuid::uuid(0x90bffb75, 0x6d1b4608, 0x874ce97c, 0xb403ab94)) == "90bffb75-6d1b-4608-874c-e97cb403ab94");
    CHECK(uuid::generateV3(uuid::uuid(0x90bffb75, 0x6d1b4608, 0x874ce97c, 0xb403ab94), "test name") == uuid::uuid(0x65736502, 0xe3ba3c40, 0xa6022fcc, 0x7e0a5a2b));
}