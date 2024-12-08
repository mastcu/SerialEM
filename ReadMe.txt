See Copyright.txt for a description of license terms for SerialEM and other
components.

SerialEM is currently developed in Visual Studio 2015 with the v140_xp
platform toolset so that it can still run on XP SP3, but it includes a pure
v140 configuration as well.  Using the v140_xp configuration requires the
Windows 7.1A Platform SDK, but at least when installing VS 2015, there is an
option to supportthis toolset which takes care of the dependencies.
Beware: the first time that you open the solution in a higher
version of Visual Studio, it will want to upgrade the project to use the
current toolset on all configurations.  Just cancel this message, then adjust
the platform toolset for the v140 configuration.

There are 5 project configurations:
Debug (Win 32 & x64)
   A Debug build that will start in the top source directory and read
   SerialEMsettings.txt there.
Release (Win 32 & x64)
   A Release build that will start in the top source directory and read
   SerialEMsettings.txt there.
NoHang (Win32 only; x64 properties are not maintained)
   A Release build that compiles the MyFileDialog class in SerialEMDoc.cpp to
   use the SDK file dialog instead of CFileDialog, which used to hang on some
   Windows 2000 machines and maybe Windows XP also.
v140 Debug (Win 32 & x64)
   A Debug build for building in VS 2015 with no dependence on Intel
   libraries and no Windows XP compatibility.
v140 Release (Win 32 & x64)
   A Debug build for building in VS 2015 with no dependence on Intel
   libraries and no Windows XP compatibility.

You must have the library collection SerialEMLibs in an adjacent directory, or
change the project properties to access this repository elsewhere.  It is
available at
http://bio3d.colorado.edu/SerialEM/SerialEMLibs
and
https://github.com/mastcu/SerialEMLibs
This collection includes libraries that you should not use unless you have a
license for them.  Namely, libifft-MKL.lib and libifft-MKL-64.lib are import
libraries for the corresponding DLLs in the binary SerialEM distributions,
which incorporate Intel Math Kernel Library FFT routines from version 15 of
the Intel compiler collection.  If you do not have a license for these, you
should compile with the v140 configurations.  To run the program after such a
compilation, copy libctffind-VCOMP.dll from SerialEMLibs or SerialEMLibs/x64
to the executable directory, and copy hdf5.dll and imodzlib1.dll from the
appropriate SerialEM distribution.


The rest of this file contains a description of the modules in SerialEM.  This
is unlikely to be up-to-date.  Every .cpp file has a corresponding .h file.

BASIC PROGRAM MODULES:
AutoTuning.cpp        Calibrates and does astigmatism and coma corrections
BaseDlg.cpp           Base class to handle help and panel formatting
BaseServer.cpp        Base class for socket servers, used by PythonServer
BaseSocket.cpp        Base class for running operations through sockets, used
                         in SerialEM and plugins
BeamAssessor.cpp      Calibrates beam intensity, adjusts intensity using the
                         calibration, and calibrates beam movement
CalibCameraTiming.cpp Calibrates camera timing
CameraController.cpp  Runs the camera and energy filter via SerialEMCCD
ChildFrm.cpp          Standard MFC MDI component, contains an individual
                         instance of a CSerialEMView window
CEOSFilter.cpp        Controls a CEOS filter, using json.cpp to communicate
ComplexTasks.cpp      Performs tasks invvolving multiple image acquisitions and
                         stage movement: finding eucentricity, resetting image
                         shift, reversing tilt direction and walking up.
DirectElectronCamera.cpp  Acquires images from Direct Electron cameras
DistortionTasks.cpp   Captures overlapping pairs for measuring distortion field
EMbufferManager.cpp   Manages buffer copying, saving, and reading from file
EMimageBuffer.cpp     Image buffer class to hold images and associated scope
                          information
EMimageExtra.cpp      A class with extra information held in an image
EMmontageController.cpp  The montage routine
EMscope.cpp           The microscope module, performs all interactions with
                         microscope except ones by threads that need their
                         own instance of the Tecnai
