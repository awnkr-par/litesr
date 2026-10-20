param(
 [ValidateSet('stable','beta')][string]$Channel = 'beta',
 [switch]$Manual,
 [int]$ParentPid = 0,
 [string]$CurrentVersion = '0.2.0-alpha.23',
 [switch]$SelfTest
)
$ErrorActionPreference = 'Stop'
$logDir = Join-Path $env:APPDATA 'LiteSR'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$logFile = Join-Path $logDir 'updater.log'
function Log([string]$message) {
 Add-Content -LiteralPath $logFile -Value ((Get-Date -Format 'yyyy-MM-dd HH:mm:ss') + ' ' + $message) -Encoding UTF8
}
Log "Updater started version=$CurrentVersion manual=$Manual parent=$ParentPid" 
Add-Type -AssemblyName System.Windows.Forms
function Notify([string]$message) {
 if ($Manual) { [void][System.Windows.Forms.MessageBox]::Show($message,'LiteSR Updates','OK','Information') }
}
function VersionKey([string]$tag) {
) { return $null }
 $major=[int]$Matches[1];$minor=[int]$Matches[2];$patch=[int]$Matches[3]
 $label=$Matches[4];$pre=if ($Matches[5]) {[int]$Matches[5]} else {0}
 $rank=if(-not $label){9}elseif($label -eq 'alpha'){1}elseif($label -eq 'beta'){2}else{0}
 return ('{0:D8}.{1:D8}.{2:D8}.{3:D2}.{4:D8}' -f $major,$minor,$patch,$rank,$pre)
}
if ($SelfTest) {
 $cases=@(
  @{a='v0.2.0-alpha.9';b='v0.2.0-alpha.10'},
  @{a='v0.2.0-alpha.22';b='v0.2.0-alpha.23'},
  @{a='v0.2.0-alpha.23';b='v0.2.0-beta.1'},
  @{a='v0.2.0-beta.1';b='v0.2.0'},
  @{a='v0.2.0';b='v0.2.1-alpha.1'}
 )
 foreach($case in $cases){if((VersionKey $case.a) -ge (VersionKey $case.b)){throw "Version order test failed: $($case.a) / $($case.b)"}}
 Write-Output 'Updater version ordering tests passed'
 exit 0
}
try {
 $installed = $CurrentVersion
 $headers = @{ 'User-Agent' = 'LiteSR-Updater/0.2.0'; 'Accept' = 'application/vnd.github+json' }
 $releases = @(Invoke-RestMethod -Uri 'https://api.github.com/repos/awnkr-par/litesr/releases?per_page=30' -Headers $headers -TimeoutSec 20)
 $eligible = @($releases | Where-Object {
   -not $_.draft -and ($Channel -eq 'beta' -or -not $_.prerelease) -and $_.tag_name -match '^v[0-9]+\.[0-9]+\.[0-9]+(?:-[0-9A-Za-z.-]+)?$'
 })
 if ($eligible.Count -eq 0) { Notify 'No releases on the selected update channel.'; exit 0 }
 $currentKey = VersionKey "v$installed"
 $release = $eligible | Where-Object { VersionKey $_.tag_name } | Sort-Object -Property @{Expression={ VersionKey $_.tag_name }} -Descending | Select-Object -First 1
 if (-not $release) { Log 'No valid release versions found'; Notify 'No valid update releases found.'; exit 0 }
 Log "Latest release=$($release.tag_name) installed=$installed"
 if ((VersionKey $release.tag_name) -le $currentKey) { Log 'Already current'; Notify "LiteSR $installed is up to date."; exit 0 }
 $assetName = "LiteSR-$($release.tag_name.Substring(1))-Setup-x64.exe"
 $installerAsset = $release.assets | Where-Object { $_.name -ceq $assetName } | Select-Object -First 1
 $hashAsset = $release.assets | Where-Object { $_.name -ceq 'SHA256SUMS.txt' } | Select-Object -First 1
 if (-not $installerAsset -or -not $hashAsset) { throw 'Release missing installer or checksum file.' }
 $answer = [System.Windows.Forms.MessageBox]::Show(
   "LiteSR $($release.tag_name) is available ($Channel channel). Download, verify, and install this update after LiteSR exits?",
   'LiteSR Update Available','YesNo','Question')
 if ($answer -ne [System.Windows.Forms.DialogResult]::Yes) { Log 'Update declined'; exit 0 }
 $folder = Join-Path ([IO.Path]::GetTempPath()) ("LiteSR-Update-" + [guid]::NewGuid().ToString('N'))
 New-Item -ItemType Directory -Path $folder | Out-Null
 $installer = Join-Path $folder $assetName
 $checksums = Join-Path $folder 'SHA256SUMS.txt'
 # browser_download_url comes from the GitHub Releases API, but restrict host and HTTPS.
 foreach ($asset in @($installerAsset,$hashAsset)) {
   $uri = [Uri]$asset.browser_download_url
   if ($uri.Scheme -ne 'https' -or $uri.Host -ne 'github.com' -or
       -not $uri.AbsolutePath.StartsWith('/awnkr-par/litesr/releases/download/')) { throw 'Unexpected download host.' }
 }
 Invoke-WebRequest -Uri $installerAsset.browser_download_url -OutFile $installer -TimeoutSec 300 -UseBasicParsing
 Invoke-WebRequest -Uri $hashAsset.browser_download_url -OutFile $checksums -TimeoutSec 30 -UseBasicParsing
 $line = Get-Content -LiteralPath $checksums | Where-Object { $_ -match ('^[a-fA-F0-9]{64}\s+\*?' + [regex]::Escape($assetName) + '$') } | Select-Object -First 1
 if (-not $line) { throw 'Installer checksum entry missing.' }
 $expected = ($line -split '\s+')[0].ToLowerInvariant()
 $actual = (Get-FileHash -LiteralPath $installer -Algorithm SHA256).Hash.ToLowerInvariant()
 if ($actual -ne $expected) { throw 'Installer checksum mismatch. Update cancelled.' }
 Log 'SHA256 verification succeeded'
 if ($ParentPid -gt 0) {
   # LiteSR only shuts down after the user explicitly approved this verified update.
   $settingsDir = Join-Path $env:APPDATA 'LiteSR'
   New-Item -ItemType Directory -Force -Path $settingsDir | Out-Null
   $marker = Join-Path $settingsDir 'update-ready.flag'
   [IO.File]::WriteAllText($marker, [string]$ParentPid)
   Log 'Sent application shutdown request'
   try { Wait-Process -Id $ParentPid -Timeout 300 -ErrorAction Stop }
   catch { Remove-Item $marker -ErrorAction SilentlyContinue; throw 'LiteSR did not close within five minutes. Update cancelled.' }
 }
 # Preserve user's previous installer choices; Inno Setup replaces app files and keeps uninstall registration.
 Log 'Launching installer'
 $p = Start-Process -FilePath $installer -ArgumentList '/SILENT','/NORESTART','/CLOSEAPPLICATIONS' -PassThru -Wait
 if ($p.ExitCode -ne 0) { throw "Installer exited with code $($p.ExitCode)." }
 Log 'Installation complete'
 [void][System.Windows.Forms.MessageBox]::Show('LiteSR updated successfully. Reopen it from Start Menu.','LiteSR','OK','Information')
} catch {
 Log ('ERROR: ' + $_.Exception.Message)
 [void][System.Windows.Forms.MessageBox]::Show(("Update could not complete: " + $_.Exception.Message),'LiteSR Updater','OK','Warning') | Out-Null
 exit 1
}
