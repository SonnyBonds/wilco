# build.h
build.h is a generator framework for writing build files for C++ in C++.

It is currently work in process and in a state of flux both in terms of interface and features. So far only tested on Mac, but should work on Linux with few if any changes. Windows support is a high priority. 

# TL;DR - Let's run the example

- Clone the repository.
- `build.h/bootstrap example/build.cpp`
- `example/build direct`
- `example/bin/Hello`
- Revel.

# Philosophy

With build.h the C++ build environment itself is the only requirement for build configuration. build.h can either perform a build itself or generate build files for a build system like Ninja (or, in the future, GNU make, Visual Studio etc). 

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
* *Operating System* - The target operating system of the build.
* *Configuration* - A named configuration, e.g. "debug" or "release".

It is easy to add custom options to projects, but they have very little inherent meaning apart from letting an *Emitter* do something useful with them. It can however sometimes be useful as metadata resolved by the selector system for custom processing in the project generation itself, or possibly by custom emitters.

## Emitters
An *Emitter* takes the project structure constructed by the generator and emits the build files needed to actually build the projects. Examples of existing emitters are:
* *Direct* builder, which builds the project without further ado.
* *Ninja* emitter, which emits Ninja files to use for building.
* *CompileCommands* emitter, which emits a compile_commands.json file that can be used as metadata by applications like VS Code.
# Getting started

A very simple generator file could look something like this:
```c++
// build.cpp

#include "build.h"

void generate(fs::path startPath, std::vector<std::string> args)
{
    Project hello("Hello", Executable);
    hello[Files] += "hello.cpp";

    cli::parseCommandLineAndEmit(startPath, args, {&hello});
}
```

This declares one project, called "Hello", with a single file; "hello.cpp" and no extra configuration. The command line is then parsed and the selected action is performed on the single 'hello' project. (Note that using the built in command line parser is a choice, and it is fully possible to implement custom command line syntax, hardcode what actions to perform, or read from a configuration file.)

To actually run this build, the script needs to be compiled. Since compiling C++ is part of the problem we want to solve here, we've got a chicken-and-egg scenario and need to bootstrap the build. To get started, assuming build.h is located in a directory called 'build.h', we run:
```
build.h/bootstrap build.cpp
```
This will figure out the build environment, and build the generator. *The generated build files include an implicit project that rebuilds the generator itself if needed, so past this point updating the build files becomes a part of the build itself.* To run the build, just run `./build`.

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

// Declare another project with an actual output
Project app("App", Executable);

// "Link" it to the configuration, to get all public options (and link to outputs if any) from it
app.links += &config;

// Add a source file. Note how no specifying a selector adds it for all configurations, local to the project (no transitivity).
app[Files] += "test.cpp";
```

# Future

This is so far mostly a proof of concept and many real life requirements for it to be properly useful are still missing. Some have been mentioned before, but a non-exhaustive list of things to work on is:
* Windows support.
* The internal builder is a decent build runner, but still some missing features and edge cases that probably need addressing at some point.
* Build environment discovery done by the bootstrapper works for simple environments, but can probably fairly easily be extended to be much more versatile.
* More emitters, specifically Visual Studio projects
* Possibly more selector criteria
* Cross compiling has been in mind when designing the system, but likely needs more support to actually be doable. Especially if full host/build/target separation is to be done.