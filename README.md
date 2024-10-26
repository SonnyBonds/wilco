# Wilco
Wilco is a system for writing build configuration for C++ in C++, including a fast parallel build runner.

Wilco is functional and used in production, but not locked down in terms of interface and features. Supports building with clang, GCC and MSVC on Windows, macOS and Linux.

In addition to building projects itself, Wilco can emit MSVC projects as well as Ninja files.

# TL;DR - Let's run the example

- Clone the repository.
- `wilco/bootstrap example/wilco.cpp`
- `example/wilco configure`
- `example/wilco build`
- `example/bin/Hello`
- Revel.

# Philosophy

With Wilco the C++ build environment itself is the only requirement for build configuration. Wilco can either perform a build itself or generate build files for a build system like Ninja or Visual Studio.

Build configuration is created by code rather than declarative data. Wilco acts as a framework for easily generating project dependency graphs. The idea is that while the helper classes are designed to make it easy to write project definitions by hand, it also puts many options for discovering & parametrizing projects in the hands of the author of the build configuration.

# Concepts
## Project
A *Project* is the main entity of a build configuration. It consists of a name, a type and a collection of *Properties*.

## Properties
*Properties* can be any kind of additional data that describes a project. Some notable examples of default properties are:
* *files* - A list of all source files to include.
* *includePaths* - A list of paths to search for includes.
* *defines* - A list of preprocessor defines to declare when compiling.
* *output.dir* - The target directory to output binaries to.
* ...etc

It is possible to add custom properies to projects, but they have very little inherent meaning apart from letting an *Action* do something useful with them. It can however sometimes be useful as metadata in the project generation itself, or possibly by custom actions.

## Actions
An *Action* takes the project structure constructed by the generator and does something useful with it, e.g. building output binaries or generating build files for an IDE or other build system. Examples of existing actions are:
* *Build* action, which builds the project without further ado.
* *Ninja* action, which generates Ninja files to use for building.

## Profiles
A profile is a set of configuration arguments, defining common combinations of options.

# Getting started

A very simple generator file could look something like this:
```c++
// build.cpp

#include "wilco.h"

cli::BoolArgument optimize{"optimize", "Enable optimizations."};

cli::Profile release("release", { "--optimize" });
cli::Profile debug("debug", { });

void configure(Environment& env)
{
    Project& hello = env.createProject("Hello", Executable);
    hello.files += "hello.cpp";
    if(optimize)
    {
        hello.features += feature::Optimize;
    }
}
```

This specifies that we have a "debug" and a "release" profile, declares one project called "Hello", with a single file; "hello.cpp", and enables optimization when using the "release" profile.

To actually run this build, the script needs to be compiled. Since compiling C++ is part of the problem we want to solve here, we've got a chicken-and-egg scenario and need to bootstrap the build. To get started, assuming Wilco is located in a directory called 'wilco', we run:
```
wilco/bootstrap wilco.cpp
```
This will figure out the build environment, and build the generator. *The generated build files include an implicit project that rebuilds the generator itself if needed, so past this point updating the build files becomes a part of the build itself.* To run the build, just run `./wilco build`.

# Examples
The "example" directory contains a very simple example setting up a Hello executable, linking to a HelloPrinter library that prints "Hello World!".

Some more examples of what project configuration may look like:
```c++
// Projects are created in a configuration
Project& lib = config.createProject("SomeLibrary", StaticLib);

// Properties are just normal members in the project:
lib.property = ...;

// Many properties are lists, and appending makes most sense.
// += is overloaded to append individual items or {lists, of, items} to list properties
lib.includePaths += "lib/include";
lib.includePaths += { "other_path", "third_path" };

// "Features" are basically tags that are turned into appropriate compiler flags when compiling
lib.features += feature::DebugSymbols;

// But custom flags can also be specified with compiler specific extensions
lib.ext<extensions::Gcc>().compilerFlags += "-Wno-everything";

// Projects have a set of exported properties that can be used by "consumers" of the project
// For example, we might want dependents to get the library's include path as well:
lib.exports.includePaths += "lib/include";

// A project that wants to use the library imports it. This applies all the
// properties in its exports section. It also adds a dependency link between
// the projects so that output of the dependency is added to the linker input
// of dependent projects.
application.import(lib);

// Bundles of project properties not attached to a project can also be created,
// for example to define a set of defaults
ProjectSettings defaults;
defaults.output.dir = "bin";
if(config.name == "debug")
{
    defaults.output.suffix = "Debug";
}

// ProjectSettings are applied to a project with import as well:
application.import(defaults);

// Source files can be added directly:
application.files += "test.cpp";

// It is also possible to add all files in a directory as source files.
// This will also make sure the configuration is re-run if any files are added or removed
application.files += env.listFiles("src");
```

# Future

While stable and working in production for non-trivial use in at least one location, the functionality that exists is crafted around what has been needed for that particular use case. Other environments may have other needs that is currently not supported. 
A non-exhaustive list of things to work on is:
* Build environment discovery done by the bootstrapper works for simple environments, but should be extended to be much more versatile.
* C++20 module support.
* More examples and project templates.
* Improve and clean up project structure.