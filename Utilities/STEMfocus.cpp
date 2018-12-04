//#include "..\stdafx.h"
//#include "..\SerialEM.h"
#include <math.h>
#include <stdio.h>
#include "..\Shared\b3dutil.h"
#include "STEMfocus.h"

static void getSizes(float *y, float *size1, float *size2);
static void sincErr(float *y, float *error);

// Compute a function shaped like a sinc curve but nearly monotonic
void sincLikeFunction(int numPts, int numNodes, float *curve)
{
  float xscale = (float)numNodes / (float)numPts;
  double pi = 3.1415926535;
  double x, cum = 0.;
  int i, ix15, icross, ixnext, ixlast, ixend, half;
  float curveLast;

  // compute cumulative function to first peak
  curve[0] = 0.;
  ix15 = B3DNINT(1.5 / xscale);
  for (i = 1; i <= ix15; i++) {
    x = pi * i * xscale;
    cum += fabs(sin(x) / x);
    curve[i] = (float)cum;
  }

  // Starting at first node, back up until the integral to the first peak is the same
  // as the current value times the distance from there to first peak
  for (icross = (int)(1. / xscale); icross > 0; icross--) {
    x = pi * icross * xscale;
    if ((ix15 - icross) * fabs(sin(x) / x) >= curve[ix15] - curve[icross])
      break;
  }

  // Define falling part of curve to that point
  curve[0] = 1.;
  for (i = 1; i <= icross; i++) {
    x = pi * i * xscale;
    curve[i] = (float)fabs(sin(x) / x);
  }

  // From the current point, compute mean to next full node, assign that to half-node
  // position and interpolate from current point to that position
  curveLast = curve[icross];
  ixlast = icross;
  for (half = 2; half <= 2 * numNodes; half++) {
    ixnext = B3DNINT(0.5 * half / xscale);
    ixend = B3DNINT(0.5 * (half + 1) / xscale);
    ixnext = B3DMIN(ixnext, numPts - 1);
    cum = 0.;
    for (i = ixlast + 1; i <= ixend; i++) {
      x = pi * i * xscale;
      cum += fabs(sin(x) / x);
    }
    cum /= (ixend - ixlast);
    for (i = ixlast + 1; i <= ixnext; i++)
      curve[i] = (float)(curveLast + (cum - curveLast) * (i - ixlast) / 
                         (ixnext - ixlast));
    curveLast = curve[ixnext];
    ixlast = ixnext;
  }

  // Fill in the end
  for (i = ixlast + 1; i < numPts; i++)
    curve[i] = curve[ixlast];
}

// Compute a total power over a range after subtracting a background determined
// from the given range; return background and power
void totalSubtractedPower(float *rotav, int numPts, float backStart, float backEnd,
                          float powStart, float powEnd, float *background, float *power)
{
  int i, ixst, ixnd;
  float sum = 0;
  float areafac = (float)(3.14159 * 0.5 / numPts);
  ixst = B3DNINT(backStart * numPts / 0.5);
  ixnd = B3DNINT(backEnd * numPts / 0.5);
  for (i = ixst; i < ixnd; i++)
    sum += rotav[i];
  *background = (float)(sum / (ixnd - ixst));
  ixst = B3DNINT(powStart * numPts / 0.5);
  ixnd = B3DNINT(powEnd * numPts / 0.5);
  *power = 0.;
  for (i = ixst; i < ixnd; i++)
    *power += (float)((rotav[i] - *background) * (2 * i + 1) * areafac * areafac);
}

// Static variables for the error function to access
static float *sRotav1;
static float *sRotav2;
static float *sSincCurve;
static int sNumSincPts;
static float sSincScale;
static int sStartInd;
static int sEndInd;
static float sExpected1;
static float sExpected2;
static int sSizeType;
static int sRelScale;
static int sVaryBkgd;
static double sWeightPow;
static float sBkgd1, sBkgd2;
static int sTrace;
static float sErrMin;

#define MAXVAR 5

