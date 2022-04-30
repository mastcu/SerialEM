#include "stdafx.h"
#include "SerialEM.h"
#include "PythonServer.h"
#include "BaseSocket.h"
#include "MacroProcessor.h"

enum { PSS_RegularCommand = 1, PSS_ChunkHandshake, PSS_OKtoRunExternalScript,
  PSS_GetBufferImage, PSS_PutImageInBuffer};

// Table of functions, with # of incoming long, bool, and double, # of outgoing
// long, bool and double.  The final number is the sum of 1 if there is a long array at
// the end, whose size is in the last long argument (include that size in the number of 
// incoming long's), plus 2 if a long array is to be returned, whose size must be in the
// last returned long (include that size in the number of outgoing long's).
static ArgDescriptor sFuncTable[] = {
  {PSS_RegularCommand,              3, 0, 0,   3, 0, 0,   3, "RegularCommand"},
  {PSS_OKtoRunExternalScript,       0, 0, 0,   0, 1, 0,   0, "OKtoRunExternalScript"},
  {PSS_GetBufferImage,              2, 0, 0,   6, 0, 0,   0, "GetBufferImage"},
  {PSS_PutImageInBuffer,            9, 0, 0,   0, 0, 0,   0, "PutImageInBuffer"},
  {-1, 0,0,0,0,0,0,0, NULL}
};

CSerialEMApp * CPythonServer::mWinApp;
EMimageBuffer *CPythonServer::mImBufs;
EMimageBuffer *CPythonServer::mFFTBufs;
int CPythonServer::mImType;
int CPythonServer::mImSizeX;
int CPythonServer::mImSizeY;
int CPythonServer::mImToBuf;
int CPythonServer::mImBaseBuf;
int CPythonServer::mMoreBinning;
int CPythonServer::mCapFlag;
char *CPythonServer::mImArray = NULL;
HANDLE CPythonServer::mJobObject = NULL;

static ScriptLangData *sScriptData;

CPythonServer::CPythonServer()
{
  SEMBuildTime(__DATE__, __TIME__);
  mWinApp = (CSerialEMApp *)AfxGetApp();
  mImBufs = mWinApp->GetImBufs();
  mFFTBufs = mWinApp->GetFFTBufs();
  mHandshakeCode = PSS_ChunkHandshake;
}

CPythonServer::~CPythonServer()
{
}

int CPythonServer::StartServerIfNeeded(int sockInd)
{
  int serverInd = (sockInd == RUN_PYTH_SOCK_ID ? 0 : 1);
  int err = 0;
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION jobInfo;
  CString mess, code;

  sScriptData = &mWinApp->mMacroProcessor->mScrpLangData;
  if (!CMacroProcessor::mScrpLangDoneEvent)
    err = 1;
  if (mHSocketThread[serverInd] && mInitialized[serverInd]) {
    if (!mStartupError[serverInd])
      return 0;
    err = 1;
  }
  if (err) {
    mess.Format("Cannot run %s Python scripts due to previous startup errors", 
      serverInd ? "external " : "");
    mWinApp->AppendToLog(mess);
    return 1;
  }

  mPort[serverInd] = CBaseSocket::GetServerPort(sockInd, serverInd ? 48888 : 48889);
  err = StartSocket(serverInd);
  if (err) {
    mess.Format("Cannot run %s Python scripts due to error (%d) in starting socket for"
      " communication", serverInd ? "external " : "", err);
    SEMMessageBox(mess);
  }

  // Create a job object if starting python processes to run scripts
  if (!err && !serverInd && !mJobObject) {
    mess = "";
    mJobObject = CreateJobObject(NULL, NULL);
    if (!mJobObject)
      mess.Format("creating job object: error code %d", GetLastError);

    // Get the existing process info and set the flag to kill processes when last handle
    // is closed, which happens when SerialEM dies.
    if (mess.IsEmpty()) {
      if (!QueryInformationJobObject(mJobObject, JobObjectExtendedLimitInformation,
        &jobInfo, sizeof(JOBOBJECT_EXTENDED_LIMIT_INFORMATION), NULL))
        mess.Format("calling QueryInformationJobObject: error code %d", 
          GetLastError);
    }
    if (mess.IsEmpty()) {
      jobInfo.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
      if (!SetInformationJobObject(mJobObject, JobObjectExtendedLimitInformation,
        &jobInfo, sizeof(JOBOBJECT_EXTENDED_LIMIT_INFORMATION)))
        mess.Format("calling SetInformationJobObject: error code %d", GetLastError);
    }
    if (mess.IsEmpty()) {
      if (!AssignProcessToJobObject(mJobObject, GetCurrentProcess()))
        mess.Format("calling AssignProcessToJobObject: error code %d", 
          GetLastError);
    }

    // This is not a fatal error but needs to be reported
    if (!mess.IsEmpty())
      mWinApp->AppendToLog("WARNING: Error" + mess);
  }
  return err;
}

void CPythonServer::ShutdownSocketIfOpen(int sockInd)
{
  int serverInd = (sockInd == RUN_PYTH_SOCK_ID ? 0 : 1);
  if (mHSocketThread[serverInd] && mInitialized[serverInd]) {
    ShutdownSocket(serverInd);
  }
}

