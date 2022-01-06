#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <set>
#include <map>
#include <vector>
#include <sstream>
#include <array>
#include <unordered_set>
#include <functional>

// We're messing with the namespaces and various operator overloads and stuff
// in this header which typically is bad form, but I've chosen to allow this 
// header to dictate the whole "environment" since the build files typically 
// form a very isolated context

namespace fs = std::filesystem;

#if _WINDOWS
static bool windows = true;
#else
static bool windows = false;
#endif

struct Project;
struct ProjectConfig;

using PostProcessor = void(*)(Project& project, ProjectConfig& resolvedConfig);


// Launders strings into directly comparable const char* pointers
struct StringPool
{
    static const char* get(const char* str)
    {
        static std::unordered_set<std::string> storage;
        return storage.insert(str).first->c_str();
    }

    static const char* get(const std::string& string)
    {
        return get(string.c_str());
    }
};

struct Id
{
    const char* const id;

    Id(const char* id) : id(StringPool::get(id)) {}
    Id(const std::string& id) : id(StringPool::get(id)) {}

    operator const char*() const
    {
        return id;
    }
};

struct NamedIdentifier
{
    const Id name;

    operator const char*() const
    {
        return name;
    }
};

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

struct CommandEntry
{
    std::string command;
    std::vector<fs::path> inputs;
    std::vector<fs::path> outputs;
    fs::path workingDirectory;
    fs::path depFile;
    std::string description;

    bool operator ==(const CommandEntry& other) const
    {
        return command == other.command &&
               outputs == other.outputs &&
               inputs == other.inputs &&
               workingDirectory == other.workingDirectory &&
               depFile == other.depFile;
    }
};

template<>
struct std::hash<CommandEntry>
{
    std::size_t operator()(CommandEntry const& command) const
    {
        std::size_t h = std::hash<std::string>{}(command.command);
        for(auto& output : command.outputs)
        {
            h = h ^ (fs::hash_value(output) << 1);
        }
        for(auto& input : command.inputs)
        {
            h = h ^ (fs::hash_value(input) << 1);
        }
        h = h ^ (fs::hash_value(command.workingDirectory) << 1);
        h = h ^ (fs::hash_value(command.depFile) << 1);
        return h;
    }
};

enum ProjectType
{
    Executable,
    StaticLib,
    SharedLib,
    Command
};

template<typename T>
struct Option : public NamedIdentifier
{
    using ValueType = T;
};

enum Transitivity
{
    Local,
    Public,
    PublicOnly
};

struct ConfigSelector
{
    constexpr ConfigSelector(std::string_view name)
        : name(name)
    {}

    constexpr ConfigSelector(const char* name)
        : name(name)
    {}

    constexpr ConfigSelector(Transitivity transitivity)
        : transitivity(transitivity)
    {}

    constexpr ConfigSelector(ProjectType projectType)
        : projectType(projectType)
    {}

    std::optional<Transitivity> transitivity;
    std::optional<std::string_view> name;
    std::optional<ProjectType> projectType;

    bool operator <(const ConfigSelector& other) const
    {
        if(transitivity != other.transitivity) return transitivity < other.transitivity;
        if(projectType != other.projectType) return projectType < other.projectType;
        if(name != other.name) return name < other.name;

        return false;
    }
};

constexpr ConfigSelector operator/(Transitivity a, ConfigSelector b)
{
    // Can these throw compile time?
    if(b.transitivity) throw std::invalid_argument("Transitivity was specified twice.");
    b.transitivity = a;

    return b;
}

constexpr ConfigSelector operator/(ProjectType a, ConfigSelector b)
{
    // Can these throw compile time?
    if(b.projectType) throw std::invalid_argument("Project type was specified twice.");
    b.projectType = a;

    return b;
}

constexpr ConfigSelector operator/(std::string_view a, ConfigSelector b)
{
    // Can these throw compile time?
    if(b.name) throw std::invalid_argument("Configuration name was specified twice.");
    b.name = std::move(a);

    return b;
}

struct ToolchainProvider
{
    virtual std::string getCompiler(Project& project, ProjectConfig& resolvedConfig, fs::path pathOffset) const = 0;
    virtual std::string getCommonCompilerFlags(Project& project, ProjectConfig& resolvedConfig, fs::path pathOffset) const = 0;
    virtual std::string getCompilerFlags(Project& project, ProjectConfig& resolvedConfig, fs::path pathOffset, const std::string& input, const std::string& output) const = 0;

    virtual std::string getLinker(Project& project, ProjectConfig& resolvedConfig, fs::path pathOffset) const = 0;
    virtual std::string getCommonLinkerFlags(Project& project, ProjectConfig& resolvedConfig, fs::path pathOffset) const = 0;
    virtual std::string getLinkerFlags(Project& project, ProjectConfig& resolvedConfig, fs::path pathOffset, const std::vector<std::string>& inputs, const std::string& output) const = 0;

