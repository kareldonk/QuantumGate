# This file is part of the QuantumGate project. For copyright and
# licensing information refer to the license file(s) in the project root.

# This PowerShell script will increment the build number in a header file.
# The header file should contain a definition for a VERSION_BUILD macro,
# for example: '#define VERSION_BUILD 1'

param([string]$file)

if (![System.IO.File]::Exists($file)) 
{
	Write-Host("EncBuild error: File '" + $file + "' not found")
	exit 1
}

$contents = [System.IO.File]::ReadAllText($file)

$result = [RegEx]::Match($contents, "\bVERSION_BUILD[\s\t]*?(\d+)")
if (!$result.Success)
{
	Write-Host("EncBuild error: Couldn't find version buildnumber definition (#define VERSION_BUILD) in file '" + $file)
	exit 1
}

$buildstr = $result.Groups[1]
$newbuildstr = [int]$buildstr.Value +  1

$contents = [RegEx]::Replace($contents, "\bVERSION_BUILD([\s\t]*?)(\d+)", "VERSION_BUILD`${1}" + $newbuildstr.ToString())

[System.IO.File]::WriteAllText($file, $contents)

Write-Host("EncBuild: Build number increased to " + $newbuildstr + " in '" + $file + "'")

exit $LASTEXITCODE