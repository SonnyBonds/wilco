#include "actions/msvc.h"
#include "toolchains/cl.h"
#include "util/commands.h"
#include "util/process.h"
#include "util/uuid.h"
#include "fileutil.h"
#include "buildconfigurator.h"
#include <sstream>
#include "database.h"
#include "commandprocessor.h"

static uuid::uuid projectNamespaceUuid(0x8a168540, 0x2d194237, 0x99da3cc8, 0xae8b0d4e);
static uuid::uuid filterNamespaceUuid(0x4d136782, 0x745f4680, 0xb816b3e3, 0x636a84c3);
static uuid::uuid folderNamespaceUuid(0xcfa5f3b8, 0xd0d646ad, 0x941dbb03, 0xedc6ddee);
static uuid::uuid solutionNamespaceUuid(0xd9842b84, 0xee2e4abc, 0x89c9656a, 0x2373e7d7);
static std::string platformStr = "x64"; // TODO: More platform support?

static std::string uuidStr(uuid::uuid uuid)
{
    std::string uuidStr = uuid;
    std::transform(uuidStr.begin(), uuidStr.end(), uuidStr.begin(), [](char c){ return std::toupper(c); });
    return "{" + uuidStr + "}";
}

static std::string calcProjectUuid(const std::string& projectName)
{
    return uuidStr(uuid::generateV3(projectNamespaceUuid, projectName));
}

static std::string calcFolderUuid(std::string folder)
{
    return uuidStr(uuid::generateV3(folderNamespaceUuid, folder));
}

static std::string calcProjectName(const std::string& projectName)
{
    return std::string(projectName) + ".vcxproj";
}

struct TagTerminator
{
    ~TagTerminator()
    {
        indent -= 2;
        stream << str::padLeft("</" + tag + ">", indent) << "\n";
    }

    std::string tag;
    std::ostream& stream;
    int& indent;
};

struct SimpleXmlWriter
{
    std::stringstream stream;
    std::filesystem::path path;
    int indent = 0;

    SimpleXmlWriter(std::filesystem::path path)
        : path(path)
    {
        stream << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
    }

    ~SimpleXmlWriter()
    {
        writeFile(path, stream.str());
    }

    void escape(std::string& input)
    {
        str::replaceAllInPlace(input, "&", "&amp;"); // TODO: Maybe don't escape if it already is an &entity;...
        str::replaceAllInPlace(input, "<", "&lt;");
        str::replaceAllInPlace(input, ">", "&gt;");
        str::replaceAllInPlace(input, "\"", "&quot;");
        str::replaceAllInPlace(input, "'", "&apos;");
    }

    std::string escaped(std::string input)
    {
        str::replaceAllInPlace(input, "&", "&amp;"); // TODO: Maybe don't escape if it already is an &entity;...
        str::replaceAllInPlace(input, "<", "&lt;");
        str::replaceAllInPlace(input, ">", "&gt;");
        str::replaceAllInPlace(input, "\"", "&quot;");
        str::replaceAllInPlace(input, "'", "&apos;");
        return input;
    }

    TagTerminator tag(std::string tag, const std::vector<std::pair<std::string, std::string>>& attributes = {})
    {
        stream << str::padLeft("<" + tag, indent);
        for(auto& attribute : attributes)
        {
            stream << " " << attribute.first << "=" << str::quote(escaped(attribute.second), ' ', {});
        }
        stream << ">\n";
        indent += 2;
        return {tag, stream, indent};
    }

    void shortTag(std::string tag, const std::vector<std::pair<std::string, std::string>>& attributes = {}, std::string content = {})
    {
        escape(content);
        
        stream << str::padLeft("<" + tag, indent);
        for(auto& attribute : attributes)
        {
            stream << " " << attribute.first << "=" << str::quote(escaped(attribute.second), ' ', {});
        }
        stream << ">" << content << "</" << tag << ">\n";
    }

    void closedTag(std::string tag, const std::vector<std::pair<std::string, std::string>>& attributes = {})
    {
        stream << str::padLeft("<" + tag, indent);
        for(auto& attribute : attributes)
        {
            stream << " " << attribute.first << "=" << str::quote(escaped(attribute.second), ' ', {});
        }
        stream << " />\n";
    }
};

