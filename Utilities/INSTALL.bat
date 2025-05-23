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

set CUDA4DLL=cudart64_41_28.dll
set CUDA6DLL=cudart64_65.dll
set CUDA8DLL=cudart64_80.dll
set CUDA4FFT=cufft64_41_28.dll
set CUDA6FFT=cufft64_65.dll
set CUDA8FFT=cufft64_80.dll
set CUDA6FFTW=cufftw64_65.dll
set CUDA8FFTW=cufftw64_80.dll

rem Find out if there is an FEI or JEOL plugin already
set HASFEIPLUG=1
IF NOT EXIST ..\FEIScopePlugin.dll IF NOT EXIST ../Plugins\FEIScopePlugin.dll SET HASFEIPLUG=0
set HASJEOLPLUG=1
IF NOT EXIST ..\JeolScopePlugin.dll IF NOT EXIST ../Plugins\JeolScopePlugin.dll SET HASJEOLPLUG=0

rem Find out if there is an API1 or API2 DE dll
set HASDEAPI1=1
set HASDEAPI2=1
IF NOT EXIST ..\DEcamAPI1Plugin.dll IF NOT EXIST ../Plugins\DEcamAPI1Plugin.dll SET HASDEAPI1=0
IF NOT EXIST ..\DEcamAPI2Plugin.dll IF NOT EXIST ../Plugins\DEcamAPI2Plugin.dll SET HASDEAPI2=0

set GMS64PLUGS=SEMCCD-GMS2-64.dll SEMCCD-GMS2.2-64.dll SEMCCD-GMS2.30-64.dll SEMCCD-GMS2.31-64.dll^
 SEMCCD-GMS3.01-64.dll SEMCCD-GMS3.30-64.dll SEMCCD-GMS3.31-64.dll SEMCCD-GMS3.42-64.dll^
 SEMCCD-GMS3.50-64.dll SEMCCD-GMS3.60-64.dll

rem # Clean up all existing and older and near-future files
for %%A in (SerialEM.exe SERIALEM.HLP SerialEM.cnt SerialEM.chm FTComm.dll jpeg62.dll zlib1.dll^
 libtiff3.dll b3dregsvr.exe b3dregsvr32.exe b3dregsvr64.exe MFC71.dll msvcp71.dll msvcr71.dll^
 register.bat register-GMS1.bat register-GMS2-32.bat register-GMS2-64.bat SEMCCD-GMS2-32.dll^
 SEMCCDps-GMS2-32.dll SEMCCDps-GMS2-64.dll SerialEMCCD.dll SerialEMCCDps.dll %GMS64PLUGS% ^
 msvcp90.dll mfc90.dll msvcr90.dll vcomp90.dll FEIScopePlugin.dll Plugins\FEIScopePlugin.dll^
 JeolScopePlugin.dll Plugins\JeolScopePlugin.dll TietzPlugin.dll Plugins\TietzPlugin.dll^
 DEcamPlugin.dll Plugins\DEcamPlugin.dll DEcamAP1Plugin.dll Plugins\API1DEcamPlugin.dll ^
 DEcamAPI2Plugin.dll Plugins\DEcamAPI2Plugin.dll DeInterface.Win32.dll Plugins\DeInterface.Win32.dll^
 libifft-MKL.dll libifft-MKL-64.dll libiomp5md.dll libctffind.dll libmmd.dll imodzlib1.dll^
 hdf5.dll SerialEM_Snapshot.txt concrt140.dll mfc140.dll msvcp140.dll ucrtbase.dll^
 vcruntime140.dll msvcp120.dll msvcr120.dll DE.Win32.dll DE.Win64.dll svml_dispmd.dll^
 ctfplotter.exe ctfplotter.adoc) DO (
  IF EXIST ..\%%A DEL ..\%%A
)

IF EXIST ..\api-ms-win-core-console-l1-1-0.dll  DEL ..\api-ms*.dll

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
  echo .
  echo ERROR!
  echo SerialEM can no longer run on Windows 2000.  You must run it on a
  echo separate computer and connect with FEI-SEMserver.exe running on this computer
  echo.
  pause
  exit
)
set NEEDREDIST=0
ver | findstr /i  " 5\.1" > nul
IF %ERRORLEVEL% EQU 0 GOTO :TestXP
GOTO :XPTestDone

:TestXP
systeminfo | findstr /i "Service Pack 3" 1> nul
IF %ERRORLEVEL% EQU 0 (
  set NEEDREDIST=1
) ELSE (
  echo .
  echo ERROR!
  echo SerialEM can no longer run on Windows XP SP 1 or SP 2.  You must run it on a
  echo separate computer and connect with FEI-SEMserver.exe running on this computer
  echo.
  pause
  exit
)