ExternalTools.cpp     Runs external programs with specified arguments
FalconHelper.cpp      Performs communication with plugin and frame stacking
                         for Falcon camera
FilterTasks.cpp       GIF-related tasks, specifically calibrating mag-dependent
                         energy shifts and refining ZLP
FocusManager.cpp      Measures beam-tilt dependent image shifts, calibrates and
                         does autofocusing
GainRefMaker.cpp      Acquires a gain reference and makes gain references
                         for the camera controller from stored gain references
GatanSocket.cpp       Communicates with SerialEMCCD via a socket
LC1100CamSink.cpp     Code used with old Direct Electron LC1100 camera
MacroCommands.cpp     Has individual script commands and essential functions
MacroProcessor.cpp    Base class for MacroCommands with supporting functions
Mailer.cpp            Sends mail
MainFrm.cpp           Standard MFC MDI component, manages the main program 
                         window and has code for program shutdown, control
                         panel management and status line entries
MapDrawItem.cpp       Class for keeping track of map drawing items
MenuTargets.cpp       Has MANY event handlers for menu items, see below
MultiTSTasks.cpp      Has specimen cooking and autocentering routines
NavHelper.cpp         Helper routines for the Navigator
ParameterIO.cpp       Reads property file, reads and writes settings and
                         calibration files
ParticleTasks.cpp     Acquires multiple images from one hole or multiple holes
PiezoAndPPControl.cpp Module for communicating with JEOL piezos associated
                         with phase plates
PluginManager.cpp     Handles loading and calling of plugins
ProcessImage.cpp      Mostly has functions in the Process menu, also has
                         code for getting image means and for correcting 
                         image defects, and for beam tasks
PythonServer.cpp      Socket code for connecting to external Python
SerialEM.cpp          The main module, has top level routines, and 
                         miscellaneous message handlers, and other things
                         that did not fit into other places
SerialEMDoc.cpp       Standard MFC MDI component, which contains all the 
                         message handlers for opening, reading and writing
                         image files and settings files
SerialEMView.cpp      Standard MFC MDI component, does the image display;
                         one instance manages the main image window that 
                         displays the stack of image buffers, and others
                         can be created for secondary image windows
setdpi.cpp            Handles dialog dpi when display is not 120 DPI
ShiftCalibrator.cpp   Calibrates image shift, stage shift, offsets between mags
ShiftManager.cpp      Does autoalignment, mouse shifting, image shift reset, 
                         and has all the routines for getting coordinate 
                         transformation matrices
TSController.cpp      The tilt series controller

FREE-STANDING WINDOWS: NON-MODAL DIALOGS
AutocenSetupDlg.cpp  To set parameters for autocentering
AutoContouring.cpp   To manage autocontouring of grid square
ComaVsISCalDlg.cpp   To set parameters for the Coma vs. Image Shift cal
CtffindParamDlg.cpp  To set parameters for analyzing power spect with ctffind
DirectElectronCamPanel.cpp
DoseMeter.cpp        A system-modal cumulative dose meter
HoleFinderDlg.cpp    To manage automatic hole finding
LogWindow.cpp        The log window
MacroEditer.cpp      Macro editing window
MacroToolbar.cpp     Toolbar with macro buttons
MultiCombinerDlg.cpp  To combine Navigator points for multiple Records
MultiShotDlg.cpp     Dialog for setting parameters for multiple records
NavAcquireDlg.cpp    To set parameters for acquisition from Navigator points
NavigatorDlg.cpp     The Navigator for storing positions and maps
NavRotAlignDlg.cpp   Window for finding alignment with rotation
OneLineScript.cpp    Window for entering and running up to 5 one-line scripts
ReadFileDlg.cpp      Window for reading sections from file
ScreenMeter.cpp      A system-modal screen meter with control on averaging
ScreenShotDialog.cpp To set parameters and take window snapshots
ShiftToMarkerDlg.cpp To set parameters for Shift to Marker operation
StageMoveTool.cpp    Window for moving stage in large steps
StateDlg.cpp         Dialog for saving and setting imaging states
TSViewRange.cpp      Dialog open while viewing the results of TS range finding
VPPConditionSetup.cpp To set parameters for conditioning phase plates
ZbyGSetupDlg.cpp     To calibrate and set parameters for Eucentricity by Focus

