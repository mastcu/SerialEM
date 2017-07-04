SerialEM is currently developed in Visual Studio 2010 with the v90 (VS 2008)
platform toolset, which is required for it to run under Windows 2000 and XP
below SP3.

To compile it with VS 2010, change the platform toolset to v100 for each
configuration being compiled.  To compile a 32-bit version without using FFTW
(which cannot be used for free with SerialEM), open XCorr.cpp and comment out
#define USE_FFTW, and edit the properties of FFT.cpp to include it in the
build.

It can be compiled in VS 2013 by reading in the current solution file,
allowing it to upgrade the project, and changing preprocessor definition
_WIN32_WINNT=0x0500 to 0x0501 and adding NO_WARN_MBCS_MFC_DEPRECATION.  The
64-bit version will compile without problems; the 32-bit Release version will
not link with the FFTW included in the package and that should be excluded as
described above.


The rest of this file contains a description of the modules in SerialEM.
Every .cpp file has a corresponding .h file.

BASIC PROGRAM MODULES:
AutoTuning.cpp        Calibrates and does astigmatism and coma corrections
BaseSocket.cpp        Base class for running operations through sockets, used
                         in SerialEM and plugins
BeamAssessor.cpp      Calibrates beam intensity, adjusts intensity using the
                         calibration, and calibrates beam movement
CalibCameraTiming.cpp Calibrates camera timing
CameraController.cpp  Runs the camera and energy filter via SerialEMCCD
ChildFrm.cpp          Standard MFC MDI component, contains an individual
                         instance of a CSerialEMView window
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
MacroProcessor.cpp    Runs scripts
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
PiezoAndPPControl.cpp Module for communicating with JEOL piezos associated
                         with phase plates
PluginManager.cpp     Handles loading and calling of plugins
ProcessImage.cpp      Mostly has functions in the Process menu, also has
                         code for getting image means and for correcting 
                         image defects, and for beam tasks
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

FREE-STANDING WINDOWS:
DirectElectronCamPanel.cpp
DoseMeter.cpp        A system-modal cumulative dose meter
LogWindow.cpp        The log window
MacroEditer.cpp      Macro editing window
MacroToolbar.cpp     Toolbar with macro buttons
NavigatorDlg.cpp     The Navigator for storing positions and maps
NavRotAlignDlg.cpp   Window for finding alignment with rotation
ReadFileDlg.cpp      Window for reading sections from file
ScreenMeter.cpp      A system-modal screen meter with control on averaging
StageMoveTool.cpp    Window for moving stage in large steps
StateDlg.cpp         Dialog for saving and setting imaging states
TSViewRange.cpp      Dialog open while viewing the results of TS range finding

MODAL DIALOG BOXES:
AutocenSetupDlg.cpp  To set parameters for autocentering
BaseDlg.cpp          Base class to handle tool tips and help button
CameraSetupDlg.cpp   To set camera exposure parameters and select camera
CookerSetupDlg.cpp   To set parameters for specimen cooking
DummyDlg.cpp         Dialog class required for no_hang solution
FalconFrameDlg.cpp   To set up frame sums acquired from Falcon or K2 camera
FilePropDlg.cpp      To set properties for saving images to files
FrameAlignDlg.cpp    To set parameters for frame alignment
GainRefDlg.cpp       To set parameters for taking gain references
K2SaveOptionDlg.cpp  To set options for K2 frame saving, and set name and
                        folder control for frame saving from all cameras
MacroControlDlg.cpp  To set parameters for controlling macros
MontageSetupDlg.cpp  To set up montaging frame number, size, overlap, binning
                         and mag
NavAcquireDlg.cpp    To start acquisition from selected Navigator points
NavBacklashDlg.cpp   To set preferences for correcting new maps for backlash
NavFileTypeDlg.cpp   To select whether an acquired file will be montage
NavImportDlg.cpp     To import a tiff file as a Navigator map
RefPolicyDlg.cpp     To set policies for which gain references to use
ThreeChoiceBox.cpp   To provide an option box with intelligible choices
                        instead of yes-No-Cancel
TSBackupDlg.cpp      To select a section to back up to in a tilt series
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
RemoteControl.cpp
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
KStoreIMOD.cpp        A class for saving and reading TIFF files
KStoreMRC.cpp         A class for saving and reading MRC image files


UTILITY FILES in Utilities
AskOneDlg.cpp         A modal dialog box for getting a one-line entry
FFT.cpp               Hand-translation of our Fortran FFT routines, can be
                         used if fftw is not available
KGetOne.cpp           Has routines for getting one integer, float, or string
                         using AskOneDlg
Smoother.cpp          A class for smoothing a noisy data stream, used for
                         the screen meter
STEMfocus.cpp         Has routines for analyzing power spectra and finding
                         focus in STEM mode
SEMUtilities.cp       A collection of utilities
XCorr.cpp             Free standing functions for computations, prefixed with
                         XCorr, Proc, or Stat generally.  This is one
                         module that gets optimized for a debug build
This directory also has the FFTW and tiff-related libs.


FILES SHARED WITH IMOD in Shared
Except for imodconfig.h, the master copy of each file is in IMOD and when it is
modified there it can be simply copied into this directory and checked in.
libcfshr.lib, libiimod.lib and libimxml.lib are static libs built with VS2008

autodoc.h
b3dutil.h             Needed for some functions and macros
cfsemshare.h          Header file with declarations for functions in libcfshr
hvemtypes.h
iimage.h
ilist.h
imodconfig.h
mrcslice.h            Needed for structure definitions
mrcfiles.h            Needed for structure definitions
parse_params.h
SEMCCDDefines.h       Definitions shared with SerialEMCCD
SharedJeolDefines.h   Definitions shared with JeolScopePlugin
CorrectDefects.cpp    Common defect correction code shared with SerialCCD too


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
