#pragma once

#include <filesystem>

#include "core/stringid.h"
#include "modules/language.h"

struct SourceFile
{
    std::filesystem::path path;
    Language language = lang::Auto;

    SourceFile() {}
    SourceFile(std::filesystem::path path) : path(std::move(path)) {}
    SourceFile(std::filesystem::path path, Language language) : path(std::move(path)), language(language) {}
    SourceFile(std::string path) : path(std::move(path)) {}
    SourceFile(const char* path) : path(path) {}

    bool operator ==(const SourceFile& other) const
    {
        return path == other.path;
    }

    bool operator <(const SourceFile& other) const
    {
        return path < other.path;
    }
};

template<>
struct std::hash<SourceFile>
{
    std::size_t operator()(SourceFile const& sourceFile) const
    {
        return std::filesystem::hash_value(sourceFile.path);
    }
};
