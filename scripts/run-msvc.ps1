# Standalone MSVC harness for the pinned Voro++ reproduction study.
# Requires a Visual Studio Developer Command Prompt / Developer PowerShell.
param(
    [ValidateSet("baseline","candidate","candidate-02","candidate-03","combined-02-03")][string]$Mode="baseline",
    [ValidateSet("full","minimal")][string]$Suite="full"
)
Set-StrictMode -Version Latest
$ErrorActionPreference="Stop"
if($Suite -eq "minimal" -and $Mode -notin @("baseline","combined-02-03")) {
    throw "Minimal reproduction supports baseline and combined-02-03 only."
}
$Root=(Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$Voro=Join-Path $Root "vendor/voro"
$Build=Join-Path $Root "build/$Mode"
$Output=Join-Path $Root "output/$Mode"
[System.IO.Directory]::CreateDirectory($Build) | Out-Null
[System.IO.Directory]::CreateDirectory($Output) | Out-Null
if(-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    throw "cl.exe is missing: run from an MSVC Developer environment."
}
if(-not (Get-Command git.exe -ErrorAction SilentlyContinue)) {
    throw "git.exe is required for the isolated candidate patch."
}
function Compile([string[]]$Options) {
    & cl.exe @Options
    if($LASTEXITCODE -ne 0) {throw "MSVC compilation failed: exit $LASTEXITCODE"}
}
function RunNative([string]$Name,[string[]]$Params,[string]$Log) {
    $Exe=Join-Path $Build "$Name.exe"
    & $Exe @Params > $Log 2>&1
    return [int]$LASTEXITCODE
}
function Show([string]$Log,[string]$Pattern) {
    Get-Content -LiteralPath $Log | Where-Object {$_ -match $Pattern} |
        ForEach-Object {Write-Host $_}
}

$TempRoot=$null
try {
    if($Mode -ne "baseline") {
        $TempRoot=Join-Path ([IO.Path]::GetTempPath()) ("voro-msvc-"+[guid]::NewGuid().ToString("N"))
        [IO.Directory]::CreateDirectory($TempRoot) | Out-Null
        $Original=$Voro
        $Voro=Join-Path $TempRoot "voro"
        Copy-Item -LiteralPath $Original -Destination $Voro -Recurse -Force
        Push-Location $Voro
        try {
            $PatchFile=if($Mode -eq "candidate-02") {"candidate-02.patch"}elseif($Mode -eq "candidate-03") {"candidate-03.patch"}elseif($Mode -eq "combined-02-03") {"candidate-02-plus-03.patch"}else{"candidate-01.patch"}
            $Patch=Join-Path $Root "patches/$PatchFile"
            & git.exe apply --check $Patch
            if($LASTEXITCODE -ne 0) {throw "Candidate patch failed preflight."}
            & git.exe apply $Patch
            if($LASTEXITCODE -ne 0) {throw "Candidate patch failed to apply."}
        } finally {Pop-Location}
    }
    Write-Host "mode=$Mode compiler=MSVC flags=/O2 /std:c++14"
    $Inc=@{}; $Obj=@{}
    $Drivers=@("repro","arithmetic","locate","coverage","unequal","stress","owner","extreme","exponent")
    if($Suite -eq "minimal") {$Drivers=@("minimal")}
    foreach($Dim in @(2,3)) {
        if($Dim -eq 2) {
            $Inc[$Dim]=Join-Path $Voro "2d/src"
            $Source=Join-Path $Voro "2d/src/voro++_2d.cc"
        } else {
            $Inc[$Dim]=Join-Path $Voro "src"
            $Source=Join-Path $Voro "src/voro++.cc"
        }
        $Obj[$Dim]=Join-Path $Build "voro$($Dim)d.obj"
        Compile -Options @("/nologo","/EHsc","/O2","/std:c++14",
            "/I$($Inc[$Dim])","/c","/Fo$($Obj[$Dim])","/Tp$Source")
        foreach($Stem in $Drivers) {
            $Name="$($Stem)$($Dim)d"
            $Driver=Join-Path $Root "repro/$Name.cpp"
            $Exe=Join-Path $Build "$Name.exe"
            Compile -Options @("/nologo","/EHsc","/O2","/std:c++14",
                "/I$($Inc[$Dim])",$Driver,$Obj[$Dim],"/Fe$Exe")
        }
    }
    if($Suite -eq "minimal") {
        # A successful minimal run requires both explicit diagnostic FAILs
        # on the original and both explicit PASSes on the combined patch.
        $Expected=if($Mode -eq "baseline") {"FAIL"} else {"PASS"}
        $ExpectedExit=if($Mode -eq "baseline") {1} else {0}
        $ExpectedFailures=if($Mode -eq "baseline") {2} else {0}
        foreach($Dim in @(2,3)) {
            $Log=Join-Path $Output "$($Dim)d_minimal.txt"
            $Code=RunNative -Name "minimal$($Dim)d" -Params @() -Log $Log
            Get-Content -LiteralPath $Log | ForEach-Object {Write-Host $_}
            if($Code -ne $ExpectedExit) {
                throw "Minimal $Mode $($Dim)D: exit=$Code, expected=$ExpectedExit"
            }
            foreach($Check in @("geometry","owner")) {
                $Pattern="^RESULT dim=$Dim check=$Check status=$Expected\s"
                if(-not (Select-String -LiteralPath $Log -Pattern $Pattern -Quiet)) {
                    throw "Minimal $Mode $($Dim)D: missing expected $Expected result for $Check"
                }
            }
            if(-not (Select-String -LiteralPath $Log -Pattern "^SUMMARY minimal dim=$Dim failed_checks=$ExpectedFailures$" -Quiet)) {
                throw "Minimal $Mode $($Dim)D: summary/check mismatch"
            }
        }
        Write-Host "SUMMARY minimal mode=$Mode status=PASS expected_native_result=$Expected"
    } else {
    $RequiredFailures=0; $KnownFailures=0
    $Cases=@(
        @("small","1","1","1"),
        @("moderate","1000000","1","1"),
        @("threshold","10000000","1","1"),
        @("extreme","100000000","1","1"),
        @("multiblock","100000000","1","2"),
        @("rescaled","100000000","100","1")
    )
    foreach($Dim in @(2,3)) {
        foreach($Case in $Cases) {
            $Label=$Case[0]
            $Log=Join-Path $Output "$($Dim)d_$Label.txt"
            $Code=RunNative -Name "repro$($Dim)d" -Params @($Case[1],$Case[2],$Case[3]) -Log $Log
            if($Code -notin @(0,1)) {throw "Unexpected native exit $Code for $Label"}
            $Outcome=if($Code -eq 0) {"PASS"}else{"FAIL"}
            Write-Host "$Mode $($Dim)D $Label : $Outcome"
            if($Code -eq 1) {
                if($Mode -in @("baseline","candidate-03") -and $Label -in @("extreme","multiblock")) {
                    ++$KnownFailures
                } else {++$RequiredFailures}
            }
        }
    }
    Write-Host "Original extreme failures (informational): $KnownFailures"
    Write-Host "Required checks failed: $RequiredFailures"
    if($RequiredFailures) {throw "Native control or candidate check failed."}

    foreach($Dim in @(2,3)) {
        $Log=Join-Path $Output "$($Dim)d_arithmetic.txt"
        $Code=RunNative -Name "arithmetic$($Dim)d" -Params @() -Log $Log
        Show -Log $Log -Pattern "^(CASE |SUMMARY )"
        if($Code -notin @(0,1)) {throw "A1-A3 abnormal exit $Code"}
        if($Code -eq 1) {
            if($Mode -in @("candidate-02","combined-02-03")) {throw "Construction arithmetic must pass A1-A3: dimension $Dim"}
            Write-Host "A1-A3 $($Dim)D differences recorded."
        }

        $Log=Join-Path $Output "$($Dim)d_location.txt"
        $Code=RunNative -Name "locate$($Dim)d" -Params @() -Log $Log
        Show -Log $Log -Pattern "^(SCENARIO |SUMMARY )"
        if($Code -notin @(0,1)) {throw "A4-A5 control/oracle failure, exit $Code"}
        if($Code -eq 1) {
            if($Mode -in @("candidate-03","combined-02-03")) {throw "Search arithmetic must pass A4-A5 in $($Dim)D"}
            Write-Host "A4-A5 $($Dim)D differences recorded."
        }

        $Log=Join-Path $Output "$($Dim)d_coverage.txt"
        $Code=RunNative -Name "coverage$($Dim)d" -Params @() -Log $Log
        Show -Log $Log -Pattern "^SUMMARY A6_A7"
        if($Code -notin @(0,1) -or ($Mode -in @("candidate","candidate-02","combined-02-03") -and $Code -ne 0)) {
            Get-Content -LiteralPath $Log |
                Where-Object {$_ -match "status=FAIL"} |
                Select-Object -First 30 | ForEach-Object {Write-Host $_}
            throw "A6-A7 control/candidate failure, exit $Code"
        }
        if($Code -eq 1) {Write-Host "A6-A7 $($Dim)D original failures recorded."}
    }
    # A8: unequal-radius cases. Original/candidate-01 errors are evidence;
    # candidate-02 is required to satisfy all selected references.
    foreach($Dim in @(2,3)) {
        $Log=Join-Path $Output "$($Dim)d_unequal.txt"
        $Code=RunNative -Name "unequal$($Dim)d" -Params @() -Log $Log
        Show -Log $Log -Pattern "^SUMMARY A8"
        if($Code -notin @(0,1) -or ($Mode -in @("candidate-02","combined-02-03") -and $Code -ne 0)) {
            Get-Content -LiteralPath $Log |
                Where-Object {$_ -match "status=FAIL|ORACLE_|SUMMARY A8"} |
                Select-Object -First 40 | ForEach-Object {Write-Host $_}
            throw "A8 $($Dim)D control/oracle/candidate-02 failure, exit $Code"
        }
        if($Code -eq 1) {Write-Host "A8 $($Dim)D numerical differences recorded."}
    }
    # A9: noncollinear stress. Only candidate-02 must be discrepancy-free.
    foreach($Dim in @(2,3)) {
        $Log=Join-Path $Output "$($Dim)d_stress.txt"
        $Code=RunNative -Name "stress$($Dim)d" -Params @() -Log $Log
        Show -Log $Log -Pattern "^SUMMARY A9"
        if($Code -notin @(0,1) -or ($Mode -in @("candidate-02","combined-02-03") -and $Code -ne 0)) {
            Get-Content -LiteralPath $Log |
                Where-Object {$_ -match "FAIL_CASE|ORACLE_|SUMMARY A9"} |
                Select-Object -First 45 | ForEach-Object {Write-Host $_}
            throw "A9 $($Dim)D oracle/control/candidate-02 failure, exit $Code"
        }
        if($Code -eq 1) {Write-Host "A9 $($Dim)D numerical differences recorded."}
    }
    # A10: reference nearest-power-site plus returned periodic image.
    # Original and geometry-only candidates retain informative errors.
    foreach($Dim in @(2,3)) {
        $Log=Join-Path $Output "$($Dim)d_owner.txt"
        $Code=RunNative -Name "owner$($Dim)d" -Params @() -Log $Log
        Show -Log $Log -Pattern "^SUMMARY A10"
        if($Code -notin @(0,1) -or ($Mode -in @("candidate-03","combined-02-03") -and $Code -ne 0)) {
            Get-Content -LiteralPath $Log |
                Where-Object {$_ -match "WRONG|ORACLE_|SUMMARY A10"} |
                Select-Object -First 40 | ForEach-Object {Write-Host $_}
            throw "A10 $($Dim)D control/oracle/candidate-03 failure, exit $Code"
        }
        if($Code -eq 1) {Write-Host "A10 $($Dim)D numerical differences recorded."}
    }
    # A11: owner search with high exponents, external periodic queries,
    # negative radii (diagnostic), and excluded near-tie decisions.
    foreach($Dim in @(2,3)) {
        $Log=Join-Path $Output "$($Dim)d_extreme.txt"
        $Code=RunNative -Name "extreme$($Dim)d" -Params @() -Log $Log
        Show -Log $Log -Pattern "^SUMMARY A11"
        if($Code -notin @(0,1) -or ($Mode -in @("candidate-03","combined-02-03") -and $Code -ne 0)) {
            Get-Content -LiteralPath $Log |
                Where-Object {$_ -match "ORACLE_|MISMATCH|SUMMARY A11"} |
                Select-Object -First 30 | ForEach-Object {Write-Host $_}
            throw "A11 $($Dim)D invalid oracle or candidate-03 search failure, exit $Code"
        }
        if($Code -eq 1) {Write-Host "A11 $($Dim)D known search errors recorded."}
    }
    # A12: the 1e308 construction regime is diagnostic, even on candidate-02.
    foreach($Dim in @(2,3)) {
        $Log=Join-Path $Output "$($Dim)d_exponent.txt"
        $Code=RunNative -Name "exponent$($Dim)d" -Params @() -Log $Log
        Show -Log $Log -Pattern "^SUMMARY A12"
        if($Code -notin @(0,1) -or ($Mode -in @("candidate-02","combined-02-03") -and $Code -ne 0)) {
            Get-Content -LiteralPath $Log |
                Where-Object {$_ -match "CASE A12|SUMMARY A12"} |
                Select-Object -Last 15 | ForEach-Object {Write-Host $_}
            throw "A12 $($Dim)D required geometry failure, exit $Code"
        }
    }
    # Stage 11. Optional, scope-only scalar diagnostics: NOT a pass/fail gate.
    foreach($Dim in @(2,3)) {
        $Name="radius_bounds$($Dim)d"
        $Source=Join-Path $Root "repro/radius_bounds.cpp"
        $Exe=Join-Path $Build "$Name.exe"
        $Log=Join-Path $Output "$($Dim)d_pruning_scope.txt"
        $BuildLog=Join-Path $Output "$($Dim)d_pruning_build.txt"
        $CompileArgs=@("/nologo","/EHsc","/O2","/std:c++14",
                "/I$($Inc[$Dim])",$Source,"/Fe$Exe")
        if($Dim -eq 2) { $CompileArgs+= "/DAUDIT_2D" }
        try {
            Compile -Options $CompileArgs *> $BuildLog
            $Code=RunNative -Name $Name -Params @() -Log $Log
            if($Code -eq 0) {Show -Log $Log -Pattern "^SUMMARY radius_poly"}
            else {Write-Warning "Scope-only $($Dim)D diagnostic exit $Code; not an acceptance gate."}
        } catch {
            Write-Warning "Scope-only $($Dim)D probe was unavailable: $_"
        }
    }
    Write-Host "MSVC run completed. Diagnostic failures are NOT accepted geometry."
    }
} finally {
    if($null -ne $TempRoot -and (Test-Path -LiteralPath $TempRoot)) {
        Remove-Item -LiteralPath $TempRoot -Recurse -Force
    }
}
# PowerShell's caller otherwise inherits the last expected native exit 1.
# Real failures throw above and never reach this point.
exit 0
