// BaseServer.cpp - Base socket class that can be used in any socket server
//
// Copyright (C) 2013-2016 by the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//
#include "StdAfx.h"
#include "SerialEM.h"
#include "BaseServer.h"
#include "PythonServer.h"
#include "MacroProcessor.h"
#include <stdio.h>

int CBaseServer::mNumLongSend[MAX_SOCK_CHAN];
int CBaseServer::mNumBoolSend[MAX_SOCK_CHAN];
int CBaseServer::mNumDblSend[MAX_SOCK_CHAN];
int CBaseServer::mNumLongRecv[MAX_SOCK_CHAN];
int CBaseServer::mNumBoolRecv[MAX_SOCK_CHAN];
int CBaseServer::mNumDblRecv[MAX_SOCK_CHAN];
long *CBaseServer::mLongArray[MAX_SOCK_CHAN];
long CBaseServer::mLongArgs[MAX_SOCK_CHAN][MAX_LONG_ARGS];   // Max is 16
double CBaseServer::mDoubleArgs[MAX_SOCK_CHAN][MAX_DBL_ARGS];  // Max is 8
BOOL CBaseServer::mBoolArgs[MAX_SOCK_CHAN][MAX_BOOL_ARGS];   // Max is 8
char *CBaseServer::mArgsBuffer[MAX_SOCK_CHAN];
int CBaseServer::mNumBytesSend[MAX_SOCK_CHAN];
int CBaseServer::mArgBufSize[MAX_SOCK_CHAN];
BOOL CBaseServer::mSendLongArray[MAX_SOCK_CHAN];

bool CBaseServer::mInitialized[MAX_SOCK_CHAN];
unsigned short CBaseServer::mPort[MAX_SOCK_CHAN]; 
HANDLE CBaseServer::mHSocketThread[MAX_SOCK_CHAN];
int CBaseServer::mStartupError[MAX_SOCK_CHAN];
int CBaseServer::mLastWSAerror[MAX_SOCK_CHAN];
bool CBaseServer::mCloseForExit[MAX_SOCK_CHAN];
char CBaseServer::mMessageBuf[MAX_SOCK_CHAN][MESS_ERR_BUFF_SIZE + 1];
char CBaseServer::mErrorBuf[MESS_ERR_BUFF_SIZE + 1] = {0x00};
int CBaseServer::mChunkSize = 16810000;     // Tests indicated 16777216 was optimal size
int CBaseServer::mHandshakeCode;
int CBaseServer::mDebugVal = 1;

// 33554432 is exactly 4K x 2K x 2, failures occurred above twice this size
// This value fits the F416 4200x4200 buffer size
int CBaseServer::mSuperChunkSize = 33620000;  
SOCKET CBaseServer::mHListener[MAX_SOCK_CHAN];
SOCKET CBaseServer::mHClient[MAX_SOCK_CHAN]; 
bool CBaseServer::mProcessingCommand = false;

CBaseServer::CBaseServer()
{
  SEMBuildTime(__DATE__, __TIME__);
  for (int i = 0; i < MAX_SOCK_CHAN; i++) {
    mInitialized[i] = false;
    mHSocketThread[i] = 0;
    mStartupError[i] = -1;
    mLastWSAerror[i] = 0;
    mCloseForExit[i] = false;
    mArgsBuffer[i] = NULL;
    mArgBufSize[i] = 0;
    mMessageBuf[i][0] = 0x00;
    mMessageBuf[i][MESS_ERR_BUFF_SIZE] = 0x00;
    mHClient[i] = INVALID_SOCKET;
  }
  mErrorBuf[MESS_ERR_BUFF_SIZE] = 0x00;
}

CBaseServer::~CBaseServer()
{
  for (int i = 0; i < MAX_SOCK_CHAN; i++)
    free(mArgsBuffer[i]);
}

void CBaseServer::ErrorToLog(const char *message)
{
  SEMTrace('0', "CBaseServer: %s", message);
}

int CBaseServer::DoProcessCommand(int sockInd, int numExpected)
{
  CSerialEMApp * winApp = (CSerialEMApp *)AfxGetApp();
  return winApp->mPythonServer->ProcessCommand(sockInd, numExpected);
}