    virtual std::vector<fs::path> process(Project& project, ProjectConfig& resolvedConfig, std::string_view config, const fs::path& workingDir, const fs::path& dataDir) const = 0;
};

Option<std::string> Platform{"Platform"};
Option<std::vector<fs::path>> IncludePaths{"IncludePaths"};
Option<std::vector<fs::path>> Files{"Files"};
Option<std::vector<fs::path>> GeneratorDependencies{"GeneratorDependencies"};
Option<std::vector<fs::path>> Libs{"Libs"};
Option<std::vector<std::string>> Defines{"Defines"};
Option<std::vector<std::string>> Features{"Features"};
Option<std::vector<std::string>> Frameworks{"Frameworks"};
Option<std::vector<BundleEntry>> BundleContents{"BundleContents"};
Option<fs::path> OutputDir{"OutputDir"};
Option<std::string> OutputStem{"OutputStem"};
Option<std::string> OutputExtension{"OutputExtension"};
Option<std::string> OutputPrefix{"OutputPrefix"};
Option<std::string> OutputSuffix{"OutputSuffix"};
Option<fs::path> OutputPath{"OutputPath"};
Option<fs::path> BuildPch{"BuildPch"};
Option<fs::path> ImportPch{"ImportPch"};
Option<std::vector<PostProcessor>> PostProcess{"PostProcess"};
Option<std::vector<CommandEntry>> Commands{"Commands"};
Option<ToolchainProvider*> Toolchain{"Toolchain"};

