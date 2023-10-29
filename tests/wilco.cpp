#include "wilco.h"

void configure(Environment& env)
{
    Project& tests = env.createProject("Tests", Executable);
    tests.features += { feature::Cpp17, feature::Exceptions };
    tests.includePaths += "../wilco";
    tests.files += "tests.cpp";
    tests.files += env.listFiles("../wilco/src");
}
