#include "emitters/msvc.h"
#include "util/commands.h"
#include "util/uuid.h"
#include "fileutil.h"
#include <sstream>

static uuid::uuid namespaceUuid(0x8a168540, 0x2d194237, 0x99da3cc8, 0xae8b0d4e);
static std::string platformStr = "x64"; // TODO: More platform support?

static std::string uuidStr(uuid::uuid uuid)
{
    std::string uuidStr = uuid;
    std::transform(uuidStr.begin(), uuidStr.end(), uuidStr.begin(), [](char c){ return std::toupper(c); });
    return "{" + uuidStr + "}";
}

static std::string calcProjectUuid(const Project& project)
{
    return uuidStr(uuid::generateV3(namespaceUuid, project.name));
}

static std::string calcProjectName(const Project& project)
{
    return project.name + ".vcxproj";
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
        str::replaceAllInPlace(input, "<", "&lt;");
        str::replaceAllInPlace(input, ">", "&gt;");
        str::replaceAllInPlace(input, "&", "&amp;"); // TODO: Maybe don't escape if it already is an &entity;...
        str::replaceAllInPlace(input, "\"", "&quot;");
        str::replaceAllInPlace(input, "'", "&apos;");
    }

    std::string escaped(std::string input)
    {
        str::replaceAllInPlace(input, "<", "&lt;");
        str::replaceAllInPlace(input, ">", "&gt;");
        str::replaceAllInPlace(input, "&", "&amp;"); // TODO: Maybe don't escape if it already is an &entity;...
        str::replaceAllInPlace(input, "\"", "&quot;");
        str::replaceAllInPlace(input, "'", "&apos;");
        return input;
    }

    TagTerminator tag(std::string tag, const std::vector<std::pair<std::string, std::string>>& attributes = {})
    {
        stream << str::padLeft("<" + tag, indent);
        for(auto& attribute : attributes)
        {
            stream << " " << attribute.first << "=" << str::quote(attribute.second, ' ', {});
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
            stream << " " << attribute.first << "=" << str::quote(attribute.second, ' ', {});
        }
        stream << ">" << content << "</" << tag << ">\n";
    }

    void closedTag(std::string tag, const std::vector<std::pair<std::string, std::string>>& attributes = {})
    {
        stream << str::padLeft("<" + tag, indent);
        for(auto& attribute : attributes)
        {
            stream << " " << attribute.first << "=" << str::quote(attribute.second, ' ', {});
        }
        stream << " />\n";
    }
};

MsvcEmitter MsvcEmitter::instance;

MsvcEmitter::MsvcEmitter()
    : Emitter("msvc", "Generate Msvc project files.")
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

static void collectDependencyReferences(const Project& project, std::vector<ProjectReference>& result)
{
    auto existingProject = std::find_if(result.begin(), result.end(), [&project](const ProjectReference& ref) { return ref.project == &project; });
    if(existingProject != result.end())
    {
        return;
    }

    if(project.type.has_value())
    {
        result.push_back({&project, calcProjectUuid(project), calcProjectName(project)});
    }
    
    for(auto& link : project.links)
    {
        collectDependencyReferences(*link, result);
    }
}

static std::vector<ProjectReference> collectDependencyReferences(const Project& project)
{
    std::vector<ProjectReference> result;

    for(auto& link : project.links)
    {
        collectDependencyReferences(*link, result);
    }

    return result;
}

