# Standalone MSVC harness for the pinned Voro++ reproduction study.
# Requires a Visual Studio Developer Command Prompt / Developer PowerShell.
param([ValidateSet("baseline","candidate","candidate-02")][string]$Mode="baseline")
Set-StrictMode -Version Latest
$ErrorActionPreference="Stop"
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
            $PatchFile=if($Mode -eq "candidate-02") {"candidate-02.patch"}else{"candidate-01.patch"}
            $Patch=Join-Path $Root "patches/$PatchFile"
            & git.exe apply --check $Patch
            if($LASTEXITCODE -ne 0) {throw "Candidate patch failed preflight."}
            & git.exe apply $Patch
            if($LASTEXITCODE -ne 0) {throw "Candidate patch failed to apply."}
        } finally {Pop-Location}
    }
    Write-Host "mode=$Mode compiler=MSVC flags=/O2 /std:c++14"
    $Inc=@{}; $Obj=@{}
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
        foreach($Stem in @("repro","arithmetic","locate","coverage","unequal")) {
            $Name="$($Stem)$($Dim)d"
            $Driver=Join-Path $Root "repro/$Name.cpp"
            $Exe=Join-Path $Build "$Name.exe"
            Compile -Options @("/nologo","/EHsc","/O2","/std:c++14",
                "/I$($Inc[$Dim])",$Driver,$Obj[$Dim],"/Fe$Exe")
        }
    }
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
                if($Mode -eq "baseline" -and $Label -in @("extreme","multiblock")) {
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
            if($Mode -eq "candidate-02") {throw "Candidate-02 must pass A1-A3: dimension $Dim"}
            Write-Host "A1-A3 $($Dim)D differences recorded."
        }

        $Log=Join-Path $Output "$($Dim)d_location.txt"
        $Code=RunNative -Name "locate$($Dim)d" -Params @() -Log $Log
        Show -Log $Log -Pattern "^(SCENARIO |SUMMARY )"
        if($Code -notin @(0,1)) {throw "A4-A5 control/oracle failure, exit $Code"}
        if($Code -eq 1) {Write-Host "A4-A5 $($Dim)D differences recorded."}

        $Log=Join-Path $Output "$($Dim)d_coverage.txt"
        $Code=RunNative -Name "coverage$($Dim)d" -Params @() -Log $Log
        Show -Log $Log -Pattern "^SUMMARY A6_A7"
        if($Code -notin @(0,1) -or ($Mode -ne "baseline" -and $Code -ne 0)) {
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
        if($Code -notin @(0,1) -or ($Mode -eq "candidate-02" -and $Code -ne 0)) {
            Get-Content -LiteralPath $Log |
                Where-Object {$_ -match "status=FAIL|ORACLE_|SUMMARY A8"} |
                Select-Object -First 40 | ForEach-Object {Write-Host $_}
            throw "A8 $($Dim)D control/oracle/candidate-02 failure, exit $Code"
        }
        if($Code -eq 1) {Write-Host "A8 $($Dim)D numerical differences recorded."}
    }
    Write-Host "MSVC run completed. Diagnostic failures are NOT accepted geometry."
} finally {
    if($null -ne $TempRoot -and (Test-Path -LiteralPath $TempRoot)) {
        Remove-Item -LiteralPath $TempRoot -Recurse -Force
    }
}
# PowerShell's caller otherwise inherits the last expected native exit 1.
# Real failures throw above and never reach this point.
exit 0