// Pass the curves and variables to be used for a fit between two curves
// Size type 1 = find both sizes, 2 = vary both sizes with a constant difference,
// 3 = find the larger one (must be the first) - the only one that works
void setupDefocusFit(float *rotav1, float *rotav2, int numRotPts, float bkgd1,
                     float bkgd2, float fitStart, float fitEnd, float *sincCurve, 
                     int numSincPts, int numSincNodes, float expected1, float expected2,
                     int sizeType, int varyBkgd, float weightPow)
{
  sRotav1 = rotav1;  
  sBkgd1 = bkgd1;
  sExpected1 = expected1;
  sRotav2 = rotav2;  
  sBkgd2 = bkgd2;
  sExpected2 = expected2;
  sSincCurve = sincCurve;
  sNumSincPts = numSincPts;
  sRelScale = 0;
  sSizeType = sizeType;
  if (sSizeType < 0) {
    sSizeType = -sSizeType;
    sRelScale = 1;
  }
  sVaryBkgd = varyBkgd;
  sWeightPow = weightPow;
  sSincScale = (float)(((0.5 / numRotPts) * numSincPts) / numSincNodes);
  sStartInd = B3DNINT(fitStart * numRotPts / 0.5);
  sEndInd = B3DNINT(fitEnd * numRotPts / 0.5);
  sTrace = 0;
}

// After setting up the fit, find the two sizes characteristic of the curves
void findDefocusSizes(float *size1, float *size2, float *errmin, int trace)
{
  float pp[MAXVAR + 1][MAXVAR + 1], yy[MAXVAR + 1];
  float ptol[MAXVAR], a[MAXVAR];
  int iter, jmin, i, nvar;
  float ftol1, ftol2, delfac, ptol1, ptol2;
  float da[MAXVAR];

  delfac = 2.;
  ftol2 = 5.e-6f;
  ftol1 = 1.e-6f;
  ptol2 = 0.001f;
  ptol1 = .0002f;
  da[0] = B3DMAX(2.f, sExpected1 / 2.f);
  da[1] = da[2] = da[3] = B3DMAX(2.f, (sBkgd2 + sBkgd1) / 2.f);
  a[0] = sExpected1;
  i = 1;
  nvar = 1;
  if (sSizeType == 1) {
    da[2] = da[1];
    nvar = 2;
    a[i++] = sExpected2;
  }
  if (sRelScale) {
    nvar++;
    a[i] = 1.;
    da[i++] = .02f;
  }
  if (sVaryBkgd) {
    nvar += 2;
    a[i] = sBkgd1;
    da[i++] = B3DMAX(2.f, (sBkgd2 + sBkgd1) / 2.f);
    a[i] = sBkgd2;
    da[i++] = B3DMAX(2.f, (sBkgd2 + sBkgd1) / 2.f);
  }

  sTrace = trace;
  sErrMin = 1.e30f;
  amoebaInit(&pp[0][0], yy, MAXVAR + 1, nvar, delfac, ptol2, a, da, sincErr, ptol);
  amoeba(&pp[0][0], yy, MAXVAR + 1, nvar, ftol2, sincErr, &iter, ptol, &jmin);
  for (i = 0; i < nvar; i++)
    a[i] = pp[i][jmin];
  
  amoebaInit(&pp[0][0], yy, MAXVAR + 1, nvar, delfac, ptol1, a, da, sincErr, ptol);
  amoeba(&pp[0][0], yy, MAXVAR + 1, nvar, ftol1, sincErr, &iter, ptol, &jmin);
  for (i = 0; i < nvar; i++)
    a[i] = pp[i][jmin];
  sincErr(a, errmin);
  getSizes(a, size1, size2);
}

#define MAXCURVES 15

