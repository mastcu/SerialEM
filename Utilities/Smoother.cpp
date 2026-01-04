// Smoother.cpp:          A class for smoothing a noisy data stream, used for
//                          the screen meter
//
// Copyright (C) 2003-2026 by the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde

#include "stdafx.h"
#include "smoother.h"
#include <math.h>

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#define new DEBUG_NEW
#endif

Smoother::Smoother()
{
  mRingStrt = NULL;
}

Smoother::~Smoother()
{
  if(mRingStrt) delete mRingStrt;
}

int Smoother::Setup (int navg, double thresh1, double thresh2 /*, double scale*/)
{
  mRingSize = navg;
  mThresh1 = thresh1;
  mThresh2 = thresh2;
  //mScale = scale;
  mRingStrt = new double [mRingSize];
  mRingp = mRingStrt;
  mRingSum = 0;
  mRingAvg = 0;
  mNumInRing = 0;
  return 0;
}

double Smoother::Readout(double value)
{

  // if this is the first value, or new value differs from average by
  // threshold1, then read it out and make it the governor

  if(mNumInRing == 0 || (fabs(value - mRingAvg) >= mThresh1))
    mGovernor = value;

  //otherwise, if the average differs from the governing value by
  //threshold2, make the average the governor instead

  else if(fabs(mGovernor - mRingAvg) >= mThresh2)
    mGovernor = mRingAvg;

  //now maintain the ring
  if(mNumInRing <mRingSize)
    mNumInRing++;
  else
    mRingSum -= *mRingp;
  *mRingp++ = value;
  mRingSum += value;
  mRingAvg = mRingSum/mNumInRing;
  if(mRingp >= mRingStrt + mRingSize)
    mRingp = mRingStrt;

  //long int theReadout = floor(mScale * mGovernor + 0.5);
  return (mGovernor);
}
