
#pragma once
#include "Utilities\json.h"

enum {
  SOCKERR_SOCKET_BUSY = 1, SOCKERR_SENDING, SOCKERR_RECVING, SOCKERR_TIMEOUT,
  SOCKERR_BUF_LENGTH, SOCKERR_FAIL_PARSE, SOCKERR_NO_MATCH, CEOSERR_FAIL_PARSE,
  CEOSERR_INVALID_RPC, CEOSERR_NO_METHOD, CEOSERR_BAD_PARAMS, CEOSERR_JSONRPC_ERR,
  CEOSERR_SERVER_ERR, CEOSERR_NO_KEY, CEOSERR_RESPONSE_PARSE, CEOS_UNRECOGNIZED
};



class CCEOSFilter
{
public:
  CCEOSFilter();
  ~CCEOSFilter();
  int Connect(CString &ipAddress, int port, CString &errStr);;
  int SendAndReceiveCmd(std::string &cmdstr, std::string &responseStr);
  int PackSendReceiveAndUnpack(json::jobject &params, const char *method, json::jobject &response);
  const char *GetErrorString(int errCode);
  int GetFilterInfo(BOOL noSpectrumOffset);
  int GetFilterMode(int &mode);
  int GetEnergyLoss(float &offset);
  int SetEnergyLoss(float offset);
  int GetSlitState(BOOL &slitIn, float &width);
  int SetSlitState(BOOL slitIn, float width);

private:
    SOCKET mConnectSocket;           // The socket
    int mIdValue;                    // Sequential ID value for message and respose
    float mMinLinerTubeOffset;       // Min and max offsets
    float mMaxLinerTubeOffset;
    float mMinHToffset;
    float mMaxHToffset;
    float mLastHToffset;             // Last values of offsets, used when setting loss
    float mLastLinerTubeOffset;
    BOOL mNoSpectrumOffset;          // Flag to use HT only
    std::string mCEOSerrMess;        // Message to append to error message
    std::string mFilterVersion;
    CSerialEMApp *mWinApp;
};