// Function to find best focus from a set of curves
// There must be at least 3 curves and the one with max power must not be at an end
// Returns 1 if this is not the case.  If there are just 3 points it returns a focus
// based on a parabola fit to the powers, and returns -1.
// If there are at least 2 on either side of the
// peak it solves for two independent line fits and finds focus from the intersection of
// the two lines; otherwise it fits all points to a pair of lines of equal slope and
// focus is the intersection there.  Either way, this implies a size for the max power
// point, and the procedure is iterated until this size stabilizes or goes negative.
// If this focus is unreasonable (outside 80% of range around peak) it returns -2 and
// the parabolic focus
//
int findFocus(float **rotavs, int numRotavs, int numRotPts, float *focusVals, 
              float *powers, float *bkgds, float estSlope, float fitStart, float fitEnd,
              float *sincCurve, int numSincPts, int numSincNodes, int varyBkgd,
              int varyScale, float weightPow, float *paraFocus, float *fitFocus, 
              float *fitSlope)
{
  float xx[MAXCURVES], yy[MAXCURVES], zz[MAXCURVES];
  float num1, num2, maxPow, minBelow, minAbove, diff, bb, expected1, expected2, roBel;
  float size1, size2, size0, signx, errmin, slopeBelow, slopeAbove, intBelow, intAbove;
  float roAbo;
  int i, indLow, indAbove, indBelow, sizeIter, ndat, numBelow, numAbove, iBelow, iAbove;
  int maxSizeIter = 10;
  float maxSize0 = 2.;

  if (numRotavs < 3 || numRotavs > MAXCURVES)
    return 1;

  // Find defocus with the highest power
  maxPow = -1.e10;
  for (i = 0; i < numRotavs; i++) {
    if (powers[i] > maxPow) {
      maxPow = powers[i];
      indLow = i;
    }
  }

  // Find interval to nearest other defocus on each side
  minBelow = 1.e10;
  minAbove = 1.e10;
  numBelow = numAbove = 0;
  for (i = 0; i < numRotavs; i++) {
    if (i == indLow)
      continue;
    diff = focusVals[i] - focusVals[indLow];
    if (diff < 0)
      numBelow++;
    else
      numAbove++;
    if (diff < 0 && -diff < minBelow) {
      minBelow = -diff;
      indBelow = i;
    } else if (diff > 0 && diff < minAbove) {
      minAbove = diff;
      indAbove = i;
    }
  }
  if (minBelow > 1.e9 || minAbove > 1.e9)
    return 2;

  // Get simple parabolic fit to the three points
  num1 = determ3(powers[indBelow], -minBelow, 1.f,
                 powers[indLow], 0.f, 1.f,
                 powers[indAbove], minAbove, 1.f);
  num2 = determ3(minBelow * minBelow, powers[indBelow], 1.f,
                 0.f, powers[indLow], 1.f,
                 minAbove * minAbove, powers[indAbove], 1.f);
  *paraFocus = (float)(num2 / (-2. * num1) + focusVals[indLow]);
  B3DCLAMP(*paraFocus, focusVals[indLow] - 0.5f * minBelow, 
           focusVals[indLow] + 0.5f * minAbove);

  if (numRotavs < 4) {
    *fitFocus = *paraFocus;
    *fitSlope = estSlope;
    return -1;
  }

  // Initial size estimate is based on half this distance (perfect focus is at worst
  // halfway between points)
  expected2 = 0.5f + estSlope * (minBelow + minAbove) / 4.f;

  // Iterate until size gets small
  sizeIter = 0;
  while (1) {

    iBelow = 0;
    iAbove = numBelow;
    for (i = 0; i < numRotavs; i++) {
      if (i == indLow)
        continue;
      expected1 = expected2 + estSlope * (float)fabs(focusVals[i] - focusVals[indLow]);
      setupDefocusFit(rotavs[i], rotavs[indLow], numRotPts, bkgds[i], bkgds[indLow],
                      fitStart, fitEnd, sincCurve, numSincPts, numSincNodes, expected1,
                      expected2, (varyScale ? -3 : 3), varyBkgd, weightPow);
      findDefocusSizes(&size1, &size2, &errmin, 0);
      //printf("%d %f %f\n", i, size1, errmin);
      //fflush(stdout);
      signx = focusVals[i] < focusVals[indLow] ? -1.f : 1.f;
      if (signx < 0)
        ndat = iBelow++;
      else
        ndat = iAbove++;
      xx[ndat] = signx * (focusVals[i] - focusVals[indLow]);
      yy[ndat] = -signx;
      zz[ndat] = size1;
    }

    // Fit to single slope if only one point on one side
    if (numBelow < 2 || numAbove < 2) {

      //printf("%d %f\n", i, expected2);
      lsFit2(xx, yy, zz, ndat, fitSlope, &bb, &size0);
      *fitFocus = bb / *fitSlope + focusVals[indLow];
      
    } else {
      
      // fit to two slopes otherwise
      lsFit(xx, zz, numBelow, &slopeBelow, &intBelow, &roBel);
      lsFit(&xx[numBelow], &zz[numBelow], numAbove, &slopeAbove, &intAbove, &roAbo);
      *fitSlope = 0.5f * (slopeBelow + slopeAbove);
      *fitFocus = (intBelow - intAbove) / (slopeBelow + slopeAbove);
      size0 = slopeAbove * *fitFocus + intAbove;
      *fitFocus += focusVals[indLow];
      /*printf("below %f %f %f  above %f %f %f\n", slopeBelow, intBelow, roBel, 
        slopeAbove, intAbove, roAbo); */
    }

    // Limit the fitted size for the purpose of computing next size; this is better than
    // trying to prevent size increases
    size0 = B3DMIN(size0, maxSize0);
    diff = size0 + *fitSlope * (float)fabs(*fitFocus - focusVals[indLow]);
    /* printf("focus %.2f  size0 %.2f  diff %.2f\n", *fitFocus, size0, diff);
       fflush(stdout); */
    sizeIter++;

    if (sizeIter == maxSizeIter)
      break;

    // stop next time if size is small
    expected2 = diff;
    if (expected2 < 0.5) {
      expected2 = 0.5;
      maxSizeIter = sizeIter + 1;
    }
  }
  if (*fitFocus < focusVals[indBelow] + 0.2 * minBelow || 
      *fitFocus > focusVals[indAbove] - 0.2 * minAbove) {
    /* printf("Replacing %.2f  %.2f\n", *fitFocus,  *fitSlope);
       fflush(stdout); */
    *fitFocus = *paraFocus;
    *fitSlope = estSlope;
    return -2;
  }
  /* for (i = 0; i < ndat; i++)
    printf(" %f%s", zz[i], (i == ndat - 1) ? "\n":"");

  printf("%d %f\n", sizeIter, expected2, errmin);
  fflush(stdout); */

  return 0;
}

