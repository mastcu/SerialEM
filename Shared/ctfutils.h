/*
 * ctfutils.h - declarations for utility functions used by ctfplotter and
 *                 ctfphaseflip
 *
 *  $Id$
 */
#ifndef CTFUTILS_H
#define CTFUTILS_H

typedef struct ilist_struct Ilist;

#define DEF_FILE_HAS_ASTIG      1
#define DEF_FILE_ASTIG_IN_RAD   2
#define DEF_FILE_HAS_PHASE      4
#define DEF_FILE_PHASE_IN_RAD   8
#define DEF_FILE_INVERT_ANGLES  16
#define DEF_FILE_HAS_CUT_ON     32

#define FREQ_FOR_PHASE  0.3      // When there is a cuton, frequency at which phase occurs

typedef struct defocus_struct {
  int startingSlice;   // Starting slice of range, numbered from 0
  int endingSlice;     // Ending slice og range
  double lAngle;       // Low angle of range in degrees
  double hAngle;       // High angle of range in degrees
  double defocus;      // Defocus value, or defocus on short axis
  double defocus2;     // Defocus value on long axis
  double astigAngle;   // Astigmatism angle of short axis in degrees or radians
  double platePhase;   // Phase shift in degrees or radians
  double cutOnFreq;    // cut-on frequency in 1/nm
} SavedDefocus;

float *readTiltAngles(const char *angleFile, int mNzz, float mAngleSign,
                      float &mMinAngle, float &mMaxAngle);
int addItemToDefocusList(Ilist *mSaved, SavedDefocus toSave);
Ilist *readDefocusFile(const char *mFnDefocus, int &defVersion, int &versFlags);
int checkAndFixDefocusList(Ilist *list, float *angles, int nz, int defVersion);

#endif
