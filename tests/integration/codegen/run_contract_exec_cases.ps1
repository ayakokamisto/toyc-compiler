param(
    [string]$BuildDir = "build",
    [string]$Compiler = "",
    [string]$Runner = "",
    [switch]$OptOnly
)

$ErrorActionPreference = "Stop"

function Resolve-Tool {
    param(
        [string]$Explicit,
        [string[]]$Candidates,
        [string]$Kind
    )

    if ($Explicit -ne "") {
        $resolved = Get-Command $Explicit -ErrorAction SilentlyContinue
        if ($null -eq $resolved) {
            throw "$Kind not found: $Explicit"
        }
        return $resolved.Source
    }

    foreach ($candidate in $Candidates) {
        $resolved = Get-Command $candidate -ErrorAction SilentlyContinue
        if ($null -ne $resolved) {
            return $resolved.Source
        }
    }

    throw "$Kind not found. Pass -$Kind explicitly or add one of: $($Candidates -join ', ')"
}

function Invoke-Checked {
    param(
        [string]$FilePath,
        [string[]]$Arguments
    )

    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "command failed with exit code ${LASTEXITCODE}: $FilePath $($Arguments -join ' ')"
    }
}

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..\..")
$buildRoot = Join-Path $repoRoot $BuildDir
$caseEmitter = Join-Path $buildRoot "codegen_contract_exec_cases.exe"
if (-not (Test-Path $caseEmitter)) {
    $caseEmitter = Join-Path $buildRoot "codegen_contract_exec_cases"
}
if (-not (Test-Path $caseEmitter)) {
    throw "Missing codegen_contract_exec_cases. Run: cmake --build $BuildDir --target codegen_contract_exec_cases"
}

$cc = Resolve-Tool `
    -Explicit $Compiler `
    -Candidates @("riscv32-unknown-linux-gnu-gcc", "riscv64-unknown-linux-gnu-gcc", "riscv64-linux-gnu-gcc") `
    -Kind "Compiler"
$qemu = Resolve-Tool `
    -Explicit $Runner `
    -Candidates @("qemu-riscv32") `
    -Kind "Runner"

$workDir = Join-Path $buildRoot "codegen-contract-exec"
New-Item -ItemType Directory -Force -Path $workDir | Out-Null

$crt0 = Join-Path $workDir "crt0.S"
@"
    .section .text
    .global _start
_start:
    call main
    andi a0, a0, 255
    li a7, 93
    ecall
"@ | Set-Content -NoNewline -Encoding ascii $crt0

$cases = @(
    @{ Name = "basic_return"; Expected = 42 },
    @{ Name = "loop_sum"; Expected = 6 },
    @{ Name = "many_params"; Expected = 55 },
    @{ Name = "global_var"; Expected = 12 },
    @{ Name = "recursion"; Expected = 120 }
)

$modes = @("")
if ($OptOnly) {
    $modes = @("--opt")
} else {
    $modes += "--opt"
}

foreach ($case in $cases) {
    foreach ($mode in $modes) {
        $suffix = if ($mode -eq "--opt") { "opt" } else { "default" }
        $asm = Join-Path $workDir "$($case.Name)-$suffix.s"
        $elf = Join-Path $workDir "$($case.Name)-$suffix.elf"

        $emitArgs = @("--case", $case.Name)
        if ($mode -ne "") {
            $emitArgs += $mode
        }
        & $caseEmitter @emitArgs | Set-Content -NoNewline -Encoding ascii $asm
        if ($LASTEXITCODE -ne 0) {
            throw "case emitter failed for $($case.Name) $suffix"
        }

        Invoke-Checked $cc @(
            "-march=rv32im",
            "-mabi=ilp32",
            "-static",
            "-nostdlib",
            "-Wl,-e,_start",
            $crt0,
            $asm,
            "-o",
            $elf
        )

        & $qemu $elf
        $actual = $LASTEXITCODE
        if ($actual -ne $case.Expected) {
            throw "$($case.Name) $suffix expected exit $($case.Expected), got $actual"
        }
        Write-Host "PASS $($case.Name) $suffix exit=$actual"
    }
}
