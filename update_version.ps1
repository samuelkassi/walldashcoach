# Adds 1 to VERSION_BUILD in the given header. Runs before each build.
param([Parameter(Mandatory = $true)][string]$HeaderPath)

$text = [System.IO.File]::ReadAllText((Resolve-Path $HeaderPath))
$match = [regex]::Match($text, '(#define VERSION_BUILD )(\d+)')
if (-not $match.Success) {
    throw "VERSION_BUILD not found in $HeaderPath"
}

$next = [int]$match.Groups[2].Value + 1
$updated = $text.Remove($match.Index, $match.Length).Insert($match.Index, $match.Groups[1].Value + $next)
[System.IO.File]::WriteAllText((Resolve-Path $HeaderPath), $updated)