void CBaseServer::DebugToLog(const char *message)
{
  SEMTrace('K', "CBaseServer: %s", message);
}

void CBaseServer::EitherToLog(const char *prefix, const char *message, bool saveErr)
{
  if (saveErr)
    ErrorToLog(message);
  else
    DebugToLog(message);
}

int CBaseServer::StartSocket(int sockInd)
{
  DWORD threadID;
  if (SEMInitializeWinsock())
    return 1;
  mHSocketThread[sockInd] = CreateThread(NULL, 0, SocketProc, &sockInd,
    CREATE_SUSPENDED, &threadID);
  if (!mHSocketThread[sockInd]) {
    Cleanup(sockInd);
    return 2;
  }
  mCloseForExit[sockInd] = false;

  // Is this an appropriate priority here?
  //SetThreadPriority(mHSocketThread, THREAD_PRIORITY_HIGHEST);

  // It returns the previous suspend count; so it should be 1.
  DWORD err = ResumeThread(mHSocketThread[sockInd]);
  if (err < 0 || err > 1) {

    // Probably should signal the thread to shut down here as in FocusRamper
    Cleanup(sockInd);
    return 3;
  }

  // Wait until the thread signals success or an error
  Sleep(2);
  while (mStartupError[sockInd] == -1)
    Sleep(2);
  mInitialized[sockInd] = true;

  return mStartupError[sockInd];
}

void CBaseServer::ShutdownSocket(int sockInd)
{
  DWORD code;
  if (!mInitialized[sockInd])
    return;
  if (mHSocketThread[sockInd]) {
    mCloseForExit[sockInd] = true;
    WaitForSingleObject(mHSocketThread[sockInd], 3 * SELECT_TIMEOUT);
    GetExitCodeThread(mHSocketThread[sockInd], &code);
    if (code == STILL_ACTIVE) {
      CloseClient(sockInd);
      closesocket(mHListener[sockInd]);

      // Terminating is cleaner with the GatanSocket case, could try suspend
      TerminateThread(mHSocketThread[sockInd], 1);
    }
    CloseHandle(mHSocketThread[sockInd]);
    mHSocketThread[sockInd] = NULL;
  } else {
    CloseClient(sockInd);
    closesocket(mHListener[sockInd]);
  }
  Cleanup(sockInd);
}


