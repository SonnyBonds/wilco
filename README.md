# build.h
build.h is a generator framework for writing build files for C++ in C++.

It is currently work in process and in a state of flux both in terms of interface and features. So far only tested on Mac, but should work on Linux with few if any changes. Windows support is a high priority. 

# Philosophy

With build.h the C++ build environment itself is the only requirement for build configuration. Currently "build environment" involves Ninja (or, in the future, GNU make, Visual Studio etc) which very well could be argued is a dependency in itself, but actual build execution is however outside of the current scope of build.h at this point at least.

Build configuration is created by code rather than declarative data. Build.h acts more as a framework for easily generating project dependency graphs and emitting them to an actual builder. The idea is that while the helper classes are designed to make it easy to write project definitions by hand, it also puts many options for discovering & parametrizing projects in the hands of the author of the build configuration.

# Concepts
## Project
A *Project* is the main entity of a build configuration. It consists of a name, a type and a collection of *Options*.

## Options
*Options* can be any kind of additional data that describes a project. Some notable examples of predefined options are:
* *Files* - A list of all source files to include.
* *IncludePaths* - A list of paths to search for includes.
* *Defines* - A list of preprocessor defines to declare when compiling.
* *OutputDir* - The target directory to output binaries to.
* ...etc

Options considered for a project when emitting are determined by their _Selector_, a group of criteria used to filter options. These are currently:
* *Transitivity* - An option can be declared to be applied only for the project itself, or _transitively to dependent projects_. This makes it easy to make sure a project linking to a library automatically gets the correct include path and defines for the library, or to set up common configuration flags, etc. 
* *Project type* - The type of the emitted project.
* *Configuration* - A named configuration, e.g. "debug" or "release".

It is easy to add custom options to projects, but they have very little inherent meaning apart from letting an *Emitter* to do something useful with them. It can however sometimes be useful as metadata resolved by the selector system for custom processing in the project generation itself, or possibly by custom emitters.

## Emitters
An *Emitter* takes the project structure constructed by the generator and emits the build files needed to actually build the projects. Currently only a Ninja emitter exists, but an example of another emitter in mind would be a Visual Studio project emitter.

# Getting started

A very simple generator file could look something like this:
```c++
// build.cpp

#include "build.h"

void generate(fs::path startPath, std::vector<std::string> args)
{
    Project hello("Hello", Executable);
    hello[Files] += "hello.cpp";

    NinjaEmitter::emit("ninja", {&hello});
}
```

This declares one project, called "Hello", with a single file; "hello.cpp" and no extra configuration. The Ninja emitter is then called directly to emit build files for this project into a directory called "ninja".

To actually run this build, the script needs to be compiled. Since compiling C++ is part of the problem we want to solve here, we've got a chicken-and-egg scenario and need to bootstrap the build. To get started, assuming build.h is located in a directory called 'build.h', we run:
```
build.h/bootstrap build.cpp
```
This will figure out the build environment, build and run the generator. *The generated build files include an implicit project that rebuilds the generator itself if needed, so past this point updating the build files becomes a part of the build itself.*

To run the build use the corresponding build tool, e.g.:
```
ninja -C ninja
```

*TODO: The build environment is currently just assumed to exist with clang++ on the path, and clang is the only supported toolchain.*

# Examples
The "example" directory contains a very simple example setting up a Hello executable, linking to a HelloPrinter library that prints "Hello World!".

Some more examples of what project configuration may look like:
```c++
// A project that doesn't generate an output and is just used as configuration doesn't need a name or type 
Project config;

// The syntax for adding options is:
config[Selector][Option] = ...;

// Many options are lists, and appending makes most sense.
// += is overloaded to append individual items or {lists, of, items} to Option vectors
config[Selector][Option] += ...;

// Selectors can be combined with a slash, e.g.
config[StaticLib / "debug"][Option] ...;

// Some more "real world"-ish examples

// Always link with some default lib
config[Public][Libs] += "libsomething.a";

// Define some things in the "debug" config
config[Public / "debug"][Defines] += { 
    "DEBUG", 
    "DEBUG_TESTS=1",
}; 

// "Features" are basically tags that are turned into appropriate compiler flags when compiling
config[Public / "debug"][Feature] += "debuginfo";

// Set an output prefix for library outputs
config[Public / StaticLib][OutputPrefix] = "lib";

// Set an output prefix for library outputs
config[Public / StaticLib][OutputPrefix] = "lib";

// Declare another project with an actual output
Project app("App", Executable);

// "Link" it to the configuration, to get all public options (and link to outputs if any) from it
app.links += &config;

// Add a source file. Note how no specifying a selector adds it for all configurations, local to the project (no transitivity).
app[Files] += "test.cpp";
```

# Future

This is so far mostly a proof of concept and many real life requirements for it to be properly useful are still missing. Some have been mentioned before, but a non-exhaustive list of things to work on is:
* Build environment discovery will likely be done when bootstrapping, with a number of ways to search through to discover a default build environment in most cases, and probably a way to force a specific. The ones currently in mind are:
    * clang++, from path
    * g++, from path
    * (windows) clang++ from installation directory (with winsdk resolved by clang)
    * (windows) cl/winsdk from environment set up by vcvars
    * (windows) cl/winsdk from vswhere + sdk search
* Toolkit setup is part of build environment discovery, but it also ties into translation of options into compiler flags. Currently this is mostly hard coded but there are plans and ideas on how to make this more configurable. Toolkits likely will be tied into project configuration to allow for multiple toolkits within the same project tree, since building the generator might not be done with the same toolkit as the main build in case of cross compiling.
* More emitters, specifically GNU Make and Visual Studio projects
* Possibly more selector criteria, e.g. platform (windows/mac)
* I'm not in love with the `project[Foo / Bar][Option]` syntax but it's not terrible. Operator overloading could make more esoteric things like `project/Foo/Bar > Files += "test.cpp";` possible but I haven't really come up with something I like better.
* The code isn't amazing and can be improved in both style, performance and structure, but I've tried to keep it decent in most aspects.
* There are a few random utils in there right now as well (e.g. a dubious JSON parser). I intend to have common build conf utilities available, but possibly as separate headers as extensions rather than part of the core.