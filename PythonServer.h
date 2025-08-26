#pragma once
#include "BaseServer.h"
class CPythonServer :
  public CBaseServer
{
public:
  CPythonServer();
  ~CPythonServer();

  int StartServerIfNeeded(int sockInd);
  void ShutdownSocketIfOpen(int sockInd);
  static int ProcessCommand(int sockInd, int numExpected);
  static long *AddReturnArrays(int &lenTot, int pnd);
  static void TryToStartExternalControl(void);
  static int mImType;
  static int mImSizeX;
  static int mImSizeY;
  static int mImToBuf;
  static int mImBaseBuf;
  static int mMoreBinning;
  static int mCapFlag;
  static char *mImArray;
  static HANDLE mJobObject;

private:
  static CSerialEMApp *mWinApp;
  static EMimageBuffer *mImBufs;
  static EMimageBuffer *mFFTBufs;
};