// The main socket thread routine, starts a listener, loops on getting connections and
// commands
DWORD WINAPI CBaseServer::SocketProc(LPVOID pParam)
{
  SOCKET hListener;
  SOCKADDR_IN sockaddr;
  struct timeval tv;
  BOOL yes = TRUE;
  int sockInd = *((int *)pParam);
  int numBytes, err, numExpected;
  fd_set readFds;      // file descriptor list for select()

  mArgsBuffer[sockInd] = (char *)malloc(ARGS_BUFFER_CHUNK);
  if (!mArgsBuffer[sockInd]) {
    mStartupError[sockInd] = 8;
    return mStartupError[sockInd];
  }
  mArgBufSize[sockInd] = ARGS_BUFFER_CHUNK;

  // Get the listener socket
  hListener = socket(PF_INET, SOCK_STREAM, 0);
  if (hListener == INVALID_SOCKET) {
    mLastWSAerror[sockInd] = WSAGetLastError();
    mStartupError[sockInd] = 4;
    return mStartupError[sockInd];
  }

  // Avoid "Address already in use" error message
  if (setsockopt(hListener, SOL_SOCKET, SO_REUSEADDR, (char *)&yes, sizeof(BOOL))) {
    mLastWSAerror[sockInd] = WSAGetLastError();
    mStartupError[sockInd] = 5;
    return mStartupError[sockInd];
  }

  // Get socket address for listener on the port
  sockaddr.sin_family = AF_INET;
  sockaddr.sin_port = htons(mPort[sockInd]);     // short, network byte order
  sockaddr.sin_addr.s_addr = INADDR_ANY;
  memset(sockaddr.sin_zero, '\0', sizeof(sockaddr.sin_zero));

  // Bind the listener socket to the port
  if (bind(hListener, (struct sockaddr *)(&sockaddr), sizeof(sockaddr))) {
    mLastWSAerror[sockInd] = WSAGetLastError();
    mStartupError[sockInd] = 6;
    return mStartupError[sockInd];
  }
  
  tv.tv_sec = 0;
  tv.tv_usec = 1000 * SELECT_TIMEOUT;

  // Listen
  if (listen(hListener, 2)) {
    mLastWSAerror[sockInd] = WSAGetLastError();
    mStartupError[sockInd] = 7;
    return mStartupError[sockInd];
  }
  mHListener[sockInd] = hListener;

  // Call out for special tasks
  /*if (DoFinishStartup(sockInd))
    return mStartupError[sockInd];*/
  
  // Set the value to indicate we are through all the startup
  mStartupError[sockInd] = 0;

  _snprintf(mMessageBuf[sockInd], MESS_ERR_BUFF_SIZE, 
    "Listening for connections on socket %d  port %d", (int)mHListener[sockInd], 
    (int)mPort[sockInd]);
  DebugToLog(mMessageBuf[sockInd]);
  
  // Loop on listening for connections and accepting them or receiving commands
  for (;;) {
    FD_ZERO(&readFds);
    FD_SET(hListener, &readFds);
    if (mHClient[sockInd] != INVALID_SOCKET)
      FD_SET(mHClient[sockInd], &readFds);
    err = select(2, &readFds, NULL, NULL, &tv);
    if (err < 0 || mCloseForExit[sockInd]) {

      // Close up on error or signal from plugin
      DebugToLog("Closing socket");
      CloseClient(sockInd);
      closesocket(hListener);
      if (err < 0 && !mCloseForExit[sockInd]) {
        mLastWSAerror[sockInd] = WSAGetLastError();
        mStartupError[sockInd] = 7;
        _snprintf(mMessageBuf[sockInd], MESS_ERR_BUFF_SIZE, 
          "WSA Error %d on select command", mLastWSAerror[sockInd]);
        ErrorToLog(mMessageBuf[sockInd]);
      }
      DebugToLog("returning");
      return mStartupError[sockInd];
    }

    // Just a timeout - continue the loop
    if (!err)
      continue;

    if (GetDebugVal() > 1) {
      _snprintf(mMessageBuf[sockInd], MESS_ERR_BUFF_SIZE, 
        "Select returned with Ready channel on port %d: listener %d client %d", 
        mPort[sockInd], FD_ISSET(hListener, &readFds), 
        (mHClient[sockInd] != INVALID_SOCKET && 
        FD_ISSET(mHClient[sockInd], &readFds)) ? 1:0);
      DebugToLog(mMessageBuf[sockInd]);
    }

    // There is something to do.  Check the client first (Does ISSET Work?)
    if (mHClient[sockInd] != INVALID_SOCKET && FD_ISSET(mHClient[sockInd], &readFds)) {
      numBytes = recv(mHClient[sockInd], mArgsBuffer[sockInd], mArgBufSize[sockInd], 0);
 
      // Close client on error or disconnect, but allow new connect
      if (numBytes <= 0) {
        ReportErrorAndClose(sockInd, numBytes, "recv from ready client");
      } else {
        memcpy(&numExpected, &mArgsBuffer[sockInd][0], sizeof(int));
 
        // Reallocate buffer if necessary
        if (ReallocArgsBufIfNeeded(numExpected, sockInd)) {
          mStartupError[sockInd] = 8;
          ErrorToLog("Failed to reallocate buffer for receiving data");
          return mStartupError[sockInd];
        }

        if (!FinishGettingBuffer(sockInd, numBytes, numExpected)) {
          if (GetDebugVal() > 1) {
            _snprintf(mMessageBuf[sockInd], MESS_ERR_BUFF_SIZE, 
              "Got %d bytes via recv on socket %d", numExpected, (int)mHClient[sockInd]);
            DebugToLog(mMessageBuf[sockInd]);
          }
          mProcessingCommand = true;
          DoProcessCommand(sockInd, numExpected);
          mProcessingCommand = false;
        }
      }
    }

    // Now check the listener, close an existing client if any, get new client
    if (FD_ISSET(hListener, &readFds)) {
      CloseClient(sockInd);
      mHClient[sockInd] = accept(hListener, NULL, NULL);
      if (mHClient[sockInd] == INVALID_SOCKET) {
        _snprintf(mMessageBuf[sockInd], MESS_ERR_BUFF_SIZE, 
          "accept connection from ready client on port %d", mPort[sockInd]);
        ReportErrorAndClose(sockInd, SOCKET_ERROR, mMessageBuf[sockInd]);
      } else {
        _snprintf(mMessageBuf[sockInd], MESS_ERR_BUFF_SIZE, 
          "Accepted connection from client program %d  ind %d", mPort[sockInd], sockInd);
        EitherToLog("", mMessageBuf[sockInd]);
        if (mDebugVal > 1)
          SEMTrace('[', "EC %d discon %d", CMacroProcessor::mScrpLangData.externalControl,
            CMacroProcessor::mScrpLangData.disconnected ? 1 : 0);

        // If main thread has not caught up to a disconnect and cleared external control,
        // just turn off the disconnect and let things go on as normal
        if (CMacroProcessor::mScrpLangData.externalControl)
          CMacroProcessor::mScrpLangData.disconnected = false;
      }
    }
  }
 
  return 0;
}

