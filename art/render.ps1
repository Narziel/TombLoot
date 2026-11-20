# Renders the Nexus images of TombLoot from art.html with Microsoft Edge (headless).
#   key.png     1920x1080  primary image / thumbnail
#   banner.png  1920x480   page header
# Usage: & .\art\render.ps1            (sizes can be changed below)
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$edge = 'C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe'

# Fonts (SIL Open Font License, from Google Fonts): downloaded once, not kept in git.
$fonts = Join-Path $here 'fonts'
if (-not (Test-Path (Join-Path $fonts 'fonts.css'))) {
    New-Item -ItemType Directory -Force $fonts | Out-Null
    $css = (Invoke-WebRequest -UseBasicParsing -UserAgent 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) Chrome/120' `
        'https://fonts.googleapis.com/css2?family=Cinzel:wght@500;700;900&family=Cormorant+Garamond:ital,wght@1,600').Content
    foreach ($m in [regex]::Matches($css, 'https://fonts\.gstatic\.com/[^)]+')) {
        $url = $m.Value
        Invoke-WebRequest -UseBasicParsing $url -OutFile (Join-Path $fonts ($url -split '/')[-1])
    }
    $css = [regex]::Replace($css, 'https://fonts\.gstatic\.com/s/[a-z]+/v\d+/', '')
    Set-Content -Encoding utf8 (Join-Path $fonts 'fonts.css') $css
}

$page = 'file:///' + ((Join-Path $here 'art.html') -replace '\\', '/')
foreach ($shot in @(@{ Name = 'key'; Size = '1920,1080' }, @{ Name = 'banner'; Size = '1920,480' })) {
    & $edge --headless=new --disable-gpu --hide-scrollbars --virtual-time-budget=15000 "--window-size=$($shot.Size)" `
        "--screenshot=$(Join-Path $here "$($shot.Name).png")" "$page`?$($shot.Name)" 2>$null
}
Start-Sleep -Seconds 2
Get-ChildItem (Join-Path $here '*.png') | Select-Object Name, Length, LastWriteTime
