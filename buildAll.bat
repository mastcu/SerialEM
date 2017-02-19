Rem Builds all versions of the program, run as cmd /C buildAll.bat

set PATH=c:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\BIN\amd64;C:\windows\Microsoft.NET\Framework64\v4.0.30319;C:\windows\Microsoft.NET\Framework64\v3.5;c:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\VCPackages;c:\Program Files (x86)\Microsoft Visual Studio 10.0\Common7\IDE;c:\Program Files (x86)\Microsoft Visual Studio 10.0\Common7\Tools;C:\Program Files (x86)\HTML Help Workshop;C:\Program Files (x86)\Microsoft SDKs\Windows\v7.0A\bin\NETFX 4.0 Tools\x64;C:\Program Files (x86)\Microsoft SDKs\Windows\v7.0A\bin\x64;C:\Program Files (x86)\Microsoft SDKs\Windows\v7.0A\bin;%PATH%
set LIB=c:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\LIB\amd64;c:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\ATLMFC\LIB\amd64;C:\Program Files (x86)\Microsoft SDKs\Windows\v7.0A\lib\x64
set INCLUDE=c:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\INCLUDE;c:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\ATLMFC\INCLUDE;C:\Program Files (x86)\Microsoft SDKs\Windows\v7.0A\include

msbuild /t:Rebuild /p:Configuration=NoHang /p:Platform=Win32 SerialEM.vcxproj
if %errorlevel% neq 0 exit /b %errorlevel%

msbuild /t:Rebuild /p:Configuration=Release /p:Platform=Win32 SerialEM.vcxproj
if %errorlevel% neq 0 exit /b %errorlevel%

msbuild /t:Rebuild /p:Configuration=Release /p:Platform=x64 SerialEM.vcxproj
if %errorlevel% neq 0 exit /b %errorlevel%