// Reallocate the argument buffer if needed; if it fails, return 1 and leave things as
// they were
int CBaseServer::ReallocArgsBufIfNeeded(int needSize, int sockInd)
{
  int newSize;
  char *newBuf;
  if (needSize < mArgBufSize[sockInd] - 4)
    return 0;
  newSize = ((needSize + ARGS_BUFFER_CHUNK - 1) / ARGS_BUFFER_CHUNK) * ARGS_BUFFER_CHUNK;
  PrintfToLog("Reallocating the argument buffer for needed size %d to new size %d",
    needSize, newSize);
  newBuf = (char *)malloc(newSize);
  if (!newBuf)
    return 1;
  memcpy(newBuf, mArgsBuffer, mArgBufSize[sockInd]);
  free(mArgsBuffer[sockInd]);
  mArgBufSize[sockInd] = newSize;
  mArgsBuffer[sockInd] = newBuf;
  return 0;
}

// Close the socket and mark as invalid
void CBaseServer::CloseClient(int sockInd)
{
  if (mHClient[sockInd] == INVALID_SOCKET)
    return;
  _snprintf(mMessageBuf[sockInd], MESS_ERR_BUFF_SIZE, 
    "Closing connection to client via socket %d", (int)mHClient[sockInd]);
  if (!mCloseForExit[sockInd])
    EitherToLog("", mMessageBuf[sockInd]);
  closesocket(mHClient[sockInd]);
  mHClient[sockInd] = INVALID_SOCKET;
}

// Call the Winsock cleanup function on last uninitialization
void CBaseServer::Cleanup(int sockInd)
{
  mInitialized[sockInd] = false;
}

// Get the rest of the message into the buffer if it is not there yet
int CBaseServer::FinishGettingBuffer(int sockInd, int numReceived, int numExpected)
{
  int numNew, ind;
  while (numReceived < numExpected) {

    // If message is too big for buffer, just get it all and throw away the start
    ind = numReceived;
    if (numExpected > mArgBufSize[sockInd])
      ind = 0;
    numNew = recv(mHClient[sockInd], &mArgsBuffer[sockInd][ind],
                  mArgBufSize[sockInd] - ind, 0);
    if (numNew <= 0) {
      ReportErrorAndClose(sockInd, numNew, "recv to get expected number of bytes");
      return 1;
    }
    numReceived += numNew;
  }
  return 0;
}