// Get the two sizes from the variable array given setup values
static void getSizes(float *y, float *size1, float *size2)
{
  *size1 = y[0];
  if (sSizeType == 1)
    *size2 = y[1];
  else if (sSizeType == 2) {
    *size2 = *size1 + sExpected2 - sExpected1;
    if (*size2 < 0)
      *size2 = -*size2;
  } else
    *size2 = sExpected2;
}
                 
// The error function called by the simplex routine
static void sincErr(float *y, float *error)
{
  float bkgd1, bkgd2, size1, size2, xsinc, sinc1, sinc2, scale = 1.0;
  double val1, val2, diff;
  int ixsinc, ind, nvar = 1, amplify = 0;
  double derr = 0.;

  getSizes(y, &size1, &size2);
  if (sSizeType == 1)
    nvar = 2;
  if (sRelScale)
    scale = y[nvar++];
  if (sVaryBkgd) {
    bkgd1 = y[nvar++];
    bkgd2 = y[nvar++];
  } else {
    bkgd1 = sBkgd1;
    bkgd2 = sBkgd2;
  }
  if (size2 <= 0) {
    amplify = 1;
    size2 = -size2 + .1f;
  }
  if (size1 < 1.e-3 * size2)
    size1 = 0.1f * size2;

  for (ind = sStartInd; ind < sEndInd; ind++) {
    xsinc = (float)(sSincScale * (ind + 0.5) * size1);
    xsinc = (float)B3DMIN(sNumSincPts - 1.01, xsinc);
    ixsinc = (int)xsinc;
    sinc1 = sSincCurve[ixsinc] + (sSincCurve[ixsinc + 1] - sSincCurve[ixsinc]) *
      (xsinc - ixsinc);
    xsinc = (float)(sSincScale * (ind + 0.5) * size2);
    xsinc = (float)B3DMIN(sNumSincPts - 1.01, xsinc);
    ixsinc = (int)xsinc;
    sinc2 = sSincCurve[ixsinc] + (sSincCurve[ixsinc + 1] - sSincCurve[ixsinc]) *
      (xsinc - ixsinc);
    val1 = sRotav1[ind] - bkgd1;
    val2 = sRotav2[ind] - bkgd2;
    if (sVaryBkgd && (val1 < 0 || val2 < 0))
      amplify = 1;
    diff = scale * val1 * sinc2 - val2 * sinc1;
    diff *= diff;
    if (sWeightPow != 1.)
      diff = pow(diff, sWeightPow);
    derr += diff;
  }
  // Amplify the error if size1 < size2 to constrain the search
  if (size1 < size2)
    derr *= 10. * B3DMAX(1., (size2 - y[0]) / size2);
  else if (amplify)
    derr *= 10.;
  *error = (float)(derr / (sEndInd - sStartInd));
  /*if (sTrace > 1 || (sTrace == 1 && *error < sErrMin)) {
    printf("%15.6f %s %f %f", *error,  *error < sErrMin ? "*" : "", size1, y[0]);
    for (ind = 1; ind < nvar; ind++)
      printf(" %f", y[ind]);
    printf("\n");
    fflush(stdout);
  } */
  sErrMin = B3DMIN(sErrMin, *error);
}

