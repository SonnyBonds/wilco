$params = @{}
$params["INPUT_CPP"] = $args[0] -replace "\\", "/"
$params["INPUT_BASE"] = (Get-Item $params["INPUT_CPP"]).Name
$params["BUILD_FILE"] = $params["INPUT_BASE"]
$params["OUTPUT_BASE"] = (Get-Item $params["INPUT_CPP"]).BaseName
$params["BUILD_DIR"] = ([String](Split-Path -Path $params["INPUT_CPP"]) -replace "\\", "/")
$params["OUTPUT"] = (Join-Path $params["BUILD_DIR"] ($params["OUTPUT_BASE"] + ".exe")) -replace "\\", "/"
$params["BUILD_H_DIR"] = $PSScriptRoot -replace "\\", "/"
$params["START_DIR"] = (Get-Location).Path -replace "\\", "/"
$params["BUILD_ARGS"] = $args | Select-Object -Skip 1

$display_help = $false
$toolchains = @{}
$selected_toolchain = ""
$selected_toolchain_desc = "(selected by default)"

function Write-Usage()
{
    $command = $MyInvocation.MyCommand
    Write-Host "Usage: $command path/to/build.cpp [--help] [--toolchain=toolchain]" 
    if($toolchains.Count -gt 0)
    {
        Write-Host "Discovered toolchains:"
    }
    foreach ($id in $toolchains.Keys)
    {
        if($selected_toolchain -eq $id)
        {
            $suffix = $selected_toolchain_desc
        }
        else
        {
            $suffix = ""
        }
        Write-Host "  " $id.PadRight(20) " - " $toolchains[$id]["Description"] $suffix
    }    
}

function Exit-Fail()
{
    param (
        $Message
    )

    $Host.UI.WriteErrorLine($Message)
    Exit 1
}

function Exit-Usage()
{
    param (
        $Message
    )

    $Host.UI.WriteErrorLine($Message)
    Write-Usage
    Exit 1
}

if (-not(Test-Path -Path $params["INPUT_CPP"] -PathType Leaf))
{

}

foreach ($arg in ($args | Select-Object -Skip 1))
{
    if($arg -eq "--help")
    {
        $display_help = $true
    }
    elseif($arg.StartsWith("--toolchain"))
    {
        $option, $value = $arg -split "=", 2
        if($value)
        {
            $selected_toolchain = $value
            $selected_toolchain_desc = "(selected)"
        }
        else
        {
            Exit-Usage "Expected value for $option"
        }
    }
    else
    {
        Exit-Usage "Unrecognized option: $arg"
    }
}

function Find-Cl {
    param (
        $Id,
        $Description
    )

    $vswhere = "c:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
    try{ $vcvars = & $vswhere -latest -find **\vcvarsall.bat }
    catch { exit -1 }
    
    $result = & "cmd.exe" /c $vcvars x64 ">nul" "2>nul" "&&" powershell -Command { 
        $result = @{}
        $result["INCLUDE_PATHS"] = $Env:INCLUDE
        $result["LIB_PATHS"] = $Env:LIB
        $result["CL"] = (Get-Command "cl.exe").Source
        $result["LINK"] = (Get-Command "link.exe").Source
        $result["LIB"] = (Get-Command "lib.exe").Source
        Write-Output $result
    }
    
    $CL = [string]$result["CL"] -replace "\\", "/"
    $LINK = [string]$result["LINK"] -replace "\\", "/";
    $LIB = [string]$result["LIB"] -replace "\\", "/";
    $SYS_INCLUDES = ($result["INCLUDE_PATHS"] -replace "\\", "/" -split ";" | ForEach-Object { '"' + $PSItem + '"' }) -join ", "
    $SYS_LIBS = ($result["LIB_PATHS"] -replace "\\", "/"  -split ";" | ForEach-Object { '"' + $PSItem + '"' }) -join ", "

    $lib_flags = ($result["LIB_PATHS"] -split ";" | ForEach-Object { "/LIBPATH:" + $PSItem })
    $include_flags = (($result["INCLUDE_PATHS"] -split ";") + (, $params["BUILD_H_DIR"]) | ForEach-Object { "/I" + $PSItem })
    $define_flags = ($params.Keys | ForEach-Object { "/D" + $PSItem + '=\"' + $params[$PSItem] + '\"' })
    
    $toolchains[$Id] = @{
        Description = $description
        Command = $CL
        Args = ("/nologo", "/std:c++17", "/EHsc", "/Zi") + $include_flags + $define_flags + ($params["INPUT_CPP"], ("/Fe:" + $params["OUTPUT"]), "/link") + $lib_flags
        Declaration = "static ClToolchainProvider ${Id}(`"$Id`", `"$CL`",  `"$LINK`",  `"$LIB`", {$SYS_INCLUDES}, {$SYS_LIBS});`n"
    }

    if(-not $script:selected_toolchain)
    {
        $script:selected_toolchain = $Id
    }
}

