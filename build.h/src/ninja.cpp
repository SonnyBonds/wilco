#include "emitters/ninja.h"

struct NinjaWriter
{
    std::ofstream _stream;
    NinjaWriter(std::filesystem::path path)
        : _stream(path)
    {
    }

    void subninja(std::string_view name)
    {
        _stream << "subninja " << name << "\n";
    }

    void variable(std::string_view name, std::string_view value)
    {
        _stream << name << " = " << value << "\n";
    }

    void rule(std::string_view name, std::string_view command, std::string_view depfile = {}, std::string_view deps = {}, std::string_view description = {}, bool generator = false)
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
        if(generator)
        {
            _stream << "  generator = 1\n";
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

static std::string emitProject(Environment& env, const std::filesystem::path& suggestedDataDir, Project& project, StringId config, bool generator)
{
    auto resolved = project.resolve(env, suggestedDataDir, config, OperatingSystem::current());
    auto root = resolved.dataDir;

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
    NinjaWriter ninja(root / ninjaName);

    std::filesystem::path pathOffset = std::filesystem::proximate(std::filesystem::current_path(), root);

    auto& commands = resolved.commands;
    if(project.type == Command && commands.value().empty())
    {
        throw std::runtime_error("Command project '" + project.name + "' has no commands.");
    }

    std::vector<std::string> projectOutputs;

    const ToolchainProvider* toolchain = resolved.toolchain;
    if(!toolchain)
    {
        toolchain = defaultToolchain;
    }

    auto toolchainOutputs = toolchain->process(project, resolved, config, root);
    for(auto& output : toolchainOutputs)
    {
        projectOutputs.push_back((pathOffset / output).string());
    }

    std::string prologue;
    // TODO: Target platform
    /*if(windows)
    {
        prologue += "cmd /c ";
    }*/
    prologue += "cd \"$cwd\" && ";
    ninja.rule("command", prologue + "$cmd", "$depfile", "", "$desc", generator);

    std::vector<std::string> generatorDep = { "_generator" };
    std::vector<std::string> emptyDep = {};

    for(auto& command : commands)
    {
        std::filesystem::path cwd = command.workingDirectory;
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
            if(generator && path.extension() == ".ninja")
            {
                outputStrs.push_back(path.string());
            }
            else
            {
                outputStrs.push_back((pathOffset / path).string());
            }
        }

        projectOutputs.insert(projectOutputs.end(), outputStrs.begin(), outputStrs.end());

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
        ninja.build(outputStrs, "command", inputStrs, {}, project.name == "_generator" ? emptyDep : generatorDep, variables);
    }

    if(!projectOutputs.empty())
    {
        ninja.build({ project.name }, "phony", projectOutputs);
    }

    return ninjaName;
}

NinjaEmitter NinjaEmitter::instance;

NinjaEmitter::NinjaEmitter()
    : Emitter("ninja", "Generate ninja build files.")
{
}

void NinjaEmitter::emit(Environment& env)
{
    auto projects = env.collectProjects();

    std::vector<std::filesystem::path> outputs;
    auto configs = env.collectConfigs();
    for(auto& config : configs)
    {
        std::filesystem::path configTargetPath = *targetPath;
        if(!config.empty())
        {
            configTargetPath = configTargetPath / config.cstr();
        }
        std::filesystem::create_directories(configTargetPath);

        auto outputFile = configTargetPath / "build.ninja";
        NinjaWriter ninja(outputFile);

        for(auto project : projects)
        {
            auto outputName = emitProject(env, configTargetPath, *project, config, false);
            if(!outputName.empty())
            {
                ninja.subninja(outputName);
                outputs.push_back(outputName);
            }
        }

        std::vector<std::filesystem::path> configurationDependencies;
        configurationDependencies.insert(configurationDependencies.end(), env.configurationDependencies.begin(), env.configurationDependencies.end());

        auto [generator, buildOutput] = createGeneratorProject(env, *targetPath);
        outputs.push_back("build.ninja");
        configurationDependencies.push_back(buildOutput);

        std::string argumentString;
        for(auto& arg : env.cliContext.allArguments)
        {
            argumentString += " " + str::quote(arg);
        }
        
        generator->commands += CommandEntry{ str::quote((env.configurationFile.parent_path() / buildOutput).string()) + argumentString, configurationDependencies, outputs, env.startupDir, {}, "Running build generator." };
        auto outputName = emitProject(env, configTargetPath, *generator, "", true);
        ninja.subninja(outputName);
    }
}

