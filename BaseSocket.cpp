// BaseSocket.cpp - base class full of statics for socket interfaces

#include "stdafx.h"
#include "SerialEM.h"
#include <winsock.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include "BaseSocket.h"

// Stupid double-declaration for class statics in C++
CSerialEMApp *CBaseSocket::mWinApp;
bool CBaseSocket::mWSAinitialized = false;
int CBaseSocket::mTypeID[MAX_SOCKET_IDS];
int CBaseSocket::mIdIndexToSockIndMap[MAX_SOCKET_IDS];
unsigned short CBaseSocket::mPortByID[MAX_SOCKET_IDS];
char *CBaseSocket::mIPaddrByID[MAX_SOCKET_IDS];
unsigned short CBaseSocket::mPort[MAX_SOCKETS];
char *CBaseSocket::mIPaddress[MAX_SOCKETS];
SOCKET CBaseSocket::mServer[MAX_SOCKETS];
SOCKADDR_IN CBaseSocket::mSockAddr[MAX_SOCKETS];
int CBaseSocket::mChunkSize = 65536;
int CBaseSocket::mNumIDs = 0;
int CBaseSocket::mNumSockets = 0;
bool CBaseSocket::mCloseBeforeNextUse[MAX_SOCKETS];
int CBaseSocket::mHandshakeCode[MAX_SOCKETS];
int CBaseSocket::mNumLongSend[MAX_SOCKETS];
int CBaseSocket::mNumBoolSend[MAX_SOCKETS];
int CBaseSocket::mNumDblSend[MAX_SOCKETS];
int CBaseSocket::mNumLongRecv[MAX_SOCKETS];
int CBaseSocket::mNumBoolRecv[MAX_SOCKETS];
int CBaseSocket::mNumDblRecv[MAX_SOCKETS];
bool CBaseSocket::mRecvLongArray[MAX_SOCKETS];
long *CBaseSocket::mLongArray[MAX_SOCKETS];
long CBaseSocket::mLongArgs[MAX_SOCKETS][MAX_LONG_ARGS];
double CBaseSocket::mDoubleArgs[MAX_SOCKETS][MAX_DBL_ARGS];
BOOL CBaseSocket::mBoolArgs[MAX_SOCKETS][MAX_BOOL_ARGS];
char *CBaseSocket::mArgsBuffer[MAX_SOCKETS];
int CBaseSocket::mArgBufSize[MAX_SOCKETS];
int CBaseSocket::mNumBytesSend[MAX_SOCKETS];


CBaseSocket::CBaseSocket(void)
{
  SEMBuildTime(__DATE__, __TIME__);
}

CBaseSocket::~CBaseSocket(void)
{
}

// Call once to initialize the static members
// Could have been done above but then it would have to be kept up to date for MAX_SOCKETS
void CBaseSocket::InitSocketStatics(void)
{
  mWinApp = (CSerialEMApp *)AfxGetApp();
  for (int i = 0; i < MAX_SOCKET_IDS; i++) {
    mPortByID[i] = 0;
    mIPaddrByID[i] = NULL;
    mTypeID[i] = -1;
    mIdIndexToSockIndMap[i] = -1;
  }
  for (int i = 0; i < MAX_SOCKETS; i++) {
    mIPaddress[i] = NULL;
    mServer[i] = INVALID_SOCKET;
    mCloseBeforeNextUse[i] = false;
    mArgsBuffer[i] = NULL;
    mArgBufSize[i] = 0;
  }
}

// Initialize for one socket: check the IP address, allocate the buffer, and
// start up Winsock for the first socket class to initialize
// Old form with fixed socket index
int CBaseSocket::InitializeOneSocket(int sockInd, const char *message)
{
  return InitializeSocketByID(sockInd, NULL, 0, message);
}

