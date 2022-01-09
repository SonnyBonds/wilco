#pragma once

#include "build.h"
#include <variant>

struct BundleEntry
{
    fs::path source;
    fs::path target;

    bool operator <(const BundleEntry& other) const
    {
        {
            int v = source.compare(other.source);
            if(v != 0) return v < 0;
        }

        return target < other.target;
    }

    bool operator ==(const BundleEntry& other) const
    {
        return source == other.source &&
               target == other.target;
    }
};

template<>
struct std::hash<BundleEntry>
{
    std::size_t operator()(BundleEntry const& entry) const
    {
        std::size_t h = std::hash<std::string>{}(entry.source);
        h = h ^ (std::hash<std::string>{}(entry.target) << 1);
        return h;
    }
};

struct VerbatimPlistValue
{
    std::string valueString;
};

using PlistValue = std::variant<std::string, bool, int, VerbatimPlistValue>;

Option<std::vector<BundleEntry>> BundleContents{"BundleContents"};
Option<std::map<std::string, PlistValue>> PlistEntries{"PlistEntries"}; 

std::string generatePlist(Project& project, ProjectConfig& resolvedConfig)
{
    std::string result;
    result += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    result += "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n";
    result += "<plist version=\"1.0\">\n";
    result += "<dict>\n";

    for(auto& entry : resolvedConfig[PlistEntries])
    {
        result += "  <key>" + entry.first + "</key>\n";
        if(auto value = std::get_if<std::string>(&entry.second))
        {
            result += "  <string>" + *value + "</string>\n";
        }
        else if(auto value = std::get_if<bool>(&entry.second))
        {
            result += (*value) ? "  <true/>\n" :  "  <false/>\n";
        }
        else if(auto value = std::get_if<int>(&entry.second))
        {
            result += "  <integer>" + std::to_string(*value) + "</integer>\n";
        }
        else if(auto value = std::get_if<VerbatimPlistValue>(&entry.second))
        {
            result += value->valueString;
        }
    }

    result += "</dict>\n";
    result += "</plist>\n";
    return result;
}


OptionCollection bundle(std::optional<std::string> bundleExtension = {})
{
    OptionCollection result;

    auto bundleFunc = [bundleExtension](Project& project, ProjectConfig& resolvedConfig)
    {
        std::string resolvedExtension;
        if(bundleExtension)
        {
            resolvedExtension = *bundleExtension;
        }
        else
        {
            resolvedExtension = std::string(project.type == Executable ? ".app" : ".bundle");
        }

        auto projectOutput = project.calcOutputPath(resolvedConfig);
        auto bundleOutput = projectOutput;
        bundleOutput.replace_extension(resolvedExtension);
        auto bundleBinary = projectOutput.filename();
        bundleBinary.replace_extension("");

        if(project.type == Executable)
        {
            resolvedConfig[PlistEntries]["CFBundleExecutable"] = bundleBinary.string();
            resolvedConfig[PlistEntries]["CFBundlePackageType"] = "APPL";
        }
        else if(project.type == SharedLib)
        {
            resolvedConfig[PlistEntries]["CFBundleExecutable"] = bundleBinary.string();
            resolvedConfig[PlistEntries]["CFBundlePackageType"] = "BNDL";
        }

        auto dataDir = resolvedConfig[DataDir];
        auto plistPath = dataDir / project.name / "Info.plist";
        writeFile(plistPath, generatePlist(project, resolvedConfig));

        resolvedConfig[Commands] += commands::copy(projectOutput, bundleOutput / "Contents/MacOS" / bundleBinary);
        resolvedConfig[Commands] += commands::copy(plistPath, bundleOutput / "Contents/Info.plist");
    };

    PostProcessor postProcessor;
    postProcessor.func = std::function(bundleFunc);
    result[PostProcess] += std::move(postProcessor);

    return result;
}
