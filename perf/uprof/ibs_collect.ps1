param([int]$DelaySec = 45, [int]$DurSec = 60, [string]$Tag = 'ibs1',
      [string]$Cli = 'C:\Program Files\AMD\AMDuProf\bin\AMDuProfCLI.exe',   # AMD uProf CLI
      [string]$Base = (Join-Path $env:TEMP 'uprof_dl'))                          # where the log and the sessions go
$cli = $Cli
$base = $Base
$log = "$base\ibs_collect_$Tag.log"
$out = "$base\sessions"
New-Item -ItemType Directory -Force -Path $out | Out-Null
"start $(Get-Date -Format HH:mm:ss) delay=$DelaySec dur=$DurSec" | Set-Content $log
$pid0 = $null
for ($i = 0; $i -lt 120; $i++) {
    $p = Get-Process reimpl-x64 -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($p) { $pid0 = $p.Id; break }
    Start-Sleep -Seconds 1
}
if (-not $pid0) { "no reimpl-x64 process within 120 s" | Add-Content $log; exit 2 }
"pid=$pid0 found at $(Get-Date -Format HH:mm:ss); waiting $DelaySec s for the town" | Add-Content $log
Start-Sleep -Seconds $DelaySec
"collect start $(Get-Date -Format HH:mm:ss)" | Add-Content $log
& $cli collect --config ibs --pid $pid0 -d $DurSec -o "$out\$Tag" 2>&1 | Add-Content $log
"collect rc=$LASTEXITCODE end $(Get-Date -Format HH:mm:ss)" | Add-Content $log
Get-ChildItem "$out\$Tag" -Recurse -ErrorAction SilentlyContinue | Select-Object FullName, Length | Out-String | Add-Content $log
