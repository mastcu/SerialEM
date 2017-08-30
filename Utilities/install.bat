@echo off
SetLocal
Rem # This cd is needed when run as administrator
cd /d %~dp0

IF "%CD%" == "C:\Program Files\SerialEM" (
  echo.
  echo ERROR!
  echo You need to run this script from the subfolder that the SerialEM
  echo package was unpacked in, not from a copy in C:\Program Files\SerialEM
  echo.
  pause
  exit
)

rem Find out if there is an FEI or JEOL plugin already
set HASFEIPLUG=1
IF NOT EXIST ..\FEIScopePlugin.dll IF NOT EXIST ../Plugins\FEIScopePlugin.dll SET HASFEIPLUG=0
set HASJEOLPLUG=1
IF NOT EXIST ..\JeolScopePlugin.dll IF NOT EXIST ../Plugins\JeolScopePlugin.dll SET HASJEOLPLUG=0

rem # Clean up all existing and older files
for %%A in (SerialEM.exe SERIALEM.HLP SerialEM.cnt SerialEM.chm FTComm.dll jpeg62.dll zlib1.dll libtiff3.dll b3dregsvr.exe b3dregsvr32.exe b3dregsvr64.exe MFC71.dll msvcp71.dll msvcr71.dll register.bat register-GMS1.bat register-GMS2-32.bat register-GMS2-64.bat SEMCCD-GMS2-32.dll SEMCCD-GMS2-64.dll SEMCCDps-GMS2-32.dll SEMCCDps-GMS2-64.dll SerialEMCCD.dll SerialEMCCDps.dll msvcp90.dll mfc90.dll msvcr90.dll vcomp90.dll FEIScopePlugin.dll Plugins\FEIScopePlugin.dll JeolScopePlugin.dll Plugins\JeolScopePlugin.dll TietzPlugin.dll Plugins\TietzPlugin.dll DEcamPlugin.dll Plugins\DEcamPlugin.dll DeInterface.Win32.dll Plugins\DeInterface.Win32.dll libifft-MKL.dll libifft-MKL-64.dll libiomp5md.dll) DO (
  IF EXIST ..\%%A DEL ..\%%A
)

FOR %%A in (Microsoft.VC90.CRT Microsoft.VC90.MFC Microsoft.VC90.OPENMP) DO (
  IF EXIST ..\%%A RMDIR /Q /S ..\%%A
)

IF EXIST ..\FocusRamper.exe (
  ..\FocusRamper /UnregServer
  del ..\FocusRamper.exe
)

REM # Check Windows Version  (w2K 5.0, XP 5.1, Vista 6.0, Win7 6.1)
ver | findstr /i " 5\.0" > nul
IF %ERRORLEVEL% EQU 0 (

  Rem # Windows 2000
  FOR %%A in (Microsoft.VC90.CRT Microsoft.VC90.MFC) DO (
    COPY /Y %%A\*.dll ..
  )
) ELSE (

  Rem # All other OS's understand manifests, etc
  FOR %%A in (Microsoft.VC90.CRT Microsoft.VC90.MFC) DO (
    XCOPY /Q /S /Y /I %%A ..\%%A
  )
)

Rem # Check properties file(s) for Tietz or DE cameras
set SAWPROPS=0
set NEEDTIETZ=0
set NEEDDECAM=0
IF EXIST "C:\Program Files\SerialEM\SerialEMproperties.txt" (
   set SAWPROPS=1
  findstr /I "^TietzCameraType" "C:\Program Files\SerialEM\SerialEMproperties.txt"

  Rem # WOW! %ERRORLEVEL% COMES OUT AS 0 IN A BATCH FILE AND THIS IS THE ONLY
  Rem # WAY TO TEST HERE
  IF NOT ERRORLEVEL 1 set NEEDTIETZ=1
  findstr /I "^DEcameraType" "C:\Program Files\SerialEM\SerialEMproperties.txt" 1> nul
  IF NOT ERRORLEVEL 1 set NEEDDECAM=1
)

