# Downloads the binary dependency that is NOT committed to Git: the SDL3
# development package for Visual Studio (prebuilt DLL + import library).
#
# The small source-only libraries (Dear ImGui, nlohmann/json, doctest, stb_image)
# ARE committed under third_party/, each with its license file, so they don't
# need downloading.
#
# Qt 6 is installed separately (it is ~2 GB); see prototypes/README.md.
#
# Usage (from the repository root):  powershell -File prototypes\fetch_deps.ps1

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

$sdlVersion = '3.4.20'
$target = Join-Path $PSScriptRoot 'third_party\SDL3'

if (Test-Path (Join-Path $target 'lib\x64\SDL3.dll')) {
    Write-Host "SDL3 $sdlVersion already present in $target"
    exit 0
}

$zip = Join-Path $env:TEMP "SDL3-devel-$sdlVersion-VC.zip"
$url = "https://github.com/libsdl-org/SDL/releases/download/release-$sdlVersion/SDL3-devel-$sdlVersion-VC.zip"
Write-Host "Downloading $url"
Invoke-WebRequest $url -OutFile $zip

$extract = Join-Path $env:TEMP "SDL3-extract-$sdlVersion"
Expand-Archive $zip $extract -Force
Copy-Item (Join-Path $extract "SDL3-$sdlVersion") $target -Recurse -Force

# Only 64-bit Windows builds are used; drop the x86 and arm64 copies.
foreach ($arch in 'x86', 'arm64') {
    Remove-Item (Join-Path $target "lib\$arch") -Recurse -Force -ErrorAction SilentlyContinue
}
Write-Host "SDL3 $sdlVersion installed to $target"