MODAL DIALOG BOXES:
CameraSetupDlg.cpp   To set camera exposure parameters and select camera
CookerSetupDlg.cpp   To set parameters for specimen cooking
DERefMakerDlg.cpp    To make gain references in DE server
DriftWaitSetupDlg.cpp To set parameters for waiting for drift
DummyDlg.cpp         Dialog class required for no_hang solution
FalconFrameDlg.cpp   To set up frame sums acquired from Falcon or K2 camera
FilePropDlg.cpp      To set properties for saving images to files
FrameAlignDlg.cpp    To set parameters for frame alignment
GainRefDlg.cpp       To set parameters for taking gain references
K2SaveOptionDlg.cpp  To set options for K2 frame saving, and set name and
                        folder control for frame saving from all cameras
MacroControlDlg.cpp  To set parameters for controlling macros
MacroSelector.cpp    A popup dialog to select one script by name/number
MontageSetupDlg.cpp  To set up montaging frame number, size, overlap, binning
                         and mag
NavAcqHoleFinder.cpp Opened by NavAcAcquireDlg for hole finder/combiner task
NavBacklashDlg.cpp   To set preferences for correcting new maps for backlash
NavFileTypeDlg.cpp   To select whether an acquired file will be montage
NavImportDlg.cpp     To import a tiff file as a Navigator map
RefPolicyDlg.cpp     To set policies for which gain references to use
StepAndAdjustIS.dlg  To set conditions for adjusting image shifts used for
                        Multiple Records
ThreeChoiceBox.cpp   To provide an option box with intelligible choices
                        instead of yes-No-Cancel
TSBackupDlg.cpp      To select a section to back up to in a tilt series
TSDoseSymDlg.cpp     To set parameters for dose-symmetric tilt series
TSExtraFile.cpp      To set up extra outputs from a tilt series
TSPolicyDlg.cpp      To set policies for errors during multiple tilt series
TSRangeDlg.cpp       To set parameters for finding usable angular range
TSResumeDlg.cpp      To resume a tilt series at a specified action point 
TSSetupdialog.cpp    To set lots of parameters for a tilt series
TSVariationsDlg.cpp  To set up variations in exposure, etc during tilt series

CONTROL PANELS:
ToolDlg.cpp           Base class of control panels handles all common features
AlignFocusWindow.cpp  Has image alignment and autofocusing controls
DirectElectronToolDlg.cpp  Has controls for DE direct detector cameras
CameraMacroTools.cpp  Has buttons for taking pictures, running macros,
                          and controlling tilt series.  Has general STOP.
EMbufferwindow.cpp    Has buttons for copying and saving buffers and controls
                          to govern the behavior of buffers
EMstatusWindow.cpp    Shows a summary of the contents of 3 or 4 buffers
FilterControlDlg.cpp  Has controls for working with the energy filter
ImageLevelDlg.cpp     Shows information about and allows control of image
                          display
LowDoseDlg.cpp        Has controls for working in low dose mode
MontageWindow.cpp     Has controls for montaging
RemoteControl.cpp     Has convenient microscope controls
ScopeStatusDlg.cpp    Shows various critical microscope parameters
STEMcontrolDlg.cpp    Has controls for working in STEM mode
TiltWindow.cpp        Has controls for tilting and setting increments


IMAGE CLASSES IN Images
KImage.cpp            Has the basic image class and KImageShort and KImageFloat
KImageBase.cpp        A base class for image collections/files
KImageScale.cpp       A class with methods for determining and keeping track of
                         image scaling
KImageStore.cpp       A base class for image storage, inherits KImageBase
KPixMap.cpp           A class that builds and holds a byte pixmap for an image
                         being displayed and sets up color tables and ramps
KStoreADOC.cpp        A class for saving and reading series of TIFF files and
                         managing mdoc files