IF EXIST "C:\ProgramData\SerialEM\SerialEMproperties.txt" (
   set SAWPROPS=1
  findstr /I "^TietzCameraType" "C:\ProgramData\SerialEM\SerialEMproperties.txt" 1> nul
  IF NOT ERRORLEVEL 1 set NEEDTIETZ=1
  findstr /I "^DEcameraType" "C:\ProgramData\SerialEM\SerialEMproperties.txt" 1> nul
  IF NOT ERRORLEVEL 1 set NEEDDECAM=1
)

COPY /Y SerialEM.exe ..
COPY /Y SerialEM.chm ..
COPY /Y libifft-*.dll ..
COPY /Y libiomp5md.dll ..

Rem # If neither properties file seen, just copy them
if %SAWPROPS% EQU 0 (
  set NEEDTIETZ=1
  set NEEDDECAM=1
  echo.
  echo Copying Tietz and Direct Electron plugins because no properties file was found
)

IF  %SAWPROPS% EQU 1 IF %NEEDTIETZ% EQU 1 (
  echo.
  echo Copying Tietz plugin because a Tietz camera is listed in a properties file
)
if %NEEDTIETZ% EQU 1 COPY /Y TietzPlugin.dll ..

IF  %SAWPROPS% EQU 1 IF %NEEDDECAM% EQU 1 (
  echo.
  echo Copying DE plugin because a DE camera is listed in a properties file
)
if %NEEDDECAM% EQU 1 (
  COPY /Y DEcamPlugin.dll ..
  COPY /Y DeInterface.Win32.dll ..
)

IF EXIST FTComm.dll COPY /Y FTComm.dll ..

Rem # New tests for different scope possibilities
set FEISCOPE=1
IF NOT EXIST C:\Tecnai IF NOT EXIST C:\Titan IF NOT EXIST "C:\Program Files\FEI" IF NOT EXIST "C:\Program Files (x86)\FEI" set FEISCOPE=0
set JEOLSCOPE=1
IF NOT EXIST "C:\Program Files\JEOL" IF NOT EXIST "C:\Program Files (x86)\JEOL" set JEOLSCOPE=0
set HITACHI=1
IF NOT EXIST "C:\Program Files\Hitachi" IF NOT EXIST "C:\Program Files (x86)\Hitachi" set HITACHI=0

Rem # They say nested IFs work but it didn't work here so just use GOTOs
IF %FEISCOPE% EQU 0 GOTO :NoRamper
set /p choice=Do you want to install FocusRamper for dynamic focusing in STEM? [y/n]
IF /I "%choice%"=="Y" (
  COPY /Y FocusRamper.exe ..
  ..\FocusRamper /RegServer
)

:NoRamper

Rem # Assess and copy scope plugin: do it if FEI is installed, do not do it if no FEI but
Rem # JEOL or Hitachi is installed, do it if it was already there, and ask if nothing is installed

set SCOPEPLUG=0
set REMOTEFEI=0
set REMOTEHITACHI=0

Rem # FEI software trumps all if it is there
IF %FEISCOPE% EQU 1 (
  echo.
  echo Copying FEI scope plugin because FEI software is installed
  set SCOPEPLUG=1
  set SCOPEDLL=FEIScopePlugin.dll
  GOTO :ScopePlug
)

Rem # JEOL software without FEI trumps Hitachi, copy plugin
IF  %JEOLSCOPE% EQU 1 (
  echo.
  echo Copying JEOL scope plugin because JEOL software is installed
  set SCOPEPLUG=1
  set SCOPEDLL=JeolScopePlugin.dll
  GOTO :ScopePlug
)

Rem # No FEI, no JEOL, but Hitachi, install Hitachi plugin if is there, or skip plugin
IF %HITACHI% EQU 1 IF NOT EXIST HitachiPlugin.dll  GOTO :NoScopePlug
IF %HITACHI% EQU 1 (
  echo.
  echo Copying Hitachi scope plugin because Hitachi software is installed
  set SCOPEPLUG=1
  set SCOPEDLL=HitachiPlugin.dll
  GOTO :ScopePlug
)