// Fortran wrappers
extern "C" {
void sinclikefunction_(int *numPts, int *numNodes, float *curve)
{
  sincLikeFunction(*numPts, *numNodes, curve);
}

void totalsubtractedpower_(float *rotav, int *numPts, float *backStart, float *backEnd,
                           float *powStart, float *powEnd, float *background,
                           float *power)
{
  totalSubtractedPower(rotav, *numPts, *backStart, *backEnd,
                           *powStart, *powEnd, background,
                       power);
}

void setupdefocusfit_(float *rotav1, float *rotav2, int *numRotPts, float *bkgd1,
                     float *bkgd2, float *fitStart, float *fitEnd, float *sincCurve, 
                     int *numSincPts, int *numSincNodes, float *expected1, 
                      float *expected2,
                     int *sizeType, int *varyBkgd, float *weightPow)
{
  setupDefocusFit(rotav1, rotav2, *numRotPts, *bkgd1,
                     *bkgd2, *fitStart, *fitEnd, sincCurve, 
                     *numSincPts, *numSincNodes, *expected1, *expected2,
                  *sizeType, *varyBkgd, *weightPow);
}

void finddefocussizes_(float *size1, float *size2, float *errmin, int *trace)
{
  findDefocusSizes(size1, size2, errmin, *trace);
}

int findfocus_(float *rotavs, int *xdim, int *numRotavs, int *numRotPts, 
               float *focusVals, float *powers, float *bkgds, float *estSlope,
               float *fitStart, float *fitEnd, float *sincCurve, int *numSincPts, 
               int *numSincNodes, int *varyBkgd, int *varyScale, float *weightPow,
               float *paraFocus, float *fitFocus, float *fitSlope)
{
  float *rotavp[MAXCURVES];
  int i;
  if (*numRotavs > MAXCURVES)
    return 1;
  for (i = 0; i < *numRotavs; i++)
    rotavp[i] = rotavs + i * *xdim;
  return findFocus(rotavp, *numRotavs, *numRotPts, focusVals, 
                   powers, bkgds, *estSlope, *fitStart, *fitEnd,
                   sincCurve, *numSincPts, *numSincNodes, *varyBkgd,
                   *varyScale, *weightPow, paraFocus, fitFocus, fitSlope);
}
}
