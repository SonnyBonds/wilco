#define CATCH_CONFIG_MAIN
#define CUSTOM_BUILD_H_MAIN
#include "build.h"
#undef INPUT
#include "catch2/catch.hpp"

TEST_CASE( "String utils" ) {
    CHECK(str::padLeft("test", 4) == "    test");
    CHECK(str::padLeft("test", 4, '#') == "####test");
    CHECK(str::padRight("test", 4) == "test    ");
    CHECK(str::padRight("test", 4, '#') == "test####");

    CHECK(str::padLeftToSize("test", 10) == "      test");
    CHECK(str::padRightToSize("test", 10) == "test      ");
    CHECK(str::padLeftToSize("test", 2) == "test");
    CHECK(str::padRightToSize("test", 2) == "test");
}

TEST_CASE( "StringId" ) {
    SECTION("hashes") {
        REQUIRE(std::hash<std::string>()("test1") == std::hash<std::string_view>()("test1"));
    }

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
    SECTION("const char* - std::string_view comparison") {
        REQUIRE(StringId("asdf") == StringId(std::string_view("asdf")));
    }
    SECTION("std::string - std::string comparison") {
        REQUIRE(StringId(std::string("asdf")) == StringId(std::string("asdf")));
    }
    SECTION("std::string - std::string_view comparison") {
        REQUIRE(StringId(std::string("asdf")) == StringId(std::string_view("asdf")));
    }
    SECTION("std::string_view - std::string_view comparison") {
        REQUIRE(StringId(std::string("asdf")) == StringId(std::string_view("asdf")));
    }
    SECTION("default - empty const char* comparison") {
        REQUIRE(StringId() == StringId(""));
    }
    SECTION("default - empty std::string comparison") {
        REQUIRE(StringId() == StringId(std::string()));
    }
    SECTION("default - empty std::string_view comparison") {
        REQUIRE(StringId() == StringId(std::string_view()));
    }

    SECTION("long string comparison") {
        REQUIRE(StringId("/a/long/string/of/data/that/could/resemble/a/path/maybe") == StringId("/a/long/string/of/data/that/could/resemble/a/path/maybe"));
        REQUIRE_FALSE(StringId("/a/long/string/of/data/that/could/resemble/a/path/xxxxx") == StringId("/a/long/string/of/data/that/could/resemble/a/path/maybe"));
    }
}

// Used to make test deduplication, while still being able to trace where it comes from
class CompareEqualString
{
public:
    std::string str;

    CompareEqualString(std::string other)
        : str(std::move(other))
    {
    }

    CompareEqualString(const char* other)
        : str(other)
    {
    }

    operator const std::string&() const
    {
        return str;
    }

    operator const char*() const
    {
        return str.c_str();
    }

    bool operator ==(const CompareEqualString& other) const
    {
        return true;
    }
};

template<>
struct std::hash<CompareEqualString>
{
    size_t operator()(const CompareEqualString&)
    {
        return 1;
    }
};

