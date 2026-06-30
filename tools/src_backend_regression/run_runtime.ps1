param(
  [string]$BuildDir = "build",
  [switch]$NoBuild
)

$ErrorActionPreference = "Stop"
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$BuildPath = Join-Path $RepoRoot $BuildDir
$Toycc = Join-Path $BuildPath "toycc.exe"
$CasesDir = Join-Path $PSScriptRoot "cases"
$ReportsDir = Join-Path $PSScriptRoot "reports"
$RuntimeScript = Join-Path $PSScriptRoot "run_runtime.sh"

function Convert-ToWslPath([string]$Path) {
  $resolved = (Resolve-Path $Path).Path
  if ($resolved -notmatch '^([A-Za-z]):\\(.*)$') {
    throw "cannot convert path to WSL form: $resolved"
  }
  $drive = $Matches[1].ToLowerInvariant()
  $tail = $Matches[2] -replace '\\', '/'
  return "/mnt/$drive/$tail"
}

Push-Location $RepoRoot
try {
  if (-not $NoBuild) {
    & cmake -S . -B $BuildDir -DTOYC_BUILD_TESTS=ON
    if ($LASTEXITCODE -ne 0) { throw "cmake configure failed with exit $LASTEXITCODE" }
    & cmake --build $BuildDir -j
    if ($LASTEXITCODE -ne 0) { throw "cmake build failed with exit $LASTEXITCODE" }
  }

  if (-not (Test-Path $Toycc)) {
    throw "current src toycc.exe not found: $Toycc"
  }
  $toyccItem = Get-Item $Toycc
  $expectedBuildRoot = (Resolve-Path $BuildPath).Path
  if (-not $toyccItem.FullName.StartsWith($expectedBuildRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "refusing toycc outside selected build dir: $($toyccItem.FullName)"
  }
  if ($toyccItem.Length -lt 1000000) {
    throw "refusing suspiciously small toycc.exe: $($toyccItem.FullName)"
  }

  $sourceNewest = Get-ChildItem -Path (Join-Path $RepoRoot "src"), (Join-Path $RepoRoot "include") -Recurse -File |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1
  if ($sourceNewest -and $toyccItem.LastWriteTime -lt $sourceNewest.LastWriteTime) {
    throw "toycc.exe is older than source file $($sourceNewest.FullName); rebuild before runtime validation"
  }

  New-Item -ItemType Directory -Force -Path $ReportsDir | Out-Null

  $repoWsl = Convert-ToWslPath $RepoRoot
  $toyccWsl = Convert-ToWslPath $Toycc
  $casesWsl = Convert-ToWslPath $CasesDir
  $reportsWsl = Convert-ToWslPath $ReportsDir
  $scriptWsl = Convert-ToWslPath $RuntimeScript

  $oldStats = $env:TOYC_MIR_DEAD_WRITEBACK_STATS
  $oldDisable = $env:TOYC_DISABLE_BLOCK_LOCAL_DEAD_WRITEBACK
  $oldWslEnv = $env:WSLENV
  $env:TOYC_MIR_DEAD_WRITEBACK_STATS = "1"
  $wslEnvEntries = @("TOYC_MIR_DEAD_WRITEBACK_STATS/u")
  if (-not [string]::IsNullOrWhiteSpace($env:TOYC_DISABLE_BLOCK_LOCAL_DEAD_WRITEBACK)) {
    $wslEnvEntries += "TOYC_DISABLE_BLOCK_LOCAL_DEAD_WRITEBACK/u"
  }
  if ([string]::IsNullOrWhiteSpace($env:WSLENV)) {
    $env:WSLENV = ($wslEnvEntries -join ":")
  } else {
    foreach ($entry in $wslEnvEntries) {
      $name = $entry.Split("/")[0]
      if ($env:WSLENV -notmatch "(^|:)$name(/u)?(:|$)") {
        $env:WSLENV = "$env:WSLENV`:$entry"
      }
    }
  }

  & wsl.exe bash $scriptWsl $repoWsl $toyccWsl $casesWsl $reportsWsl
  if ($LASTEXITCODE -ne 0) {
    throw "WSL runtime validation failed with exit $LASTEXITCODE"
  }
  $env:TOYC_MIR_DEAD_WRITEBACK_STATS = $oldStats
  $env:TOYC_DISABLE_BLOCK_LOCAL_DEAD_WRITEBACK = $oldDisable
  $env:WSLENV = $oldWslEnv
}
finally {
  $env:TOYC_MIR_DEAD_WRITEBACK_STATS = $oldStats
  $env:TOYC_DISABLE_BLOCK_LOCAL_DEAD_WRITEBACK = $oldDisable
  $env:WSLENV = $oldWslEnv
  Pop-Location
}
