/*  sliceproc.h: include file for slice processing functions
 *
 *  $Id$
 */
#ifndef IMOD_SLICEPROC_H
#define IMOD_SLICEPROC_H

#include "mrcslice.h"

enum {ANISO_CLEAR_AT_END, ANISO_CLEAR_ONLY, ANISO_LEAVE_OPEN};

#ifdef __cplusplus
extern "C" {
#endif

  int sliceByteConvolve(Islice *sin, int mask[3][3]);
  int sliceByteAdd(Islice *sin, int inVal);
  int sliceByteEdgeTwo(Islice *sin, int center);
  int sliceByteEdgeSobel(Islice *sin);
  int sliceByteEdgePrewitt(Islice *sin);
  int sliceByteEdgeLaplacian(Islice *sin);
  int sliceByteSharpen(Islice *sin);
  int sliceByteSmooth(Islice *sin);
  int sliceByteConvolve(Islice *sin, int mask[3][3]);
  int sliceByteThreshold(Islice *sin, int val);
  int sliceByteGrow(Islice *sin, int val);
  int sliceByteShrink(Islice *sin, int val);
  int sliceByteGraham(Islice *sin);
  int sliceMinMax(Islice *s);
  int sliceMedianFilter(Islice *sout, struct MRCvolume *v, int size);
  void updateMatrix(float **image, float **imageOld, int m, int n,
                    int CC, double k, double lambda);
  int sliceAnisoDiff(Islice *sl,  int outMode, int CC, double k, double lambda,
                     int iterations, int clearFlag);
  float **allocate2D_float(int m, int n );
  void sliceByteAnisoDiff(Islice *sl, float **image, float **image2, int CC,
                          double k, double lambda, int iterations, 
                          int *iterDone);
  void sliceScaleAndFree(Islice *sout, Islice *sin);


#ifdef __cplusplus
}
#endif

#endif