// New form with typeID and assigned socket index
int CBaseSocket::InitializeSocketByID(int typeID, int *sockInd, int addToPort, 
                                      const char *message)
{
  CString report;

  // Socket index is next socket; OR the ID itself if index address is NULL
  int indSock = sockInd ? mNumSockets : typeID;
  int err, indID = LookupTypeID(typeID);

  // Make sure there is an address for this type/index
  if (indID < 0 || !mIPaddrByID[indID]) {
    SEMMessageBox(CString("No IP address has been defined for the ") + message);
    return 1;
  }

  // Make sure that a verbatim socket index is OK
  if (!sockInd && (indSock < 0 || indSock >= MAX_SOCKETS)) {
    SEMMessageBox(CString("The fixed socket index is invalid for the ") + message);
    return 1;
  }

  // Make sure there are not too many sockets
  if (indSock >= MAX_SOCKETS) {
    SEMMessageBox(CString("There are too many sockets in use, cannot open socket for "
      "the ") + message);
    return 1;
  }

  // Make sure the index isn't already assigned
  if (mIPaddress[indSock]) {
    SEMMessageBox(CString("Cannot use a fixed socket index for the ") + message + 
      CString("\n\nGet an updated version of the plugin that uses a socket ID"));
    return 1;
  }

  // Allocate buffer
  if (!mArgsBuffer[indSock]) {
    NewArray(mArgsBuffer[indSock], char, ARGS_BUFFER_CHUNK);
    if (!mArgsBuffer[indSock]) {
      SEMMessageBox(CString("Failed to allocate little buffer for ") + message);
      return 1;
    }
    mArgBufSize[indSock] = ARGS_BUFFER_CHUNK;
  }

  // Make sure WinSock can initialize
  err = SEMInitializeWinsock();
  if (err)
    return err;

  // All is good: commit to this socket, copy port and address, return assigned index
  mNumSockets = B3DMAX(mNumSockets, indSock + 1);
  mIPaddress[indSock] = mIPaddrByID[indID];
  mPort[indSock] = mPortByID[indID] + addToPort;
  mIdIndexToSockIndMap[indID] = indSock;
  SEMTrace('K', "Initializing for socket at IP %s  port %d", mIPaddress[indSock], 
    (int)mPort[indSock]);
  if (sockInd)
    *sockInd = indSock;
  return 0;
}

// Reallocate the argument buffer if needed; if it fails, return 1 and leave things as
// they were
int CBaseSocket::ReallocArgsBufIfNeeded(int sockInd, int needSize)
{
  int newSize;
  char *newBuf;
  if (needSize < mArgBufSize[sockInd] - 3)
    return 0;
  newSize = ((needSize + ARGS_BUFFER_CHUNK - 1) / ARGS_BUFFER_CHUNK) * ARGS_BUFFER_CHUNK;
  NewArray(newBuf, char, newSize);
  if (!newBuf)
    return 1;
  memcpy(newBuf, mArgsBuffer[sockInd], mArgBufSize[sockInd]);
  mArgBufSize[sockInd] = newSize;
  mArgsBuffer[sockInd] = newBuf;
  return 0;
}

// Call once to uninitialize on program end
void CBaseSocket::UninitializeWSA(void)
{
  if (mWSAinitialized)
    WSACleanup();
  mWSAinitialized = false;
  for (int i = 0; i < MAX_SOCKETS; i++) {
    mIPaddress[i] = NULL;
    delete mArgsBuffer[i];
    mArgsBuffer[i] = NULL;
  }
  for (int j = 0; j < mNumIDs; j++) {
    if (mIPaddrByID[j])
      free(mIPaddrByID[j]);
    mIPaddrByID[j] = NULL;
  }
}

// Set the IP or port for a given ID: Look up the type and see if it already has an index
// if not record this type in the next slot
void CBaseSocket::SetServerIP(int typeID, CString ipString)
{
  int ind = LookupTypeID(typeID);
  if (ind < 0) {
    if (mNumIDs >= MAX_SOCKET_IDS)
      return;
    ind = mNumIDs++;
    mTypeID[ind] = typeID;
  }
  mIPaddrByID[ind] = _strdup((LPCTSTR)ipString);
}

void CBaseSocket::SetServerPort(int typeID, int port)
{
  int ind = LookupTypeID(typeID);
  if (ind < 0) {
    if (mNumIDs >= MAX_SOCKET_IDS)
      return;
    ind = mNumIDs++;
    mTypeID[ind] = typeID;
  }
  mPortByID[ind] = (unsigned short)port;
}

// Return the port actually assigned
unsigned short CBaseSocket::GetServerPort(int typeID, unsigned short dfltPort)
{
  int ind = LookupTypeID(typeID);
  if (ind >= 0 && mPortByID[ind] > 0)
    return mPortByID[ind];
  return dfltPort;
}

