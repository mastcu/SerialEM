#ifndef MACROCONTROL_H
#define MACROCONTROL_H

struct MacroControl {
  BOOL limitIS;
  BOOL limitTiltUp;
  BOOL limitTiltDown;
  BOOL limitRuns;
  BOOL limitScaleMax;
  BOOL limitMontError;
  float tiltUpLimit;
  float tiltDownLimit;
  float ISlimit;
  int runLimit;
  int scaleMaxLimit;
  float montErrorLimit;
};

#endif