KStoreIMOD.cpp        A class for saving and reading TIFF, JPEG, and HDF
                         files, and reading some other supported file types 
KStoreMRC.cpp         A class for saving and reading MRC image files


UTILITY FILES in Utilities
AskOneDlg.cpp         A modal dialog box for getting a one-line entry
b3dregsvr.c           Simple registration program used because regsvr32 hung
dpiaware.manifest     Manifest used for Debug builds to specify DPI aware
INSTALL.bat           Program installer
KGetOne.cpp           Has routines for getting one integer, float, or string
                         using AskOneDlg
makeHideDisable       Script to produce about_hide_disable.htm file
Smoother.cpp          A class for smoothing a noisy data stream, used for
                         the screen meter
STEMfocus.cpp         Has routines for analyzing power spectra and finding
                         focus in STEM mode
SEMUtilities.cp       A collection of utilities
win10.manifest        Manifest for other builds to set Windows 10 compatibility
XCorr.cpp             Free standing functions for computations, prefixed with
                         XCorr, Proc, or Stat generally.  This is one
                         module that gets optimized for a debug build

HEADER FILES WITH MACROS FOR USE IN MULTIPLE PLACES
MacroMasterList.h     Centralized description of each script command, used to
                         make command function declarations in
                         MacroCommands.h, the enum list defining CME_ plus the
                         upper case of each name in MacroProcessor.h,
                         the cmdList table of names and properties in
                         MacroCommands.cpp, and the functions and tables in
                         the Python serialem module.
SettingsTests.h       Macros for settings that have a single value, used to
                         read/write settings and report and set values
                         generically from script in ParameterIO.cpp
PropertyTests.h       Macros for properties that have a single value, used to
                         read in properties and report and set values
                         generically from script in ParameterIO.cpp
NavAdocPuts.h         Code lines including macros used to write either
                         directly to the Navigator file instead of through the
                         autodoc structures, or to an XML file
NavAdocParams.h       A hybrid file with macros for reading/writing montage,
                         tilt series, and file option parameter to both
                         settings and navigator files, and code writing other
                         parameters to the Navigator file.
Image/MdocDefines.h   Centralized description of most items in EMimageExtra
                         and saved in the mdoc file, used in EMimageExtra.h to
                         define structure members and in EMimageExtra.cpp to
                         set their default values, and used in KStoreADOC.cpp
                         to define string variables for ADOC_ strings, in
                         KStoreADOC.h to make extern statements for accessing
                         those strings, and in KStoreADOC.cpp to get values in
                         and out of EMimageExtra with the Adoc... autodoc
                         functions

FILES SHARED WITH IMOD OR PLUGINS in Shared
Except for imodconfig.h, the master copy of each file is
in IMOD.  When one is modified there, it can be simply copied into this
directory and checked in, except that CorrectDefects.h needs some changes for
SerialEM.

Headers shared with IMOD
autodoc.h
b3dutil.h
cfft.h
cfsemshare.h
CorrectDefects.h
cppdefs.h
ctffind.h
ctfutils.h
framealign.h
frameutil.h
gpuframe.h
holefinder.h
hvemtypes.h
icont.h
iimage.h
ilist.h
imat.h
imesh.h
imodconfig.h
imodel.h
iobj.h
iplane.h
ipoint.h
istore.h
iview.h
mrcfiles.h
mrcslice.h
mxmlwrap.h
parse_params.h
sliceproc.h

Headers shared with plugins
SEMCCDDefines.h       Definitions shared with SerialEMCCD
SharedJeolDefines.h   Definitions shared with JeolScopePlugin

.cpp files shared with IMOD
CorrectDefects.cpp    Common defect correction code shared with SerialEMCCD too
framealign.cpp        Frame alignment module shared with SerialEMCCD
frameutil.cpp         Frame alignment utilties shared with SerialEMCCD
holefinder.cpp        Main operations for Hole finder


FOLDER CHOOSER in XFolderDialog
XFileOpenListView.cpp
XFolderDialog.cpp
XHistoryCombo.cpp
XWinVer.cpp