// Return the server IP if one is defined, or 127.0.0.1
void CBaseSocket::GetServerIP(int typeID, std::string & serverIP)
{
  int ind = LookupTypeID(typeID);
  if (ind < 0 || !mIPaddrByID[ind])
    serverIP = "127.0.0.1";
  else
    serverIP = mIPaddrByID[ind];
}

// Look up whether the server with the given ID is remote
bool CBaseSocket::ServerIsRemote(int typeID)
{
  int ind = LookupTypeID(typeID);
  if (ind < 0 || !mIPaddrByID[ind])
    return false;
  return strcmp("127.0.0.1", mIPaddrByID[ind]) != 0;
}

int CBaseSocket::LookupTypeID(int typeID)
{
  for (int ind = 0; ind < mNumIDs; ind++)
    if (typeID == mTypeID[ind])
      return ind;
  return -1;
}

// Returns 1 if an ID has already been assigned to a socket, -1 if no IP entered for it,
// or 0 if it is assignable
int CBaseSocket::IsTypeIDassigned(int typeID)
{
  int indID = LookupTypeID(typeID);
  if (indID < 0 || !mIPaddrByID[indID])
    return -1;
  return (mIdIndexToSockIndMap[indID] < 0 ? 0 : 1);
}

void CBaseSocket::CloseBeforeNextUse(int typeID)
{
  int idInd = LookupTypeID(typeID);
  if (idInd >= 0 && mIdIndexToSockIndMap[idInd] >= 0)
    mCloseBeforeNextUse[mIdIndexToSockIndMap[idInd]] = true;
}

// 11/17/22: This is to disable deprecation warning on inet_addr that says to use
// inet_pton or InetPton (unicode version).  Neither of these is defined even when
// including Ws2tcpip.h as instructed.  Maybe later...
#pragma warning(disable:4996) 

// Open the socket and connect it to the server
int CBaseSocket::OpenServerSocket(int sockInd)
{
  mSockAddr[sockInd].sin_addr.S_un.S_addr = inet_addr(mIPaddress[sockInd]); 
  mSockAddr[sockInd].sin_family = AF_INET;
  mSockAddr[sockInd].sin_port = htons(mPort[sockInd]);     // short, network byte order
  memset(mSockAddr[sockInd].sin_zero, '\0', sizeof(mSockAddr[sockInd].sin_zero));

  mServer[sockInd] = socket(PF_INET, SOCK_STREAM, 0);
  if(mServer[sockInd] == INVALID_SOCKET) {
    SEMTrace('K', "BaseSocket: Cannot open server socket at IP %s on port %d (%d)",
      mIPaddress[sockInd], mPort[sockInd], WSAGetLastError());
    return 1;
  }

  // Connect the Socket.
  if(connect(mServer[sockInd], (PSOCKADDR) &mSockAddr[sockInd], sizeof(SOCKADDR_IN))) {
    SEMTrace('K', "BaseSocket: Error connecting to Server socket at IP %s on port %d (%d)"
      , mIPaddress[sockInd], mPort[sockInd], WSAGetLastError());
    CloseServer(sockInd);
    return 1;
  }
  SEMTrace('K', "BaseSocket: Connected to Server socket at IP %s on port %d",
      mIPaddress[sockInd], mPort[sockInd]);
  return 0;
}

// Close the socket and mark it as invalid
void CBaseSocket::CloseServer(int sockInd)
{
  closesocket(mServer[sockInd]);
  mServer[sockInd] = INVALID_SOCKET;
  mCloseBeforeNextUse[sockInd] = false;
}

