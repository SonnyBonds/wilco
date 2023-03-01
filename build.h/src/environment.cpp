#include "core/environment.h"
#include "util/process.h"
#include "fileutil.h"
#include "dependencyparser.h"
#include <sstream>
#include <fstream>

Environment::Environment(cli::Context& cliContext)
    : configurationFile(process::findCurrentModulePath().replace_extension(".cpp")) // Wish this was a bit more robust but __BASE_FILE__ isn't available everywhere...
    , startupDir(std::filesystem::current_path())
    , buildHDir(std::filesystem::absolute(__FILE__).parent_path().parent_path())
    , cliContext(cliContext)
{
}

Environment::~Environment()
{
}

std::string Environment::readFile(std::filesystem::path path)
{
    addConfigurationDependency(path);
    return ::readFile(path);
}

bool Environment::writeFile(std::filesystem::path path, const std::string& data)
{
    addConfigurationDependency(path);
    return ::writeFile(path, data);
}

std::vector<std::filesystem::path> Environment::listFiles(const std::filesystem::path& path, bool recurse)
{
    std::vector<std::filesystem::path> result;

    if(!std::filesystem::exists(path))
    {
        auto parentPath = path;
        while (parentPath.has_parent_path() && parentPath.has_relative_path())
        {
            parentPath = parentPath.parent_path();
            if(std::filesystem::exists(parentPath))
            {
                addConfigurationDependency(parentPath);
                break;
            }
        }
        return result;
    }

    addConfigurationDependency(path);

    auto scan = [&](auto&& iterator)
    {
        for(auto entry : iterator)
        {
            if(entry.is_directory()) {
                addConfigurationDependency(entry.path());
                continue;
            }
            if(!entry.is_regular_file()) continue;

            result.push_back(entry.path().lexically_normal());
        }
    };

    if(recurse)
    {
        scan(std::filesystem::recursive_directory_iterator(path));
    }
    else
    {
        scan(std::filesystem::directory_iterator(path));
    }

    return result;
}

void Environment::addConfigurationDependency(std::filesystem::path path)
{
    configurationDependencies.insert(path);    
}

Project& Environment::createProject(std::string name, ProjectType type)
{
    projects.emplace_back(new Project(std::move(name), type));
    return *projects.back();
}

ConfigDependencyChecker::ConfigDependencyChecker(Environment& env, std::filesystem::path path)
    : _env(env), _path(path)
{
    _dirty = false;

    std::string argumentString;
    bool first = true;
    for(auto& arg : env.cliContext.allArguments)
    {
        if(first)
        {
            first = false;
            // Skip the "action" argument
            continue;
        }
        argumentString += " " + str::quote(arg);
    }
    if(_env.writeFile(_path.string() + ".cmdline", argumentString))
    {
        _dirty = true;
        return;
    }

    auto depFilePath{_path.string() + ".confdeps"};
    auto depData = readFile(depFilePath);
    if(depData.empty())
    {
        _dirty = true;
        return;
    }
    else
    {
        std::error_code ec;
        auto outputTime = std::filesystem::last_write_time(depFilePath, ec);
        _dirty = parseDependencyData(depData, [outputTime](std::string_view path){
            std::error_code ec;
            auto time = std::filesystem::last_write_time(path, ec);
            return ec || time > outputTime;
        });
    }
}

ConfigDependencyChecker::~ConfigDependencyChecker()
{
    std::stringstream depData;
    depData << ":\n";
    for(auto& dep : _env.configurationDependencies)
    {
        depData << "  " << str::replaceAll(dep.string(), " ", "\\ ") << " \\\n";
    }

    auto depFilePath{_path.string() + ".confdeps"};
    writeFile(depFilePath, depData.str(), false);
}

bool ConfigDependencyChecker::isDirty()
{
    return _dirty;    
}