int CBaseServer::PrepareCommand(int sockInd, int numBytes, ArgDescriptor *funcTable,
                                const char *upgradeMess, int &ind)
{
  int funcCode, needed, needAdd = 0;

  // Get the function code as the second element of the buffer
  if (numBytes < 8 || numBytes > mArgBufSize[sockInd]) {
    SendArgsBack(sockInd, numBytes < 8 ? -4 : -5);  // Inadequate length or too big
    return 1;
  }
  memcpy(&funcCode, &mArgsBuffer[sockInd][sizeof(int)], sizeof(long));

  // Look up the function code in the table
  ind = 0;
  while (funcTable[ind].funcCode >= 0 && funcTable[ind].funcCode != funcCode)
    ind++;
  if (funcTable[ind].funcCode < 0) {
    _snprintf(mMessageBuf[sockInd], MESS_ERR_BUFF_SIZE, "Function code not found: %d - "
      "%s", funcCode, upgradeMess);
    ErrorToLog(mMessageBuf[sockInd]);
    SendArgsBack(sockInd, -1);
    return 1;
  }
  
  // Set the variables for receiving and sending arguments.  Add 1 to the longs for
  // the function code coming in and the return value going out
  mNumLongRecv[sockInd] = funcTable[ind].numLongRecv + 1;
  mNumBoolRecv[sockInd] = funcTable[ind].numBoolRecv;
  mNumDblRecv[sockInd] = funcTable[ind].numDblRecv;
  mNumLongSend[sockInd] = funcTable[ind].numLongSend + 1;
  mNumBoolSend[sockInd] = funcTable[ind].numBoolSend;
  mNumDblSend[sockInd] = funcTable[ind].numDblSend;
  mSendLongArray[sockInd] = (funcTable[ind].hasLongArray & 2) > 0;
  needed = sizeof(int) + mNumLongRecv[sockInd] * sizeof(long) +
    mNumBoolRecv[sockInd] * sizeof(BOOL) + mNumDblRecv[sockInd] * sizeof(double);
  if (needed > numBytes) {
    _snprintf(mMessageBuf[sockInd], MESS_ERR_BUFF_SIZE, "Command %d  %s not long enough:"
      " needed = %d (4 + %d x 4 + %d x 4 + %d x 8)  numBytes = %d", funcCode, 
      funcTable[ind].label, needed, mNumLongRecv[sockInd], mNumBoolRecv[sockInd], 
      mNumDblRecv[sockInd], numBytes);
    ErrorToLog(mMessageBuf[sockInd]);

    SendArgsBack(sockInd, -4);  // Inadequate length, don't even unpack it
    return 1;
  }
  if (UnpackReceivedData(sockInd)) {
    SendArgsBack(sockInd, -5);  // Message too big
    return 1;
  }
  if (funcTable[ind].hasLongArray & 1)
    needAdd = mLongArgs[sockInd][mNumLongRecv[sockInd] - 1] * sizeof(long);
  needed += needAdd;
  if (needed != numBytes) {
    _snprintf(mMessageBuf[sockInd], MESS_ERR_BUFF_SIZE, "Command %d  %s wrong length:"
      " needed = %d (4 + %d x 4 + %d x 4 + %d " "x 8 + %d) numBytes = %d", funcCode, 
      funcTable[ind].label, needed, mNumLongRecv[sockInd], mNumBoolRecv[sockInd], 
      mNumDblRecv[sockInd], needAdd, numBytes);
    ErrorToLog(mMessageBuf[sockInd]);
    SendArgsBack(sockInd, -6);   // Wrong length
    return 1;
  }
  if (GetDebugVal() > 1) {
    _snprintf(mMessageBuf[sockInd], MESS_ERR_BUFF_SIZE, "Processing command %d  %s", 
      funcCode, funcTable[ind].label);
    DebugToLog(mMessageBuf[sockInd]);
  }
  return 0;
}


