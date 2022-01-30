#pragma once

#include "core/option.h"
#include "core/project.h"
#include "core/stringid.h"

#include "emitters/compilecommands.h"
#include "emitters/direct.h"
#include "emitters/ninja.h"

#include "modules/bundle.h"
#include "modules/command.h"
#include "modules/postprocess.h"
#include "modules/standardoptions.h"
#include "modules/toolchain.h"

#include "toolchains/gcclike.h"
#include "toolchains/detected.h"

#include "util/cli.h"
#include "util/commands.h"
#include "util/file.h"
#include "util/glob.h"
#include "util/main.h"
#include "util/operators.h"
#include "util/process.h"
#include "util/string.h"
