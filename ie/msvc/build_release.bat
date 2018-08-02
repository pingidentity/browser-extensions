set CONFIGURATION=Release

cmd.exe /C " msbuild.exe /p:Platform=Win32 /p:Configuration=%CONFIGURATION% ForgeIE.sln "
cmd.exe /C " msbuild.exe /p:Platform=x64 /p:Configuration=%CONFIGURATION% ForgeIE.sln "

set RELEASE_PATH_32=..\build\Win32\Release
set RELEASE_PATH_64=..\build\x64\Release
set INSTALL_DIR=C:\Program Files\Ping Identity\PingOne Extension

if exist "%INSTALL_DIR%" (
    xcopy /q /y "%RELEASE_PATH_64%\bho64.dll" "%INSTALL_DIR%"
    xcopy /q /y "%RELEASE_PATH_64%\forge64.dll" "%INSTALL_DIR%"
    xcopy /q /y "%RELEASE_PATH_64%\forge64.exe" "%INSTALL_DIR%"
    xcopy /q /y "%RELEASE_PATH_64%\frame64.dll" "%INSTALL_DIR%"

    xcopy /q /y "%RELEASE_PATH_32%\bho32.dll" "%INSTALL_DIR%"
    xcopy /q /y "%RELEASE_PATH_32%\forge32.dll" "%INSTALL_DIR%"
    xcopy /q /y "%RELEASE_PATH_32%\forge32.exe" "%INSTALL_DIR%"
    xcopy /q /y "%RELEASE_PATH_32%\frame32.dll" "%INSTALL_DIR%"
)