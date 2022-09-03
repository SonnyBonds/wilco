#pragma once

#include "core/environment.h"
#include "core/project.h"
#include "core/property.h"
#include "core/stringid.h"

#include "emitters/compilecommands.h"
#include "emitters/direct.h"
#include "emitters/msvc.h"
#include "emitters/ninja.h"
#include "emitters/query.h"

#include "modules/bundle.h"
#include "modules/command.h"
#include "modules/toolchain.h"

#include "toolchains/cl.h"
#include "toolchains/gcclike.h"
#include "toolchains/detected.h"

#include "util/cli.h"
#include "util/commands.h"
#include "util/hash.h"
#include "util/main.h"
#include "util/process.h"
#include "util/string.h"
#include "util/uuid.h"
