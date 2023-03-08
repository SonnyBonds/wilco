#include "core/environment.h"
#include "util/process.h"
#include "fileutil.h"

std::set<std::filesystem::path> Environment::configurationDependencies;

Environment::Environment(cli::Context& cliContext)
    : cliContext(cliContext)
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

