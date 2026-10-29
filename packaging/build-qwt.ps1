# Download, verify and build Qwt (release only) with MSVC against the Qt 6
# found via $env:QMAKE or qmake on PATH, installing it into the given prefix.
#
# usage: packaging\build-qwt.ps1 -Prefix C:\Qwt [-Version 6.3.0]
#        $env:QWT_TARBALL = 'C:\path\qwt-6.3.0.tar.bz2'  (offline builds)
#
# The Visual Studio developer environment is loaded automatically (vswhere)
# when cl.exe is not already on PATH.
param(
  [Parameter(Mandatory = $true)][string]$Prefix,
  [string]$Version = '6.3.0'
)
$ErrorActionPreference = 'Stop'

# SHA-256 of the official SourceForge tarballs (same values as Homebrew's)
$checksums = @{
  '6.3.0' = 'dcb085896c28aaec5518cbc08c0ee2b4e60ada7ac929d82639f6189851a6129a'
}
if (-not $checksums.ContainsKey($Version)) {
  throw "No known checksum for Qwt $Version; add it to $PSCommandPath"
}

function Import-VsDevEnv {
  if (Get-Command cl.exe -ErrorAction SilentlyContinue) { return }
  $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
  $vs = & $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
  if (-not $vs) { throw 'Visual Studio with C++ tools not found' }
  $vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
  cmd /c "`"$vcvars`" >nul && set" | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)$') { Set-Item -Path "env:$($Matches[1])" -Value $Matches[2] }
  }
}

$qmake = $env:QMAKE
if (-not $qmake) { $qmake = (Get-Command qmake6, qmake -ErrorAction SilentlyContinue | Select-Object -First 1).Source }
if (-not $qmake) { throw 'qmake for Qt 6 not found; set $env:QMAKE' }
Write-Host "Building Qwt $Version with $qmake ($(& $qmake -query QT_VERSION))"

Import-VsDevEnv

$work = Join-Path ([IO.Path]::GetTempPath()) ("qwt-" + [guid]::NewGuid())
New-Item -ItemType Directory -Path $work | Out-Null
try {
  $tarball = Join-Path $work "qwt-$Version.tar.bz2"
  if ($env:QWT_TARBALL) {
    Copy-Item $env:QWT_TARBALL $tarball
  } else {
    & curl.exe -fsSL --retry 5 --retry-delay 5 -o $tarball `
      "https://downloads.sourceforge.net/project/qwt/qwt/$Version/qwt-$Version.tar.bz2"
    if ($LASTEXITCODE) { throw 'Qwt download failed' }
  }
  $hash = (Get-FileHash -Algorithm SHA256 $tarball).Hash.ToLower()
  if ($hash -ne $checksums[$Version]) { throw "Qwt checksum mismatch: $hash" }
  Write-Host "Checksum OK"

  & tar.exe -xjf $tarball -C $work
  if ($LASTEXITCODE) { throw 'Qwt extraction failed' }
  Push-Location (Join-Path $work "qwt-$Version")

  # install prefix + only what QSapecNG needs
  $prefixFwd = ($Prefix -replace '\\', '/')
  $cfg = Get-Content -Raw qwtconfig.pri
  $cfg = $cfg -replace '(?m)^(\s*QWT_INSTALL_PREFIX\s*=)\s*C:/Qwt-.*$', "`$1 $prefixFwd"
  $cfg = $cfg -replace '(?m)^(QWT_CONFIG\s*\+=\s*Qwt(Designer|Examples|Playground|Tests))', '#$1'
  Set-Content -NoNewline -Path qwtconfig.pri -Value $cfg

  # release only (the default on Windows is debug_and_release + build_all)
  $bld = Get-Content -Raw qwtbuild.pri
  $bld = $bld -replace '(?m)^(\s*)CONFIG\s*\+=\s*debug_and_release\s*$', '$1CONFIG += release'
  $bld = $bld -replace '(?m)^\s*CONFIG\s*\+=\s*build_all\s*$', ''
  Set-Content -NoNewline -Path qwtbuild.pri -Value $bld

  & $qmake qwt.pro
  if ($LASTEXITCODE) { throw 'qmake failed' }
  $make = 'nmake'
  if (Get-Command jom.exe -ErrorAction SilentlyContinue) { $make = 'jom' }
  & $make
  if ($LASTEXITCODE) { throw 'Qwt build failed' }
  & $make install
  if ($LASTEXITCODE) { throw 'Qwt install failed' }
  Pop-Location
  Write-Host "Qwt installed to $Prefix"
}
finally {
  Remove-Item -Recurse -Force $work -ErrorAction SilentlyContinue
}