Rem # Now if FEI already installed, reinstall it
if %HASFEIPLUG% EQU 1 (
  echo Copying FEI scope plugin because it was already installed
  set SCOPEPLUG=1
  set REMOTEFEI=1
  set SCOPEDLL=FEIScopePlugin.dll
  GOTO :ScopePlug
)

Rem # If Jeol already installed, reinstall it
if %HASJEOLPLUG% EQU 1 (
  echo Copying JEOL scope plugin because it was already installed
  set SCOPEPLUG=1
  set SCOPEDLL=JeolScopePlugin.dll
  GOTO :ScopePlug
)

set HASHITACHIPLUG=1
if NOT EXIST ..\HitachiPlugin.dll IF NOT EXIST ..\Plugins\HitachiPlugin.dll set HASHITACHIPLUG=0
IF  EXIST HitachiPlugin.dll IF %HASHITACHIPLUG% EQU 1 (
  echo Copying Hitachi scope plugin because it was already installed
  set SCOPEPLUG=1
  set REMOTEHITACHI=1
  set SCOPEDLL=HitachiPlugin.dll
)

Rem # Nothing at all: ask first about FEI
set /p choice=Do you want to install the plugin for accessing an FEI scope remotely? [y/n]
IF /I "%choice%"=="Y" (
  set SCOPEPLUG=1
  set REMOTEFEI=1
  set SCOPEDLL=FEIScopePlugin.dll
  GOTO :ScopePlug
)

Rem # Then ask about Hitachi if plugin is there
if NOT EXIST HitachiPlugin.dll  GOTO :NoScopePlug
set /p choice=Do you want to install the plugin for accessing a Hitachi scope remotely? [y/n]
IF /I "%choice%"=="Y" (
  set SCOPEPLUG=1
  set REMOTEHITACHI=1
  set SCOPEDLL=HitachiPlugin.dll
)

:ScopePlug

IF %SCOPEPLUG% EQU 1 (
  IF EXIST "..\HitachiPlugin.dll" DEL "..\HitachiPlugin.dll"
  IF EXIST "..\Plugins\HitachiPlugin.dll" DEL "..\Plugins\HitachiPlugin.dll"
  COPY /Y %SCOPEDLL%  "..\"
)

:NoScopePlug

IF %FEISCOPE% EQU 0 GOTO :NoFEIserver
set COPYSERVER=0
IF NOT EXIST b3dregsvr32.exe (
  echo Copying FEI-SEMserver.exe to SerialEM folder because this is a 64-bit FEI scope
  set COPYSERVER=1
) ELSE IF EXIST ..\FEI-SEMserver.exe (
  echo Copying FEI-SEMserver.exe to SerialEM folder to replace the version there
  set COPYSERVER=1
)
IF %COPYSERVER% EQU 1 (
  IF EXIST ..\FEI-SEMserver.exe DEL ..\FEI-SEMserver.exe
  COPY /Y  FEI-SEMserver.exe "..\"
)

:NoFEIserver

Rem # Update JeolCamPlugin.dll if it is there
IF EXIST JeolCamPlugin.dll IF EXIST  ..\JeolCamPlugin.dll (
  echo.
  echo Copying JeolCamPlugin.dll to SerialEM folder because it is already there
  DEL  ..\JeolCamPlugin.dll
  COPY /Y  JeolCamPlugin.dll "..\"
)

Rem # Update FrameGPU.dll if it is there
IF EXIST FrameGPU.dll IF EXIST  C:\ProgramData\Gatan\Plugins\FrameGPU.dll (
  echo.
  echo Copying FrameGPU.dll to ProgramData\Gatan\Plugins because it is already there
  DEL C:\ProgramData\Gatan\Plugins\FrameGPU.dll
  COPY /Y FrameGPU.dll C:\ProgramData\Gatan\Plugins
)


set REGISTER=0
IF EXIST "C:\Program Files\Gatan\DigitalMicrograph\Plugins" (

  Rem # GMS 1
  set REGISTER=register-GMS1.bat
  COPY /Y SerialEMCCD.dll ..
  COPY /Y SerialEMCCDps.dll ..
  echo. 
  echo The DM plugin for GMS 1.x will now be copied and registered
  GOTO :CheckPlugDone
)