// Send a message in the argument buffer to the server and get a reply
// returns 1 for an error, and the negative of the number of bytes received if
// it is not as many as are needed for the command so that can be reponded to later
int CBaseSocket::ExchangeMessages(int sockInd)
{
  int nbytes, err, trial, numReceived, numExpected, needed;
  double startTime, timeDiff;
  if (!mWSAinitialized) {
    SEMTrace('K', "BaseSocket: Winsock not initialized");
    return 1;
  }
  if (mCloseBeforeNextUse[sockInd])
    CloseServer(sockInd);
  if (mServer[sockInd] == INVALID_SOCKET && OpenServerSocket(sockInd)) {
    SEMTrace('K', "BaseSocket: Failed to open socket");
    return 1;
  }
  for (trial = 0; trial < 2; trial++) {

    // Try to send the message
    nbytes = send(mServer[sockInd], mArgsBuffer[sockInd], mNumBytesSend[sockInd], 0);
    //SEMTrace('1', "BaseSocket: send returned %d", nbytes);
    if (nbytes <= 0) {

      // If that fails, close the server, and on first trial, reopen it and try again
      SEMTrace('K', "BaseSocket: send error %d", WSAGetLastError());
      CloseServer(sockInd);
      if (trial || OpenServerSocket(sockInd)) {
        SEMTrace('K', "BaseSocket: send error %d on trial %d", WSAGetLastError(), trial);
        return 1;
      }
      continue;
    }

    // Make sure everything was sent; if this fails give up
    if (FinishSendingBuffer(sockInd, mArgsBuffer[sockInd], mNumBytesSend[sockInd], 
      nbytes)) {
      SEMTrace('K', "BaseSocket: send error %d when finishing sending buffer", 
        WSAGetLastError());
      CloseServer(sockInd);
      return 1;
    }

    // Try to get the reply
    startTime = GetTickCount();
    numReceived = recv(mServer[sockInd], mArgsBuffer[sockInd], mArgBufSize[sockInd], 0);
    if (numReceived <= 0) {

      // If that fails with lost connection within a short time on first try, reopen
      // and try again
      err = WSAGetLastError();
      timeDiff = SEMTickInterval(startTime);
      if ((numReceived == 0 || err == WSAECONNABORTED || err == WSAECONNRESET) && 
        timeDiff < 200.) {
        SEMTrace('K', "BaseSocket: recv error %d after %.0f, retry %d", err, 
          timeDiff, 1 - trial);
        CloseServer(sockInd);
        if (trial || OpenServerSocket(sockInd)) {
          SEMTrace('K', "BaseSocket: recv error %d on trial %d", recv, trial); //want
          return 1;
        }
      }
    } else
      break;
  }

  // Find out how many bytes are in message and make sure we have the whole thing
  memcpy(&numExpected, &mArgsBuffer[sockInd][0], sizeof(int));
  ReallocArgsBufIfNeeded(sockInd, numExpected);
  if (FinishGettingBuffer(sockInd, mArgsBuffer[sockInd], numReceived, numExpected, 
                          mArgBufSize[sockInd])) {
    CloseServer(sockInd);
    SEMTrace('K', "BaseSocket: recv error %d when finishing getting args buffer", 
      WSAGetLastError()); //want
    return 1;
  }
  if (numExpected > mArgBufSize[sockInd]) {
    SEMTrace('K', "BaseSocket: received message too big (%d bytes) for arg buffer", 
      numExpected);  //want
    return 1;
  }
  
  needed = sizeof(int) + mNumLongRecv[sockInd] * sizeof(long) + mNumBoolRecv[sockInd] * 
    sizeof(BOOL) + mNumDblRecv[sockInd] * sizeof(double);

  if ((!mRecvLongArray[sockInd] && needed != numExpected) ||
    (mRecvLongArray[sockInd] && needed > numExpected))
    return -numExpected;
  return 0;
}

// Make sure the entire message has been received, based on initial byte count
int CBaseSocket::FinishGettingBuffer(int sockInd, char *buffer, int numReceived, 
                                      int numExpected, int bufSize)
{
  int numNew, ind, err = 0;
  DWORD timeout;
  if (numExpected > 1024 && SEMGetRecvImageTimeout() > 0.) {
    timeout = (DWORD)(SEMGetRecvImageTimeout() * 1000);
    setsockopt(mServer[sockInd], SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout,
      sizeof(timeout));
  }

  while (numReceived < numExpected) {

    // If message is too big for buffer, just get it all and throw away the start
    ind = numReceived;
    if (numExpected > bufSize)
      ind = 0;
    numNew = recv(mServer[sockInd], &buffer[ind], bufSize - ind, 0);
    if (numNew <= 0) {
      if (WSAGetLastError() != WSAETIMEDOUT) {
        err = 1;
        break;
      }
      err = (numExpected - numReceived) + 1;
      break;
    }
    if (numNew + numReceived < numExpected)
      SEMTrace('K', "got %d new, total %d, left %d", numNew, numNew + numReceived,
        numExpected - (numNew + numReceived));
    numReceived += numNew;
  }
  if (numExpected > 1024 && SEMGetRecvImageTimeout() > 0.) {
    timeout = (DWORD)(10000 * 1000);
    setsockopt(mServer[sockInd], SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout,
      sizeof(timeout));
  }
  return err;
}