ActionInstance<MsvcEmitter> MsvcEmitter::instance;

MsvcEmitter::MsvcEmitter()
    : Action("msvc", "Generate Msvc project files.")
{
}

static const char* typeString(ProjectType type)
{
    switch(type)
    {
    case Executable:
        return "Application";
    case StaticLib:
        return "StaticLibrary";
    case SharedLib:
        return "DynamicLibrary";
    default:
        return "Utility";
    }
}

namespace
{
    struct ProjectReference
    {
        const Project* project;
        std::string uuid;
        std::string name;
    };
}

static void collectDependencyReferences(const Project& project, std::vector<ProjectReference>& result, StringId config)
{
    auto existingProject = std::find_if(result.begin(), result.end(), [&project](const ProjectReference& ref) { return ref.project == &project; });
    if(existingProject != result.end())
    {
        return;
    }

    result.push_back({&project, calcProjectUuid(project.name), calcProjectName(project.name)});
    
    for(auto& dependency : project.dependencies)
    {
        collectDependencyReferences(*dependency, result, config);
    }
}

static std::vector<ProjectReference> collectDependencyReferences(const Project& project, StringId config)
{
    std::vector<ProjectReference> result;

    for(auto& dependency : project.dependencies)
    {
        collectDependencyReferences(*dependency, result, config);
    }

    return result;
}

namespace
{

struct ProjectConfig
{
    StringId name;
    Project* project = nullptr;
    std::unordered_set<StringId> ignorePch;
};

struct ProjectMatrixEntry
{
    std::string name;
    std::vector<ProjectConfig> configs;
};

}

