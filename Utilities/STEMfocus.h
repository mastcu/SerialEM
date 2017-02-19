#ifndef STEMFOCUS_H
#define STEMFOCUS_H
void sincLikeFunction(int numPts, int numNodes, float *curve);
void totalSubtractedPower(float *rotav, int numPts, float backStart, float backEnd,
                          float powStart, float powEnd, float *background, float *power);
void setupDefocusFit(float *rotav1, float *rotav2, int numRotPts, float bkgd1,
                     float bkgd2, float fitStart, float fitEnd, float *sincCurve, 
                     int numSincPts, int numSincNodes, float expected1, float expected2,
                     int sizeType, int varyBkgd, float weightPow);
void findDefocusSizes(float *size1, float *size2, float *errmin, int trace);
int findFocus(float **rotavs, int numRotavs, int numRotPts, float *focusVals, 
              float *powers, float *bkgds, float estSlope, float fitStart, float fitEnd,
              float *sincCurve, int numSincPts, int numSincNodes, int varyBkgd,
              int varyScale, float weightPow, float *paraFocus, float *fitFocus, 
              float *fitSlope);
#endif
