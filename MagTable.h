#ifndef MAGTABLE_H
#define MAGTABLE_H

struct MagTable {
  int mag;                          // Mag on film
  int screenMag;                    // Screen mag
  int EFTEMmag;                     // "Film" mag in EFTEM
  int EFTEMscreenMag;               // "Screen" mag in EFTEM
  double STEMmag;                   // Mag in STEM mode
  float tecnaiRotation;             // Rotation reported by tecnai
  float EFTEMtecnaiRotation;        // Rotation in EFTEM
  float deltaRotation[MAX_CAMERAS]; // Change in rotation from previous mag
  float rotation[MAX_CAMERAS];      // Measured or derived rotation of tilt axis
  int rotDerived[MAX_CAMERAS];      // Value of how derived the angle is
  float pixelSize[MAX_CAMERAS];     // Pixel size in nm
  int pixDerived[MAX_CAMERAS];      // Value of how derived the pixel size is
  ScaleMat matIS[MAX_CAMERAS];      // Pixels/unit of image shift
  int calibrated[MAX_CAMERAS];      // Flag if calibrated
  ScaleMat matStage[MAX_CAMERAS];   // Pixels per micron of stage move
  double stageCalFocus[MAX_CAMERAS]; // Absolute focus at which stage cal was done
  int stageCalibrated[MAX_CAMERAS]; // Flag that stage calibration was done
  float neutralISX[2], neutralISY[2]; // Neutral values for image shift on JEOL, nG/GIF
  double offsetISX, offsetISY;      // Image shift offset for this mag -working value
  double calOffsetISX[2];           // Actual calibrated offsets, non-GIF and GIF
  double calOffsetISY[2];           // Actual calibrated offsets, non-GIF and GIF
  int focusTicks;                   // Number of focus ticks for SetobjFocus
};

struct FocusTable {
  float slopeX, slopeY;
  double beamTilt;
  int numPoints;
  int calibrated;
  int magInd;
  int camera;
  int direction;
  int probeMode;
  float defocus[MAX_CAL_FOCUS_LEVELS];
  float shiftX[MAX_CAL_FOCUS_LEVELS];   // per mrad of beam tilt
  float shiftY[MAX_CAL_FOCUS_LEVELS];
};

struct RelRotations {
  int camera;
  int fromMag;
  int toMag;
  double rotation;
};
#endif
