#include "actions/ninja.h"
#include "util/process.h"
#include "buildconfigurator.h"
#include "database.h"
#include "commandprocessor.h"

struct NinjaWriter
{
    std::ofstream _stream;
    NinjaWriter(std::filesystem::path path)
        : _stream(path)
    {
    }

    std::string escape(std::string input)
    {
        str::replaceAllInPlace(input, "$", "$$");
        str::replaceAllInPlace(input, ":", "$:");
        str::replaceAllInPlace(input, " ", "$ ");
        str::replaceAllInPlace(input, "\n", "$\n");
        return input;
    }

    void subninja(std::string_view name)
    {
        _stream << "subninja " << name << "\n";
    }

    void variable(std::string_view name, std::string_view value)
    {
        _stream << name << " = " << value << "\n";
    }

    void rule(std::string_view name, std::string_view command, std::vector<std::pair<std::string_view, std::string_view>> properties)
    {
        _stream << "rule " << name << "\n";
        _stream << "  command = " << command << "\n";
        for(auto& prop : properties)
        {
            _stream << "  " << prop.first << " = " << prop.second << "\n";
        }
        _stream << "\n";
    }

    void build(const std::vector<std::string>& outputs, std::string_view rule, const std::vector<std::string>& inputs, const std::vector<std::string>& implicitInputs = {}, const std::vector<std::string>& orderInputs = {}, std::vector<std::pair<std::string_view, std::string_view>> variables = {})
    {
        _stream << "build ";
        for(auto& output : outputs)
        {
            _stream << escape(output) << " ";
        }

        _stream << ": " << rule << " ";

        for(auto& input : inputs)
        {
            _stream << escape(input) << " ";
        }

        if(!implicitInputs.empty())
        {
            _stream << "| ";
            for(auto& implicitInput : implicitInputs)
            {
                _stream << escape(implicitInput) << " ";
            }
        }
        if(!orderInputs.empty())
        {
            _stream << "|| ";
            for(auto& orderInput : orderInputs)
            {
                _stream << escape(orderInput) << " ";
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

static std::string_view depFileFormatStr(const CommandEntry& command)
{
    switch(command.depFile.format)
    {
        case DepFile::Format::GCC:
            return "gcc";
        case DepFile::Format::MSVC:
            return "msvc";
        default:
            throw std::runtime_error("Unknown depfile format for '" + command.description + "'.");
    }
}

static std::string emitProject(Environment& env, const std::filesystem::path& projectDir, Project& project, std::string profileName, bool wilco)
{
    std::filesystem::path dataDir = project.dataDir;
    if(dataDir.empty())
    {
        dataDir = projectDir;
    }

    if(project.name.empty())
    {
        throw std::runtime_error("Trying to emit project with no name.");
    }

    std::cout << "Emitting '" << project.name << "'";
    if(!profileName.empty())
    {
        std::cout << " (" << profileName << ")";
    }
    std::cout << "\n";

    auto ninjaName = project.name + ".ninja";
    NinjaWriter ninja(projectDir / ninjaName);

    std::filesystem::path pathOffset = std::filesystem::proximate(std::filesystem::current_path(), projectDir);

    auto& commands = project.commands;
    if(project.type == Command && commands.empty())
    {
        throw std::runtime_error("Command project '" + project.name + "' has no commands.");
    }

    std::vector<std::string> projectOutputs;

    const ToolchainProvider* toolchain = project.toolchain;
    if(!toolchain)
    {
        toolchain = defaultToolchain;
    }

    auto toolchainOutputs = toolchain->process(project, projectDir, dataDir);
    for(auto& output : toolchainOutputs)
    {
        projectOutputs.push_back((pathOffset / output).string());
    }

    std::string prologue;
    // TODO: Current isn't necessarily the host system
    if(OperatingSystem::current() == Windows)
    {
        prologue += "cmd /c ";
    }
    prologue += "cd \"$cwd\" && ";
    if(wilco)
    {
        ninja.rule("command", prologue + "$cmd", {
            {"depfile", "$command_depfile"}, 
            {"deps", "$command_depfile_format"}, 
            {"description", "$command_desc"},
            {"rspfile", "$command_rspfile"},
            {"rspfile_content", "$command_rspfile_content"},
            {"generator", "1"},
            {"restat", "1"},
        });
    }
    else
    {
        ninja.rule("command", prologue + "$cmd", {
            {"depfile", "$command_depfile"}, 
            {"deps", "$command_depfile_format"}, 
            {"description", "$command_desc"},
            {"rspfile", "$command_rspfile"},
            {"rspfile_content", "$command_rspfile_content"},
        });
    }

    std::vector<std::string> wilcoDep = { "_wilco" };
    std::vector<std::string> emptyDeps = { };
 
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
            if(wilco && path.extension() == ".ninja")
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
        if(!command.depFile.path.empty())
        {
            depfileStr = (pathOffset / command.depFile).string();
        }

        std::string rspfileStr;
        if(!command.rspFile.empty())
        {
            rspfileStr = (pathOffset / command.rspFile).string();
        }

        std::vector<std::pair<std::string_view, std::string_view>> variables;
        variables.push_back({"cmd", command.command});
        variables.push_back({"cwd", cwdStr});
        if(!depfileStr.empty())
        {
            variables.push_back({"command_depfile", depfileStr});
            variables.push_back({"command_depfile_format", depFileFormatStr(command)});
        }
        if(!rspfileStr.empty())
        {
            variables.push_back({"command_rspfile", rspfileStr});
            variables.push_back({"command_rspfile_content", command.rspContents});
        }
        if(!command.description.empty())
        {
            variables.push_back({"command_desc", command.description});
        }

        std::vector<std::string> confDeps;
        if(wilco)
        {
            for(auto& dep : env.configurationDependencies)
            {
                confDeps.push_back((pathOffset / dep).string());
            }            
        }
        ninja.build(outputStrs, "command", inputStrs, confDeps, wilco ? emptyDeps : wilcoDep, variables);
    }

    if(!projectOutputs.empty())
    {
        ninja.build({ project.name }, "phony", projectOutputs);
    }

    return ninjaName;
}

ActionInstance<NinjaEmitter> NinjaEmitter::instance;

NinjaEmitter::NinjaEmitter()
    : Action("ninja", "Generate ninja build files.")
{
}

void NinjaEmitter::run(cli::Context cliContext)
{
    auto configDatabasePath = *targetPath / ".ninja_db";
    Database configDatabase;
    bool configDirty = !configDatabase.load(configDatabasePath);

    std::vector<std::string> args;
    args = cliContext.allArguments;
    {
        auto previousArgs = BuildConfigurator::getPreviousConfigDatabaseArguments(configDatabase);
        if(!previousArgs || args != *previousArgs)
        {
            configDirty = true;
        }
    }

    if(!configDirty)
    {
        auto configCommands = filterCommands(configDatabase, cliContext.startPath, {});
        configDirty = !configCommands.empty();
    }

    if(!configDirty)
    {
        return;
    }

    cliContext.extractArguments(arguments);

    auto profiles = cli::Profile::list();

    if(profiles.empty())
    {
        profiles.push_back(cli::Profile{ "", {} });
    }

    std::set<std::filesystem::path> configDependencies;

    for(auto& profile : profiles)
    {
        std::vector<std::string> confArgs = { std::string("--profile=") + profile.name.c_str() };
        confArgs.insert(confArgs.end(), cliContext.unusedArguments.begin(), cliContext.unusedArguments.end());
        cli::Context configureContext(cliContext.startPath, cliContext.invocation, confArgs);

        Environment env = BuildConfigurator::configureEnvironment(configureContext);

        std::filesystem::path profileTargetPath = *targetPath;
        if(!profile.name.empty())
        {
            profileTargetPath = profileTargetPath / profile.name.c_str();
        }
        std::filesystem::create_directories(profileTargetPath);

        auto outputFile = profileTargetPath / "build.ninja";
        NinjaWriter ninja(outputFile);

        std::vector<std::filesystem::path> outputs;
        for(auto& project : env.projects)
        {
            auto outputName = emitProject(env, profileTargetPath, *project, profile.name, false);
            if(!outputName.empty())
            {
                ninja.subninja(outputName);
                outputs.push_back(outputName);
            }
        }

        outputs.push_back("build.ninja");

        auto& wilcoProject = env.createProject("_wilco", Command);

        std::string argumentString;
        for(auto& arg : cliContext.allArguments)
        {
            argumentString += " " + str::quote(arg);
        }
        
        wilcoProject.commands += CommandEntry{ str::quote(process::findCurrentModulePath().string()) + argumentString, {}, outputs, cliContext.startPath, {}, "Check build config." };
        auto outputName = emitProject(env, profileTargetPath, wilcoProject, "", true);
        ninja.subninja(outputName);

        configDependencies.insert(env.configurationDependencies.begin(), env.configurationDependencies.end());
    }

    BuildConfigurator::updateConfigDatabase(configDependencies, configDatabase, args);

    configDatabase.save(configDatabasePath);
}

