// KImageBase.cpp:        A base class for image collections/files
//
// Copyright (C) 2003 by Boulder Laboratory for 3-Dimensional Electron 
// Microscopy of Cells ("BL3DEMC") and the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Authors: David Mastronarde and James Kremer
// Modified for Tecnai by DNM, 2-14-02

#include "stdafx.h"
#include "KImageBase.h"

KImageBase::KImageBase()
{
  mOrigin.x = mOrigin.y = mOrigin.z = mOrigin.t = 0;
  mCur = mOrigin;
  
  mWidth  = 0;
  mHeight = 0;
  mDepth  = 0;
  mLength = 1;
}

KImageBase::~KImageBase()
{

}

void KImageBase::getMaxBounds(KCoord &outCoord)
{
  outCoord.x = mOrigin.x + mWidth;
  outCoord.y = mOrigin.y + mHeight;
  outCoord.z = mOrigin.z + mDepth;
  outCoord.t = mOrigin.t + mLength;
}

void KImageBase::getMinBounds(KCoord &outCoord)
{
  outCoord = mOrigin;
}

void KImageBase::getCur(KCoord &outCoord)
{
  outCoord = mCur;
}

void KImageBase::setCur(KCoord &inCoord)
{
  mCur = inCoord;
  fixCoord(mCur);
}

void KImageBase::getCurPos(KCoord &outCoord)
{
  outCoord.x = mCur.x;
  outCoord.y = mCur.y;
  outCoord.z = mCur.z;
}

void KImageBase::setCurPos(KCoord &inCoord)
{
  mCur.x = inCoord.x;
  mCur.y = inCoord.y;
  mCur.z = inCoord.z;
  fixCoord(mCur);
}
void KImageBase::getCurTime(KCoord &outCoord)
{
  outCoord.t = mCur.t;
}

void KImageBase::setCurTime(KCoord &inCoord)
{
  mCur.t = inCoord.t;
  fixCoord(mCur);
}

int KImageBase::fixCoord(KCoord &ioCoord)
{
  int changed = 0;
  if (ioCoord.x < mOrigin.x)  { changed++; ioCoord.x = mOrigin.x;}
  if (ioCoord.y < mOrigin.y)  { changed++; ioCoord.y = mOrigin.y;}
  if (ioCoord.z < mOrigin.z)  { changed++; ioCoord.z = mOrigin.z;}
  KCoord max;
  getMaxBounds(max);
  if (ioCoord.x >= max.x) { changed++; ioCoord.x = max.x - 1;}
  if (ioCoord.y >= max.y) { changed++; ioCoord.y = max.y - 1;}
  if (ioCoord.z >= max.z) { changed++; ioCoord.z = max.z - 1;}
  return(changed);
}

