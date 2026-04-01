$ProgressPreference = 'SilentlyContinue'
$url = "https://reqrypt.org/download/WinDivert-2.2.0-D.zip"
$zipPath = "WinDivert.zip"
Write-Host "Downloading WinDivert..."
Invoke-WebRequest -Uri $url -OutFile $zipPath
Write-Host "Unzipping..."
Expand-Archive -Path $zipPath -DestinationPath . -Force
Write-Host "Done"