template<typename T>
std::vector<T>& operator +=(std::vector<T>& s, T other) {
    s.push_back(std::move(other));
    return s;
}
template<typename T>
std::vector<T>& operator +=(std::vector<T>& s, std::initializer_list<T> other) {
    s.insert(s.end(), std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
    return s;
}
template<typename T>
std::vector<T>& operator +=(std::vector<T>& s, std::vector<T> other) {
    s.insert(s.end(), std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
    return s;
}

template<typename T, typename U>
std::vector<T>& operator +=(std::vector<T>& s, U other) {
    s.push_back(std::move(other));
    return s;
}
template<typename T, typename U>
std::vector<T>& operator +=(std::vector<T>& s, std::initializer_list<U> other) {
    s.insert(s.end(), std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
    return s;
}
template<typename T, typename U>
std::vector<T>& operator +=(std::vector<T>& s, std::vector<U> other) {
    s.insert(s.end(), std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
    return s;
}

template<typename T>
struct OptionHash
{
    size_t operator()(const T& a) const
    { 
        return std::hash<T>()(a);
    };
};

template<>
struct OptionHash<fs::path>
{
    size_t operator()(const fs::path& a) const
    { 
        return fs::hash_value(a);
    };
};

struct OptionStorage
{
    using Data = std::unique_ptr<void, void(*)(const void*)>;

    OptionStorage()
        : _data{nullptr, &OptionStorage::nullDeleter}
    {
    }

    template<typename T> 
    const T& get() const
    {
        if(!_data)
        {
            static T empty;
            return empty;
        }
        return *static_cast<T*>(_data.get());
    }

    template<typename T> 
    T& getOrAdd()
    {
        if(!_data)
        {
            static auto deleter = [](const void* data)
            {
                delete static_cast<const T*>(data);
            };
            _data = Data(new T{}, deleter);

            static auto cloner = [](const OptionStorage& b)
            {
                OptionStorage clone;
                clone.getOrAdd<T>() = b.get<T>();
                return clone;
            };
            _cloner = cloner;

            static auto combiner = [](OptionStorage& a, const OptionStorage& b)
            {
                combineValues(a.getOrAdd<T>(), b.get<T>());
            };
            _combiner = combiner;

            static auto deduplicator = [](OptionStorage& a)
            {
                deduplicateValues(a.get<T>());
            };
            _deduplicator = deduplicator;
        }
        return *static_cast<T*>(_data.get());
    }

    void combine(const OptionStorage& other)
    {
        _combiner(*this, other);
    }

    void deduplicate()
    {
        _deduplicator(*this);
    }

    OptionStorage clone() const
    {
        return _cloner(*this);
    }

private:
    template<typename U>
    static void combineValues(U& a, U b)
    {
        a = b;
    }

    template<typename U>
    static void combineValues(std::vector<U>& a, std::vector<U> b)
    {
        a.insert(a.end(), std::make_move_iterator(b.begin()), std::make_move_iterator(b.end()));
    }

    template<typename U>
    static void deduplicateValues(U& v)
    {
    }

    template<typename U>
    static void deduplicateValues(std::vector<U>& v)
    {
        // Tested a few methods and this was the fastest one I came up with that's also pretty simple

        // Could probably also use a custom insertion ordered set instead of vectors to hold options
        // from the start, but this was simpler (and some quick tests indicated possibly faster)
 
        struct DerefEqual
        {
            bool operator ()(const U* a, const U* b) const
            {
                return *a == *b;
            }
        };

        struct DerefHash
        {
            size_t operator ()(const U* a) const
            {
                return OptionHash<U>()(*a);
            }
        };

        std::unordered_set<const U*, DerefHash, DerefEqual> dups;
        dups.reserve(v.size());
        v.erase(std::remove_if(v.begin(), v.end(), [&dups](const U& a) { 
            return !dups.insert(&a).second;
        }), v.end());
    }

    static void nullDeleter(const void*) {}

    std::function<OptionStorage(const OptionStorage&)> _cloner;
    std::function<void(OptionStorage&, const OptionStorage&)> _combiner;
    std::function<void(OptionStorage&)> _deduplicator;
    Data _data;
};

struct OptionCollection
{
    template<typename T>
    T& operator[](Option<T> option)
    {
        return _storage[option].template getOrAdd<T>();
    }

    void combine(const OptionCollection& other)
    {
        for(auto& entry : other._storage)
        {
            auto it = _storage.find(entry.first);
            if(it != _storage.end())
            {
                it->second.combine(entry.second);
            }
            else
            {
                _storage[entry.first] = entry.second.clone();
            }
        }
    }

    void deduplicate()
    {
        for(auto& entry : _storage)
        {
            entry.second.deduplicate();
        }
    }

private:
    std::map<const char*, OptionStorage> _storage;
};

struct ProjectConfig
{
    OptionCollection options;
    std::vector<Project*> links;

    template<typename T>
    T& operator[](Option<T> option)
    {
        return options[option];
    }

    ProjectConfig& operator +=(const OptionCollection& collection)
    {
        options.combine(collection);
        return *this;
    }
};

struct Project : public ProjectConfig
{
    std::string name;
    std::optional<ProjectType> type;
    std::map<ConfigSelector, ProjectConfig, std::less<>> configs;

    Project(std::string name = {}, std::optional<ProjectType> type = {})
        : name(std::move(name)), type(type)
    {
    }

    ProjectConfig resolve(std::optional<ProjectType> projectType, std::string_view configName)
    {
        auto config = internalResolve(projectType, configName, true);
        config.options.deduplicate();
        return config;
    }

    ProjectConfig& operator[](ConfigSelector selector)
    {
        return configs[selector];
    }
    
    template<typename T>
    T& operator[](Option<T> option)
    {
        return options[option];
    }

    void discover(std::set<Project*>& discoveredProjects, std::vector<Project*>& orderedProjects)
    {
        for(auto& link : links)
        {
            link->discover(discoveredProjects, orderedProjects);
        }

        if(discoveredProjects.insert(this).second)
        {
            orderedProjects.push_back(this);
        }
    }

    fs::path calcOutputPath(ProjectConfig& resolvedConfig)
    {
        auto path = resolvedConfig[OutputPath];
        if(!path.empty())
        {
            return path;
        }

        auto stem = resolvedConfig[OutputStem];
        if(stem.empty())
        {
            stem = name;
        }

        return resolvedConfig[OutputDir] / (resolvedConfig[OutputPrefix] + stem + resolvedConfig[OutputSuffix] + resolvedConfig[OutputStem]);
    }

private:
    ProjectConfig internalResolve(std::optional<ProjectType> projectType, std::string_view configName, bool local)
    {
        std::vector<ProjectConfig*> resolveConfigs;

        for(auto& entry : configs)
        {
            if(local)
            {
                if(entry.first.transitivity && entry.first.transitivity == PublicOnly) continue;
            }
            else
            {
                if(!entry.first.transitivity || entry.first.transitivity == Local) continue;
            }
            if(entry.first.projectType && entry.first.projectType != projectType) continue;
            if(entry.first.name && entry.first.name != configName) continue;
            resolveConfigs.push_back(&entry.second);
        }

        ProjectConfig result;

        auto resolveLink = [&](Project* link)
        {
            auto resolved = link->internalResolve(projectType, configName, false);
            result.links += resolved.links;
            result.options.combine(resolved.options);
        };

        for(auto& link : links)
        {
            resolveLink(link);
        }

        for(auto config : resolveConfigs)
        {
            for(auto& link : config->links)
            {
                resolveLink(link);
            }
        }

        auto addOptions = [](auto& a, auto& b)
        {
            a.combine(b);
        };

        if(local)
        {
            addOptions(result.options, options);
        }
        for(auto config : resolveConfigs)
        {
            addOptions(result.options, config->options);
        }
        
        return result;
    }
};

struct GccLikeToolchainProvider : public ToolchainProvider
{
    std::string compiler;
    std::string linker;
    std::string archiver;

    GccLikeToolchainProvider(std::string compiler, std::string linker, std::string archiver)
        : compiler(compiler)
        , linker(linker)
        , archiver(archiver)
    {
    }

    virtual std::string getCompiler(Project& project, ProjectConfig& resolvedConfig, fs::path pathOffset) const override 
    {
        return compiler;
    }

    virtual std::string getCommonCompilerFlags(Project& project, ProjectConfig& resolvedConfig, fs::path pathOffset) const override
    {
        std::string flags;

        for(auto& define : resolvedConfig[Defines])
        {
            flags += " -D\"" + define + "\"";
        }
        for(auto& path : resolvedConfig[IncludePaths])
        {
            flags += " -I\"" + (pathOffset / path).string() + "\"";
        }
        if(resolvedConfig[Platform] == "x64")
        {
            flags += " -m64 -arch x86_64";
        }

        std::map<std::string, std::string> featureMap = {
            { "c++17", " -std=c++17"},
            { "libc++", " -stdlib=libc++"},
            { "optimize", " -O3"},
            { "debuginfo", " -g"},
        };
        for(auto& feature : resolvedConfig[Features])
        {
            auto it = featureMap.find(feature);
            if(it != featureMap.end())
            {
                flags += it->second;
            }
        }

        return flags;
    }

    virtual std::string getCompilerFlags(Project& project, ProjectConfig& resolvedConfig, fs::path pathOffset, const std::string& input, const std::string& output) const override
    {
        return " -MMD -MF " + output + ".d " + " -c -o " + output + " " + input;
    }

    virtual std::string getLinker(Project& project, ProjectConfig& resolvedConfig, fs::path pathOffset) const override
    {
        if(project.type == StaticLib)
        {
            return archiver;
        }
        else
        {
            return linker;
        }
    }

    virtual std::string getCommonLinkerFlags(Project& project, ProjectConfig& resolvedConfig, fs::path pathOffset) const override
    {
        std::string flags;

        switch(*project.type)
        {
        default:
            throw std::runtime_error("Project type in '" + project.name + "' not supported by toolchain.");
        case StaticLib:
            flags += " -rcs";
            break;
        case Executable:
        case SharedLib:
            for(auto& path : resolvedConfig[Libs])
            {
                flags += " " + (pathOffset / path).string();
            }

            for(auto& framework : resolvedConfig[Frameworks])
            {
                flags += " -framework " + framework;
            }

            if(project.type == SharedLib)
            {
                auto features = resolvedConfig[Features];
                if(std::find(features.begin(), features.end(), "bundle") != features.end())
                {
                    flags += " -bundle";
                }
                else
                {
                    flags += " -shared";
                }
            }
            break;
        }

        return flags;
    }

    virtual std::string getLinkerFlags(Project& project, ProjectConfig& resolvedConfig, fs::path pathOffset, const std::vector<std::string>& inputs, const std::string& output) const override
    {
        std::string flags;

        switch(*project.type)
        {
        default:
            throw std::runtime_error("Project type in '" + project.name + "' not supported by toolchain.");
        case StaticLib:
            flags += " \"" + output + "\"";
            for(auto& input : inputs)
            {
                flags += " \"" + input + "\"";
            }
            break;
        case Executable:
        case SharedLib:
            flags += " -o \"" + output + "\"";
            for(auto& input : inputs)
            {
                flags += " \"" + input + "\"";
            }
            break;
        }

        return flags;
    }

    std::vector<fs::path> process(Project& project, ProjectConfig& resolvedConfig, std::string_view config, const fs::path& workingDir, const fs::path& dataDir) const override
    {
        Option<std::vector<fs::path>> LinkedOutputs{"_LinkedOutputs"};
        fs::path pathOffset = fs::proximate(fs::current_path(), workingDir);

        if(project.type != Executable &&
           project.type != SharedLib &&
           project.type != StaticLib)
        {
            return {};
        }

        auto compiler = getCompiler(project, resolvedConfig, pathOffset);
        auto commonCompilerFlags = getCommonCompilerFlags(project, resolvedConfig, pathOffset);
        auto linker = getLinker(project, resolvedConfig, pathOffset);
        auto commonLinkerFlags = getCommonLinkerFlags(project, resolvedConfig, pathOffset);

        auto buildPch = resolvedConfig[BuildPch];
        auto importPch = resolvedConfig[ImportPch];

        if(!buildPch.empty())
        {
            auto input = buildPch;
            auto inputStr = (pathOffset / input).string();
            auto output = dataDir / fs::path("pch") / (input.string() + ".pch");
            auto outputStr = (pathOffset / output).string();

            CommandEntry command;
            command.command = compiler + commonCompilerFlags + " -x c++-header -Xclang -emit-pch " + getCompilerFlags(project, resolvedConfig, pathOffset, inputStr, outputStr);
            command.inputs = { input };
            command.outputs = { output };
            command.workingDirectory = workingDir;
            command.depFile = output.string() + ".d";
            command.description = "Compiling " + project.name + " PCH: " + input.string();
            resolvedConfig[Commands] += std::move(command);
        }

        std::vector<fs::path> pchInputs;
        if(!importPch.empty())
        {
            auto input = dataDir / fs::path("pch") / (importPch.string() + ".pch");
            auto inputStr = (pathOffset / input).string();
            commonCompilerFlags += " -Xclang -include-pch -Xclang " + inputStr;

            pchInputs.push_back(input);
        }

        std::vector<fs::path> linkerInputs;
        for(auto& input : resolvedConfig[Files])
        {
            auto ext = fs::path(input).extension().string();
            auto exts = { ".c", ".cpp", ".mm" }; // TODO: Not hardcode these maybe
            if(std::find(exts.begin(), exts.end(), ext) == exts.end()) continue;

            auto inputStr = (pathOffset / input).string();
            auto output = dataDir / fs::path("obj") / project.name / (input.string() + ".o");
            auto outputStr = (pathOffset / output).string();

            CommandEntry command;
            command.command = compiler + commonCompilerFlags + getCompilerFlags(project, resolvedConfig, pathOffset, inputStr, outputStr);
            command.inputs = { input };
            command.inputs += pchInputs;
            command.outputs = { output };
            command.workingDirectory = workingDir;
            command.depFile = output.string() + ".d";
            command.description = "Compiling " + project.name + ": " + input.string();
            resolvedConfig[Commands] += std::move(command);

            linkerInputs.push_back(output);
        }

        std::vector<fs::path> outputs;

        if(!linker.empty())
        {
            for(auto& output : resolvedConfig[LinkedOutputs])
            {
                linkerInputs.push_back(output);
            }

            std::vector<std::string> linkerInputStrs;
            linkerInputStrs.reserve(linkerInputs.size());
            for(auto& input : linkerInputs)
            {
                linkerInputStrs.push_back((pathOffset / input).string());
            }

            auto output = project.calcOutputPath(resolvedConfig);
            auto outputStr = (pathOffset / output).string();

            CommandEntry command;
            command.command = linker + commonLinkerFlags + getLinkerFlags(project, resolvedConfig, pathOffset, linkerInputStrs, outputStr);
            command.inputs = std::move(linkerInputs);
            command.outputs = { output };
            command.workingDirectory = workingDir;
            command.depFile = output.string() + ".d";
            command.description = "Linking " + project.name + ": " + output.string();
            resolvedConfig[Commands] += std::move(command);

            outputs.push_back(output);

            if(project.type == StaticLib)
            {
                project[Public / config][LinkedOutputs] += output;
            }
        }

        return outputs;
    }
};

class NinjaEmitter
{
public:
    static void emit(fs::path targetPath, std::set<Project*> projects, const std::string& config = {})
    {
        fs::create_directories(targetPath);

        auto outputFile = targetPath / "build.ninja";
        NinjaEmitter ninja(outputFile);

        std::vector<Project*> orderedProjects;
        std::set<Project*> discoveredProjects;
        for(auto project : projects)
        {
            project->discover(discoveredProjects, orderedProjects);
        }

        std::vector<fs::path> generatorDependencies;
        for(auto& project : orderedProjects)
        {
            generatorDependencies += (*project)[GeneratorDependencies];
            for(auto& entry : project->configs)
            {
                generatorDependencies += (*project)[GeneratorDependencies];
            }
        }

        auto buildOutput = fs::path(BUILD_FILE).replace_extension("");
        Project generator("_generator", Executable);
        generator[Features] += { "c++17", "optimize" };
        generator[IncludePaths] += BUILD_H_DIR;
        generator[OutputPath] = buildOutput;
        generator[Defines] += {
            "START_DIR=\\\"" START_DIR "\\\"",
            "BUILD_H_DIR=\\\"" BUILD_H_DIR "\\\"",
            "BUILD_DIR=\\\"" BUILD_DIR "\\\"",
            "BUILD_FILE=\\\"" BUILD_FILE "\\\"",
            "BUILD_ARGS=\\\"" BUILD_ARGS "\\\"",
        };
        generator[Files] += BUILD_FILE;

        generatorDependencies += buildOutput;
        generator[Commands] += { "\"" + (BUILD_DIR / buildOutput).string() + "\" " BUILD_ARGS, generatorDependencies, { outputFile }, START_DIR, {}, "Running build generator." };

        orderedProjects.push_back(&generator);

        for(auto project : orderedProjects)
        {
            auto outputName = emitProject(targetPath, *project, config);
            if(!outputName.empty())
            {
                ninja.subninja(outputName);
            }
        }
    }

private:
    static std::string emitProject(fs::path& root, Project& project, std::string_view config)
    {
        auto resolved = project.resolve(project.type, config);

        for(auto& processor : resolved[PostProcess])
        {
            processor(project, resolved);
        }

        if(!project.type.has_value())
        {
            return {};
        }

        if(project.name.empty())
        {
            throw std::runtime_error("Trying to emit project with no name.");
        }

        std::cout << "Emitting '" << project.name << "'";
        if(!config.empty())
        {
            std::cout << " (" << config << ")";
        }
        std::cout << "\n";

        auto ninjaName = project.name + ".ninja";
        NinjaEmitter ninja(root / ninjaName);

        fs::path pathOffset = fs::proximate(fs::current_path(), root);

        auto& commands = resolved[Commands];
        if(project.type == Command && commands.empty())
        {
            throw std::runtime_error("Command project '" + project.name + "' has no commands.");
        }

        std::vector<std::string> projectOutputs;

        const ToolchainProvider* toolchain = resolved[Toolchain];
        if(!toolchain)
        {
            // TODO: Will be set up elsewhere later
            static GccLikeToolchainProvider defaultToolchainProvider("g++", "g++", "ar");
            toolchain = &defaultToolchainProvider; 
        }

        auto toolchainOutputs = toolchain->process(project, resolved, config, root, root);
        for(auto& output : toolchainOutputs)
        {
            projectOutputs.push_back((pathOffset / output).string());
        }

        std::string prologue;
        if(windows)
        {
            prologue += "cmd /c ";
        }
        prologue += "cd \"$cwd\" && ";
        ninja.rule("command", prologue + "$cmd", "$depfile", "", "$desc");

        for(auto& command : commands)
        {
            fs::path cwd = command.workingDirectory;
            if(cwd.empty())
            {
                cwd = ".";
            }
            std::string cwdStr = (pathOffset / cwd).string();

            std::vector<std::string> inputStrs;
            inputStrs.reserve(command.inputs.size());
            for(auto& path : command.inputs)
            {
                inputStrs.push_back((pathOffset / path).string());
            }

            std::vector<std::string> outputStrs;
            outputStrs.reserve(command.outputs.size());
            for(auto& path : command.outputs)
            {
                outputStrs.push_back((pathOffset / path).string());
            }

            projectOutputs += outputStrs;

            std::string depfileStr;
            if(!command.depFile.empty())
            {
                depfileStr = (pathOffset / command.depFile).string();
            }

            std::vector<std::pair<std::string_view, std::string_view>> variables;
            variables.push_back({"cmd", command.command});
            variables.push_back({"cwd", cwdStr});
            variables.push_back({"depfile", depfileStr});
            if(!command.description.empty())
            {
                variables.push_back({"desc", command.description});
            }
            ninja.build(outputStrs, "command", inputStrs, {}, {}, variables);
        }

        if(!projectOutputs.empty())
        {
            ninja.build({ project.name }, "phony", projectOutputs);
        }

        return ninjaName;
    }

private:
    NinjaEmitter(fs::path path)
        : _stream(path)
    {
    }

    std::ofstream _stream;

    void subninja(std::string_view name)
    {
        _stream << "subninja " << name << "\n";
    }

    void variable(std::string_view name, std::string_view value)
    {
        _stream << name << " = " << value << "\n";
    }

    void rule(std::string_view name, std::string_view command, std::string_view depfile = {}, std::string_view deps = {}, std::string_view description = {})
    {
        _stream << "rule " << name << "\n";
        _stream << "  command = " << command << "\n";
        if(!depfile.empty())
        {
            _stream << "  depfile = " << depfile << "\n";
        }
        if(!deps.empty())
        {
            _stream << "  deps = " << deps << "\n";
        }
        if(!description.empty())
        {
            _stream << "  description = " << description << "\n";
        }
        _stream << "\n";
    }

    void build(const std::vector<std::string>& outputs, std::string_view rule, const std::vector<std::string>& inputs, const std::vector<std::string>& implicitInputs = {}, const std::vector<std::string>& orderInputs = {}, std::vector<std::pair<std::string_view, std::string_view>> variables = {})
    {
        _stream << "build ";
        for(auto& output : outputs)
        {
            _stream << output << " ";
        }

        _stream << ": " << rule << " ";

        for(auto& input : inputs)
        {
            _stream << input << " ";
        }

        if(!implicitInputs.empty())
        {
            _stream << "| ";
            for(auto& implicitInput : implicitInputs)
            {
                _stream << implicitInput << " ";
            }
        }
        if(!orderInputs.empty())
        {
            _stream << "|| ";
            for(auto& orderInput : orderInputs)
            {
                _stream << orderInput << " ";
            }
        }
        _stream << "\n";
        for(auto& variable : variables)
        {
            _stream << "  " << variable.first << " = " << variable.second << "\n";
        }

        _stream << "\n";
    }
};

struct RunResult
{
    int exitCode;
    std::string output;
};

RunResult runCommand(std::string command)
{
    RunResult result;
    {
        auto processPipe = popen(command.c_str(), "r");
        try
        {
            std::array<char, 2048> buffer;
            while(auto bytesRead = fread(buffer.data(), 1, buffer.size(), processPipe))
            {
                result.output.append(buffer.data(), bytesRead);
            }
            
        }
        catch(...)
        {
            pclose(processPipe);
            throw;
        }
        auto status = pclose(processPipe);
        result.exitCode = WEXITSTATUS(status);
    }

    return result;
}

std::string readFile(fs::path path)
{
    std::ifstream stream(path);
    std::stringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

struct JsonValue
{
    enum Type
    {
        VALUE,
        OBJECT,
        ARRAY,
        ERROR
    } type;

    JsonValue() : error("No value"), type(ERROR) {}
    JsonValue(std::string_view value) : value(value), type(VALUE) {}
    JsonValue(std::map<std::string_view, JsonValue> object) : object(object), type(OBJECT) {}
    JsonValue(std::vector<JsonValue> array) : array(array), type(ARRAY) {}
    std::string_view value;
    std::map<std::string_view, JsonValue> object;
    std::vector<JsonValue> array;
    std::string error;

    static JsonValue errorValue(std::string message)
    {
        JsonValue value;
        value.error = std::move(message);
        value.type = ERROR;
        return value;
    }

    size_t size() const
    {
        switch(type)
        {
        case ERROR:
        case VALUE:
            return 0;
        case OBJECT:
            return object.size();
        case ARRAY:
            return array.size();
        }
    }

    bool empty() const
    {
        switch(type)
        {
        case ERROR:
            return true;
        case VALUE:
            return value.empty();
        case OBJECT:
            return object.empty();
        case ARRAY:
            return array.empty();
        }
    }

    const JsonValue& operator [](int index) const
    {
        if(type == ERROR)
        {
            return *this;
        }
        else if(type != ARRAY)
        {
            static JsonValue value = errorValue("Entry is not an array.");
            return value;
        }
        else if(index < 0 || index >= array.size())
        {
            static JsonValue value = errorValue("Index out of range.");
            return value;
        }
        return array[index];
    }

    const JsonValue& operator [](std::string_view key) const 
    {
        if(type == ERROR)
        {
            return *this;
        }
        else if(type != OBJECT)
        {
            static JsonValue value = errorValue("Entry is not an object.");
            return value;
        }
        
        auto it = object.find(key);
        if(it != object.end())
        {
            return it->second;
        }

        {
            static JsonValue value = errorValue("Key not found.");
            return value;
        }
    }

    operator const std::string_view& () const
    {
        return value;
    }

    std::string_view unquoted() const
    {
        if(value.size() >= 2 && value[0] == '"' && value[value.size()-1] == '"')
        {
            return std::string_view(value.begin()+1, value.size() - 2);
        }
        return value;
    }

private:
};

// Believe it or not but this is not a fantastic JSON parser
JsonValue parseJson(std::string_view::iterator& it, std::string_view::iterator end)
{
    auto skipWhitespace = [&]()
    {
        while(std::isspace(*it))
        {
            ++it;
            if(it == end)
            {
                return false;
            }
        }
        return true;
    };

    auto readUntilSeparator = [&](){
        if(!skipWhitespace())
        {
            return JsonValue::errorValue("Unexpected end of file.");
        }

        auto start = it;
        bool inString = false;
        char prev = 0;
        while(inString || (*it != ',' && *it != '}' && *it != ']' && *it != ':')) 
        {
            if(*it == '"' && prev != '\\')
            {
                inString = !inString;
            }
            prev = *it;
            ++it;
            if(it == end)
            {
                return JsonValue::errorValue("Unexpected end of file.");
            }
        }
        return JsonValue(std::string_view(start, it-start));
    };

    if(!skipWhitespace())
    {
        return JsonValue::errorValue("Unexpected end of file.");
    }
    if(*it == '[')
    {
        ++it;

        std::vector<JsonValue> array;
        while(true)
        {
            auto value = readUntilSeparator();
            if(value.type == JsonValue::ERROR)
            {
                return value;
            }

            array.push_back(std::move(value));
            
            if(*it == ']')
            {
                ++it;
                return JsonValue(std::move(array));
            }
            else if(*it != ',')
            {
                return JsonValue::errorValue(std::string("Unexpected '") + *it + "'.");
            }
            ++it;
        }
    }
    else if(*it == '{')
    {
        ++it;

        std::map<std::string_view, JsonValue> object;
        while(true)
        {
            auto key = readUntilSeparator();
            if(key.type == JsonValue::ERROR)
            {
                return key;
            }
            key.value = key.unquoted();

            if(*it != ':')
            {
                return JsonValue::errorValue(std::string("Unexpected '") + *it + "' after reading object key '" + std::string(key.value) + "'.");
            }
            ++it;

            auto value = parseJson(it, end);
            if(value.type == JsonValue::ERROR)
            {
                return value;
            }
            object.insert(std::make_pair(key.value, std::move(value)));

            if(*it == '}')
            {
                ++it;
                return JsonValue(std::move(object));
            }
            else if(*it != ',')
            {
                return JsonValue::errorValue(std::string("Unexpected '") + *it + "' after reading object value with key '" + std::string(key.value) + "'.");
            }
            ++it;
        }
    }
    else
    {
        return readUntilSeparator();
    }
}

JsonValue parseJson(std::string_view json)
{
    auto it = json.begin();
    return parseJson(it, json.end());
}

std::pair<std::string, std::string> splitString(std::string_view str, char delimiter)
{
    auto pos = str.find(delimiter);
    if(pos != str.npos)
    {
        return { std::string(str.substr(0, pos)), std::string(str.substr(pos+1, str.size()-pos-1)) };
    }
    else
    {
        return { std::string(str), "" };
    }
}

std::vector<std::pair<std::string, std::string>> parseOptionArguments(const std::vector<std::string> arguments)
{
    std::vector<std::pair<std::string, std::string>> result;
    for(auto& arg : arguments)
    {
        if(arg.size() > 1 && arg[0] == '-' && arg[1] == '-')
        {
            result.push_back(splitString(arg.substr(2), '='));
        }
    }

    return result;
}

std::vector<std::string> parsePositionalArguments(const std::vector<std::string> arguments, bool skipFirst = true)
{
    std::vector<std::string> result;
    for(auto& arg : arguments)
    {
        if(skipFirst)
        {
            skipFirst = false;
            continue;
        }
        if(arg.size() < 2 || arg[0] != '-' || arg[1] != '-')
        {
            result.push_back(arg);
        }
    }

    return result;
}

OptionCollection sourceList(fs::path path, bool recurse = true)
{
    if(!fs::exists(path) || !fs::is_directory(path))
    {
        throw std::runtime_error("Source directory '" + path.string() + "' does not exist.");
    }

    OptionCollection result;
    auto& files = result[Files];
    auto generatorDeps = result[GeneratorDependencies];

    // Add the directory as a dependency to rescan if the contents change
    generatorDeps += path;

    for(auto entry : fs::recursive_directory_iterator(path))
    {
        if(entry.is_directory())
        {
            // Add subdirectories as dependencies to rescan if the contents change
            generatorDeps += path;
            continue;
        }
        if(!entry.is_regular_file()) continue;

        auto exts = { ".c", ".cpp", ".mm", ".h", ".hpp" }; // TODO: Not hardcode these maybe
        auto ext = entry.path().extension().string();
        if(std::find(exts.begin(), exts.end(), ext) != exts.end())
        {
            files += entry.path();
        }
    }

    return result;
}

void parseCommandLineAndEmit(fs::path startPath, const std::vector<std::string> arguments, std::set<Project*> projects, std::set<std::string> configs)
{
    auto optionArgs = parseOptionArguments(arguments);
    auto positionalArgs = parsePositionalArguments(arguments);

    if(configs.empty())
    {
        throw std::runtime_error("No configurations available.");
    }

    std::vector<std::string> availableEmitters = { "ninja" };
    std::vector<std::pair<std::string, fs::path>> emitters;
    for(auto& arg : optionArgs)
    {
        if(std::find(availableEmitters.begin(), availableEmitters.end(), arg.first) != availableEmitters.end())
        {
            auto targetDir = arg.second;
            if(targetDir.empty())
            {
                targetDir = arg.first + "build";
            }
            emitters.push_back({arg.first, targetDir});
        }
    }

    if(emitters.empty())
    {
        std::cout << "Usage: " << arguments[0] << " --emitter[=†argetDir]\n";
        std::cout << "Example: " << arguments[0] << " --ninja=ninjabuild\n\n";
        std::cout << "Available emitters: \n";
        for(auto& emitter : availableEmitters)
        {
            std::cout << "  --" << emitter << "\n";
        }
        std::cout << "\n\n";
        throw std::runtime_error("No emitters specified.");
    }

    for(auto& emitter : emitters)
    {
        if(emitter.first == "ninja")
        {
            for(auto& config : configs)
            {
                auto outputPath = emitter.second / config;
                if(!outputPath.is_absolute())
                {
                    outputPath = startPath / outputPath;
                }
                NinjaEmitter::emit(outputPath, projects, config);
            }
        }
    }
}

void generate(fs::path startPath, std::vector<std::string> args);
int main(int argc, const char** argv)
{
    try
    {
        auto startPath = fs::current_path();
        fs::current_path(BUILD_DIR);
        startPath = fs::proximate(startPath);
        generate(startPath, std::vector<std::string>(argv, argv+argc));
    }
    catch(const std::exception& e)
    {
        std::cerr << "ERROR: " << e.what() << '\n';
        return -1;
    }
}