static std::string emitProject(Environment& env, std::ostream& solutionStream, const std::filesystem::path& suggestedDataDir, Project& project, std::vector<StringId> configs)
{
    struct ResolvedConfig
    {
        StringId name;
        ProjectSettings properties;
    };
    
    std::vector<ResolvedConfig> resolvedConfigs;
    if(configs.empty())
    {
        resolvedConfigs.push_back({"", project.resolve(env, suggestedDataDir, "", OperatingSystem::current())});
    }
    else
    {
        resolvedConfigs.reserve(configs.size());
        for(auto& config : configs)
        {
            resolvedConfigs.push_back({config, project.resolve(env, suggestedDataDir, config, OperatingSystem::current())});
        }
    }

    auto root = resolvedConfigs.front().properties.dataDir;

    if(!project.type.has_value())
    {
        return {};
    }

    if(project.name.empty())
    {
        throw std::runtime_error("Trying to emit project with no name.");
    }

    std::cout << "Emitting '" << project.name << "'\n";

    auto vcprojName = calcProjectName(project);

    auto projectUuid = calcProjectUuid(project);

    std::filesystem::path pathOffset = std::filesystem::proximate(std::filesystem::current_path(), root);

    auto dependencies = collectDependencyReferences(project);

    // This whole project output is a fair bit of cargo cult emulation
    // of reference project files. Some tags probably aren't even needed.
    SimpleXmlWriter xml(root / vcprojName);
    {
        auto tag = xml.tag("Project", {
            {"DefaultTargets", "Build"}, 
            {"ToolsVersion", "16.0"}, 
            {"xmlns", "http://schemas.microsoft.com/developer/msbuild/2003"}
        });

        {
            auto tag = xml.tag("ItemGroup", {{"Label", "ProjectConfigurations"}});
            for(auto& config : configs)
            {
                auto tag = xml.tag("ProjectConfiguration", {{"Include", std::string(config.cstr()) + "|" + platformStr}});
                xml.shortTag("Configuration", {}, config.cstr());
                xml.shortTag("Platform", {}, platformStr);
            }
        }

        {
            auto tag = xml.tag("PropertyGroup", {{"Label", "Globals"}});
            xml.shortTag("ProjectGuid", {}, projectUuid);
            xml.shortTag("RootNamespace", {}, project.name);
            xml.shortTag("WindowsTargetPlatformMinVersion", {}, "10.0.10240.0");
            xml.shortTag("Keyword", {}, "Win32Proj");
        }

        xml.closedTag("Import", {{ "Project", "$(VCTargetsPath)\\Microsoft.Cpp.Default.props"}});

        for(auto& config : resolvedConfigs)
        {
            auto tag = xml.tag("PropertyGroup", {
                {"Condition", "'$(Configuration)|$(Platform)'=='" + std::string(config.name.cstr()) + "|" + platformStr + "'"}, 
                {"Label", "Configuration"}
            });

            xml.shortTag("VCProjectVersion", {}, "16.0");
            xml.shortTag("ConfigurationType", {}, typeString(*project.type));
            xml.shortTag("PlatformToolset", {}, "v143"); // TODO: Configurable toolset
            xml.shortTag("PreferredToolArchitecture", {}, "x64"); // TODO: Configurable toolset
        }

        xml.closedTag("Import", {{ "Project", "$(VCTargetsPath)\\Microsoft.Cpp.props"}});

        {
            auto tag = xml.tag("ImportGroup", {{"Label", "ExtensionSettings"}});
        }

        for(auto& config : resolvedConfigs)
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

        for(auto& config : resolvedConfigs)
        {
            auto tag = xml.tag("PropertyGroup", {
                {"Condition", "'$(Configuration)|$(Platform)'=='" + std::string(config.name.cstr()) + "|" + platformStr + "'"}, 
                {"Label", "PropertySheets"}
            });

            auto outputPath = std::filesystem::absolute(project.calcOutputPath(config.properties));
            xml.shortTag("OutDir", {}, outputPath.parent_path().string() + "\\");
            xml.shortTag("IntDir", {}, (root / std::filesystem::path("obj") / std::string(config.name.cstr()) / project.name).string() + "\\");
            xml.shortTag("TargetName", {}, outputPath.stem().string());
            xml.shortTag("TargetExt", {}, outputPath.extension().string());
        }

        for(auto& config : resolvedConfigs)
        {
            auto tag = xml.tag("ItemDefinitionGroup", {
                {"Condition", "'$(Configuration)|$(Platform)'=='" + std::string(config.name.cstr()) + "|" + platformStr + "'"}, 
                {"Label", "PropertySheets"}
            });

            {
                auto tag = xml.tag("ClCompile");

                std::string includePaths;
                for(auto path : config.properties.includePaths)
                {
                    if(path.is_absolute())
                    {
                        includePaths += path.string() + ";";
                    }
                    else
                    {
                        includePaths += (pathOffset / path).string() + ";";
                    }
                }
                xml.shortTag("AdditionalIncludeDirectories", {}, includePaths + "%(AdditionalIncludeDirectories)");

                std::string defines;
                for(auto define : config.properties.defines)
                {
                    defines += define + ";";
                }
                xml.shortTag("PreprocessorDefinitions", {}, defines + "%(PreprocessorDefinitions)");
                    
                xml.shortTag("MultiProcessorCompilation", {}, "true");

                auto& pchHeader = config.properties.ext<extensions::Msvc>().pch.header;
                if (pchHeader.isSet())
                {
                    xml.shortTag("PrecompiledHeader", {}, "Use");
                    xml.shortTag("PrecompiledHeaderFile", {}, pchHeader.value().filename().string());
                }

                std::map<Feature, std::string> featureMap = {
                    { feature::Cpp11, "<LanguageStandard>stdcpp11</LanguageStandard>"},
                    { feature::Cpp14, "<LanguageStandard>stdcpp14</LanguageStandard>"},
                    { feature::Cpp17, "<LanguageStandard>stdcpp17</LanguageStandard>"},
                    { feature::Cpp20, "<LanguageStandard>stdcpp20</LanguageStandard>"},
                    { feature::Cpp23, "<LanguageStandard>stdcpp23</LanguageStandard>"},
                };

                for(auto& feature : config.properties.features)
                {
                    auto it = featureMap.find(feature);
                    if(it != featureMap.end())
                    {
                        xml.stream << str::padLeft(it->second, xml.indent);
                    }
                }

                std::string extraFlags;
                for (auto& flag : config.properties.ext<extensions::Msvc>().compilerFlags)
                {
                    extraFlags += std::string(flag) + " ";
                }
                xml.shortTag("AdditionalOptions", {}, extraFlags + "%(AdditionalOptions)");

                // TODO: Convert more features to project option
            }

            if(project.type == StaticLib)
            {
                auto tag = xml.tag("Lib");

                std::string extraFlags;
                for (auto& flag : config.properties.ext<extensions::Msvc>().archiverFlags)
                {
                    extraFlags += std::string(flag) + " ";
                }
                xml.shortTag("AdditionalOptions", {}, extraFlags + "%(AdditionalOptions)");
            }
            else
            {
                auto tag = xml.tag("Link");

                std::string extraFlags;
                for (auto& flag : config.properties.ext<extensions::Msvc>().compilerFlags)
                {
                    extraFlags += std::string(flag) + " ";
                }
                xml.shortTag("AdditionalOptions", {}, extraFlags + "%(AdditionalOptions)");
            }
        
            if(!config.properties.commands.value().empty())
            {
                CommandEntry chained = commands::chain(config.properties.commands);
                auto tag = xml.tag("CustomBuildStep");
                xml.shortTag("Command", {}, "\"%comspec%\" /s /c " + chained.command);

                std::set<std::filesystem::path> inputs;
                std::set<std::filesystem::path> outputs;
                for(auto& command : config.properties.commands)
                {
                    inputs.insert(command.inputs.begin(), command.inputs.end());
                }

                for(auto& command : config.properties.commands)
                {
                    for(auto& output : command.outputs)
                    {
                        auto inputIt = inputs.find(output);
                        if(inputIt != inputs.end())
                        {
                            inputs.erase(inputIt);
                        }
                        else
                        {
                            outputs.insert(output);
                        }
                    }
                }
                std::string inputsStr;
                std::string outputsStr;
                for(auto& input : inputs)
                {
                    inputsStr += input.string() + ";";
                }
                xml.shortTag("Inputs", {}, inputsStr);

                for(auto& output : outputs)
                {
                    outputsStr += output.string() + ";";
                }
                xml.shortTag("Outputs", {}, outputsStr);
            }
        }

        {
            std::set<std::string> objFiles;
            auto tag = xml.tag("ItemGroup");
            for(auto& input : resolvedConfigs.front().properties.files)
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

                for (auto& config : resolvedConfigs)
                {
                    const auto& msvcExt = config.properties.ext<extensions::Msvc>();
                    if (msvcExt.pch.source.isSet() && msvcExt.pch.source == input.path.string())
                    {
                        xml.shortTag("PrecompiledHeader", { {"Condition", "'$(Configuration)|$(Platform)'=='" + std::string(config.name.cstr()) + "|" + platformStr + "'"} }, "Create");
                    }
                    /*else if(msvcExt.pch.ignoredFiles.value())
                    {
                        xml.shortTag("PrecompiledHeader", { {"Condition", "'$(Configuration)|$(Platform)'=='" + std::string(config.name.cstr()) + "|" + platformStr + "'"} }, "Create");
                    }*/
                }
            }
        }

        {
            auto tag = xml.tag("ItemGroup");
            for(auto& input : resolvedConfigs.front().properties.files)
            {
                auto language = input.language != lang::Auto ? input.language : Language::getByPath(input.path);
                if(language == lang::C || language == lang::Cpp)
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
    }

    SimpleXmlWriter filtersXml(root / (vcprojName + ".filters"));
    {
        auto tag = filtersXml.tag("Project", {
            {"ToolsVersion", "16.0"},
            {"xmlns", "http://schemas.microsoft.com/developer/msbuild/2003"}
        });

        std::set<std::filesystem::path> existingFilters;
        {
            auto tag = filtersXml.tag("ItemGroup");

            for (auto& input : resolvedConfigs.front().properties.files)
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
                    filtersXml.shortTag("UniqueIdentifier", {}, uuidStr(uuid::generateV3(namespaceUuid, filterPathStr)));
                }
            }
        }

        {
            auto tag = filtersXml.tag("ItemGroup");
            for (auto& input : resolvedConfigs.front().properties.files)
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
            for (auto& input : resolvedConfigs.front().properties.files)
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

    solutionStream << "Project(\"{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}\") = \"" << project.name << "\", \"" << vcprojName << "\", \"" << projectUuid << "\"\n";
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

void MsvcEmitter::emit(Environment& env)
{
    std::filesystem::create_directories(*targetPath);

    auto [generator, buildOutput] = createGeneratorProject(env, *targetPath);

    {
        std::string argumentString;
        for(auto& arg : env.cliContext.allArguments)
        {
            argumentString += " " + str::quote(arg);    
        }

        auto buildPath = env.configurationFile.parent_path() / buildOutput;

        CommandEntry runCommand;
        runCommand.inputs = { buildPath };
        runCommand.command = "cd " + str::quote(env.startupDir.string()) + " && " + str::quote(buildPath.string()) + argumentString;
        generator->commands += runCommand;
    }

    auto projects = env.collectProjects();
    for(auto& project : projects)
    {
        if(project != generator)
        {
            project->links += generator;
        }
    }

    auto configs = env.collectConfigs();
    // Order matters for output and StringId order is not totally deterministic
    std::sort(configs.begin(), configs.end(), [](StringId& a, StringId& b) {
        return strcmp(a.cstr(), b.cstr()) == -1;
    });

    
    std::string solutionName = "Solution"; // TODO: Solution name
    std::stringstream solutionStream;
    solutionStream << "Microsoft Visual Studio Solution File, Format Version 12.00\n";
    solutionStream << "# Visual Studio Version 17\n";

    for(auto project : projects)
    {
        emitProject(env, solutionStream, *targetPath, *project, configs);
    }

    solutionStream << "Global\n";
    solutionStream << "\tGlobalSection(SolutionConfigurationPlatforms) = preSolution\n";
    for(auto& config : configs)
    {
        auto cfgStr = std::string(config.cstr()) + "|" + platformStr;
        solutionStream << "\t\t" << cfgStr << " = " << cfgStr << "\n";
    }
    solutionStream << "\tEndGlobalSection\n";
    solutionStream << "\tGlobalSection(ProjectConfigurationPlatforms) = postSolution\n";
    for(auto project : projects)
    {
        if(!project->type.has_value())
        {
            continue;
        }

        for(auto& config : configs)
        {
            auto cfgStr = std::string(config.cstr()) + "|" + platformStr;
            auto uuidStr = calcProjectUuid(*project);
            solutionStream << "\t\t" << uuidStr << "." << cfgStr << ".ActiveCfg = " << cfgStr << "\n";
            solutionStream << "\t\t" << uuidStr << "." << cfgStr << ".Build.0 = " << cfgStr << "\n";
        }
    }
    solutionStream << "\tEndGlobalSection\n";
    solutionStream << "\tGlobalSection(ExtensibilityGlobals) = postSolution\n";
    solutionStream << "\t\tSolutionGuid = " << uuidStr(uuid::generateV3(namespaceUuid, solutionName)) << "\n";
    solutionStream << "\tEndGlobalSection\n";
    solutionStream << "EndGlobal\n";

    writeFile(*targetPath / (solutionName + ".sln"), solutionStream.str());
}