// Send all or the remainder of a buffer
int CBaseSocket::FinishSendingBuffer(int sockInd, char *buffer, int numBytes,
                                      int numTotalSent)
{
  int numToSend, numSent;
  while (numTotalSent < numBytes) {
    numToSend = numBytes - numTotalSent;
    if (numToSend > mChunkSize)
      numToSend = mChunkSize;
    numSent = send(mServer[sockInd], &buffer[numTotalSent], numToSend, 0);
    if (numSent < 0) {
      return 1;
    }
    numTotalSent += numSent;
  }
  return 0;
}


/////////////////////////////////////////////////////////////////////
// Support functions for the function calls

// Set the variables for starting to exchange a message, putting function code in first
// long argument position
void CBaseSocket::InitializePacking(int sockInd, int funcCode)
{
  mLongArgs[sockInd][0] = funcCode;
  mNumLongSend[sockInd] = 1;
  mNumDblSend[sockInd] = 0;
  mNumBoolSend[sockInd] = 0;
  mNumLongRecv[sockInd] = 0;
  mNumDblRecv[sockInd] = 0;
  mNumBoolRecv[sockInd] = 0;
  mRecvLongArray[sockInd] = false;
  mLongArray[sockInd] = NULL;
}

// Once arguments have been placed in the arrays, this routine packs them into a message,
// sends the message, received the reply, unpacks it into the argument arrays, and sets
// the return code to a negative value in various error cases
void CBaseSocket::SendAndReceiveArgs(int sockInd)
{
 // This value was set to actual arguments for clarity; add one now for the return value
 mNumLongRecv[sockInd]++;
 int funcCode = mLongArgs[sockInd][0];
 if (PackDataToSend(sockInd)) {
   mLongArgs[sockInd][0] = -1;
   SEMTrace('K', "BaseSocket: Data to send are too large for argument buffer");
   return;
 }
 int err = ExchangeMessages(sockInd);
 if (err > 0) {
   mLongArgs[sockInd][0] = -1;
   return;
 }
 if (UnpackReceivedData(sockInd, -err)) {
   mLongArgs[sockInd][0] = -1;
   SEMTrace('K', "BaseSocket: Received data are too large for argument buffer");
   return;
 }
 if (mLongArgs[sockInd][0] < 0) {
   SEMTrace('K', "BaseSocket: Server return code %d on chan %d", mLongArgs[sockInd][0], 
     sockInd);
   return;
 }
 if (err < 0) {
   SEMTrace('K', "BaseSocket: wrong number of bytes received (%d) than needed for "
     "function %d", -err, funcCode);
   mLongArgs[sockInd][0] = -1;
 }
}

// Simply call a function with one integer argument
int CBaseSocket::SendOneArgReturnRetVal(int sockInd, int funcCode, int argument)
{
  InitializePacking(sockInd, funcCode);
  mLongArgs[sockInd][mNumLongSend[sockInd]++] = argument;
  SendAndReceiveArgs(sockInd);
  return mLongArgs[sockInd][0];
}

// Call a function with no arguments to return a string
const char *CBaseSocket::GetOneString(int sock, int funcCode)
{
  static char *empty = "";
  InitializePacking(sock, funcCode);
  mRecvLongArray[sock] = true;
  mNumLongRecv[sock] = 1;
  SendAndReceiveArgs(sock);
  if (mLongArgs[sock][0])
    return empty;
  return (const char *)mLongArray[sock];
}

// Adds a string as a long array after copying it into the supplied array; this should be
// called AFTER all long arguments are added
void CBaseSocket::AddStringAsLongArray(int sock, const char *name, long *longArr, 
                                              int maxLen)
{
  int len = ((int)strlen(name) + 4) / 4;
  if (len > maxLen)
    len = maxLen;
  strncpy_s((char *)longArr, len * 4, name, _TRUNCATE);
  mLongArray[sock] = longArr;
  mLongArgs[sock][mNumLongSend[sock]++] = len;
}

