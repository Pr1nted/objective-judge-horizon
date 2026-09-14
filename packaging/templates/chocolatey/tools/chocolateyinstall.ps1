$ErrorActionPreference = 'Stop'
$toolsDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
Install-ChocolateyZipPackage -PackageName 'ojh' -Url64bit '{{url_windows_x64}}' -Checksum64 '{{sha256_windows_x64}}' -ChecksumType64 'sha256' -UnzipLocation $toolsDir
