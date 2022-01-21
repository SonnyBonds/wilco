#define CATCH_CONFIG_MAIN
#define CUSTOM_BUILD_H_MAIN
#include "catch2/catch.hpp"
#include "build.h"

TEST_CASE( "StringId" ) {
    SECTION("use new id") {
        size_t referenceStorageSize = StringId::getStorageSize();
        StringId("asdf1");
        REQUIRE(StringId::getStorageSize() == referenceStorageSize+1);
    }

    SECTION("use existing id - const char*") {
        size_t referenceStorageSize = StringId::getStorageSize();
        StringId("asdf1");
        REQUIRE(StringId::getStorageSize() == referenceStorageSize);
    }

    SECTION("use existing id - std::string") {
        size_t referenceStorageSize = StringId::getStorageSize();
        StringId(std::string("asdf1"));
        REQUIRE(StringId::getStorageSize() == referenceStorageSize);
    }

    SECTION("const char* - const char* comparison") {
        REQUIRE(StringId("asdf") == StringId("asdf"));
    }
    SECTION("const char* - std::string comparison") {
        REQUIRE(StringId("asdf") == StringId(std::string("asdf")));
    }
    /*SECTION("const char* - std::string_view comparison") {
        REQUIRE(StringId("asdf") == StringId(std::string_view("asdf")));
    }*/
    SECTION("std::string - std::string comparison") {
        REQUIRE(StringId(std::string("asdf")) == StringId(std::string("asdf")));
    }
    /*SECTION("std::string - std::string_view comparison") {
        REQUIRE(StringId(std::string("asdf")) == StringId(std::string_view("asdf")));
    }*/
    /*SECTION("std::string_view - std::string_view comparison") {
        REQUIRE(StringId(std::string("asdf")) == StringId(std::string_view("asdf")));
    }*/
    SECTION("default - empty const char* comparison") {
        REQUIRE(StringId() == StringId(""));
    }
    SECTION("default - empty std::string comparison") {
        REQUIRE(StringId() == StringId(std::string()));
    }
    /*SECTION("default - empty std::string_view comparison") {
        REQUIRE(StringId() == StringId(std::string_view())));
    }*/
}