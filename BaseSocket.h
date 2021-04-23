#pragma once
#include <winsock.h>

#define MAX_SOCKETS 8
#define MAX_SOCKET_IDS 10
#define GATAN_SOCK_ID    0
#define FEI_SOCK_ID      1
#define JEOLCAM_SOCK_ID  4
#define TIETZ_SOCK_ID    5
#define HITACHI_SOCK_ID  6
#define RUN_PYTH_SOCK_ID 7
#define EXT_PYTH_SOCK_ID 8
// Note that SavvyScan was given 9!

// Macros for adding arguments
#define LONG_ARG(a, b) mLongArgs[a][mNumLongSend[a]++] = b;
#define BOOL_ARG(a, b) mBoolArgs[a][mNumBoolSend[a]++] = b;
#define DOUBLE_ARG(a, b) mDoubleArgs[a][mNumDblSend[a]++] = b;

class DLL_IM_EX CBaseSocket
{
public:
  CBaseSocket(void);
  ~CBaseSocket(void);

protected:
  static CSerialEMApp *mWinApp;
  static bool mWSAinitialized;
  
  static unsigned short mPortByID[MAX_SOCKET_IDS];
  static char *mIPaddrByID[MAX_SOCKET_IDS];
  static int mTypeID[MAX_SOCKET_IDS];
  static int mIdIndexToSockIndMap[MAX_SOCKET_IDS];
  static unsigned short mPort[MAX_SOCKETS];
  static char *mIPaddress[MAX_SOCKETS];
  static SOCKET mServer[MAX_SOCKETS];
  static SOCKADDR_IN mSockAddr[MAX_SOCKETS];
  static int mChunkSize;
  static int mNumIDs;
  static int mNumSockets;
  static bool mCloseBeforeNextUse[MAX_SOCKETS];

// Declarations needed on both sides (without array on other side)
// IF A MAX IS CHANGED, PLUGINS BUILT WITH OLDER SIZE WILL NOT LOAD
#define ARGS_BUFFER_CHUNK 1024
#define MAX_LONG_ARGS 16
#define MAX_DBL_ARGS 8
#define MAX_BOOL_ARGS 8

  static int mHandshakeCode[MAX_SOCKETS];
  static int mNumLongSend[MAX_SOCKETS];
  static int mNumBoolSend[MAX_SOCKETS];
  static int mNumDblSend[MAX_SOCKETS];
  static int mNumLongRecv[MAX_SOCKETS];
  static int mNumBoolRecv[MAX_SOCKETS];
  static int mNumDblRecv[MAX_SOCKETS];
  static bool mRecvLongArray[MAX_SOCKETS];
  static long *mLongArray[MAX_SOCKETS];
  static long mLongArgs[MAX_SOCKETS][MAX_LONG_ARGS];   // Max is 16
  static double mDoubleArgs[MAX_SOCKETS][MAX_DBL_ARGS];  // Max is 8
  static BOOL mBoolArgs[MAX_SOCKETS][MAX_BOOL_ARGS];   // Max is 8
  static char *mArgsBuffer[MAX_SOCKETS];
  static int mArgBufSize[MAX_SOCKETS];
  static int mNumBytesSend[MAX_SOCKETS];

public:
  static void SetWSAinitialized(bool inVal) {mWSAinitialized = inVal;};
  static bool GetWSAinitialized() {return mWSAinitialized;};
  static void InitSocketStatics(void);
  static int InitializeOneSocket(int sockInd, const char *message);
  static int InitializeSocketByID(int typeID, int *sockInd, int addToPort, const char *message);
  static void UninitializeWSA(void);
  static int ExchangeMessages(int sockInd);
  static int OpenServerSocket(int sockInd);
  static void CloseServer(int sockInd);
  static int ReallocArgsBufIfNeeded(int sockInd, int needSize);

  static void InitializePacking(int funcCode) {InitializePacking(0, funcCode);};
  static void InitializePacking(int sockInd, int funcCode);
  static void SendAndReceiveArgs(int sockInd);
  static int SendOneArgReturnRetVal(int sockInd, int funcCode, int argument);
  static const char *GetOneString(int sock, int funcCode);
  static void AddStringAsLongArray(int sock, const char *name, long *longArr, int maxLen);
  static long *AddLongsAndStrings(int sock, long *longVals, int numLongs, 
                                  const char **strings, int numStrings);
  static int SendAndReceiveForImage(int sockInd, short * imArray, long * arrSize, 
                                    long * width, long * height, int bytePerPixel);
  static int FinishGettingBuffer(int sockInd, char *buffer, int numReceived, 
                                 int numExpected, int bufSize);
  static int FinishSendingBuffer(int sockInd, char *buffer, int numBytes, 
                                 int numTotalSent);
  static int UnpackReceivedData(int sockInd, int limitedNum);
  static int PackDataToSend(int sockInd);
  static void SetServerIP(int typeID, CString ipString);
  static void SetServerPort(int typeID, int port);
  static unsigned short GetServerPort(int typeID, unsigned short dfltPort);
  static int LookupTypeID(int typeID);
  void Release() {};   // Needed to keep macros happy
  static void CloseBeforeNextUse(int typeID);
  static bool ServerIsRemote(int typeID);
};