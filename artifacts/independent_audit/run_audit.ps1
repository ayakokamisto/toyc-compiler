param(
    [string]$RepoRoot = "E:\TOYC"
)

$ErrorActionPreference = "Continue"

$AuditRoot = Join-Path $RepoRoot "artifacts\independent_audit"
$CasesDir = Join-Path $AuditRoot "audit_cases"
$LogsDir = Join-Path $AuditRoot "logs"
$JavaOut = Join-Path $AuditRoot "java_output"
$CppOut = Join-Path $AuditRoot "cpp_output"
$CppExe = Join-Path $AuditRoot "build_cpp\toycc.exe"
$JavaJar = Join-Path $RepoRoot "toy-c-compiler-master\target\toyc.jar"

New-Item -ItemType Directory -Force $CasesDir, $LogsDir, $JavaOut, $CppOut | Out-Null

function Write-Text($Path, $Text) {
    [System.IO.File]::WriteAllText($Path, $Text, [System.Text.UTF8Encoding]::new($false))
}

function To-WslPath($Path) {
    $full = [System.IO.Path]::GetFullPath($Path)
    $drive = $full.Substring(0, 1).ToLowerInvariant()
    $rest = $full.Substring(2).Replace('\', '/')
    return "/mnt/$drive$rest"
}

function Invoke-CapturedProcess($Exe, $ArgList, $StdoutPath, $StderrPath, $TimeoutSeconds, $InputPath = $null) {
    $psi = [System.Diagnostics.ProcessStartInfo]::new()
    $psi.FileName = $Exe
    $psi.Arguments = ($ArgList | ForEach-Object {
        $arg = [string]$_
        '"' + $arg.Replace('"', '\"') + '"'
    }) -join ' '
    $psi.RedirectStandardInput = $InputPath -ne $null
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true

    $p = [System.Diagnostics.Process]::Start($psi)
    $stdoutTask = $p.StandardOutput.ReadToEndAsync()
    $stderrTask = $p.StandardError.ReadToEndAsync()

    if ($InputPath -ne $null) {
        $bytes = [System.IO.File]::ReadAllBytes($InputPath)
        $p.StandardInput.BaseStream.Write($bytes, 0, $bytes.Length)
        $p.StandardInput.BaseStream.Flush()
        $p.StandardInput.Close()
    }

    $finished = $p.WaitForExit($TimeoutSeconds * 1000)
    if (-not $finished) {
        try { & taskkill.exe /T /F /PID $p.Id | Out-Null } catch {}
        try { $p.Kill() } catch {}
        try { $p.WaitForExit(3000) } catch {}
        Write-Text $StdoutPath ($stdoutTask.Result)
        Write-Text $StderrPath (($stderrTask.Result) + "`nPROCESS_TIMEOUT after ${TimeoutSeconds}s`n")
        return 124
    }

    Write-Text $StdoutPath ($stdoutTask.Result)
    Write-Text $StderrPath ($stderrTask.Result)
    return $p.ExitCode
}

function Invoke-WithInput($Exe, $ArgList, $InputPath, $StdoutPath, $StderrPath) {
    $exeText = '"' + [string]$Exe + '"'
    $argText = ($ArgList | ForEach-Object {
        $arg = [string]$_
        '"' + $arg.Replace('"', '\"') + '"'
    }) -join ' '
    if ($argText.Length -gt 0) {
        $exeText = "$exeText $argText"
    }
    $command = 'type "' + $InputPath + '" | ' + $exeText + ' > "' + $StdoutPath + '" 2> "' + $StderrPath + '"'
    $cmdPath = Join-Path $LogsDir ("invoke_" + [System.IO.Path]::GetFileNameWithoutExtension($StdoutPath) + "_" + [System.Guid]::NewGuid().ToString("N") + ".cmd")
    Write-Text $cmdPath ("@echo off`r`n" + $command + "`r`n")
    $tmpOut = "$StdoutPath.cmd.stdout.tmp"
    $tmpErr = "$StderrPath.cmd.stderr.tmp"
    $exit = Invoke-CapturedProcess "cmd.exe" @("/d", "/c", $cmdPath) $tmpOut $tmpErr 20
    Remove-Item -LiteralPath $tmpOut, $tmpErr -ErrorAction SilentlyContinue
    return $exit
}

function Invoke-WslCommand($Command, $LogPath) {
    $cmdPath = Join-Path $LogsDir ("wsl_" + [System.IO.Path]::GetFileNameWithoutExtension($LogPath) + "_" + [System.Guid]::NewGuid().ToString("N") + ".cmd")
    $escaped = $Command.Replace('"', '\"')
    Write-Text $cmdPath ("@echo off`r`nwsl -e bash -lc `"$escaped`" > `"$LogPath`" 2>&1`r`n")
    $stdout = "$LogPath.cmd.stdout.tmp"
    $stderr = "$LogPath.cmd.stderr.tmp"
    $exit = Invoke-CapturedProcess "cmd.exe" @("/d", "/c", $cmdPath) $stdout $stderr 15
    $extra = ""
    if (Test-Path $stdout) { $extra += [System.IO.File]::ReadAllText($stdout) }
    if (Test-Path $stderr) { $extra += [System.IO.File]::ReadAllText($stderr) }
    if ($extra.Length -gt 0) {
        [System.IO.File]::AppendAllText($LogPath, $extra, [System.Text.UTF8Encoding]::new($false))
    }
    Remove-Item -LiteralPath $stdout, $stderr -ErrorAction SilentlyContinue
    return $exit
}

function Parse-SpikeResult($Text, $ExitCode) {
    if ([string]::IsNullOrWhiteSpace($Text) -and $ExitCode -eq 0) {
        return @{ Status = "OK"; Raw = "0"; Signed = 0; Note = "empty output with spike exit 0" }
    }
    $numberPattern = '(-?0x[0-9a-fA-F]+|-?[0-9]+)'
    $failed = [regex]::Match($Text, "\*\*\* FAILED \*\*\* \(tohost = $numberPattern\)")
    if ($failed.Success) {
        $rawText = $failed.Groups[1].Value
        if ($rawText.StartsWith("-")) {
            [int32]$signed = [int32]::Parse($rawText)
            return @{ Status = "OK"; Raw = $rawText; Signed = $signed; Note = "FAILED negative tohost direct decode" }
        }
        [uint64]$raw = if ($rawText.StartsWith("0x")) { [Convert]::ToUInt64($rawText.Substring(2), 16) } else { [uint64]$rawText }
        [uint64]$decoded = (($raw -shl 1) -bor 1) -band 0xffffffff
        [int32]$signed = [int32][uint32]$decoded
        return @{ Status = "OK"; Raw = $rawText; Signed = $signed; Note = "FAILED tohost decode" }
    }
    $tohost = [regex]::Match($Text, "tohost\s*=\s*$numberPattern")
    if ($tohost.Success) {
        $rawText = $tohost.Groups[1].Value
        [uint64]$raw = if ($rawText.StartsWith("0x")) { [Convert]::ToUInt64($rawText.Substring(2), 16) } else { [uint64]$rawText }
        [uint32]$u32 = [uint32]($raw -band 0xffffffff)
        [int32]$signed = [int32]$u32
        return @{ Status = "OK"; Raw = $rawText; Signed = $signed; Note = "direct tohost decode" }
    }
    return @{ Status = "PARSE_FAIL"; Raw = ""; Signed = ""; Note = "no tohost pattern" }
}

$runtimeCases = @(
    @{ Name="p1_return_0"; Expected=0; Source='int main() { return 0; }' },
    @{ Name="p1_return_42"; Expected=42; Source='int main() { return 42; }' },
    @{ Name="p1_return_minus_1"; Expected=-1; Source='int main() { return -1; }' },
    @{ Name="p1_int_min"; Expected=-2147483648; Source='int main() { return -2147483648; }' },
    @{ Name="p1_precedence"; Expected=7; Source='int main() { return 1 + 2 * 3; }' },
    @{ Name="p1_parentheses"; Expected=9; Source='int main() { return (1 + 2) * 3; }' },
    @{ Name="p1_div_mod"; Expected=5; Source='int main() { return 17 / 5 + 17 % 5; }' },
    @{ Name="p1_logic_compare"; Expected=1; Source='int main() { return 3 < 4 && 5 != 6; }' },
    @{ Name="p1_logic_norm_and"; Expected=1; Source='int main() { return 7 && 9; }' },
    @{ Name="p1_logic_norm_or"; Expected=1; Source='int main() { return 0 || (8 - 3); }' },
    @{ Name="p1_short_and_div0"; Expected=0; Source='int main() { return 0 && (1 / 0); }' },
    @{ Name="p1_short_or_div0"; Expected=1; Source='int main() { return 1 || (1 / 0); }' },
    @{ Name="p2_local_init"; Expected=7; Source="int main() {`n    int x = 7;`n    return x;`n}" },
    @{ Name="p2_assignment"; Expected=42; Source="int main() {`n    int x = 1;`n    x = x + 41;`n    return x;`n}" },
    @{ Name="p2_scope_shadow"; Expected=2; Source="int main() {`n    int x = 2;`n    {`n        int x = 7;`n        x = x + 1;`n    }`n    return x;`n}" },
    @{ Name="p2_dangling_else"; Expected=2; Source="int main() {`n    int x = 0;`n    if (1)`n        if (0) x = 1;`n        else x = 2;`n    return x;`n}" },
    @{ Name="p2_while_continue_sum"; Expected=25; Source="int main() {`n    int i = 0;`n    int sum = 0;`n    while (i < 10) {`n        i = i + 1;`n        if (i % 2 == 0) continue;`n        sum = sum + i;`n    }`n    return sum;`n}" },
    @{ Name="p2_while_break"; Expected=3; Source="int main() {`n    int i = 0;`n    while (1) {`n        i = i + 1;`n        if (i == 3) break;`n    }`n    return i;`n}" },
    @{ Name="p2_add_call"; Expected=42; Source="int add(int a, int b) { return a + b; }`nint main() { return add(17, 25); }" },
    @{ Name="p2_fact"; Expected=120; Source="int fact(int n) {`n    if (n <= 1) return 1;`n    return n * fact(n - 1);`n}`nint main() { return fact(5); }" },
    @{ Name="p2_fib"; Expected=21; Source="int fib(int n) {`n    if (n <= 1) return n;`n    return fib(n - 1) + fib(n - 2);`n}`nint main() { return fib(8); }" },
    @{ Name="p2_nested_call"; Expected=42; Source="int id(int x) { return x; }`nint add(int a, int b) { return a + b; }`nint main() { return add(id(17), id(25)); }" },
    @{ Name="p2_void_sink"; Expected=42; Source="void sink(int x) { return; }`nint main() { sink(7); return 42; }" },
    @{ Name="p2_sum9"; Expected=45; Source="int sum9(int a, int b, int c, int d, int e,`n         int f, int g, int h, int i) {`n    return a+b+c+d+e+f+g+h+i;`n}`nint main() { return sum9(1,2,3,4,5,6,7,8,9); }" },
    @{ Name="p2_sum12"; Expected=78; Source="int sum12(int a, int b, int c, int d, int e, int f,`n          int g, int h, int i, int j, int k, int l) {`n    return a+b+c+d+e+f+g+h+i+j+k+l;`n}`nint main() { return sum12(1,2,3,4,5,6,7,8,9,10,11,12); }" },
    @{ Name="p2_sum9_nested_ids"; Expected=45; Source="int id(int x) { return x; }`nint sum9(int a, int b, int c, int d, int e,`n         int f, int g, int h, int i) {`n    return a+b+c+d+e+f+g+h+i;`n}`nint main() { return sum9(id(1), id(2), id(3), id(4), id(5), id(6), id(7), id(8), id(9)); }" },
    @{ Name="p2_forward9"; Expected=45; Source="int sum9(int a, int b, int c, int d, int e,`n         int f, int g, int h, int i) { return a+b+c+d+e+f+g+h+i; }`nint forward9(int a, int b, int c, int d, int e,`n             int f, int g, int h, int i) { return sum9(a,b,c,d,e,f,g,h,i); }`nint main() { return forward9(1,2,3,4,5,6,7,8,9); }" },
    @{ Name="p2_sumdown9"; Expected=24; Source="int sumDown9(int n, int a, int b, int c, int d,`n             int e, int f, int g, int h) {`n    if (n == 0) { return a+b+c+d+e+f+g+h; }`n    return sumDown9(n-1, a+1, b+1, c+1, d+1, e+1, f+1, g+1, h+1);`n}`nint main() { return sumDown9(2,1,1,1,1,1,1,1,1); }" },
    @{ Name="p2c_global_read"; Expected=7; Source="int g = 7;`nint main() { return g; }" },
    @{ Name="p2c_global_const_expr"; Expected=7; Source="int g = 1 + 2 * 3;`nint main() { return g; }" },
    @{ Name="p2c_global_write"; Expected=42; Source="int g = 1;`nint main() { g = g + 41; return g; }" },
    @{ Name="p2c_global_shadow_local"; Expected=42; Source="int g = 7;`nint main() { int g = 42; return g; }" },
    @{ Name="p2c_global_shadow_param"; Expected=42; Source="int g = 7;`nint id(int g) { return g; }`nint main() { return id(42); }" },
    @{ Name="p2c_global_state_bump"; Expected=5; Source="int g = 1;`nint bump() { g = g + 1; return g; }`nint main() { return bump() + bump(); }" },
    @{ Name="p2c_global_cross_fn"; Expected=42; Source="int g = 3;`nint addg(int x) { return x + g; }`nint main() { return addg(39); }" },
    @{ Name="p2c_global_ninth_arg"; Expected=45; Source="int g = 9;`nint sum9(int a, int b, int c, int d, int e,`n         int f, int h, int i, int j) { return a+b+c+d+e+f+h+i+j; }`nint main() { return sum9(1,2,3,4,5,6,7,8,g); }" },
    @{ Name="p2c_global_init_short_and"; Expected=0; Source="int g = 0 && (1 / 0);`nint main() { return g; }" },
    @{ Name="p2c_global_init_short_or"; Expected=1; Source="int g = 1 || (1 / 0);`nint main() { return g; }" },
    @{ Name="p2c_global_uninit"; Expected=0; Source="int g;`nint main() { return g; }" }
)

$diagnosticCases = @(
    @{ Name="diag_redef_local"; Keyword="redefinition"; Source="int main() {`n    int x = 1;`n    int x = 2;`n    return x;`n}" },
    @{ Name="diag_undef_var"; Keyword="undeclared"; Source="int main() {`n    return x;`n}" },
    @{ Name="diag_break_outside"; Keyword="break"; Source="int main() {`n    break;`n    return 0;`n}" },
    @{ Name="diag_continue_outside"; Keyword="continue"; Source="int main() {`n    continue;`n    return 0;`n}" },
    @{ Name="diag_missing_return"; Keyword="return"; Source="int main() {`n    int x = 1;`n}" },
    @{ Name="diag_wrong_arg_count"; Keyword="expects"; Source="int f(int x) { return x; }`nint main() { return f(); }" },
    @{ Name="diag_void_return_value"; Keyword="void"; Source="void sink() { return; }`nint main() { return sink(); }" },
    @{ Name="diag_void_in_expr"; Keyword="void"; Source="void sink() { return; }`nint main() { return sink() + 1; }" },
    @{ Name="diag_global_init_id"; Keyword="constant"; Source="int a = 3;`nint b = a + 4;`nint main() { return b; }" },
    @{ Name="diag_global_init_call"; Keyword="constant"; Source="int id(int x) { return x; }`nint g = id(7);`nint main() { return g; }" }
)

$rowsPath = Join-Path $LogsDir "audit_results.psv"
Write-Text $rowsPath "name|kind|expected|java_compile|cpp_compile|java_link|cpp_link|java_spike|cpp_spike|java_signed|cpp_signed|conclusion|note`n"

function Add-Row($Line) {
    [System.IO.File]::AppendAllText($rowsPath, $Line + "`n", [System.Text.UTF8Encoding]::new($false))
}

foreach ($case in $runtimeCases) {
    $name = $case.Name
    $srcPath = Join-Path $CasesDir "$name.tc"
    Write-Text $srcPath ($case.Source + "`n")
    $javaAsm = Join-Path $JavaOut "$name.s"
    $javaErr = Join-Path $JavaOut "$name.compile.stderr"
    $cppAsm = Join-Path $CppOut "$name.s"
    $cppErr = Join-Path $CppOut "$name.compile.stderr"
    $javaExit = Invoke-WithInput "java" @("-jar", $JavaJar, "-opt") $srcPath $javaAsm $javaErr
    $cppExit = Invoke-WithInput $CppExe @() $srcPath $cppAsm $cppErr
    $javaLink = "SKIP"; $cppLink = "SKIP"; $javaSpike = "SKIP"; $cppSpike = "SKIP"
    $javaSigned = ""; $cppSigned = ""; $note = ""
    if ($javaExit -eq 0) {
        $elf = Join-Path $JavaOut "$name.elf"
        $cmd = "riscv64-unknown-elf-gcc -march=rv32im -mabi=ilp32 -nostdlib -static -T /tmp/rvtest_link.ld -o $(To-WslPath $elf) /tmp/rvtest_start.s $(To-WslPath $javaAsm)"
        $javaLinkExit = Invoke-WslCommand $cmd (Join-Path $JavaOut "$name.link.log")
        $javaLink = "exit=$javaLinkExit"
        if ($javaLinkExit -eq 0) {
            $spikeLog = Join-Path $JavaOut "$name.spike.log"
            $spikeExit = Invoke-WslCommand "timeout 5s /home/ayako/.local/bin/spike --isa=rv32im $(To-WslPath $elf)" $spikeLog
            $parsed = Parse-SpikeResult ([System.IO.File]::ReadAllText($spikeLog)) $spikeExit
            $javaSpike = "$($parsed.Status),exit=$spikeExit,raw=$($parsed.Raw),$($parsed.Note)"
            $javaSigned = $parsed.Signed
        }
    }
    if ($cppExit -eq 0) {
        $elf = Join-Path $CppOut "$name.elf"
        $cmd = "riscv64-unknown-elf-gcc -march=rv32im -mabi=ilp32 -nostdlib -static -T /tmp/rvtest_link.ld -o $(To-WslPath $elf) /tmp/rvtest_start.s $(To-WslPath $cppAsm)"
        $cppLinkExit = Invoke-WslCommand $cmd (Join-Path $CppOut "$name.link.log")
        $cppLink = "exit=$cppLinkExit"
        if ($cppLinkExit -eq 0) {
            $spikeLog = Join-Path $CppOut "$name.spike.log"
            $spikeExit = Invoke-WslCommand "timeout 5s /home/ayako/.local/bin/spike --isa=rv32im $(To-WslPath $elf)" $spikeLog
            $parsed = Parse-SpikeResult ([System.IO.File]::ReadAllText($spikeLog)) $spikeExit
            $cppSpike = "$($parsed.Status),exit=$spikeExit,raw=$($parsed.Raw),$($parsed.Note)"
            $cppSigned = $parsed.Signed
        }
    }
    $conclusion = "CPP_UNVERIFIED"
    if ($javaExit -ne 0) { $conclusion = "JAVA_UNVERIFIED"; $note = "java compile failed" }
    elseif ($cppExit -ne 0) { $conclusion = "DIVERGENT"; $note = "cpp compile failed" }
    elseif ("$javaSigned" -ne "" -and "$cppSigned" -ne "" -and [int]$javaSigned -eq [int]$case.Expected -and [int]$cppSigned -eq [int]$case.Expected) { $conclusion = "CONFIRMED_EQUIVALENT" }
    elseif ("$javaSigned" -ne "" -and "$cppSigned" -ne "" -and [int]$javaSigned -ne [int]$cppSigned) { $conclusion = "DIVERGENT"; $note = "different spike signed result" }
    elseif ("$javaSigned" -ne "" -and "$cppSigned" -ne "" -and [int]$javaSigned -eq [int]$cppSigned -and [int]$javaSigned -ne [int]$case.Expected) { $conclusion = "DIVERGENT"; $note = "same result differs from expected" }
    Add-Row "$name|runtime|$($case.Expected)|exit=$javaExit|exit=$cppExit|$javaLink|$cppLink|$javaSpike|$cppSpike|$javaSigned|$cppSigned|$conclusion|$note"
}

foreach ($case in $diagnosticCases) {
    $name = $case.Name
    $srcPath = Join-Path $CasesDir "$name.tc"
    Write-Text $srcPath ($case.Source + "`n")
    $javaOutPath = Join-Path $JavaOut "$name.compile.stdout"
    $javaErr = Join-Path $JavaOut "$name.compile.stderr"
    $cppOutPath = Join-Path $CppOut "$name.compile.stdout"
    $cppErr = Join-Path $CppOut "$name.compile.stderr"
    $javaExit = Invoke-WithInput "java" @("-jar", $JavaJar, "-opt") $srcPath $javaOutPath $javaErr
    $cppExit = Invoke-WithInput $CppExe @() $srcPath $cppOutPath $cppErr
    $cppErrText = [System.IO.File]::ReadAllText($cppErr)
    $keywordOk = $cppErrText.ToLowerInvariant().Contains($case.Keyword.ToLowerInvariant())
    $conclusion = "DIVERGENT"
    $note = ""
    if ($javaExit -ne 0 -and $cppExit -ne 0 -and $keywordOk) { $conclusion = "CONFIRMED_EQUIVALENT" }
    elseif ($javaExit -ne 0 -and $cppExit -ne 0) { $conclusion = "LIKELY_EQUIVALENT"; $note = "cpp stderr lacks keyword $($case.Keyword)" }
    elseif ($javaExit -eq 0 -and $cppExit -ne 0) { $note = "java accepts cpp rejects" }
    elseif ($javaExit -ne 0 -and $cppExit -eq 0) { $note = "java rejects cpp accepts" }
    else { $note = "both accepted diagnostic probe" }
    Add-Row "$name|compile_error|$($case.Keyword)|exit=$javaExit|exit=$cppExit|SKIP|SKIP|SKIP|SKIP|||$conclusion|$note"
}
