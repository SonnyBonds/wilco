#include "core/environment.h"
#include "core/project.h"
#include "modules/bundle.h"

namespace macos
{

void BundleBuilder::addBinary(const Project& sourceProject, std::optional<std::string_view> filenameOverride)
{
    addBinary(sourceProject.output.fullPath(), filenameOverride);
}

void BundleBuilder::addBinary(std::filesystem::path binaryPath, std::optional<std::string_view> filenameOverride)
{
    if(filenameOverride)
    {
        items.push_back({binaryPath, std::filesystem::path("Contents/MacOS") / *filenameOverride});
    }
    else
    {
        items.push_back({binaryPath, std::filesystem::path("Contents/MacOS") / binaryPath.filename().string()});
    }
}

void BundleBuilder::addResource(std::filesystem::path resourcePath, std::optional<std::string_view> filenameOverride)
{
    if(filenameOverride)
    {
        items.push_back({resourcePath, std::filesystem::path("Contents/Resources") / *filenameOverride});
    }
    else
    {
        items.push_back({resourcePath, std::filesystem::path("Contents/Resources") / resourcePath.filename()});
    }
}

void BundleBuilder::addResource(std::filesystem::path resourcePath, std::filesystem::path targetPath, std::optional<std::string_view> filenameOverride)
{
    if(filenameOverride)
    {
        items.push_back({resourcePath, std::filesystem::path("Contents/Resources") / targetPath / *filenameOverride});
    }
    else
    {
        items.push_back({resourcePath, std::filesystem::path("Contents/Resources") / targetPath / resourcePath.filename()});
    }
}

void BundleBuilder::addResources(Environment& env, std::filesystem::path resourceRoot)
{
    for(auto& resource : env.listFiles(resourceRoot))
    {
        items.push_back({resource, std::filesystem::path("Contents/Resources") / std::filesystem::proximate(resource, resourceRoot)});
    }
}

void BundleBuilder::addResources(Environment& env, std::filesystem::path resourceRoot, std::filesystem::path targetPath)
{
    for(auto& resource : env.listFiles(resourceRoot))
    {
        items.push_back({resource, std::filesystem::path("Contents/Resources") / targetPath / std::filesystem::proximate(resource, resourceRoot)});
    }
}

void BundleBuilder::addGeneric(std::filesystem::path sourcePath, std::filesystem::path targetPath, std::optional<std::string_view> filenameOverride)
{
    if(filenameOverride)
    {
        items.push_back({sourcePath, targetPath / *filenameOverride});
    }
    else
    {
        items.push_back({sourcePath, targetPath / sourcePath.filename()});
    }
}


CommandEntry BundleBuilder::generateCommand()
{
    std::vector<CommandEntry> result;

    auto outputPath = output.fullPath();
    for(auto& item : items)
    {
        result.push_back(commands::copy(item.source, outputPath / item.target));
    }

    result.push_back(commands::copy(plistFile, outputPath / "Contents/Info.plist"));

    auto command = commands::chain(result, "Creating bundle " + outputPath.string());
    command.outputs.push_back(output.fullPath());
    return command;
}

}

#if TODO

static std::string generatePlist(const extensions::MacOSBundle& bundleSettings)
{
    std::string result;
    result += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    result += "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n";
    result += "<plist version=\"1.0\">\n";
    result += "<dict>\n";

#if TODO
    for(auto& entry : bundleSettings.plistEntries)
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
#endif

    result += "</dict>\n";
    result += "</plist>\n";
    return result;
}

struct BundleEventHandler : public EventHandler
{
    void postResolve(Environment& env, const Project& project, ProjectSettings& resolvedSettings, std::optional<ProjectType> type, StringId configName, OperatingSystem targetOS) override
    {
        if(targetOS != MacOS || !resolvedSettings.hasExt<extensions::MacOSBundle>())
        {
            return;
        }

        auto& bundleExt = resolvedSettings.ext<extensions::MacOSBundle>();
        if(!bundleExt.create)
        {
            return;
        }

        std::string resolvedExtension;
#if TODO
        if(bundleExt.extension.isSet())
        {
            resolvedExtension = bundleExt.extension.value();
        }
        else
        {
            resolvedExtension = std::string(project.type == Executable ? ".app" : ".bundle");
        }

        auto projectOutput = project.calcOutputPath(resolvedSettings);
        auto bundleOutput = projectOutput;
        bundleOutput.replace_extension(resolvedExtension);
        auto bundleBinary = projectOutput.filename();
        bundleBinary.replace_extension("");

        if(project.type == Executable)
        {
            bundleExt.plistEntries["CFBundleExecutable"] = bundleBinary.string();
            bundleExt.plistEntries["CFBundlePackageType"] = "APPL";
        }
        else if(project.type == SharedLib)
        {
            bundleExt.plistEntries["CFBundleExecutable"] = bundleBinary.string();
            bundleExt.plistEntries["CFBundlePackageType"] = "BNDL";
        }

        std::filesystem::path dataDir = resolvedSettings.dataDir;
        auto plistPath = dataDir / project.name / "Info.plist";
        env.writeFile(plistPath, generatePlist(bundleExt));

        resolvedSettings.commands += commands::copy(projectOutput, bundleOutput / "Contents/MacOS" / bundleBinary);
        resolvedSettings.commands += commands::copy(plistPath, bundleOutput / "Contents/Info.plist");

        for(auto& entry : bundleExt.contents)
        {
            resolvedSettings.commands += commands::copy(entry.source, bundleOutput / entry.target);
        }
#endif
    }
} instance;

#endif
