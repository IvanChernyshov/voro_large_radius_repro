# Pinned Voro++ dev: native x64 MSVC baseline-vs-correction verification.
# Applies the experimental patch only to a temporary source copy.
param(
    [ValidateSet("baseline","combined")][string]$Mode="baseline",
    [ValidateSet("minimal","extended")][string]$Suite="extended"
)
Set-StrictMode -Version Latest
$ErrorActionPreference="Stop"
$Root=(Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$Original=Join-Path $Root "vendor/voro-dev"
$Voro=$Original
$Build=Join-Path $Root "build/dev-$Mode-msvc"
$Output=Join-Path $Root "output/dev-$Mode-msvc"
[System.IO.Directory]::CreateDirectory($Build) | Out-Null
[System.IO.Directory]::CreateDirectory($Output) | Out-Null
if(-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {throw "MSVC cl.exe unavailable; use a VS Developer PowerShell."}
if($Mode -eq "combined" -and -not (Get-Command git.exe -ErrorAction SilentlyContinue)) {throw "git.exe needed to apply dev correction."}
function Compile([string[]]$Options) {
    & cl.exe @Options
    if($LASTEXITCODE -ne 0) {throw "MSVC compilation error (exit=$LASTEXITCODE)"}
}
function RunNative([string]$Name,[string]$Log) {
    $Exe=Join-Path $Build "$Name.exe"
    & $Exe > $Log 2>&1
    return [int]$LASTEXITCODE
}
$TempRoot=$null
try {
    if($Mode -eq "combined") {
        $TempRoot=Join-Path ([System.IO.Path]::GetTempPath()) ("voro-dev-msvc-"+[guid]::NewGuid().ToString("N"))
        [System.IO.Directory]::CreateDirectory($TempRoot) | Out-Null
        $Voro=Join-Path $TempRoot "voro"
        Copy-Item -LiteralPath $Original -Destination $Voro -Recurse -Force
        $Patch=Join-Path $Root "patches/dev/candidate-02-plus-03.patch"
        Push-Location $Voro
        try {
            & git.exe apply --check $Patch
            if($LASTEXITCODE -ne 0) {throw "Dev patch preflight failed"}
            & git.exe apply $Patch
            if($LASTEXITCODE -ne 0) {throw "Dev patch application failed"}
        } finally {Pop-Location}
    }
    Write-Host "dev snapshot=61099f682c6672a275b6be7de5923f2a1a7bf05e mode=$Mode suite=$Suite compiler=MSVC"
    $Inc=Join-Path $Voro "src"
    $Obj=Join-Path $Build "voro-dev.obj"
    $Source=Join-Path $Voro "src/voro++.cc"
    Compile -Options @("/nologo","/EHsc","/O2","/std:c++14","/I$Inc","/c","/Fo$Obj","/Tp$Source")
    $Drivers=@("minimal2d","minimal3d")
    if($Suite -eq "extended") {
        $Drivers+=@("arithmetic2d","owner2d","stress2d","arithmetic3d","owner3d","stress3d")
    }
    foreach($Name in $Drivers) {
        $Driver=Join-Path $Root "repro/dev/$Name.cpp"
        $Exe=Join-Path $Build "$Name.exe"
        Compile -Options @("/nologo","/EHsc","/O2","/std:c++14","/I$Inc",$Driver,$Obj,"/Fe$Exe")
    }
    $ExpectedStatus=if($Mode -eq "baseline") {"FAIL"} else {"PASS"}
    $ExpectedExit=if($Mode -eq "baseline") {1} else {0}
    $ExpectedFailures=if($Mode -eq "baseline") {2} else {0}
    foreach($Dim in @(2,3)) {
        $Log=Join-Path $Output "$($Dim)d_minimal.txt"
        $Code=RunNative -Name "minimal$($Dim)d" -Log $Log
        Get-Content -LiteralPath $Log | ForEach-Object {Write-Host $_}
        if($Code -ne $ExpectedExit) {throw "Dev minimal $Dim D: native exit=$Code, expected=$ExpectedExit"}
        foreach($Check in @("geometry","owner")) {
            $Match="^RESULT dim=$Dim check=$Check status=$ExpectedStatus\s"
            if(-not (Select-String -LiteralPath $Log -Pattern $Match -Quiet)) {
                throw "Dev minimal $Dim D: missing expected $ExpectedStatus result for $Check"
            }
        }
        if(-not (Select-String -LiteralPath $Log -Pattern "^SUMMARY minimal dim=$Dim failed_checks=$ExpectedFailures$" -Quiet)) {
            throw "Dev minimal $Dim D: invalid summary"
        }
    }
    if($Suite -eq "extended") {
        foreach($Dim in @(2,3)) {
            foreach($Stem in @("arithmetic","owner","stress")) {
                $Name="$Stem$($Dim)d"
                $Log=Join-Path $Output "$($Dim)d_$Stem.txt"
                $Code=RunNative -Name $Name -Log $Log
                Get-Content -LiteralPath $Log |
                    Where-Object {$_ -match "^SUMMARY (A10|A9|dimension=)" } |
                    ForEach-Object {Write-Host $_}
                if($Code -notin @(0,1) -or ($Mode -eq "combined" -and $Code -ne 0)) {
                    Get-Content -LiteralPath $Log |
                        Where-Object {$_ -match "ORACLE_|FAIL_CASE|WRONG|SUMMARY |voro\+\+:" } |
                        Select-Object -Last 35 | ForEach-Object {Write-Host $_}
                    throw "Dev $Mode $Dim D ${Stem}: native failure $Code"
                }
            }
        }
    }
    Write-Host "SUMMARY dev-msvc mode=$Mode suite=$Suite status=PASS expected_native_result=$ExpectedStatus"
} finally {
    if($null -ne $TempRoot -and (Test-Path -LiteralPath $TempRoot)) {
        Remove-Item -LiteralPath $TempRoot -Recurse -Force
    }
}
# Final diagnostic exit 1 is expected for baseline; do not propagate it.
exit 0