// Send a buffer back, in chunks if necessary
int CBaseServer::SendBuffer(int sockInd, char *buffer, int numBytes)
{
  int numTotalSent = 0;
  int numToSend, numSent;
  if (GetDebugVal() > 1) {
    _snprintf(mMessageBuf[sockInd], MESS_ERR_BUFF_SIZE, "In SendBuffer, socket %d, "
      "sending %d bytes", (int)mHClient[sockInd], numBytes);
    DebugToLog(mMessageBuf[sockInd]);
  }
  while (numTotalSent < numBytes) {
    numToSend = numBytes - numTotalSent;
    if (numToSend > mChunkSize)
      numToSend = mChunkSize;
    if (GetDebugVal() > 1) {
      _snprintf(mMessageBuf[sockInd], MESS_ERR_BUFF_SIZE, "Going to send %d bytes to "
      "socket %d", numToSend, (int)mHClient[sockInd]);
      DebugToLog(mMessageBuf[sockInd]);
    }
    numSent = send(mHClient[sockInd], &buffer[numTotalSent], numToSend, 0);
    if (numSent < 0) {
      ReportErrorAndClose(sockInd, numSent, "send a chunk of bytes back");
      return 1;
    }
    if (numSent < numToSend) {
      _snprintf(mMessageBuf[sockInd], MESS_ERR_BUFF_SIZE, 
        "requested send %d bytes, only %d sent so far", numToSend, numSent);
      DebugToLog(mMessageBuf[sockInd]);
    }
    numTotalSent += numSent;
  }
  return 0;
}

// Close the connection upon error; report it unless it is clearly a SerialEM disconnect
void CBaseServer::ReportErrorAndClose(int sockInd, int retval, const char *message)
{
  if (retval == SOCKET_ERROR || !retval) {
    mLastWSAerror[sockInd] = WSAGetLastError();
    _snprintf(mMessageBuf[sockInd], MESS_ERR_BUFF_SIZE, "WSA Error %d on call to %s", 
      mLastWSAerror[sockInd], message);

    // Connections from remote machine (or at least from Linux) close gracefully and
    // give 0 return value and 0 error
    if (mLastWSAerror[sockInd] == WSAECONNRESET || (!retval && !mLastWSAerror[sockInd])) {
      DebugToLog(mMessageBuf[sockInd]);
      DebugToLog("Disconnecting");

      // Set as disconnected and cancel any pending user stop error
      CMacroProcessor::mScrpLangData.disconnected = true;
      if (CMacroProcessor::mScrpLangData.errorOccurred == SCRIPT_USER_STOP)
        CMacroProcessor::mScrpLangData.errorOccurred = 0;
    } else
      ErrorToLog(mMessageBuf[sockInd]);
  } else {
    _snprintf(mMessageBuf[sockInd], MESS_ERR_BUFF_SIZE, "retval %d on call to %s", 
      retval, message);
    ErrorToLog(mMessageBuf[sockInd]);
  }
  CloseClient(sockInd);
}

// Close up on error or signal from plugin
void CBaseServer::CloseOnExitOrSelectError(int sockInd, int err)
{
  DebugToLog("Closing socket");
  CloseClient(sockInd);
  closesocket(mHListener[sockInd]);
  //if (mCloseForExit)
    //Cleanup();
  if (err < 0) {
    mLastWSAerror[sockInd] = WSAGetLastError();
    mStartupError[sockInd] = 7;
    _snprintf(mMessageBuf[sockInd], MESS_ERR_BUFF_SIZE, 
      "WSA Error %d on select command", mLastWSAerror[sockInd]);
    ErrorToLog(mMessageBuf[sockInd]);
  }
}

// Wait for the client to acknowledge receipt of a superchunk of image
int CBaseServer::ListenForHandshake(int sockInd, int superChunk)
{
  struct timeval tv;
  int numBytes, err, numExpected, command;
  fd_set readFds;      // file descriptor list for select()
  tv.tv_sec = 0;
  tv.tv_usec = superChunk / 5;    // This is 5 MB /sec

  FD_ZERO(&readFds);
  FD_SET(mHClient[sockInd], &readFds);
  err = select(1, &readFds, NULL, NULL, &tv);
  if (err < 0 || mCloseForExit[sockInd]) {
    CloseOnExitOrSelectError(sockInd, err);
    return mStartupError[sockInd];
  }

  // A timeout - close client for this so client fails
  if (!err) {
    ReportErrorAndClose(sockInd, 0, "timeout on handshake from client");
    return 1;
  }

  numBytes = recv(mHClient[sockInd], mArgsBuffer[sockInd], mArgBufSize[sockInd], 0);

  // Close client on error or disconnect or too few bytes or anything wrong
  memcpy(&numExpected, &mArgsBuffer[sockInd][0], sizeof(int));
  memcpy(&command, &mArgsBuffer[sockInd][4], sizeof(int));
  if (command != mHandshakeCode || numExpected != 8 || numBytes != 8) {
    ReportErrorAndClose(sockInd, numBytes, "recv handshake from ready client");
    return 1;
  }
  return 0;
}

