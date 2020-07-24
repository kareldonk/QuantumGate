@rem This file is part of the QuantumGate project. For copyright and
@rem licensing information refer to the license file(s) in the project root.

@echo off

rem QuantumGate project root folder needs to be passed in as the first parameter.
if [%1] == [] goto :need_path

set "args=%*"

if not exist %1QuantumGate.sln (
    goto :no_solution
)

rem This tries to set up the Visual Studio Developer Command Prompt.
rem If this fails the msbuild command and tests won't work.
call SetupVSDevCmd.bat
if "%ERRORLEVEL%" neq "0" (
    exit /B 1
)

rem Skip cleaning?
if /i "%args:-noclean=%" neq "%args%" goto :build

echo.
echo Cleaning build folders...
echo.
call msbuild %1QuantumGate.sln -nologo -m /p:Configuration=Debug /p:Platform=Win32 -v:quiet -t:Clean
call msbuild %1QuantumGate.sln -nologo -m /p:Configuration=Release /p:Platform=Win32 -v:quiet -t:Clean
call msbuild %1QuantumGate.sln -nologo -m /p:Configuration=Debug /p:Platform=x64 -v:quiet -t:Clean
call msbuild %1QuantumGate.sln -nologo -m /p:Configuration=Release /p:Platform=x64 -v:quiet -t:Clean

:build
echo.
echo Building solution...
echo.

echo Configuration Debug-Win32...
echo.
call msbuild %1QuantumGate.sln -nologo -m /p:Configuration=Debug /p:Platform=Win32 -v:quiet
if "%ERRORLEVEL%" neq "0" (
    goto :build_fail
)

echo.
echo Configuration Release-Win32...
echo.
call msbuild %1QuantumGate.sln -nologo -m /p:Configuration=Release /p:Platform=Win32 -v:quiet
if "%ERRORLEVEL%" neq "0" (
    goto :build_fail
)

echo.
echo Configuration Debug-x64...
echo.
call msbuild %1QuantumGate.sln -nologo -m /p:Configuration=Debug /p:Platform=x64 -v:quiet
if "%ERRORLEVEL%" neq "0" (
    goto :build_fail
)

echo.
echo Configuration Release-x64...
echo.
call msbuild %1QuantumGate.sln -nologo -m /p:Configuration=Release /p:Platform=x64 -v:quiet
if "%ERRORLEVEL%" neq "0" (
    goto :build_fail
)

echo.
exit /B 0

:need_path
echo.
echo Usage: BuildSolution.bat [Path] [Options]
echo.
echo Arguments:
echo    Path: The full path to the QuantumGate project root folder as the first parameter (required).
echo.
echo Options:
echo    -noclean: Specifies not to clean the build folders.
echo.
exit /B 1

:no_solution
echo.
echo ERROR: The QuantumGate.sln solution file was not found in the specified folder (%1). Make sure the folder path ends with a '\'.
echo.
exit /B 1

:exit_vserror
echo.
echo ERROR: Cannot find VsDevCmd.bat. Cannot initialize Visual Studio Developer Command Prompt.
echo.
exit /B 1

:build_fail
echo.
echo ERROR: There was a problem building the solution.
echo.
exit /B 1