// Adds an optional array of longs then an optional collection of strings by allocating
// an array and returning it; this should be called AFTER all long arguments are added
long *CBaseSocket::AddLongsAndStrings(int sock, long *longVals, int numLongs, 
                                      const char **strings, int numStrings)
{
  int ind, len, charsLeft, lenTot = numLongs * sizeof(long);
  long *longArr;
  char *nameStr;
  for (ind = 0; ind < numStrings; ind++) 
    lenTot += (int)strlen(strings[ind]) + 1;
  lenTot = (lenTot + 5) / 4;
  longArr = (long *)malloc(lenTot * sizeof(long));
  if (!longArr)
    return NULL;

  // Pack the names after the binnings and terminate with an empty string (not needed...)
  nameStr = (char *)(&longArr[numLongs]);
  charsLeft = (lenTot - numLongs) * sizeof(long) - 1;
  for (ind = 0; ind < numLongs; ind++)
    longArr[ind] = longVals[ind];
  for (ind = 0; ind < numStrings; ind++) {
    strncpy_s(nameStr, charsLeft, strings[ind], _TRUNCATE);
    len = (int)strlen(strings[ind]) + 1;
    nameStr += len;
    charsLeft -= len;
  }
  nameStr[0] = 0x00;
  mLongArray[sock] = longArr;
  mLongArgs[sock][mNumLongSend[sock]++] = lenTot;
  return longArr;
}



// Exchanges messages for an image acquisition then, if all is good, acquires the image
// buffer of the expected size
int CBaseSocket::SendAndReceiveForImage(int sockInd, short *imArray, long *arrSize,
                                         long *width, 
                                         long *height, int bytesPerPixel)
{
  int numBytes, numChunks, nsent, chunkSize, numToGet, chunk, totalRecv = 0, err = 0;
  double startTicks;
  mNumLongRecv[sockInd] = 4;
  SendAndReceiveArgs(sockInd);
  if (mLongArgs[sockInd][0] < 0)
    return 1;
  *arrSize = mLongArgs[sockInd][1];
  *width = mLongArgs[sockInd][2];
  *height = mLongArgs[sockInd][3];
  numChunks = mLongArgs[sockInd][4];
  if (mLongArgs[sockInd][0])
    return mLongArgs[sockInd][0];
  numBytes = *arrSize * bytesPerPixel;
  memset(imArray, 0, numBytes);
  SEMTrace('K', "Return args received (%d %d %d), expecting %d bytes for image in %d "
    "chunks", *arrSize, *width, *height, numBytes, numChunks);
  startTicks = GetTickCount();
  chunkSize = (numBytes + numChunks - 1) / numChunks;
  for (chunk = 0; chunk < numChunks; chunk++) {
    if (chunk) {
      InitializePacking(sockInd, mHandshakeCode[sockInd]);
      if (PackDataToSend(sockInd)) {
        mCloseBeforeNextUse[sockInd] = true;
        return 1;
      }
      nsent = send(mServer[sockInd], mArgsBuffer[sockInd], mNumBytesSend[sockInd], 0);
      if (nsent <= 0) {
        SEMTrace('K', "BaseSocket: send error %d", WSAGetLastError());
        CloseServer(sockInd);
        return 1;
      }
    }
    numToGet = B3DMIN(numBytes - totalRecv, chunkSize);
    err = FinishGettingBuffer(sockInd, (char *)imArray + totalRecv, 0, numToGet,
      numToGet);
    if (err) {
      if (err > 1)
        SEMTrace('K', "BaseSocket: Timeout while receiving image (chunk # %d) from "
          "server; %d bytes left", chunk, err - 1);
      else
        SEMTrace('K', "BaseSocket: Error %d while receiving image (chunk # %d) from "
          "server", WSAGetLastError(), chunk);
      mCloseBeforeNextUse[sockInd] = true;
      return 1;
    }
    totalRecv += numToGet;
  }
  SEMTrace('K',"Transfer rate %.1f MB/s", numBytes / 
    (1000. * B3DMAX(1., SEMTickInterval(startTicks))));
  return 0;
}