static std::string emitProject(std::ostream& solutionStream, const std::filesystem::path& projectDir, const ProjectMatrixEntry& projectEntry)
{
    std::cout << "Emitting '" << projectEntry.name << "'\n";

    auto vcprojName = calcProjectName(projectEntry.name);
    auto projectUuid = calcProjectUuid(projectEntry.name);

    std::filesystem::path pathOffset = std::filesystem::proximate(std::filesystem::current_path(), projectDir);

    // TODO: Take all configurations into account
    auto dependencies = collectDependencyReferences(*projectEntry.configs.front().project, projectEntry.name);

    // This whole project output is a fair bit of cargo cult emulation
    // of reference project files. Some tags probably aren't even needed.
    SimpleXmlWriter xml(projectDir / vcprojName);
    {
        auto tag = xml.tag("Project", {
            {"DefaultTargets", "Build"}, 
            {"ToolsVersion", "16.0"}, 
            {"xmlns", "http://schemas.microsoft.com/developer/msbuild/2003"}
        });

        {
            auto tag = xml.tag("ItemGroup", {{"Label", "ProjectConfigurations"}});
            for(auto& config : projectEntry.configs)
            {
                auto tag = xml.tag("ProjectConfiguration", {{"Include", std::string(config.name.cstr()) + "|" + platformStr}});
                xml.shortTag("Configuration", {}, config.name.cstr());
                xml.shortTag("Platform", {}, platformStr);
            }
        }

        {
            auto tag = xml.tag("PropertyGroup", {{"Label", "Globals"}});
            xml.shortTag("ProjectGuid", {}, projectUuid);
            xml.shortTag("RootNamespace", {}, projectEntry.name);
            xml.shortTag("WindowsTargetPlatformMinVersion", {}, "10.0.10240.0");
            xml.shortTag("Keyword", {}, "Win32Proj");
        }

        xml.closedTag("Import", {{ "Project", "$(VCTargetsPath)\\Microsoft.Cpp.Default.props"}});

        for(auto& config : projectEntry.configs)
        {
            auto tag = xml.tag("PropertyGroup", {
                {"Condition", "'$(Configuration)|$(Platform)'=='" + std::string(config.name.cstr()) + "|" + platformStr + "'"}, 
                {"Label", "Configuration"}
            });

            xml.shortTag("VCProjectVersion", {}, "16.0");
            xml.shortTag("ConfigurationType", {}, typeString(config.project->type));
            xml.shortTag("PlatformToolset", {}, "v143"); // TODO: Configurable toolset
            xml.shortTag("PreferredToolArchitecture", {}, "x64"); // TODO: Configurable toolset
        }

        xml.closedTag("Import", {{ "Project", "$(VCTargetsPath)\\Microsoft.Cpp.props"}});

        {
            auto tag = xml.tag("ImportGroup", {{"Label", "ExtensionSettings"}});
        }

        for(auto& config : projectEntry.configs)
        {
            auto tag = xml.tag("ImportGroup", {
                {"Condition", "'$(Configuration)|$(Platform)'=='" + std::string(config.name.cstr()) + "|" + platformStr + "'"}, 
                {"Label", "PropertySheets"}
            });

            xml.closedTag("Import", {
                { "Project", "$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props" },
                { "Condition", "exists('$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props')" },
                { "Label", "LocalAppDataPlatform" },
            });
        }

        xml.closedTag("PropertyGroup", {{"Label", "UserMacros"}});

        for(auto& config : projectEntry.configs)
        {
            auto tag = xml.tag("PropertyGroup", {
                {"Condition", "'$(Configuration)|$(Platform)'=='" + std::string(config.name.cstr()) + "|" + platformStr + "'"}, 
                {"Label", "PropertySheets"}
            });

            auto outputPath = std::filesystem::absolute(config.project->output);
            xml.shortTag("OutDir", {}, outputPath.parent_path().string() + "\\");
            xml.shortTag("IntDir", {}, (projectDir / std::filesystem::path("obj") / std::string(config.name.cstr()) / config.project->name).string() + "\\");
            xml.shortTag("TargetName", {}, outputPath.stem().string());
            xml.shortTag("TargetExt", {}, outputPath.extension().string());
        }

        for(auto& config : projectEntry.configs)
        {
            auto tag = xml.tag("ItemDefinitionGroup", {
                {"Condition", "'$(Configuration)|$(Platform)'=='" + std::string(config.name.cstr()) + "|" + platformStr + "'"}, 
                {"Label", "PropertySheets"}
            });

            std::string includePaths;
            for (auto path : config.project->includePaths)
            {
                if (path.is_absolute())
                {
                    includePaths += path.string() + ";";
                }
                else
                {
                    includePaths += (pathOffset / path).string() + ";";
                }
            }

            std::string defines;
            for (auto define : config.project->defines)
            {
                defines += define + ";";
            }

            {
                auto tag = xml.tag("ClCompile");

                xml.shortTag("AdditionalIncludeDirectories", {}, includePaths + "%(AdditionalIncludeDirectories)");
                xml.shortTag("PreprocessorDefinitions", {}, defines + "%(PreprocessorDefinitions)");
                xml.shortTag("MultiProcessorCompilation", {}, "true");

                auto& pchHeader = config.project->ext<extensions::Msvc>().pch.header;
                if (!pchHeader.empty())
                {
                    xml.shortTag("PrecompiledHeader", {}, "Use");
                    xml.shortTag("PrecompiledHeaderFile", {}, pchHeader.filename().string());
                }

                std::map<Feature, std::string> featureMap = {
                    { feature::Cpp11, "<LanguageStandard>stdcpp11</LanguageStandard>"},
                    { feature::Cpp14, "<LanguageStandard>stdcpp14</LanguageStandard>"},
                    { feature::Cpp17, "<LanguageStandard>stdcpp17</LanguageStandard>"},
                    { feature::Cpp20, "<LanguageStandard>stdcpp20</LanguageStandard>"},
                    { feature::Cpp23, "<LanguageStandard>stdcpp23</LanguageStandard>"},
                    { feature::msvc::StaticRuntime, "<RuntimeLibrary>MultiThreaded</RuntimeLibrary>"},
                    { feature::msvc::StaticDebugRuntime, "<RuntimeLibrary>MultiThreadedDebug</RuntimeLibrary>"},
                    { feature::msvc::SharedRuntime, "<RuntimeLibrary>MultiThreadedDLL</RuntimeLibrary>"},
                    { feature::msvc::SharedDebugRuntime, "<RuntimeLibrary>MultiThreadedDebugDLL</RuntimeLibrary>"},
                };

                for(auto& feature : config.project->features)
                {
                    auto it = featureMap.find(feature);
                    if(it != featureMap.end())
                    {
                        xml.stream << str::padLeft(it->second, xml.indent) << "\n";
                    }
                }

                std::string extraFlags;
                for (auto& flag : config.project->ext<extensions::Msvc>().compilerFlags)
                {
                    extraFlags += std::string(flag) + " ";
                }
                xml.shortTag("AdditionalOptions", {}, extraFlags + "%(AdditionalOptions)");
                xml.shortTag("CompileAs", {}, "CompileAsCpp");

                // TODO: Convert more features to project option
            }

            {
                auto tag = xml.tag("ResourceCompile");
            
                xml.shortTag("AdditionalIncludeDirectories", {}, includePaths + "%(AdditionalIncludeDirectories)");
                xml.shortTag("PreprocessorDefinitions", {}, defines + "%(PreprocessorDefinitions)");
            }

            if(config.project->type == StaticLib)
            {
                auto tag = xml.tag("Lib");

                std::string extraFlags;
                for (auto& flag : config.project->ext<extensions::Msvc>().archiverFlags)
                {
                    extraFlags += std::string(flag) + " ";
                }
                xml.shortTag("AdditionalOptions", {}, extraFlags + "%(AdditionalOptions)");
            }
            else
            {
                {
                    auto tag = xml.tag("Link");

                    std::string additionalDependencies;
                    for (auto& lib : config.project->libs)
                    {
                        additionalDependencies += (pathOffset / lib).lexically_normal().string();
                        if (!lib.has_extension())
                        {
                            additionalDependencies += ".lib";
                        }
                        additionalDependencies += ";";
                    }
                    for (auto& lib : config.project->systemLibs)
                    {
                        additionalDependencies += lib.string();
                        if (!lib.has_extension())
                        {
                            additionalDependencies += ".lib";
                        }
                        additionalDependencies += ";";
                    }
                    xml.shortTag("AdditionalDependencies", {}, additionalDependencies + "%(AdditionalDependencies)");

                    std::string additionalLibraryDirectories;
                    for (auto& libPath : config.project->libPaths)
                    {
                        additionalLibraryDirectories += (pathOffset / libPath).lexically_normal().string() + ";";
                    }
                    xml.shortTag("AdditionalLibraryDirectories", {}, additionalLibraryDirectories + "%(AdditionalLibraryDirectories)");

                    std::string extraFlags;
                    for (auto& flag : config.project->ext<extensions::Msvc>().linkerFlags)
                    {
                        extraFlags += std::string(flag) + " ";
                    }
                    xml.shortTag("AdditionalOptions", {}, extraFlags + "%(AdditionalOptions)");
                }
            }
        }

        {
            std::set<std::string> objFiles;
            auto tag = xml.tag("ItemGroup");
            for(auto& input : projectEntry.configs.front().project->files) // TODO: Not just pick files from the first config, but do something clever
            {
                auto language = input.language != lang::Auto ? input.language : Language::getByPath(input.path);
                if(language != lang::C && language != lang::Cpp)
                {
                    continue;
                }
                
                auto tag = xml.tag("ClCompile", {{"Include", (pathOffset / input.path).string()}});
                // Disambiguate files with the same name. Don't do this for all files, because msbuild
                // only compiles files in parallel that have the exact same compile settings, and specific obj output
                // will cause them to differ.
                if (!objFiles.insert(input.path.filename().string()).second)
                {
                    std::string objPath = input.path.string();
                    str::replaceAllInPlace(objPath, ":", "_");
                    str::replaceAllInPlace(objPath, "..", "__");
                    xml.shortTag("ObjectFileName", {}, "$(IntDir)\\" + objPath + ".obj");
                }

                if (language == lang::C)
                {
                    xml.shortTag("CompileAs", {}, "CompileAsC");
                }

                for (auto& config : projectEntry.configs)
                {
                    const auto& msvcExt = config.project->ext<extensions::Msvc>();
                    if (!msvcExt.pch.source.empty() && msvcExt.pch.source == input.path)
                    {
                        xml.shortTag("PrecompiledHeader", { {"Condition", "'$(Configuration)|$(Platform)'=='" + std::string(config.name.cstr()) + "|" + platformStr + "'"} }, "Create");
                    }
                    else if(config.ignorePch.find(StringId(input.path.string())) != config.ignorePch.end())
                    {
                        xml.shortTag("PrecompiledHeader", { {"Condition", "'$(Configuration)|$(Platform)'=='" + std::string(config.name.cstr()) + "|" + platformStr + "'"} }, "NotUsing");
                    }
                }
            }
        }

        std::string buildDependsOn = "$(BuildDependsOn)";

        for (auto& config : projectEntry.configs)
        {
            int index = 0;
            auto outputPath = std::filesystem::absolute(config.project->output);
            bool postBuild = false;
            for(auto& command : config.project->commands)
            {
                if(command.inputs.empty())
                {
                    throw std::runtime_error(std::string("Command '") + command.description + "' in project '" + config.project->name + "' has no inputs.");
                }
                if(command.outputs.empty())
                {
                    throw std::runtime_error(std::string("Command '") + command.description + "' in project '" + config.project->name + "' has no outputs.");
                }

                auto targetName = "Command" + std::to_string(index) + "_" + config.name.cstr();

                std::string inputsString;
                std::string outputsString;
                std::string readTlogString;
                std::string writeTlogString;
                {
                    auto tag = xml.tag("ItemGroup");
                    bool first = true;
                    for(auto& input : command.inputs)
                    {
                        if(!first)
                        {
                            inputsString += ";";
                        }
                        first = false;
                        auto relativePath = (pathOffset / input).lexically_normal();
                        auto absolutePath = std::filesystem::absolute(relativePath);
                        if(!postBuild && outputPath == absolutePath)
                        {
                            postBuild = true;
                        }

                        inputsString += relativePath.string();
                        if(!readTlogString.empty())
                        {
                            readTlogString += ";";
                        }
                        readTlogString += "^" + absolutePath.string();

                        if(writeTlogString.empty())
                        {
                            writeTlogString += "^";
                        }
                        else
                        {
                            writeTlogString += "|";
                        }
                        writeTlogString += absolutePath.string();
                    }
                    first = true;
                    for(auto& output : command.outputs)
                    {
                        if(!first)
                        {
                            outputsString += ";";
                        }
                        first = false;
                        auto relativePath = (pathOffset / output).lexically_normal();
                        auto absolutePath = std::filesystem::absolute(relativePath);
                        outputsString += relativePath.string();

                        if(!writeTlogString.empty())
                        {
                            writeTlogString += ";";
                        }
                        writeTlogString += absolutePath.string();
                    }
                }

                {
                    auto tag = xml.tag("Target", { {"Name", targetName}, {"Inputs", inputsString}, {"Outputs", outputsString}, {"Condition", "'$(Configuration)|$(Platform)'=='" + std::string(config.name.cstr()) + "|" + platformStr + "'"} });

                    xml.shortTag("WriteLinesToFile", { 
                        { "File", "$(TLogLocation)" + targetName + ".read.1u.tlog" }, 
                        { "Lines", readTlogString }, 
                        { "Overwrite", "true" }, 
                        { "Encoding", "Unicode" }, 
                    });
                    xml.shortTag("WriteLinesToFile", { 
                        { "File", "$(TLogLocation)" + targetName + ".write.1u.tlog" }, 
                        { "Lines", writeTlogString }, 
                        { "Overwrite", "true" }, 
                        { "Encoding", "Unicode" }, 
                    });
                    xml.shortTag("Message", { {"Text", command.description}, {"Importance", "high"} });
                    xml.shortTag("Exec", { {"Command", "cd " + str::quote((pathOffset / command.workingDirectory).string()) + " && " + command.command} });
                }

                if(postBuild)
                {
                    buildDependsOn = buildDependsOn + ";" + targetName;
                }
                else
                {
                    buildDependsOn = targetName + ";" + buildDependsOn;
                }

                ++index;
            }
        }

        {
            std::set<std::string> resFiles;
            auto tag = xml.tag("ItemGroup");
            for(auto& input : projectEntry.configs.front().project->files) // TODO: Not just pick files from the first config, but do something clever
            {
                auto language = input.language != lang::Auto ? input.language : Language::getByPath(input.path);
                if (language != lang::Rc)
                {
                    continue;
                }

                auto tag = xml.tag("ResourceCompile", { {"Include", (pathOffset / input.path).string()} });

                // Disambiguate files with the same name. Don't do this for all files, because msbuild
                // only compiles files in parallel that have the exact same compile settings, and specific obj output
                // will cause them to differ.
                if (!resFiles.insert(input.path.filename().string()).second)
                {
                    std::string resPath = input.path.string();
                    str::replaceAllInPlace(resPath, ":", "_");
                    str::replaceAllInPlace(resPath, "..", "__");
                    xml.shortTag("ResourceOutputFileName", {}, "$(IntDir)\\" + resPath + ".res");
                }
            }
        }

        {
            auto tag = xml.tag("ItemGroup");
            for(auto& input : projectEntry.configs.front().project->files) // TODO: Not just pick files from the first config, but do something clever
            {
                auto language = input.language != lang::Auto ? input.language : Language::getByPath(input.path);
                if(language == lang::C || language == lang::Cpp || language == lang::Rc)
                {
                    continue;
                }

                xml.shortTag("None", {{"Include", (pathOffset / input.path).string()}});
            }
        }

        if(!dependencies.empty())
        {
            auto tag = xml.tag("ItemGroup");
            for(auto& dependency : dependencies)
            {
                auto tag = xml.tag("ProjectReference", {{"Include", dependency.name}});
                xml.shortTag("Project", {}, dependency.uuid);
            }
        }        

        xml.closedTag("Import", {{ "Project", "$(VCTargetsPath)\\Microsoft.Cpp.targets"}});
        {
            auto tag = xml.tag("ImportGroup", {{"Label", "ExtensionTargets"}});
        }

        {
            auto tag = xml.tag("PropertyGroup");
            xml.shortTag("BuildDependsOn", {}, buildDependsOn);
        }
    }

    SimpleXmlWriter filtersXml(projectDir / (vcprojName + ".filters"));
    {
        auto tag = filtersXml.tag("Project", {
            {"ToolsVersion", "16.0"},
            {"xmlns", "http://schemas.microsoft.com/developer/msbuild/2003"}
        });

        std::set<std::filesystem::path> existingFilters;
        {
            auto tag = filtersXml.tag("ItemGroup");

            for (auto& input : projectEntry.configs.front().project->files) // TODO: Not just pick files from the first config, but do something clever
            {
                auto filterPath = input.path.lexically_normal();
                while (filterPath.has_parent_path() && filterPath.has_relative_path())
                {
                    filterPath = filterPath.parent_path();
                    if (!existingFilters.insert(filterPath).second)
                    {
                        continue;
                    }
                    auto filterPathStr = filterPath.string();
                    auto tag = filtersXml.tag("Filter", { {"Include", filterPathStr} });
                    filtersXml.shortTag("UniqueIdentifier", {}, uuidStr(uuid::generateV3(filterNamespaceUuid, filterPathStr)));
                }
            }
        }

        {
            auto tag = filtersXml.tag("ItemGroup");
            for (auto& input : projectEntry.configs.front().project->files) // TODO: Not just pick files from the first config, but do something clever
            {
                auto language = input.language != lang::Auto ? input.language : Language::getByPath(input.path);
                if (language != lang::C && language != lang::Cpp)
                {
                    continue;
                }

                auto tag = filtersXml.tag("ClCompile", { {"Include", (pathOffset / input.path).string()} });
                filtersXml.shortTag("Filter", {}, input.path.lexically_normal().parent_path().string());
            }
        }

        {
            auto tag = filtersXml.tag("ItemGroup");
            for (auto& input : projectEntry.configs.front().project->files) // TODO: Not just pick files from the first config, but do something clever
            {
                auto language = input.language != lang::Auto ? input.language : Language::getByPath(input.path);
                if (language == lang::C || language == lang::Cpp)
                {
                    continue;
                }

                auto tag = filtersXml.tag("None", { {"Include", (pathOffset / input.path).string()} });
                filtersXml.shortTag("Filter", {}, input.path.lexically_normal().parent_path().string());
            }
        }
    }

    solutionStream << "Project(\"{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}\") = \"" << projectEntry.name << "\", \"" << vcprojName << "\", \"" << projectUuid << "\"\n";
    if(!dependencies.empty())
    {
        solutionStream << "\tProjectSection(ProjectDependencies) = postProject\n";
        for(auto& dependency : dependencies)
        {
            solutionStream << "\t\t" << dependency.uuid << " = " << dependency.uuid << "\n";
        }
        solutionStream << "\tEndProjectSection\n";
    }
    solutionStream << "EndProject\n";

    return vcprojName;
}

