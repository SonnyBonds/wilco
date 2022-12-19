#include "core/environment.h"
#include "util/process.h"
#include "fileutil.h"
#include <fstream>

Environment::Environment(cli::Context& cliContext)
    : configurationFile(process::findCurrentModulePath().replace_extension(".cpp")) // Wish this was a bit more robust but __BASE_FILE__ isn't available everywhere...
    , startupDir(std::filesystem::current_path())
    , buildHDir(std::filesystem::absolute(__FILE__).parent_path().parent_path())
    , cliContext(cliContext)
{
    configurations.insert({});
}

Environment::~Environment()
{
}

Project& Environment::createProject(std::string name, ProjectType type)
{
    _projects.emplace_back(new Project(name, type));
    _projects.back()->import(defaults);
    return *_projects.back();
}

std::vector<Project*> Environment::collectProjects()
{
    // TODO: Probably possible to do this more efficiently

    std::vector<Project*> orderedProjects;
    std::set<Project*> collectedProjects;

    for(auto& project : _projects)
    {
        collectOrderedProjects(project.get(), collectedProjects, orderedProjects);
    }

    return orderedProjects;
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

void Environment::collectOrderedProjects(Project* project, std::set<Project*>& collectedProjects, std::vector<Project*>& orderedProjects)
{
#if TODO
    for(auto link : project->links)
    {
        collectOrderedProjects(link, collectedProjects, orderedProjects);
    }
#endif

    if(collectedProjects.insert(project).second)
    {
        orderedProjects.push_back(project);
    }
}