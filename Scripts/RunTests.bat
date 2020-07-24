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
call msbuild %1QuantumGate.sln -nologo -m /p:Configuration=ReleaseUnitTests /p:Platform=Win32 -v:quiet -t:Clean
call msbuild %1QuantumGate.sln -nologo -m /p:Configuration=ReleaseUnitTests /p:Platform=x64 -v:quiet -t:Clean

:build
rem Skip build process?
if /i "%args:-nobuild=%" neq "%args%" goto :run_tests

echo.
echo Building tests...
echo.

echo.
echo Configuration Release-Win32...
echo.
call msbuild %1QuantumGate.sln -nologo -m /p:Configuration=ReleaseUnitTests /p:Platform=Win32 -v:quiet
if "%ERRORLEVEL%" neq "0" (
    goto :build_fail
)

echo.
echo Configuration Release-x64...
echo.
call msbuild %1QuantumGate.sln -nologo -m /p:Configuration=ReleaseUnitTests /p:Platform=x64 -v:quiet
if "%ERRORLEVEL%" neq "0" (
    goto :build_fail
)

:run_tests
echo.
echo Running tests...
echo.

echo.
echo Configuration Release-Win32...
echo.
call vstest.console.exe %1Build\Win32\ReleaseUnitTests\UnitTests.dll /Parallel
if "%ERRORLEVEL%" neq "0" (
    goto :test_fail
)

echo.
echo Configuration Release-x64...
echo.
call vstest.console.exe %1Build\x64\ReleaseUnitTests\UnitTests.dll /Parallel
if "%ERRORLEVEL%" neq "0" (
    goto :test_fail
)

echo.
exit /B 0

:need_path
echo.
echo Usage: RunTests.bat [Path] [Options]
echo.
echo Arguments:
echo    Path: The full path to the QuantumGate project root folder as the first parameter (required).
echo.
echo Options:
echo    -noclean: Specifies not to clean the build folders.
echo    -nobuild: Specifies not to build the tests again.
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
echo ERROR: There was a problem building the tests.
echo.

:test_fail
echo.
echo ERROR: There was a problem running the tests.
echo.
exit /B 1