void MsvcEmitter::run(cli::Context cliContext)
{
    auto configDatabasePath = *targetPath / ".msvc_db";
    Database configDatabase;
    bool configDirty = !configDatabase.load(configDatabasePath);

    std::vector<std::string> baseArguments;

    for(auto& argument : cli::Argument::globalList())
    {
        if(!argument->rawValue.empty())
        {
            baseArguments.push_back(argument->rawValue);
        }
    }

    if(!configDirty)
    {
        if(configDatabase.getCommands().empty() || configDatabase.getCommands()[0].command != str::join(baseArguments, "\n"))
        {
            configDirty = true;
        }
    }

    if(!configDirty)
    {
        auto configCommands = filterCommands(cliContext.startPath, configDatabase, *targetPath, {});
        configDirty = !configCommands.empty();
    }

    cliContext.extractArguments(arguments);
    
    if(!configDirty)
    {
        return;
    }

    struct ProfileEntry
    {
        StringId name;
        std::vector<std::unique_ptr<Project>> projects;
    };

    std::filesystem::create_directories(*targetPath);

    std::vector<StringId> profileNames;
    std::set<std::string> projectNames;
    std::vector<ProfileEntry> profiles;

    for(auto& profile : cli::Profile::list())
    {
        std::vector<std::string> confArgs = { std::string("--profile=") + profile.name.cstr() };
        confArgs.insert(confArgs.end(), baseArguments.begin(), baseArguments.end());
        cli::Context configureContext(cliContext.startPath, cliContext.invocation, confArgs);

        Environment env = BuildConfigurator::configureEnvironment(configureContext);

        auto& generatorProject = env.createProject("_generator", Command);
        {
            auto buildHDir = std::filesystem::absolute(__FILE__).parent_path().parent_path();
            generatorProject.includePaths += buildHDir;
            generatorProject.features += feature::Cpp17;
            generatorProject.files += configureContext.configurationFile;
            
            std::string argumentString;
            for(auto& arg : cliContext.allArguments)
            {   
                argumentString += " " + str::quote(arg);
            }

            // Input/output is set to dummy values to trigger a run every build.
            // The VS up-to-date check does not support folders, and will trigger
            // always anyway
            generatorProject.commands += CommandEntry{ str::quote(process::findCurrentModulePath().string()) + argumentString, { "__dummy__input__" }, { "__dummy__output__" }, cliContext.startPath, {}, "Check build config." };
        }

        for(auto& project : env.projects)
        {
            if(project->name.empty())
            {
                throw std::runtime_error("Trying to emit project with no name.");
            }

            if(project.get() != &generatorProject)
            {
                project->dependencies += &generatorProject;
            }

            projectNames.insert(project->name);
        }

        profiles.push_back({profile.name, std::move(env.projects)});
    }

    BuildConfigurator::updateConfigDatabase(configDatabase, baseArguments);
    
    std::vector<ProjectMatrixEntry> projectMatrix;
    for(auto& projectName : projectNames)
    {
        ProjectMatrixEntry projectEntry;
        projectEntry.name = projectName;
        projectEntry.configs.reserve(profiles.size());

        for(auto& profile : profiles)
        {
            ProjectConfig configEntry = { profile.name };
            for(auto& project : profile.projects)
            {
                if(project->name == projectName)
                {
                    configEntry.project = project.get();
                    break;
                }
            }
            if(configEntry.project == nullptr)
            {
                throw std::runtime_error(std::string("MSVC generator currently requires defining all projects in all profiles, and '") + projectName + "' was mising from the '" + std::string(profile.name) + "' profile.");
            }

            const auto& msvcExt = configEntry.project->ext<extensions::Msvc>();
            configEntry.ignorePch.reserve(msvcExt.pch.ignoredFiles.size());
            for (auto& file : msvcExt.pch.ignoredFiles)
            {
                configEntry.ignorePch.insert(StringId(file.lexically_normal().string()));
            }

            projectEntry.configs.push_back(std::move(configEntry));
        }

        projectMatrix.push_back(std::move(projectEntry));
    }

    std::string solutionName = "Solution"; // TODO: Solution name
    std::stringstream solutionStream;
    solutionStream << "Microsoft Visual Studio Solution File, Format Version 12.00\n";
    solutionStream << "# Visual Studio Version 17\n";

    std::set<std::string> folders;
    for(auto& projectEntry : projectMatrix)
    {
        auto& folder = projectEntry.configs.front().project->ext<extensions::Msvc>().solutionFolder;
        if (!folder.empty())
        {
            folders.insert(folder);
        }

        emitProject(solutionStream, *targetPath, projectEntry);
    }

    for (auto& folder : folders)
    {
        auto quotedFolder = str::quote(folder);
        solutionStream << "Project(\"{2150E333-8FDC-42A3-9474-1A3956D46DE8}\") = " + quotedFolder + ", " + quotedFolder + ", \"" + calcFolderUuid(folder) + "\"\nEndProject\n";
    }

    solutionStream << "Global\n";
    solutionStream << "\tGlobalSection(SolutionConfigurationPlatforms) = preSolution\n";
    for(auto& profile : profiles)
    {
        auto cfgStr = std::string(profile.name.cstr()) + "|" + platformStr;
        solutionStream << "\t\t" << cfgStr << " = " << cfgStr << "\n";
    }
    solutionStream << "\tEndGlobalSection\n";

    solutionStream << "\tGlobalSection(ProjectConfigurationPlatforms) = postSolution\n";
    for(auto& profile : profiles)
    {
        for(auto& project : profile.projects)
        {
            auto cfgStr = std::string(profile.name.cstr()) + "|" + platformStr;
            auto uuidStr = calcProjectUuid(project->name);
            solutionStream << "\t\t" << uuidStr << "." << cfgStr << ".ActiveCfg = " << cfgStr << "\n";
            solutionStream << "\t\t" << uuidStr << "." << cfgStr << ".Build.0 = " << cfgStr << "\n";
        }
    }
    solutionStream << "\tEndGlobalSection\n";

    if (!folders.empty())
    {
        solutionStream << "\tGlobalSection(NestedProjects) = preSolution\n";
        for (auto& projectEntry : projectMatrix)
        {
            auto& folder = projectEntry.configs.front().project->ext<extensions::Msvc>().solutionFolder;
            if (!folder.empty())
            {
                solutionStream << "\t\t" << calcProjectUuid(projectEntry.name) + " = " + calcFolderUuid(folder) + "\n";
            }
        }
        solutionStream << "\tEndGlobalSection\n";
    }

    solutionStream << "\tGlobalSection(ExtensibilityGlobals) = postSolution\n";
    solutionStream << "\t\tSolutionGuid = " << uuidStr(uuid::generateV3(solutionNamespaceUuid, solutionName)) << "\n";
    solutionStream << "\tEndGlobalSection\n";
    solutionStream << "EndGlobal\n";

    writeFile(*targetPath / (solutionName + ".sln"), solutionStream.str());

    configDatabase.save(configDatabasePath);
}