:XPTestDone
COPY /Y Microsoft.VC140\*.dll .. > nul

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

IF EXIST SerialEM.exe COPY /Y SerialEM.exe ..
IF EXIST SerialEM.DontRunHere.bin COPY /Y SerialEM.DontRunHere.bin ..\SerialEM.exe
COPY /Y SerialEM.chm ..
COPY /Y libifft-*.dll ..
COPY /Y libiomp5md.dll ..
COPY /Y libmmd.dll ..
COPY /Y libctffind.dll ..
COPY /Y imodzlib1.dll ..
COPY /Y hdf5.dll ..
IF EXIST svml_dispmd.dll  COPY /Y svml_dispmd.dll ..
IF EXIST ctfplotter.exe  COPY /Y ctfplotter.exe ..
IF EXIST ctfplotter.adoc  COPY /Y ctfplotter.adoc ..
IF EXIST msvcp120.dll  COPY /Y msvcp120.dll ..
IF EXIST msvcr120.dll  COPY /Y msvcr120.dll ..
COPY /Y SerialEM_Snapshot.txt ..

Rem # If neither properties file seen, just copy them
if %SAWPROPS% EQU 0 (
  set NEEDTIETZ=1
  echo.
  echo Copying Tietz plugin because no properties file was found
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

IF %HASDEAPI1% EQU 1 (
  echo.
  echo Copying DE plugin for API1 because it was already present
  set NEEDDECAM=1  
)
IF %HASDEAPI2% EQU 1 (
  echo.
  echo Copying DE plugin for API2 because it was already present
  set NEEDDECAM=1  
)

IF %NEEDDECAM% EQU 0 GOTO :DEcamDone
IF %HASDEAPI1% EQU 1 GOTO :CopyAPI1
IF %HASDEAPI2% EQU 1 GOTO :CopyAPI2

CHOICE /C 120 /M "Enter 2 to copy the API2-only DLLs for a DE camera, 1 to copy the API1 DLLs, or 0 to skip copying: "
IF %ERRORLEVEL%==3 GOTO :DEcamDone
IF %ERRORLEVEL%==2 GOTO :CopyAPI2
IF %ERRORLEVEL%==1 GOTO :CopyAPI1

:CopyAPI1
COPY /Y DE-API1\*.dll ..
GOTO :DEcamDone

:CopyAPI2
COPY /Y DE-API2\*.dll ..

:DEcamDone

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
  GOTO :ScopePlug
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

set COPYSERVER=0
IF %FEISCOPE% EQU 0 GOTO :NoFEIserver
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

Rem # Copy AutoIT to be with FEI components or to update it
set COPYAUTOIT=0
IF EXIST ..\FeiScopePlugin.dll (
  echo Copying SEM-AutoIT.exe to SerialEM folder to be with FEI plugin
  set COPYAUTOIT=1
) ELSE IF EXIST ..\FEI-SEMserver.exe (
  echo Copying SEM-AutoIT.exe to SerialEM folder to be with FEI-SEMserver
  set COPYAUTOIT=1
) ELSE IF EXIST ..\SEM-AutoIT.exe (
  echo Copying SEM-AutoIT.exe to SerialEM folder to replace the version there
  set COPYAUTOIT=1
)
IF %COPYAUTOIT% EQU 1 (
  IF EXIST ..\SEM-AutoIT.exe DEL ..\SEM-AutoIT.exe
  COPY /Y SEM-AutoIT.exe "..\"
)

Rem # Update JeolCamPlugin.dll if it is there
IF EXIST JeolCamPlugin.dll IF EXIST  ..\JeolCamPlugin.dll (
  echo.
  echo Copying JeolCamPlugin.dll to SerialEM folder because it is already there
  DEL  ..\JeolCamPlugin.dll
  COPY /Y  JeolCamPlugin.dll "..\"
)

IF EXIST PythonModules (
  echo.
  echo Copying Python modules
  IF EXIST ..\PythonModules RMDIR /Q /S ..\PythonModules
  XCOPY /Q /S /Y /I PythonModules ..\PythonModules
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

Rem YOU CANNOT DO AN ELSE AFTER A COMPOUND IF
IF %Major% EQU 3 IF %Minor% GEQ 30 GOTO :VS2015GM3

IF %Major% EQU 3 (
  set versRange64=3.01 to 3.2x
  set SEMCCD64=SEMCCD-GMS3.01-64.dll
  set BIT64=1
) ELSE IF %Minor% LSS 30 (
  set versRange32=2.0-2.2
  set SEMCCD32=SEMCCD-GMS2.0-32.dll
  set versRange64=2.2
  set SEMCCD64=0
) ELSE IF %Minor% EQU 30 (
  set versRange32=2.30 and higher
  set SEMCCD32=SEMCCD-GMS2.30-32.dll
  set versRange64=2.30
  set SEMCCD64=0
) ELSE (
  set versRange32=2.30 and higher
  set SEMCCD32=SEMCCD-GMS2.30-32.dll
  set versRange64=2.31 - 2.3x
  set SEMCCD64=SEMCCD-GMS2.31-64.dll
)
GOTO :SetupSEMCCDdone

:VS2015GM3
IF %Minor% GEQ 62 (
  set versRange64=3.62 and higher
  set SEMCCD64=SEMCCD-GMS3.62-64.dll
  set BIT64=1
) ELSE IF %Minor% GEQ 60 (
  set versRange64=3.60-3.61
  set SEMCCD64=SEMCCD-GMS3.60-64.dll
  set BIT64=1
) ELSE IF %Minor% GEQ 50 (
  set versRange64=3.50-3.5x
  set SEMCCD64=SEMCCD-GMS3.50-64.dll
  set BIT64=1
) ELSE IF %Minor% GEQ 40 (
  set versRange64=3.40-3.4x
  set SEMCCD64=SEMCCD-GMS3.42-64.dll
  set BIT64=1
) ELSE IF %Minor% GEQ 31 (
  set versRange64=3.31-3.3x
  set SEMCCD64=SEMCCD-GMS3.31-64.dll
  set BIT64=1
) ELSE IF EXIST SEMCCD-GMS3.30-64.dll (
  set versRange64=3.30
  set SEMCCD64=SEMCCD-GMS3.30-64.dll
  set BIT64=1
) ELSE (
  echo.
  echo The DM plugin for GMS 3.30 is no longer distributed but you may find it
  echo in the FrameAlign folder on the SerialEM download site
  GOTO :CheckPlugDone
)

:SetupSEMCCDdone

dir C:\ProgramData\Gatan\Licenses\*64-Bit* > nul 2>&1
IF %ERRORLEVEL% EQU 0 (
  set BIT64=1
)
dir C:\ProgramData\Gatan\Licenses\*_x64.* > nul 2>&1
IF %ERRORLEVEL% EQU 0 (
  set BIT64=1
)
dir C:\ProgramData\Gatan\Licenses\*_64.* > nul 2>&1
IF %ERRORLEVEL% EQU 0 (
  set BIT64=1
)

IF %BIT64% EQU 1 IF %SEMCCD64% EQU 0 (
  echo.
  echo Detected 64-bit GMS version %Major%.%Minor%
  echo The DM plugin for 64-bit GMS %versRange64% is no longer included
  echo No new SEMCCD plugin is being installed
  echo You should upgrade your GMS version to 2.31 or higher
  GOTO :CheckPlugDone
)

if %BIT64% EQU 1 (

  Rem # 64-bit GMS2
  set REGISTER=register-GMS2-64.bat
  COPY /Y %SEMCCD64% ..\
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
set packDir=%cd%
IF %REGISTER% == 0 GOTO :RegisterDone

echo First make sure DigitalMicrograph is not running!
echo Allow a little extra time if you have to close it.
pause

@echo off
tasklist /fi "IMAGENAME eq DigitalMicrograph.exe" /nh | find "DigitalMicrograph.exe" > nul 2>&1
IF %errorlevel%==0 (
  echo Running instance of Digital Micrograph detected, requesting termination now, will forcibly end process in 15 seconds ...
  taskkill /im DigitalMicrograph.exe > nul
  ping -n 15 localhost> nul
  taskkill /f /im DigitalMicrograph.exe > nul 2>&1
)

COPY /Y %REGISTER% ..
CD ..
Rem # Use call to return to here
CALL %REGISTER%

:RegisterDone

CD "%packDir%"

Rem # Manage migration of FrameGPU and CUDA into Shrmemframe
set GATANPLUGDIR=C:\ProgramData\Gatan\Plugins
set SHRMEMDIR=C:\ProgramData\Gatan\Plugins\Shrmemframe
set INPLUG4=0
set INPLUG6=0
set INPLUG8=0
set INSHRMEM4=0
set INSHRMEM6=0
set INSHRMEM8=0
IF EXIST %GATANPLUGDIR%\FrameGPU.dll IF EXIST %GATANPLUGDIR%\%CUDA4DLL% IF EXIST %GATANPLUGDIR%\%CUDA4FFT% set INPLUG4=1
IF EXIST %GATANPLUGDIR%\FrameGPU.dll IF EXIST %GATANPLUGDIR%\%CUDA6DLL% IF EXIST %GATANPLUGDIR%\%CUDA6FFT% IF EXIST %GATANPLUGDIR%\%CUDA6FFTW% set INPLUG6=1
IF EXIST %GATANPLUGDIR%\FrameGPU.dll IF EXIST %GATANPLUGDIR%\%CUDA8DLL% IF EXIST %GATANPLUGDIR%\%CUDA8FFT% IF EXIST %GATANPLUGDIR%\%CUDA8FFTW% set INPLUG8=1
IF EXIST %SHRMEMDIR%\FrameGPU.dll IF EXIST %SHRMEMDIR%\%CUDA4DLL% IF EXIST %SHRMEMDIR%\%CUDA4FFT% set INSHRMEM4=1
IF EXIST %SHRMEMDIR%\FrameGPU.dll IF EXIST %SHRMEMDIR%\%CUDA6DLL% IF EXIST %SHRMEMDIR%\%CUDA6FFT% IF EXIST %SHRMEMDIR%\%CUDA6FFTW% set INSHRMEM6=1
IF EXIST %SHRMEMDIR%\FrameGPU.dll IF EXIST %SHRMEMDIR%\%CUDA8DLL% IF EXIST %SHRMEMDIR%\%CUDA8FFT% IF EXIST %SHRMEMDIR%\%CUDA8FFTW% set INSHRMEM8=1

IF %INPLUG4% == 1 IF %INSHRMEM4% == 0 IF %INSHRMEM6% == 0 IF %INSHRMEM8% == 0 (
   echo.
   echo Moving CUDA 4 .dlls from %GATANPLUGDIR% into Shrmemframe subfolder
   MOVE /Y %GATANPLUGDIR%\FrameGPU.dll %SHRMEMDIR%
   MOVE /Y %GATANPLUGDIR%\%CUDA4DLL% %SHRMEMDIR%
   MOVE /Y %GATANPLUGDIR%\%CUDA4FFT% %SHRMEMDIR%
   set INSHRMEM4=1
)
IF %INPLUG6% == 1 IF %INSHRMEM8% == 0 IF  %INSHRMEM6% == 0 IF %INSHRMEM4% == 0 (
   echo.
   echo Moving CUDA 6 .dlls from %GATANPLUGDIR% into Shrmemframe subfolder
   MOVE /Y %GATANPLUGDIR%\FrameGPU.dll %SHRMEMDIR%
   MOVE /Y %GATANPLUGDIR%\%CUDA6DLL% %SHRMEMDIR%
   MOVE /Y %GATANPLUGDIR%\%CUDA6FFT% %SHRMEMDIR%
   MOVE /Y %GATANPLUGDIR%\%CUDA6FFTW% %SHRMEMDIR%
)
IF %INPLUG8% == 1 IF %INSHRMEM8% == 0 IF  %INSHRMEM6% == 0 IF %INSHRMEM4% == 0 (
   echo.
   echo Moving CUDA 8 .dlls from %GATANPLUGDIR% into Shrmemframe subfolder
   MOVE /Y %GATANPLUGDIR%\FrameGPU.dll %SHRMEMDIR%
   MOVE /Y %GATANPLUGDIR%\%CUDA8DLL% %SHRMEMDIR%
   MOVE /Y %GATANPLUGDIR%\%CUDA8FFT% %SHRMEMDIR%
   MOVE /Y %GATANPLUGDIR%\%CUDA8FFTW% %SHRMEMDIR%
)

Rem # Update FrameGPU.dll if it is there
call :UpdateFrameGPU %GATANPLUGDIR%
call :UpdateFrameGPU %SHRMEMDIR%
call :UpdateFrameGPU ..

Rem # Only 64-bit package will have shrmemframe since it has the right libraries
set NOSHRMEM=0
IF %REGISTER% == 0 GOTO :ShrMemDone
IF %BIT64% == 0 GOTO :ShrMemDone

IF NOT EXIST Shrmemframe (
  set NOSHRMEM=1
  GOTO :ShrMemDone
)

echo Copying frame alignment components

IF NOT EXIST %SHRMEMDIR% MKDIR %SHRMEMDIR%
IF EXIST %SHRMEMDIR%\Microsoft.VC90.CRT RMDIR /Q /S %SHRMEMDIR%\Microsoft.VC90.CRT
COPY /Y Shrmemframe\shrmemframe.exe %SHRMEMDIR%
IF EXIST Shrmemframe\msvcr120.dll (
   COPY /Y libiomp5md.dll %SHRMEMDIR%
   COPY /Y libmmd.dll %SHRMEMDIR%
   COPY /Y svml_dispmd.dll %SHRMEMDIR%
   COPY /Y Shrmemframe\msvcp120.dll %SHRMEMDIR%
   COPY /Y Shrmemframe\msvcr120.dll %SHRMEMDIR%
)
IF NOT EXIST Shrmemframe\msvcr120.dll (
   COPY /Y Shrmemframe\libiomp5md.dll %SHRMEMDIR%
   COPY /Y Shrmemframe\libmmd.dll %SHRMEMDIR%
   XCOPY /Q /S /Y /I Shrmemframe\Microsoft.VC90.CRT  %SHRMEMDIR%\Microsoft.VC90.CRT
)

:ShrMemDone
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
IF %NOSHRMEM% == 1 (
  echo ...
  echo If you have a K2 or K3 camera, for CPU-based frame alignment
  echo you need to install a Shrmemframe package for GMS %versRange64%
  echo ...
)
IF %NEEDREDIST% EQU 1 (
  echo ...
  echo You must have a Visual C++ 2015 Redistributables package installed
  echo to run this version of SerialEM on this computer
  echo ...
)
pause
EXIT /B 0

Rem # Update FrameGPU.dll if it is there
:UpdateFrameGPU
set CUDA4LIB=%~1\%CUDA4DLL%
set CUDA6LIB=%~1\%CUDA6DLL%
set CUDA8LIB=%~1\%CUDA8DLL%
set PLUGFRAME=%~1\FrameGPU.dll

IF NOT EXIST  %PLUGFRAME% GOTO :FrameGPUdone
if EXIST %CUDA4LIB% IF EXIST %CUDA8LIB% GOTO :TwoCUDAs
if EXIST %CUDA4LIB% IF EXIST %CUDA6LIB% GOTO :TwoCUDAs
if EXIST %CUDA6LIB% IF EXIST %CUDA8LIB% GOTO :TwoCUDAs


IF EXIST  %CUDA4LIB% IF EXIST FrameGPU4.dll (
  echo.
  echo Updating %PLUGFRAME% with FrameGPU4.dll for CUDA 4
  DEL %PLUGFRAME%
  COPY /Y FrameGPU4.dll %PLUGFRAME%
  GOTO :FrameGPUdone
)
IF EXIST  %CUDA4LIB% IF NOT EXIST FrameGPU4.dll (
  echo.
  echo A new version of FrameGPU is available, but only for CUDA 6 and 8.
  echo You should upgrade to CUDA 6 or 8 and remove the CUDA 4 libraries.
  echo Obtain package and follow instructions at
  echo http://bio3d.colorado.edu/SerialEM/download.html#FrameGPU
  echo Unpack the package in the location of %PLUGFRAME%
  echo The existing CUDA 4 FrameGPU.dll may continue to work until then
  (echo FrameGPU.dll cannot be upgraded because CUDA 4 is no longer supported. & echo You should upgrade with a CUDA 6 or 8 package. & echo The existing CUDA 4 FrameGPU.dll may continue to work until then. & echo See details in installer window.) | msg * /TIME:300 /W 
  GOTO :FrameGPUdone
)

IF EXIST  %CUDA6LIB% IF EXIST FrameGPU6.dll (
  echo.
  echo Updating %PLUGFRAME% with FrameGPU6.dll for CUDA 6
  DEL %PLUGFRAME%
  COPY /Y FrameGPU6.dll %PLUGFRAME%
  GOTO :FrameGPUdone
)
IF EXIST  %CUDA8LIB% IF EXIST FrameGPU8.dll (
  echo.
  echo Updating %PLUGFRAME% with FrameGPU8.dll for CUDA 8
  DEL %PLUGFRAME%
  COPY /Y FrameGPU8.dll %PLUGFRAME%
  GOTO :FrameGPUdone
)

:TwoCUDAs
  echo.
  echo Not updating %PLUGFRAME% because you have more than one set
  echo of CUDA 8 libraries.  You will have to do this manually.
  echo You should eliminate CUDA 4 and use CUDA 6 or 8 if possible;
  echo CUDA 4 is no longer supported
  GOTO :FrameGPUdone

:FrameGPUdone
EXIT /B 0