int CPythonServer::ProcessCommand(int sockInd, int numBytes)
{
  int ind, numItems, lenTot, needAdd = 0, retSend = 0;
  long *longArgs = mLongArgs[sockInd];
  BOOL *boolArgs = mBoolArgs[sockInd];
  double *doubleArgs = mDoubleArgs[sockInd];
  double *dblArr;
  char *strArr;
  long *longArr = NULL;
  KImage *image;
  EMimageBuffer *imBufs;
  char *imArray;
  int arrSize, numChunks;
  bool doSendArgs = true;

  if (PrepareCommand(sockInd, numBytes, sFuncTable,
    "SerialEM probably needs to be upgraded to match the current Python module version\n",
    ind))
    return 1;

  if (mLongArgs[sockInd][0] != PSS_OKtoRunExternalScript && sockInd > 0 && 
    !sScriptData->externalControl) {
    TryToStartExternalControl();
    if (!sScriptData->externalControl) {
      SendArgsBack(sockInd, -9);
      return 0;
    }
  }

  // THE FUNCTION CALLS
  // A main point about the socket interface: the function code is received in
  // mLongArgs[0] and the return value is passed back in mLongArgs[0], so the first
  // integer argument passed in either direction is in mLongArgs[1]
  switch (mLongArgs[sockInd][0]) {
  case PSS_OKtoRunExternalScript:
    if (!sScriptData->externalControl) {
      TryToStartExternalControl();
      boolArgs[0] = sScriptData->externalControl > 0;
    } else
      boolArgs[0] = false;
    break;

  case PSS_RegularCommand:
    sScriptData->functionCode = longArgs[1];
    numItems = longArgs[2] + 1;
    sScriptData->lastNonEmptyInd = longArgs[2];
    dblArr = (double *)(&mLongArray[sockInd][numItems]);
    strArr = (char *)(&dblArr[numItems]);
    for (ind = 0; ind < numItems; ind++) {
      sScriptData->itemInt[ind] = mLongArray[sockInd][ind];
      sScriptData->itemDbl[ind] = dblArr[ind];
      sScriptData->strItems[ind] = strArr;
      strArr += strlen(strArr) + 1;
    }
    sScriptData->commandReady = 1;
    while (WaitForSingleObject(CMacroProcessor::mScrpLangDoneEvent, 1000)) {
      Sleep(2);
    }
    longArgs[1] = sScriptData->highestReportInd;
    longArgs[2] = sScriptData->errorOccurred;
    longArr = AddReturnArrays(lenTot);
    if (!longArr) {
      retSend = -2;
    } else {
      mLongArray[sockInd] = longArr;
      mLongArgs[sockInd][3] = lenTot;
    }
    break;

  case PSS_GetBufferImage:
    imBufs = longArgs[2] ? mFFTBufs : mImBufs;
    image = imBufs[longArgs[1]].mImage;
    longArgs[5] = 0;
    if (image) {
      longArgs[1] = image->getType();
      longArgs[2] = image->getRowBytes();
      longArgs[3] = image->getWidth();
      longArgs[4] = image->getHeight();
      longArgs[5] = longArgs[2] * longArgs[4];
      SendImageBack(sockInd, image->getData(), longArgs[5], 6);
      doSendArgs = false;
    }
    break;

  case PSS_PutImageInBuffer:
    mImType = longArgs[1];
    mImSizeX = longArgs[2];
    mImSizeY = longArgs[3];
    arrSize = longArgs[4];
    mImToBuf = longArgs[5];
    mImBaseBuf = longArgs[6];
    mMoreBinning = longArgs[7];
    mCapFlag = longArgs[8];
    numChunks = longArgs[9];
    NewArray(imArray, char, arrSize);
    if (!imArray) {
      retSend = -2;
    } else {
      doSendArgs = false;
      if (ReceiveImage(sockInd, imArray, arrSize, numChunks)) {
        delete[] imArray;
      } else {
        mImArray = imArray;
        while (WaitForSingleObject(CMacroProcessor::mScrpLangDoneEvent, 1000))
          Sleep(2);
      }
    }
    break;

  default:
    retSend = -1;  // Incorrect command
    break;
  }
  if (doSendArgs)
    SendArgsBack(sockInd, retSend);
  free(longArr);
  return 0;
}

long *CPythonServer::AddReturnArrays(int &lenTot)
{
  int numLongs = sScriptData->highestReportInd + 1;
  int ind, len, charsLeft;
  long *longArr;
  char *nameStr;
  lenTot = numLongs * (sizeof(long) + sizeof(double));
  for (ind = 0; ind < numLongs; ind++) {
    if (sScriptData->repValIsString[ind])
      lenTot += (int)sScriptData->reportedStrs[ind].GetLength() + 1;
  }
  lenTot = (lenTot + 5) / 4;
  longArr = (long *)malloc(lenTot * sizeof(long));
  if (!longArr)
    return NULL;

  // Pack the  and terminate with an empty string (not needed...)
  charsLeft = (lenTot - numLongs) * sizeof(long) - 1;
  for (ind = 0; ind < numLongs; ind++)
    longArr[ind] = sScriptData->repValIsString[ind] ? 1 : 0;
  nameStr = (char *)(&longArr[numLongs]);
  memcpy(nameStr, &sScriptData->reportedVals[0], numLongs * sizeof(double));
  charsLeft -= numLongs * sizeof(double);
  nameStr += numLongs * sizeof(double);

  for (ind = 0; ind < numLongs; ind++) {
    if (sScriptData->repValIsString[ind]) {
      strncpy_s(nameStr, charsLeft, (LPCTSTR)sScriptData->reportedStrs[ind], _TRUNCATE);
      len = (int)sScriptData->reportedStrs[ind].GetLength() + 1;
      nameStr += len;
      charsLeft -= len;
    }
  }
  nameStr[0] = 0x00;
  return longArr;
}

void CPythonServer::TryToStartExternalControl(void)
{
  sScriptData->externalControl = -1;
  Sleep(10);
  for (int ind = 0; ind < 500; ind++) {
    if (sScriptData->externalControl >= 0 && sScriptData->waitingForCommand)
      break;
    Sleep(10);
  }
}
