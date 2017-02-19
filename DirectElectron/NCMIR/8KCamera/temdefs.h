/* temdefs.h */
/* Definitions for TEM Parameters (FasTEM) */
/* Copyright (c) 1999 by Jeol USA, Inc. ALL RIGHTS RESERVED */

#ifndef TEMDEFS_H
#define TEMDEFS_H



enum {
  /*  FasTem instrument model  */
  FASTEM_2010 = 1,       
  FASTEM_2010F,       
  FASTEM_2011,       
  FASTEM_3000F,       
  FASTEM_3010,       
  FASTEM_3011,       
  FASTEM_4010,
  FASTEM_3000SFF,
  FASTEM_4000EX,
  FASTEM_4000FX,
  FASTEM_1300SARM
};



#define ACC       0  // Accelerating Voltage value is in .xVal
#define HIGH      1  // MAG Mode index is in .xVal
#define LM        2  // Low Mag Mode index is in .xVal
#define SAMAG     3  // Selected Area Mag index is in .xVal
#define DIFF      4  // Diffraction Mode index is in .xVal
#define SPOT      5  // Probe size value is in .xVal
#define ALPHA     6  // Alpha angle is in .xVal mode is in .actMode (0 = TEM, 1 = CBD, 2 = EDS, 3 = NBD)
#define BRIGHT    7  // Bright setting value is in .xVal
#define OL        8  // Objective Focus  value is in .xVal
#define DIFFF     9  // Diffraction Focus value is in .xVal
#define WOBBLER  10  // Wobbler value is in xVal
#define DARK     11  // Dark or Bright mode is in .actMode (0 = Bright, 1 = Dark) selected value is in .xVal

// lens control for LFCABS command
#define LCCOND1  12  // Condenser lens 1 output value is in .xVal current is in .xVolVal
#define LCCOND2  13  // Condenser lens 2 output value is in .xVal current is in .xVolVal
#define LCCOND3  14  // Condenser lens 3 output value is in .xVal current is in .xVolVal
#define LCCM     15  // Condenser mini lens output value is in .xVal current is in .xVolVal
#define LCOBJF   16  // Objective lens fine output value is in .xVal current is in .xVolVal
#define LCOBJC   17  // Objective lens coarse output value is in .xVal current is in .xVolVal
#define LCOM     18  // Objective Mini Lens output value is in .xVal current is in .xVolVal
#define LCINT1   19  // Intermediate lens 1 output value is in .xVal current is in .xVolVal
#define LCINT2   20  // Intermediate lens 2 output value is in .xVal current is in .xVolVal
#define LCINT3   21  // Intermediate lens 3 output value is in .xVal current is in .xVolVal
#define LCPROJ   22  // Projector lens output value is in .xVal current is in .xVolVal
#define LCIFOCUS 23  // The following values are not lenses but used internally by the instrument
#define LCSPOTZ  24
#define LCOBJFZF 25
#define LCOBJFZC 26
#define LCOMZ    27
#define LCOBJOUF 28
#define LCOMOUF  29
#define LCOFTVZM 30
#define LCOFZM   31

// deflector control for DFC command
#define DFGUN1   32  // Gun Shift deflectors output values are in .xVal, .yVal current is in .xVolVal, .yVolVal
#define DFGUN2   33  // Gun Tilt  deflectors output values are in .xVal, .yVal current is in .xVolVal, .yVolVal
#define DFSPOT1  34  // Spot Alignment 1 deflectors output values are in .xVal, .yVal current is in .xVolVal, .yVolVal
#define DFSPOT2  35  // Spot Alignment 2 deflectors output values are in .xVal, .yVal current is in .xVolVal, .yVolVal
#define DFCSTIG  36  // Condenser Stigmator deflectors output values are in .xVal, .yVal current is in .xVolVal, .yVolVal
#define DFCOND1  37  // Condenser Shift deflector output values are in .xVal, .yVal current is in .xVolVal, .yVolVal
#define DFCOND2  38  // Condenser Tilt deflector output values are in .xVal, .yVal current is in .xVolVal, .yVolVal
#define DFOSTIG  39  // Objective Stigamator deflector output values are in .xVal, .yVal current is in .xVolVal, .yVolVal
#define DFIMAGE1 40  // Image Shift 1 deflector output values are in .xVal, .yVal current is in .xVolVal, .yVolVal
#define DFIMAGE2 41  // Image Shift 2 deflector output values are in .xVal, .yVal current is in .xVolVal, .yVolVal
#define DFISTIG  42  // Intermediate Stigmator defelector output values are in .xVal, .yVal current is in .xVolVal, .yVolVal
#define DFIPROJ  43  // Projector defelector output values are in .xVal, .yVal current is in .xVolVal, .yVolVal
#define DFSHIFT  44  // The following values are not deflector outputs but used internally by the instrument
#define DFTILT   45
#define DFANGLE  46
#define DFDEFSEN 47

