set CONFIGURATION=Release

cmd.exe /C " msbuild.exe /p:Platform=Win32 /p:Configuration=%CONFIGURATION% ForgeIE.sln "
cmd.exe /C " msbuild.exe /p:Platform=x64 /p:Configuration=%CONFIGURATION% ForgeIE.sln "