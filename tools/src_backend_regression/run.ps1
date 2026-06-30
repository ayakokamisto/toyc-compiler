param(
  [string]$BuildDir = "build",
  [switch]$NoBuild
)

$ErrorActionPreference = "Stop"
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$BuildPath = Join-Path $RepoRoot $BuildDir
$CasesDir = Join-Path $PSScriptRoot "cases"
$ReportsDir = Join-Path $PSScriptRoot "reports"
New-Item -ItemType Directory -Force -Path $ReportsDir | Out-Null

function Find-CommandName([string[]]$Names) {
  foreach ($name in $Names) {
    $cmd = Get-Command $name -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($null -ne $cmd) { return $cmd.Source }
  }
  return $null
}

function Count-Opcode([string]$Text, [string]$Opcode) {
  $matches = [regex]::Matches($Text, "(?m)^\s*$Opcode\s")
  return $matches.Count
}

function Check-Assembly([string]$Text) {
  $issues = New-Object System.Collections.Generic.List[string]
  foreach ($needle in @("%v", "FrameSlot", "SlotId", "FunctionId")) {
    if ($Text.Contains($needle)) { $issues.Add("backend placeholder '$needle' leaked") }
  }
  $usedRegs = [regex]::Matches($Text, "\bs(?:[1-9]|10|11)\b") | ForEach-Object { $_.Value } | Sort-Object -Unique
  foreach ($reg in $usedRegs) {
    if (-not [regex]::IsMatch($Text, "(?m)^\s*sw\s+$reg,")) { $issues.Add("missing save for $reg") }
    if (-not [regex]::IsMatch($Text, "(?m)^\s*lw\s+$reg,")) { $issues.Add("missing restore for $reg") }
  }
  if ([regex]::IsMatch($Text, "(?m)^\s*(mul|div|rem)\s")) {
    $issues.Add("M extension opcode emitted")
  }
  return $issues
}

Push-Location $RepoRoot
try {
  if (-not $NoBuild) {
    & cmake -S . -B $BuildDir -DTOYC_BUILD_TESTS=ON
    if ($LASTEXITCODE -ne 0) { throw "cmake configure failed with exit $LASTEXITCODE" }
    & cmake --build $BuildDir -j
    if ($LASTEXITCODE -ne 0) { throw "cmake build failed with exit $LASTEXITCODE" }
  }

  $Toycc = Join-Path $BuildPath "toycc.exe"
  if (-not (Test-Path $Toycc)) { $Toycc = Join-Path $BuildPath "toycc" }
  if (-not (Test-Path $Toycc)) { throw "toycc not found under $BuildPath" }

  $Qemu = Find-CommandName @("qemu-riscv32-static", "qemu-riscv32")
  $Gcc = Find-CommandName @("riscv32-unknown-elf-gcc", "riscv32-linux-gnu-gcc")
  $RuntimeAvailable = ($null -ne $Qemu -and $null -ne $Gcc)

  $Rows = New-Object System.Collections.Generic.List[string]
  $Rows.Add("| case | expected | compile | static | runtime | lw | sw | call | mul | div | rem |")
  $Rows.Add("| --- | ---: | --- | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |")

  foreach ($case in Get-ChildItem $CasesDir -Filter "*.tc" | Sort-Object Name) {
    $name = [IO.Path]::GetFileNameWithoutExtension($case.Name)
    $source = Get-Content $case.FullName -Raw
    $expectedMatch = [regex]::Match($source, "//\s*EXPECT:\s*(\d+)")
    $expected = if ($expectedMatch.Success) { [int]$expectedMatch.Groups[1].Value } else { -1 }
    $asmPath = Join-Path $ReportsDir "$name.s"
    $errPath = Join-Path $ReportsDir "$name.stderr"
    $compileStatus = "pass"
    $staticStatus = "pass"
    $runtimeStatus = "skipped: no riscv runner"

    $source | & $Toycc > $asmPath 2> $errPath
    if ($LASTEXITCODE -ne 0) {
      $compileStatus = "fail:$LASTEXITCODE"
      $staticStatus = "skipped"
    }

    $asmText = ""
    if (Test-Path $asmPath) { $asmText = Get-Content $asmPath -Raw }
    if ($compileStatus -eq "pass") {
      $issues = Check-Assembly $asmText
      if ($issues.Count -gt 0) { $staticStatus = "fail: " + ($issues -join "; ") }
    }

    if ($compileStatus -eq "pass" -and $staticStatus -eq "pass" -and $RuntimeAvailable) {
      $startPath = Join-Path $ReportsDir "$name.start.s"
      $elfPath = Join-Path $ReportsDir "$name.elf"
      @"
.section .text
.globl _start
_start:
  call main
  li a7, 93
  ecall
"@ | Set-Content -Path $startPath -Encoding ASCII
      & $Gcc -march=rv32i -mabi=ilp32 -nostdlib -static $startPath $asmPath -o $elfPath 2>> $errPath
      if ($LASTEXITCODE -eq 0) {
        & $Qemu $elfPath
        $actual = $LASTEXITCODE
        if ($actual -eq $expected) { $runtimeStatus = "pass:$actual" } else { $runtimeStatus = "fail:$actual" }
      } else {
        $runtimeStatus = "skipped: link failed"
      }
    }

    $Rows.Add("| $name | $expected | $compileStatus | $staticStatus | $runtimeStatus | $(Count-Opcode $asmText "lw") | $(Count-Opcode $asmText "sw") | $(Count-Opcode $asmText "call") | $(Count-Opcode $asmText "mul") | $(Count-Opcode $asmText "div") | $(Count-Opcode $asmText "rem") |")
  }

  $Report = New-Object System.Collections.Generic.List[string]
  $Report.Add("# src backend regression report")
  $Report.Add("")
  $Report.Add("- generated_at: $(Get-Date -Format s)")
  $Report.Add("- toycc: $Toycc")
  $Report.Add("- riscv_gcc: $(if ($Gcc) { $Gcc } else { 'missing' })")
  $Report.Add("- riscv_qemu: $(if ($Qemu) { $Qemu } else { 'missing' })")
  $Report.Add("")
  $Report.AddRange($Rows)
  $Report.Add("")
  if (-not $RuntimeAvailable) {
    $Report.Add("Runtime execution skipped because a local RISC-V compiler and qemu runner were not both available.")
  }
  $Report | Set-Content -Path (Join-Path $ReportsDir "latest.md") -Encoding UTF8
}
finally {
  Pop-Location
}
