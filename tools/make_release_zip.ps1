# Builds the source zip for bakkesplugins.com from the latest commit, so only
# tracked plugin files can end up in it. Run from the project root.
param([string]$OutputPath = "build\release\WallDashTrainer-source.zip")

$paths = @(
    "WallDashTrainer.sln", "WallDashTrainer.vcxproj", "WallDashTrainer.rc", "BakkesMod.props",
    "pch.h", "pch.cpp", "logging.h", "version.h", "update_version.ps1",
    "README.md", "LICENSE", "THIRD_PARTY.md", "src", "third_party"
)

New-Item -ItemType Directory -Force (Split-Path $OutputPath) | Out-Null
git archive --format=zip --output=$OutputPath HEAD -- @paths
if ($LASTEXITCODE -ne 0) { throw "git archive failed" }

$size = (Get-Item $OutputPath).Length / 1MB
Write-Output ("{0}: {1:N1} MB, limit 20 MB" -f $OutputPath, $size)
