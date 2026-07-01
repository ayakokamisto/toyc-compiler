param(
  [string]$JavaRoot = "toy-c-compiler-master",
  [string]$Src2Root = "src2",
  [string]$NewCppCompiler = "build\toycc.exe",
  [string]$CasesDir = "toy-c-compiler-master\src\test\resources",
  [string]$ReportsDir = "tools\triple_diff\reports",
  [switch]$NoBuildJava,
  [switch]$NoBuildSrc2
)

$ErrorActionPreference = "Stop"
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$Script = Join-Path $PSScriptRoot "run_triple_diff.sh"
$ReportsPath = Join-Path $RepoRoot $ReportsDir

function Convert-ToWslPath([string]$Path, [switch]$AllowMissing) {
  $candidate = if ([System.IO.Path]::IsPathRooted($Path)) { $Path } else { Join-Path $RepoRoot $Path }
  if ((-not (Test-Path $candidate)) -and $AllowMissing) {
    $full = [System.IO.Path]::GetFullPath($candidate)
  } else {
    $full = (Resolve-Path $candidate).Path
  }
  if ($full -notmatch '^([A-Za-z]):\\(.*)$') {
    throw "cannot convert path to WSL form: $full"
  }
  $drive = $Matches[1].ToLowerInvariant()
  $tail = $Matches[2] -replace '\\', '/'
  return "/mnt/$drive/$tail"
}

Push-Location $RepoRoot
try {
  New-Item -ItemType Directory -Force -Path $ReportsPath | Out-Null
  $NoBuildJavaFlag = if ($NoBuildJava) { "1" } else { "0" }
  $NoBuildSrc2Flag = if ($NoBuildSrc2) { "1" } else { "0" }

  $argsList = @(
    (Convert-ToWslPath $RepoRoot),
    (Convert-ToWslPath $JavaRoot),
    (Convert-ToWslPath $Src2Root),
    (Convert-ToWslPath $NewCppCompiler -AllowMissing),
    (Convert-ToWslPath $CasesDir),
    (Convert-ToWslPath $ReportsPath),
    $NoBuildJavaFlag,
    $NoBuildSrc2Flag
  )

  & wsl.exe bash (Convert-ToWslPath $Script) @argsList
  if ($LASTEXITCODE -ne 0) {
    throw "triple diff failed with exit $LASTEXITCODE"
  }
}
finally {
  Pop-Location
}
