/*
 *  frameutil.h -- header file for frameutil.cpp
 *
 *  $Id$
 */
#ifndef FRAMEUTIL_H
#define FRAMEUTIL_H
#include <vector>

typedef void (*CharArgType)(const char *message);

void utilCoordsForWrap(int nxFrom, int nyFrom, int nxTo, int nyTo, int xOffset,
                         int yOffset, int ixFrom0[4], int ixTo0[4], int iyFrom0[4],
                         int iyTo0[4], int ixFrom1[4], int ixTo1[4], int iyFrom1[4],
                         int iyTo1[4]);
void utilRollSavedFrames(std::vector<float *> &savedVec, int numFrames);
void utilDumpFFT(float *fft, int nxPad, int nyPad, const char *descrip, int doReal,
                 int frame = 0);
void utilDumpImage(float *buf, int nxDim, int nxPad, int nyPad, int ifCorr, 
                   const char *descrip, int frame = 0);
void utilPrint(const char *format, ...);
void utilSetPrintFunc(CharArgType func);

#endif