HEADER FILES FOR STRUCTURES:
ControlSet.h          Has ControlSet with properties of a camera exposure,
                          CameraParameters with all properties of a camera
                          LowDoseParams with properties of one low dose area
                          FilterParams with the GIF settings
FileOptions.h         Has FileOptions, which defines options for image storage
MacroControl.h        Has MacroControl, with macro control settings
MagTable.h            Has ScaleMat, a basic 2x2 scaling matrix
                          MagTable, which has properties and calibrations for
                          one magnification, and FocusTable, which holds
                          a focus calibration
MontageParam.h        Has MontParam with montaging parameters
TiltSeriesParam.h     Has TiltSeriesParam with parameters controlling a tilt
                          series


OTHER NOTABLE HEADER FILES in main folder
StandardScopeCalls.h  Has macros with scope plugin calls that can be included
                          in various ways; shared with FEIScopePlugin
PropertyTests.h       Has macros that can be used for reading in properties
                          and manipulating them with script commands
SettingsTests.h       Has macros that can be used for reading and writing
                          settings and  manipulating them with script commands


A NOTE ON WANDERING FILE/CLASS NOMENCLATURE:
The basic image classes were adopted from HVEM SerialEM, where Jim Kremer
named them starting with K for Kremer.
Most other classes start with "C" courtesy of Class Wizard, except for ones
that were ported from HVEM.
Many of the other files were named starting with "EM", and this was carried 
over for a while in porting, until I realized it was useless.  Thus, the
presence or absence of "EM" is irrelevant.
The control panels were generally named "...Window" in HVEM SerialEM, so some
of them are named "...Window" here, but then I started calling them "...Dlg".
It would be nice if they were all named "...Panel", but renaming is hard at
this point.

ON COMMAND TARGETS:
Originally I did not know how to make a class that did not have a window be
a command target, and message responses accumulated in SerialEM.cpp.  Then
I started shifting some of those classes to inherit CCmdTarget and was able
to move the message handlers to their respective files.  Hand editing of
SerialEM.clw was needed to get them recognized by Class Wizard as command
targets.  Meanwhile, several newer classes were defined as derived from
CCmdTarget upon creation, and they presented no problems.  SerialEM has
an override of OnCmdMsg to pass messages to all of these CCmdTarget classes.
But then I got tired of having the targets clutter up the modules, so I
created MenuTargets to collect the targets in.  This is not supposed to do
anything complicated, but there are a few items that are.

HELP FILES:
Help was added to the application long after creation and converted to HTML
help in 2010.  Everything is in the hlp directory.
aliases.h        Translates IDs to links in the help; must be updated when a
                    menu entry is added
SerialEM.hhc     Has table of contents; must be updated when a new help file
                    is added
SerialEM.hhp     Lists all help files; should also be updated when a new help
                    file is added

Also note that when a new help file is added, it has to be added manually to
each section in SerialEM.vcxproj


REMNANTS FROM THE ORIGINAL PROJECT README.TXT PRODUCED BY MFC WIZARD:

SerialEM.rc
    This is a listing of all of the Microsoft Windows resources that the
    program uses.  It includes the icons, bitmaps, and cursors that are stored
    in the RES subdirectory.  This file can be directly edited in Microsoft
	Visual C++.

SerialEM.clw
    This file contains information used by ClassWizard to edit existing
    classes or add new classes.  ClassWizard also uses this file to store
    information needed to create and edit message maps and dialog data
    maps and to create prototype member functions.

res\SerialEM.ico
    This is an icon file, which is used as the application's icon.  This
    icon is included by the main resource file SerialEM.rc.

res\SerialEMDoc.ico
    This is an icon file, which is used as the icon for MDI child windows
    for the CSerialEMDoc class.  This icon is included by the main
    resource file SerialEM.rc.

StdAfx.h, StdAfx.cpp
    These files are used to build a precompiled header (PCH) file
    named SerialEM.pch and a precompiled types file named StdAfx.obj.

Resource.h
    This is the standard header file, which defines new resource IDs.
    Microsoft Visual C++ reads and updates this file.