IF NOT EXIST "C:\Program Files\Gatan\Plugins" GOTO :CheckPlugDone

FOR /F "tokens=2 delims=." %%A IN ('reg QUERY "HKLM\SOFTWARE\Gatan" /v Version') do set Minor=%%A
FOR /F "tokens=3 delims= " %%A IN ('reg QUERY "HKLM\SOFTWARE\Gatan" /v Version') do set FullVers=%%A
set Major=%FullVers:~0,1%

set BIT64=0
IF %Major% EQU 3 (
  set versRange64=2.31 and higher
  set SEMCCD64=SEMCCD-GMS2.31-64.dll
  set BIT64=1
) ELSE IF %Minor% LSS 30 (
  set versRange32=2.0-2.2
  set SEMCCD32=SEMCCD-GMS2.0-32.dll
  set versRange64=2.2
  set SEMCCD64=SEMCCD-GMS2.2-64.dll
) ELSE IF %Minor% EQU 30 (
  set versRange32=2.30 and higher
  set SEMCCD32=SEMCCD-GMS2.30-32.dll
  set versRange64=2.30
  set SEMCCD64=SEMCCD-GMS2.30-64.dll
) ELSE (
  set versRange32=2.30 and higher
  set SEMCCD32=SEMCCD-GMS2.30-32.dll
  set versRange64=2.31 and higher
  set SEMCCD64=SEMCCD-GMS2.31-64.dll
)

dir C:\ProgramData\Gatan\Licenses\*64-Bit* > nul 2>&1
IF %ERRORLEVEL% EQU 0 (
  set BIT64=1
)

if %BIT64% EQU 1 (

  Rem # 64-bit GMS2
  set REGISTER=register-GMS2-64.bat
  COPY /Y %SEMCCD64% ..\SEMCCD-GMS2-64.dll
  IF EXIST SEMCCDps-GMS2-32.dll  COPY /Y  SEMCCDps-GMS2-32.dll ..
  COPY /Y  SEMCCDps-GMS2-64.dll ..
  COPY /Y  b3dregsvr64.exe ..
  IF EXIST b3dregsvr32.exe COPY /Y  b3dregsvr32.exe ..
  echo.
  echo Detected 64-bit GMS version %Major%.%Minor%
  echo The DM plugin for 64-bit GMS %versRange64% will now be copied and registered
) ELSE (

  Rem # 32-bit GMS2
  set REGISTER=register-GMS2-32.bat
  COPY /Y %SEMCCD32% ..\SEMCCD-GMS2-32.dll
  COPY /Y  SEMCCDps-GMS2-32.dll ..
  COPY /Y  b3dregsvr32.exe ..
  echo. 
  echo Detected 32-bit GMS version %Major%.%Minor%
  echo The DM plugin for 32-bit GMS %versRange32% will now be copied and registered
)

:CheckPlugDone

Rem # Run the register batch file if one was defined
IF NOT %REGISTER% == 0 (
  echo First make sure DigitalMicrograph is not running!
  pause
  COPY /Y %REGISTER% ..
  CD ..
  Rem # Use call to return to here
  CALL %REGISTER%
)

echo.
echo All Done.
echo There should not be any error messages in the above.
if NOT %REMOTEFEI% == 0 (
  echo ...
  echo To access an FEI scope remotely, copy FEI-SEMserver.exe from this package 
  echo directory to somewhere on the scope computer such as
  echo C:\Program Files\SerialEM
  echo Run it to start the server before starting SerialEM.
  echo ... 
)
if NOT %REMOTEHITACHI% == 0 (
  echo ...
  echo To access a Hitachi scope remotely, copy ShMemSEMserver.exe from this
  echo package directory to somewhere on the scope computer such as
  echo C:\Program Files\SerialEM
  echo Run it to start the server before starting SerialEM.
  echo ... 
)
pause