// Send the arguments back, packing the return value in the first long
int CBaseServer::SendArgsBack(int sockInd, int retval)
{
  mLongArgs[sockInd][0] = retval;

  // Emergency error code is negative, just send one word back
  if (retval < 0) {
    mNumLongSend[sockInd] = 1;
    mNumBoolSend[sockInd] = 0;
    mNumDblSend[sockInd] = 0;
    mSendLongArray[sockInd] = false;
  }
  if (PackDataToSend(sockInd)) {
    ErrorToLog("FAILED TO REALLOCATE DATA BUFFER BIG ENOUGH TO SEND REPLY TO REQUESTER");
    SendArgsBack(sockInd, -3);
    return 1;
  }
  return SendBuffer(sockInd, mArgsBuffer[sockInd], mNumBytesSend[sockInd]);
}

// Send the arguments for an image transfer then send the image if there is no error
void CBaseServer::SendImageBack(int sockInd, void *imArray, int imSize, int indChunks)
{
  int numChunks, chunkSize, numToSend, numLeft, err, totalSent = 0;

  // determine number of superchunks and send that back as fourth long
  numChunks = (imSize + mSuperChunkSize - 1) / mSuperChunkSize;
  mLongArgs[sockInd][indChunks] = numChunks;
  err = SendArgsBack(sockInd, 0);
  _snprintf(mMessageBuf[sockInd], MESS_ERR_BUFF_SIZE, "err sending args %d,"
    " sending image %d in %d chunks", err, imSize, numChunks);
  DebugToLog(mMessageBuf[sockInd]);
  if (!err) {

    // Loop on the chunks until done, getting acknowledgement after each
    numLeft = imSize;
    chunkSize = (imSize + numChunks - 1) / numChunks;
    while (totalSent < imSize) {
      numToSend = chunkSize;
      if (chunkSize > imSize - totalSent)
        numToSend = imSize - totalSent;
      if (SendBuffer(sockInd, (char *)imArray + totalSent, numToSend))
        break;
      totalSent += numToSend;
      if (totalSent < imSize && ListenForHandshake(sockInd, numToSend))
        break;
    }
  }
}

// Get an image being sent in for a buffer
int CBaseServer::ReceiveImage(int sockInd, char *imArray, int numBytes, int numChunks)
{
  int err, chunkSize, numToGet, chunk, totalRecv = 0;
  double startTicks;
  err = SendArgsBack(sockInd, 0);
  _snprintf(mMessageBuf[sockInd], MESS_ERR_BUFF_SIZE, "err sending args back %d,"
    " receiving image %d in %d chunks", err, numBytes, numChunks);
  DebugToLog(mMessageBuf[sockInd]);
  if (err)
    return 1;
  memset(imArray, 0, numBytes);
  startTicks = GetTickCount();
  chunkSize = (numBytes + numChunks - 1) / numChunks;
  for (chunk = 0; chunk < numChunks; chunk++) {
    numToGet = B3DMIN(numBytes - totalRecv, chunkSize);
    err = FinishGettingBuffer(sockInd, (char *)imArray + totalRecv, 0, numToGet,
      numToGet);
    if (err) {
      if (err > 1)
        SEMTrace('K', "BaseServer: Timeout while receiving image (chunk # %d) from "
          "server; %d bytes left", chunk, err - 1);
      else
        SEMTrace('K', "BaseServer: Error %d while receiving image (chunk # %d) from "
          "server", WSAGetLastError(), chunk);
      CloseClient(sockInd);
      return 1;
    }
    totalRecv += numToGet;
    SEMTrace('K', "BaseServer: Chunk # %d  total %d", chunk, totalRecv);
  }
  SEMTrace('K', "Transfer rate %.1f MB/s", numBytes /
    (1000. * B3DMAX(1., SEMTickInterval(startTicks))));
  return 0;
}