#define PHOTO    48  // Photo function
#define SCR      49  // Flourescent Screen
#define EXPM     50  // No longer used
#define EXPT     51  // No longer used
#define EXPD     52  // Exposure or Current Density output in .xVal
#define STGPOS   53  //
#define STGZ     54  //
#define STGTILTY 55  // 
#define STGTILTX 56  //
#define FILM     57  // no longer used
#define FOCUS    58  // Focus parameters
#define OLAPT    59  // 1.3 MEV only
#define DETECTOR 60  // Active detector in .xVal
#define ASIDMODE 61  // ASID parameters.
#define ASIDDET   62
#define ASIDDMODE 63
#define ASIDSCAN  64
#define ASIDSMODE 65
#define FLC       66
#define BLANKING  67
#define STEMP     68
#define STEMCL    69
#define V1VALVE   70
#define MDS_MODE  77
#define FILAMENT  78


#define PROBE_TEM 0
#define PROBE_EDS 1
#define PROBE_CBD 2
#define PROBE_NBD 3

#define MAGMODE_HIGH  0
#define MAGMODE_LM    1
#define MAGMODE_SAMAG 2
#define MAGMODE_DIFF  3

#define DETECTOR_TVTOP 0
#define DETECTOR_TVBOT 1
#define DETECTOR_STEM  2
#define DETECTOR_MSC   3
#define DETECTOR_EDS   4
#define DETECTOR_GIF   5


#define MDS_MODE_OFF      0
#define MDS_MODE_SEARCH   1
#define MDS_MODE_SEARCH2  2
#define MDS_MODE_FOCUS    3
#define MDS_MODE_PHOTO    4
#define MDS_MODE_PHOTO2   5


#define TTUNGSTEN 0
#define TFIELDEM  1


#define GON_ABSOLUTE_MOVE 0
#define GON_RELATIVE_MOVE 1
#define GON_JOYSTICK_MOVE 2
#define GON_HOME_MOVE     3


#define GON_X_AXIS   0
#define GON_Y_AXIS   1
#define GON_Z_AXIS   2
#define GON_TX_AXIS  3
#define GON_TY_AXIS  4



typedef struct _MDSCOND   // individual condition structure
{
  int detector;           // selected detector (0 - 5)
  int magCode;            // selected MAG Mode (0 - 3) 
  int magIndex;           // selected MAG index 
  int spot;               // spot size   
  int bright;             // COND3 value
  int image1X;            // Image shift
  int image1Y;
  int image2X;
  int image2Y;
  int projX;              // Projector alignment
  int projY;
  int objC;               // Objective focus 
  int objF;
  int diffFocus;          // Diffraction Focus
  int cond1X;             // Beam Shift and Tilt
  int cond1Y;
  int cond2X;
  int cond2Y;
  int blankSelect;        // Blanking State (0 - off 1 - on)
  int dfPhotoSelect;      // Thru focus selected
  int dfPhotoCount;       // Thru focus count
  int dfPhotoStep;        // Defocus step size
  int dfPhotoDelay;       // Photo delay  
  
} MDSCOND;


typedef struct _MDSDATA
{
  MDSCOND search;
  MDSCOND diffSearch;
  MDSCOND focus;
  MDSCOND photoSet;
  MDSCOND diffPhotoSet;
} MDSDATA;
  

typedef struct _HCONE_ENTRIES
{
  int cond1X;
  int cond1Y;
  int cond2X;
  int cond2Y;
  int image1X;
  int image1Y;
  int image2X;
  int image2Y;
  int init;
} HCONE_ENTRIES;

typedef struct _HCONE_DATA
{
  int dataCount;
  int cycles;
  int delay;
  HCONE_ENTRIES entries[360];
} HCONE_DATA;


typedef struct _FILMPARMS
{
  int   sensitivity;
  int   unused;
  int   filmNo;
  float exposure;
  char  filmCode[3];
  char  expMode[2];
  char  photoText[20];
} FILMPARMS;

#endif