// Unpack the received argument buffer, skipping over the first word with byte count
int CBaseSocket::UnpackReceivedData(int sockInd, int limitedNum)
{
  int numBytes, numUnpacked = sizeof(int);

  if (mNumLongRecv[sockInd] > MAX_LONG_ARGS || mNumBoolRecv[sockInd] > MAX_BOOL_ARGS || 
    mNumDblRecv[sockInd] > MAX_DBL_ARGS)
    return 1;

  // If the message is basically empty then just let the caller report that; otherwise
  // then try to get the error code from the next word and return
  if (limitedNum > 0 && limitedNum < 8) {
    mLongArgs[sockInd][0] = 0;
    return 0;
  }
  numBytes = mNumLongRecv[sockInd] * sizeof(long);
  if (limitedNum > 0)
    numBytes = 4;
  memcpy(mLongArgs[sockInd], &mArgsBuffer[sockInd][numUnpacked], numBytes);
  if (limitedNum > 0)
    return 0;
  numUnpacked += numBytes;
  numBytes = mNumBoolRecv[sockInd] * sizeof(BOOL);
  if (numBytes)
    memcpy(mBoolArgs[sockInd], &mArgsBuffer[sockInd][numUnpacked], numBytes);
  numUnpacked += numBytes;
  numBytes = mNumDblRecv[sockInd] * sizeof(double);
  if (numBytes)
    memcpy(mDoubleArgs[sockInd], &mArgsBuffer[sockInd][numUnpacked], numBytes);
  numUnpacked += numBytes;

  // If receiving a long array, size is in last long arg; copy address 
  if (mRecvLongArray[sockInd] && mNumLongRecv[sockInd] > 0) {
    mLongArray[sockInd] = (long *)(&mArgsBuffer[sockInd][numUnpacked]);
    numUnpacked += sizeof(long) * mLongArgs[sockInd][mNumLongRecv[sockInd] - 1];
  }
  return 0;
}

// Pack the data into the argument buffer as longs, BOOLS, doubles, and the long array
int CBaseSocket::PackDataToSend(int sockInd)
{
  int numAdd;
  mNumBytesSend[sockInd] = sizeof(int);
  if (mNumLongSend[sockInd]) {
    numAdd = mNumLongSend[sockInd] * sizeof(long);
    if (numAdd + mNumBytesSend[sockInd] > mArgBufSize[sockInd])
      return 1;
    memcpy(&mArgsBuffer[sockInd][mNumBytesSend[sockInd]], mLongArgs[sockInd], numAdd);
    mNumBytesSend[sockInd] += numAdd;
  }
  if (mNumBoolSend[sockInd]) {
    numAdd = mNumBoolSend[sockInd] * sizeof(BOOL);
    if (numAdd + mNumBytesSend[sockInd] > mArgBufSize[sockInd])
      return 1;
    memcpy(&mArgsBuffer[sockInd][mNumBytesSend[sockInd]], mBoolArgs[sockInd], numAdd);
    mNumBytesSend[sockInd] += numAdd;
  }
  if (mNumDblSend[sockInd]) {
    numAdd = mNumDblSend[sockInd] * sizeof(double);
    if (numAdd + mNumBytesSend[sockInd] > mArgBufSize[sockInd])
      return 1;
    memcpy(&mArgsBuffer[sockInd][mNumBytesSend[sockInd]], mDoubleArgs[sockInd], numAdd);
    mNumBytesSend[sockInd] += numAdd;
  }

  // If there is a long array to send, the last long arg has the size
  if (mLongArray[sockInd]) {
    numAdd = mLongArgs[sockInd][mNumLongSend[sockInd] - 1] * sizeof(long);
    if (ReallocArgsBufIfNeeded(sockInd, numAdd + mNumBytesSend[sockInd]))
      return 1;
    memcpy(&mArgsBuffer[sockInd][mNumBytesSend[sockInd]], mLongArray[sockInd], numAdd);
    mNumBytesSend[sockInd] += numAdd;
  }

  // Put the number of bytes at the beginning of the message
  memcpy(&mArgsBuffer[sockInd][0], &mNumBytesSend[sockInd], sizeof(int));
  return 0;
}
