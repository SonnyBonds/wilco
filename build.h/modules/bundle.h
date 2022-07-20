#pragma once

#include <functional>
#include <string>
#include <variant>

#include "core/option.h"
#include "core/project.h"
#include "modules/feature.h"
#include "modules/postprocess.h"
#include "util/commands.h"
#include "util/file.h"

struct BundleEntry
{
    std::filesystem::path source;
    std::filesystem::path target;

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
        std::size_t h = std::filesystem::hash_value(entry.source);
        h = h ^ (std::filesystem::hash_value(entry.target) << 1);
        return h;
    }
};

struct VerbatimPlistValue
{
    std::string valueString;
};

using PlistValue = std::variant<std::string, bool, int, VerbatimPlistValue>;

struct bundle
{
    Property<std::vector<BundleEntry>> contents;
    Property<std::map<std::string, PlistValue>> plistEntries; 
};

#if 0
std::string generatePlist(Project& project, OptionCollection& resolvedOptions)
{
    std::string result;
    result += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    result += "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n";
    result += "<plist version=\"1.0\">\n";
    result += "<dict>\n";

    for(auto& entry : resolvedOptions[PlistEntries])
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

    auto bundleFunc = [bundleExtension](Project& project, OptionCollection& resolvedOptions)
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

        auto projectOutput = project.calcOutputPath(resolvedOptions);
        auto bundleOutput = projectOutput;
        bundleOutput.replace_extension(resolvedExtension);
        auto bundleBinary = projectOutput.filename();
        bundleBinary.replace_extension("");

        if(project.type == Executable)
        {
            resolvedOptions[PlistEntries]["CFBundleExecutable"] = bundleBinary.string();
            resolvedOptions[PlistEntries]["CFBundlePackageType"] = "APPL";
        }
        else if(project.type == SharedLib)
        {
            resolvedOptions[PlistEntries]["CFBundleExecutable"] = bundleBinary.string();
            resolvedOptions[PlistEntries]["CFBundlePackageType"] = "BNDL";
        }

        auto dataDir = resolvedOptions[DataDir];
        auto plistPath = dataDir / project.name / "Info.plist";
        file::write(plistPath, generatePlist(project, resolvedOptions));

        resolvedOptions[Commands] += commands::copy(projectOutput, bundleOutput / "Contents/MacOS" / bundleBinary);
        resolvedOptions[Commands] += commands::copy(plistPath, bundleOutput / "Contents/Info.plist");

        for(auto& entry : resolvedOptions[BundleContents])
        {
            resolvedOptions[Commands] += commands::copy(entry.source, bundleOutput / entry.target);
        }
    };

    PostProcessor postProcessor;
    postProcessor.func = std::function(bundleFunc);
    result[PostProcess] += std::move(postProcessor);
    result[Features] += feature::MacOSBundle;

    return result;
}

#endif