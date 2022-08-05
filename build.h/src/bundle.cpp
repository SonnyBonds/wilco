#include "core/eventhandler.h"
#include "core/project.h"
#include "modules/bundle.h"

static std::string generatePlist(const extensions::MacOSBundle& bundleSettings)
{
    std::string result;
    result += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    result += "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n";
    result += "<plist version=\"1.0\">\n";
    result += "<dict>\n";

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

    result += "</dict>\n";
    result += "</plist>\n";
    return result;
}

struct BundleEventHandler : public EventHandler
{
    void postResolve(const Project& project, ProjectSettings& resolvedSettings, std::optional<ProjectType> type, StringId configName, OperatingSystem targetOS) override
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
        file::write(plistPath, generatePlist(bundleExt));

        resolvedSettings.commands += commands::copy(projectOutput, bundleOutput / "Contents/MacOS" / bundleBinary);
        resolvedSettings.commands += commands::copy(plistPath, bundleOutput / "Contents/Info.plist");

        for(auto& entry : bundleExt.contents)
        {
            resolvedSettings.commands += commands::copy(entry.source, bundleOutput / entry.target);
        }
    }
} instance;