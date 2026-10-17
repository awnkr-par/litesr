param(
 [ValidateSet('stable','beta')][string]$Channel = 'beta',
 [switch]$Manual,
 [int]$ParentPid = 0
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms
function Notify([string]$message) {
 if ($Manual) { [void][System.Windows.Forms.MessageBox]::Show($message,'LiteSR Updates','OK','Information') }
}
try {
 $installed = '0.2.0-alpha.15'
 $headers = @{ 'User-Agent' = 'LiteSR-Updater/0.2.0'; 'Accept' = 'application/vnd.github+json' }
 $releases = @(Invoke-RestMethod -Uri 'https://api.github.com/repos/awnkr-par/litesr/releases?per_page=30' -Headers $headers -TimeoutSec 20)
 $eligible = @($releases | Where-Object {
   -not $_.draft -and ($Channel -eq 'beta' -or -not $_.prerelease) -and $_.tag_name -match '^v[0-9]+\.[0-9]+\.[0-9]+(?:-[0-9A-Za-z.-]+)?$'
 })
 if ($eligible.Count -eq 0) { Notify 'No releases on the selected update channel.'; exit 0 }
 function VersionKey([string]$tag) {
   if ($tag -notmatch '^v?(\d+)\.(\d+)\.(\d+)(?:-(.+))?$') { return $null }
   $suffix = $Matches[4]
   $rank = if (-not $suffix) { 2 } else { 1 }
   return ('{0:D8}.{1:D8}.{2:D8}.{3}.{4}' -f [int]$Matches[1],[int]$Matches[2],[int]$Matches[3],$rank,$suffix)
 }
 $currentKey = VersionKey "v$installed"
 $release = $eligible | Sort-Object -Property @{Expression={ VersionKey $_.tag_name }} -Descending | Select-Object -First 1
 if ((VersionKey $release.tag_name) -le $currentKey) { Notify "LiteSR $installed is up to date for the $Channel channel."; exit 0 }
 $assetName = "LiteSR-$($release.tag_name.Substring(1))-Setup-x64.exe"
 $installerAsset = $release.assets | Where-Object { $_.name -ceq $assetName } | Select-Object -First 1
 $hashAsset = $release.assets | Where-Object { $_.name -ceq 'SHA256SUMS.txt' } | Select-Object -First 1
 if (-not $installerAsset -or -not $hashAsset) { throw 'Release missing installer or checksum file.' }
 $answer = [System.Windows.Forms.MessageBox]::Show(
   "LiteSR $($release.tag_name) is available ($Channel channel). Download, verify, and install this update after LiteSR exits?",
   'LiteSR Update Available','YesNo','Question')
 if ($answer -ne [System.Windows.Forms.DialogResult]::Yes) { exit 0 }
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
 if ($ParentPid -gt 0) {
   # LiteSR only shuts down after the user explicitly approved this verified update.
   $settingsDir = Join-Path $env:APPDATA 'LiteSR'
   New-Item -ItemType Directory -Force -Path $settingsDir | Out-Null
   $marker = Join-Path $settingsDir 'update-ready.flag'
   [IO.File]::WriteAllText($marker, 'approved')
   try { Wait-Process -Id $ParentPid -Timeout 300 -ErrorAction Stop }
   catch { Remove-Item $marker -ErrorAction SilentlyContinue; throw 'LiteSR did not close within five minutes. Update cancelled.' }
 }
 # Preserve user's previous installer choices; Inno Setup replaces app files and keeps uninstall registration.
 $p = Start-Process -FilePath $installer -ArgumentList '/SILENT','/NORESTART','/CLOSEAPPLICATIONS' -PassThru -Wait
 if ($p.ExitCode -ne 0) { throw "Installer exited with code $($p.ExitCode)." }
 [void][System.Windows.Forms.MessageBox]::Show('LiteSR updated successfully. You can reopen it from Start Menu.','LiteSR','OK','Information')
} catch {
 [void][System.Windows.Forms.MessageBox]::Show(("Update could not complete: " + $_.Exception.Message),'LiteSR Updater','OK','Warning') | Out-Null
 exit 1
}