function Find-Clang {
    param (
        $Id,
        $Description
    )

    $where = "on path"
    $COMPILER = Get-Command "clang++.exe" -ErrorAction silentlycontinue
    $LINKER = $COMPILER
    $ARCHIVER = Get-Command "llvm-ar.exe" -ErrorAction silentlycontinue
    if(!$COMPILER -or !$LINKER -or !$ARCHIVER)
    {
        $COMPILER = "$env:ProgramFiles/LLVM/bin/clang++.exe"
        $LINKER = $COMPILER
        $ARCHIVER = "$env:ProgramFiles/LLVM/bin/llvm-ar.exe"
        $where = "in $env:ProgramFiles"
    }
    if(!$COMPILER -or !$LINKER -or !$ARCHIVER)
    {
        return
    }
    if (-not(Test-Path -Path $COMPILER -PathType Leaf)) { return }
    if (-not(Test-Path -Path $LINKER -PathType Leaf)) { return }
    if (-not(Test-Path -Path $ARCHIVER -PathType Leaf)) { return }

    $include_flags = ((,$params["BUILD_H_DIR"]) | ForEach-Object { "-I" + $PSItem })
    $define_flags = ($params.Keys | ForEach-Object { "-D" + $PSItem + '=\"' + $params[$PSItem] + '\"' })

    $COMPILER = $COMPILER -replace "\\", "/"
    $LINKER = $LINKER -replace "\\", "/"
    $ARCHIVER = $ARCHIVER -replace "\\", "/"

    $toolchains[$Id] = @{
        Description = $Description + $where
        
        Command = "$COMPILER"
        Args = (,"-std=c++17") + $include_flags + $define_flags + ($params["INPUT_CPP"], "-o", $params["OUTPUT"])
        Declaration = "static GccLikeToolchainProvider ${Id}(`"$Id`", `"$COMPILER`",  `"$LINKER`",  `"$ARCHIVER`");`n"
    }
}

Find-Cl "msvc" "Latest MSVC installation as found by vswhere"
Find-Clang "clang" "Clang as found "

if($display_help)
{
    Write-Usage
    Exit 0
}

if(-not $selected_toolchain)
{
    Exit-Fail "No toolchain found"
}
elseif (-not $toolchains.ContainsKey($selected_toolchain))
{
    Exit-Fail "Can't find toolchain $selected_toolchain"
}

$toolchain_contents = 
@"
#pragma once

#include "toolchains/cl.h"
#include "toolchains/gcclike.h"

namespace detected_toolchains
{`n`n
"@

foreach ($id in $toolchains.Keys)
{
    $toolchain_contents += $toolchains[$id]["Declaration"]    
}

$toolchain_contents += "`n}`n`nToolchainProvider* defaultToolchain = &detected_toolchains::${selected_toolchain};`n"

$toolchain = $toolchains[$selected_toolchain]
& $toolchain["Command"] $toolchain["Args"]

Set-Content -Path ($params["BUILD_H_DIR"] + "/toolchains/_detected_toolchains.h") -Encoding ASCII -Value $toolchain_contents