TEST_CASE( "Resolve Config" ) {
    
    using strvec = std::vector<std::string>;
    using streqvec = std::vector<CompareEqualString>;

    Option<strvec> LocalOption{"LocalOption"};
    Option<strvec> PublicOption{"PublicOption"};
    Option<strvec> PublicOnlyOption{"PublicOnlyOption"};
    Option<strvec> StaticLibOption{"StaticLibOption"};
    Option<strvec> TypeOption{"TypeOption"};
    Option<streqvec> DuplicateOption{"DuplicateOption"};
    StringId configA = "configA";
    StringId configB = "configB";

    Project baseProject;
    baseProject[LocalOption] += "None";
    baseProject[configA][LocalOption] += "A";
    baseProject[configB][LocalOption] += "B";
    baseProject[Public][DuplicateOption] += { "Base 1", "Base 2" };
    baseProject[Public / configA][DuplicateOption] += { "Base A 1", "Base A 2"};
    baseProject[Public / configB][DuplicateOption] += { "Base B 1", "Base B 2"};
    baseProject[Public][PublicOption] += "P None";
    baseProject[Public / configA][PublicOption] += "P A";
    baseProject[Public / configB][PublicOption] += "P B";
    baseProject[PublicOnly][PublicOnlyOption] += "PO None";
    baseProject[PublicOnly / configA][PublicOnlyOption] += "PO A";
    baseProject[PublicOnly / configB][PublicOnlyOption] += "PO B";
    baseProject[Public / StaticLib][TypeOption] += "Static On Base";
    baseProject[Public / Executable][TypeOption] += "Executable On Base";

    Project noTypeProject;
    noTypeProject.links += &baseProject;
    noTypeProject[Public / StaticLib][TypeOption] += "Static On None";
    noTypeProject[Public / Executable][TypeOption] += "Executable On None";
    noTypeProject[Public][DuplicateOption] += { "None 1", "None 2" };
    noTypeProject[Public / configA][DuplicateOption] += { "None A 1", "None A 2"};
    noTypeProject[Public / configB][DuplicateOption] += { "None B 1", "None B 2"};

    Project staticLibProject;
    staticLibProject.links += &noTypeProject;
    staticLibProject.type = StaticLib;
    staticLibProject[Public / StaticLib][TypeOption] += "Static On Static";
    staticLibProject[Public / Executable][TypeOption] += "Executable On Static";
    staticLibProject[Public][DuplicateOption] += { "Static 1", "Static 2" };
    staticLibProject[Public / configA][DuplicateOption] += { "Static A 1", "Static A 2"};
    staticLibProject[Public / configB][DuplicateOption] += { "Static B 1", "Static B 2"};

    Project executableProject;
    executableProject.links += &staticLibProject;
    executableProject.type = Executable;
    executableProject[Public / StaticLib][TypeOption] += "Static On Executable";
    executableProject[Public / Executable][TypeOption] += "Executable On Executable";
    executableProject[Public][DuplicateOption] += { "Executable 1", "Executable 2" };
    executableProject[Public / configA][DuplicateOption] += { "Executable A 1", "Executable A 2"};
    executableProject[Public / configB][DuplicateOption] += { "Executable B 1", "Executable B 2"};

    SECTION("base no config") {
        auto resolved = baseProject.resolve("", OperatingSystem::current());
        REQUIRE(resolved[LocalOption] == strvec{"None"});
        REQUIRE(resolved[PublicOption] == strvec{"P None"});
        REQUIRE(resolved[PublicOnlyOption] == strvec{});
        REQUIRE(resolved[TypeOption] == strvec{});
        REQUIRE(resolved[DuplicateOption] == streqvec{ "Single" });
    }

    SECTION("base config A") {
        auto resolved = baseProject.resolve(configA, OperatingSystem::current());
        REQUIRE(resolved[LocalOption] == strvec{"None", "A"});
        REQUIRE(resolved[PublicOption] == strvec{"P None", "P A"});
        REQUIRE(resolved[PublicOnlyOption] == strvec{});
        REQUIRE(resolved[DuplicateOption] == streqvec{ "Single" });
    }

    SECTION("base config B") {
        auto resolved = baseProject.resolve(configB, OperatingSystem::current());
        REQUIRE(resolved[LocalOption] == strvec{"None", "B"});
        REQUIRE(resolved[PublicOption] == strvec{"P None", "P B"});
        REQUIRE(resolved[PublicOnlyOption] == strvec{});
        REQUIRE(resolved[DuplicateOption] == streqvec{ "Single" });
    }

    SECTION("no type no config") {
        auto resolved = noTypeProject.resolve("", OperatingSystem::current());
        REQUIRE(resolved[LocalOption] == strvec{});
        REQUIRE(resolved[PublicOption] == strvec{"P None"});
        REQUIRE(resolved[PublicOnlyOption] == strvec{"PO None"});
        REQUIRE(resolved[TypeOption] == strvec{});
        REQUIRE(resolved[DuplicateOption] == streqvec{ "Single" });
    }

    SECTION("no type config A") {
        auto resolved = noTypeProject.resolve(configA, OperatingSystem::current());
        REQUIRE(resolved[LocalOption] == strvec{});
        REQUIRE(resolved[PublicOption] == strvec{"P None", "P A"});
        REQUIRE(resolved[PublicOnlyOption] == strvec{"PO None", "PO A"});
        REQUIRE(resolved[DuplicateOption] == streqvec{ "Single" });
    }

    SECTION("no type config B") {
        auto resolved = noTypeProject.resolve(configB, OperatingSystem::current());
        REQUIRE(resolved[LocalOption] == strvec{});
        REQUIRE(resolved[PublicOption] == strvec{"P None", "P B"});
        REQUIRE(resolved[PublicOnlyOption] == strvec{"PO None", "PO B"});
        REQUIRE(resolved[TypeOption] == strvec{});
        REQUIRE(resolved[DuplicateOption] == streqvec{ "Single" });
    }

    SECTION("staticlib no config") {
        auto resolved = staticLibProject.resolve("", OperatingSystem::current());
        REQUIRE(resolved[TypeOption] == strvec{"Static On Base", "Static On None", "Static On Static"});
        REQUIRE(resolved[DuplicateOption] == streqvec{ "Single" });
    }

    SECTION("executable no config") {
        auto resolved = executableProject.resolve("", OperatingSystem::current());
        REQUIRE(resolved[TypeOption] == strvec{"Executable On Base", "Executable On None", "Executable On Static", "Executable On Executable"});
        REQUIRE(resolved[DuplicateOption] == streqvec{ "Single" });
    }
}

TEST_CASE( "String util" ) {
    CHECK(str::trim(std::string(" \n \tsome text\t  \n")) == "some text");
    CHECK(str::trim(std::string("some text")) == "some text");
    CHECK(str::trim(std::string_view(" \n \tsome text\t  \n")) == "some text");
    CHECK(str::trim(std::string_view("some text")) == "some text");
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
        REQUIRE(!DirectBuilder::parseDependencyData(dependencyData, [&result](std::string_view path){
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
        REQUIRE(!DirectBuilder::parseDependencyData(dependencyData, [&result](std::string_view path){
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