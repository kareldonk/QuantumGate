@rem This file is part of the QuantumGate project. For copyright and
@rem licensing information refer to the license file(s) in the project root.

@echo off

set vsdevcmd="C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat"
if not exist %vsdevcmd% (
    goto :exit_vserror
)
call %vsdevcmd%

exit /B 0

:exit_vserror
echo.
echo ERROR: Cannot find VsDevCmd.bat. Cannot setup Visual Studio Developer Command Prompt. You may need to change the path inside the 'SetupVSDevCmd.bat' file.
echo.
exit /B 1
