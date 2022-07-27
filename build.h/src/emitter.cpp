#include "core/emitter.h"
#include "util/glob.h"

static std::vector<Emitter*>& getEmitters()
{
    static std::vector<Emitter*> emitters;
    return emitters;
}

void Emitters::install(Emitter* emitter)
{
    getEmitters().push_back(emitter);
}

const std::vector<Emitter*>& Emitters::list()
{
    return getEmitters();
}

Emitter::Emitter(StringId name, std::string description)
    : name(name)
    , description(std::move(description))
{
    Emitters::install(this);
}

std::pair<Project*, std::filesystem::path> Emitter::createGeneratorProject(Environment& env, std::filesystem::path targetPath)
{
    targetPath = targetPath / ".build.h";
    std::string ext;
    if(OperatingSystem::current() == Windows)
    {
        ext = ".exe";
    }
    auto tempOutput = targetPath / std::filesystem::path(env.configurationFile).filename().replace_extension(ext);
    auto prevOutput = targetPath / std::filesystem::path(env.configurationFile).filename().replace_extension(ext + ".prev");
    auto buildOutput = std::filesystem::path(env.configurationFile).replace_extension(ext);
    Project& project = env.createProject("_generator", Executable);
    project.links = std::vector<Project*>(); 
    project.features += { feature::Cpp17, feature::DebugSymbols, feature::Exceptions, feature::Optimize };
    project.includePaths += env.buildHDir;
    project.output.path = tempOutput;
    project.files += env.configurationFile;
    project.files += glob::files(env.buildHDir / "src");
    project.commands += commands::chain({commands::move(buildOutput, prevOutput), commands::copy(tempOutput, buildOutput)}, "Replacing '" + buildOutput.filename().string() + "'.");

    return { &project, buildOutput };
}
