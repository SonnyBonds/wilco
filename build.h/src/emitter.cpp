#include "core/emitter.h"
#include "util/commands.h"

cli::PathArgument targetPath{"build-path", "Target path for build files.", "buildfiles"};

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
}