// Finish getting full message into the buffer
int CBaseServer::FinishGettingBuffer(int sockInd, char *buffer, int numReceived,
  int numExpected, int bufSize)
{
  int numNew, ind, err = 0;

  while (numReceived < numExpected) {

    // If message is too big for buffer, just get it all and throw away the start
    ind = numReceived;
    if (numExpected > bufSize)
      ind = 0;
    numNew = recv(mHClient[sockInd], &buffer[ind], bufSize - ind, 0);
    //SEMTrace('K', "BaseServer: ind %d %d expc %d  nn %d", ind, numReceived, numExpected,
      //numNew);
    if (numNew <= 0) {
      if (WSAGetLastError() != WSAETIMEDOUT) {
        err = 1;
        break;
      }
      err = (numExpected - numReceived) + 1;
      break;
    }
    numReceived += numNew;
  }
  return err;
}

// Unpack the received argument buffer, skipping over the first word, the byte count
int CBaseServer::UnpackReceivedData(int sockInd)
{
  int numBytes, numUnpacked = sizeof(int);
  if (mNumLongRecv[sockInd] > MAX_LONG_ARGS || mNumBoolRecv[sockInd] > MAX_BOOL_ARGS || 
    mNumDblRecv[sockInd] > MAX_DBL_ARGS)
    return 1;
  numBytes = mNumLongRecv[sockInd] * sizeof(long);
  memcpy(mLongArgs[sockInd], &mArgsBuffer[sockInd][numUnpacked], numBytes);
  numUnpacked += numBytes;
  numBytes = mNumBoolRecv[sockInd] * sizeof(BOOL);
  if (numBytes)
    memcpy(mBoolArgs[sockInd], &mArgsBuffer[sockInd][numUnpacked], numBytes);
  numUnpacked += numBytes;
  numBytes = mNumDblRecv[sockInd] * sizeof(double);
  if (numBytes)
    memcpy(mDoubleArgs[sockInd], &mArgsBuffer[sockInd][numUnpacked], numBytes);
  numUnpacked += numBytes;

  // Here is the starting address of whatever comes next for the few expecting it
  mLongArray[sockInd] = (long *)(&mArgsBuffer[sockInd][numUnpacked]);
  return 0;
}


#define ADD_TO_BUFFER(a, num) \
  if (num) {   \
    memcpy(&mArgsBuffer[sockInd][mNumBytesSend[sockInd]], a, num); \
    mNumBytesSend[sockInd] += num; \
  }

// Pack the data into the argument buffer as longs, BOOLS, doubles
int CBaseServer::PackDataToSend(int sockInd)
{
  int numAddLong = mNumLongSend[sockInd] * sizeof(long);
  int numAddBool = mNumBoolSend[sockInd] * sizeof(BOOL);
  int numAddDbl = mNumDblSend[sockInd] * sizeof(double);
  int numAddArr = mSendLongArray[sockInd] ? 
    mLongArgs[sockInd][mNumLongSend[sockInd] - 1] * sizeof(long) : 0;
  int totAdd = numAddLong + numAddBool + numAddDbl + numAddArr;

  // Make sure buffer is big enough
  mNumBytesSend[sockInd] = sizeof(int);
  if (ReallocArgsBufIfNeeded(totAdd + mNumBytesSend[sockInd], sockInd))
    return 1;

  ADD_TO_BUFFER(mLongArgs[sockInd], numAddLong);
  ADD_TO_BUFFER(mBoolArgs[sockInd], numAddBool);
  ADD_TO_BUFFER(mDoubleArgs[sockInd], numAddDbl);
  ADD_TO_BUFFER(mLongArray[sockInd], numAddArr);

  // Put the number of bytes at the beginning of the message
  memcpy(&mArgsBuffer[sockInd][0], &mNumBytesSend[sockInd], sizeof(int));
  return 0;
}
