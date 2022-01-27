#pragma once
#define MAX_SOCK_CHAN 2
#define MESS_ERR_BUFF_SIZE 320

#include <winsock.h>
//#include "WinDef.h"

// Required global functions
//int DoFinishStartup(int sockInd);

#define MAX_LONG_ARGS 16
#define MAX_DBL_ARGS 8
#define MAX_BOOL_ARGS 8
#define ARGS_BUFFER_CHUNK 1024
#define SELECT_TIMEOUT 50

// This lists the actual arguments in and out, excluding function code in recv and
// return value on send
struct ArgDescriptor {
  int funcCode;
  int numLongRecv;
  int numBoolRecv;
  int numDblRecv;
  int numLongSend;
  int numBoolSend;
  int numDblSend;
  int hasLongArray;  // Sum of 1 if receiving and 2 if sending
  const char *label;
};

class CBaseServer
{
 public:
  CBaseServer();
  ~CBaseServer() {};

 protected:

  static int mNumLongSend[MAX_SOCK_CHAN];
  static int mNumBoolSend[MAX_SOCK_CHAN];
  static int mNumDblSend[MAX_SOCK_CHAN];
  static int mNumLongRecv[MAX_SOCK_CHAN];
  static int mNumBoolRecv[MAX_SOCK_CHAN];
  static int mNumDblRecv[MAX_SOCK_CHAN];
  static long *mLongArray[MAX_SOCK_CHAN];
  static long mLongArgs[MAX_SOCK_CHAN][MAX_LONG_ARGS];   // Max is 16
  static double mDoubleArgs[MAX_SOCK_CHAN][MAX_DBL_ARGS];  // Max is 8
  static BOOL mBoolArgs[MAX_SOCK_CHAN][MAX_BOOL_ARGS];   // Max is 8
  static char *mArgsBuffer[MAX_SOCK_CHAN];
  static int mNumBytesSend[MAX_SOCK_CHAN];
  static int mArgBufSize[MAX_SOCK_CHAN];
  static BOOL mSendLongArray[MAX_SOCK_CHAN];

  static bool mInitialized[MAX_SOCK_CHAN];
  static unsigned short mPort[MAX_SOCK_CHAN]; 
  static HANDLE mHSocketThread[MAX_SOCK_CHAN];
  static int mStartupError[MAX_SOCK_CHAN];
  static int mLastWSAerror[MAX_SOCK_CHAN];
  static bool mCloseForExit[MAX_SOCK_CHAN];
  static int mChunkSize;
  static bool mProcessingCommand;
  static int mDebugVal;

  static int mSuperChunkSize;  
  static int mHandshakeCode;
  static SOCKET mHListener[MAX_SOCK_CHAN];
  static SOCKET mHClient[MAX_SOCK_CHAN]; 

 public:
   static int GetDebugVal() { return mDebugVal; };
   static void ErrorToLog(const char *message);
   static int DoProcessCommand(int sockInd, int numExpected);
  static void DebugToLog(const char *message);
  static void EitherToLog(const char *prefix, const char *message, bool saveErr = false);
  static char mErrorBuf[MESS_ERR_BUFF_SIZE + 1];
  static char mMessageBuf[MAX_SOCK_CHAN][MESS_ERR_BUFF_SIZE + 1];
  
  static int StartSocket(int sockInd);
  static void ShutdownSocket(int sockInd);
  static void Cleanup(int sockInd);
  static DWORD WINAPI SocketProc(LPVOID pParam);
  static int FinishGettingBuffer(int sockInd, int numReceived, int numExpected);
  static void CloseClient(int sockInd);
  static int SendBuffer(int sockInd, char *buffer, int numBytes);
  static void ReportErrorAndClose(int sockInd, int retval, const char *message);
  static void CloseOnExitOrSelectError(int sockInd, int err);
  static int ListenForHandshake(int sockInd, int superChunk);
  static int SendArgsBack(int sockInd, int retval);
  static int UnpackReceivedData(int sockInd);
  static void SendImageBack(int sockInd, void *imArray, int imSize, int indChunks);
  static int ReceiveImage(int sockInd, char *imArray, int numBytes, int numChunks);
  static int FinishGettingBuffer(int sockInd, char *buffer, int numReceived,
    int numExpected, int bufSize);
  static int PackDataToSend(int sockInd);
  static int PrepareCommand(int sockInd, int numBytes, ArgDescriptor *funcTable, 
    const char *upgradeMess, int &ind);
  static int FinishGettingBuffer(int numReceived, int numExpected)
  {return FinishGettingBuffer(0, numReceived, numExpected);};
  static void CloseClient() {CloseClient(0);};
  static int SendBuffer(char *buffer, int numBytes) {return SendBuffer(0, buffer, numBytes);};
  static void ReportErrorAndClose(int retval, const char *message)
  {ReportErrorAndClose(0, retval, message);};
  static int ListenForHandshake(int superChunk) {return ListenForHandshake(0, superChunk);};
  static int SendArgsBack(int retval) {return SendArgsBack(0, retval);};
  static int UnpackReceivedData() {return UnpackReceivedData(0);};
  static int PackDataToSend() {return PackDataToSend(0);};
  static void ShutdownSocket() {ShutdownSocket(0);};
  static unsigned short GetPortForSocketIndex(int ind) { return (ind >= 0 && ind < MAX_SOCK_CHAN) ? mPort[ind] : 0; };

};

