// CEOSFilter.cpp:    Interface to CEOS energy filter
//
// Copyright (C) 2003-2026 by the Regents of the University of
// Colorado.  See Copyright.txt for full notice of copyright and limitations.
//
// Author: David Mastronarde
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "SerialEM.h"
#include "EMscope.h"
#include "CEOSFilter.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include "Utilities\json.h"

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#define new DEBUG_NEW
#endif

CCEOSFilter::CCEOSFilter()
{
  mConnectSocket = INVALID_SOCKET;
  mIdValue = 1;
  mWinApp = (CSerialEMApp *)AfxGetApp();
}


CCEOSFilter::~CCEOSFilter()
{
  if (mConnectSocket != INVALID_SOCKET)
    closesocket(mConnectSocket);
  mConnectSocket = INVALID_SOCKET;
}

// Establishes socket communication at the given address and port
// Modified from HitachiPlugin
int CCEOSFilter::Connect(CString &ipAddress, int port, CString &errStr)
{
  mConnectSocket = UtilConnectSocket(ipAddress, port, errStr, "CEOSFilter",
    "CEOS filter");
  return mConnectSocket == INVALID_SOCKET ? 1 : 0;
}

#define SOCK_BUF_SIZE 8192
#define SOCK_RECV_SLEEP 1
#define SOCK_TIMEOUT  5000.

// Does the socket communication, sending a string and getting the full response string
int CCEOSFilter::SendAndReceiveCmd(std::string &cmdstr, std::string &responseStr)
{
  int msgSz = 0;
  int sizeSent = 0;
  int oneRec;
  int sizeRec = 0,sizeCum = 0;
  int colonInd, err = 0;
  char buffer[SOCK_BUF_SIZE + 1];
  double startTime;
//  char *ptr;
  CString str;

  str.Format("%d:%s,", cmdstr.size(), cmdstr.c_str());
  msgSz = (int)str.GetLength();
  sizeSent = send(mConnectSocket, (LPCTSTR)str, msgSz, 0);
  if (sizeSent == SOCKET_ERROR)
    return SOCKERR_SENDING;

  //Get response
  startTime = GetTickCount();
  msgSz = 0;
  while (1) {
    oneRec = recv(mConnectSocket, &buffer[sizeRec], SOCK_BUF_SIZE - sizeRec, 0);
    if (!oneRec)
      return SOCKERR_RECVING;
    if (oneRec != SOCKET_ERROR) {
      sizeRec += oneRec;
      buffer[sizeRec] = 0x00;

      // Get the size as soon as possible
      if (!msgSz && strchr(buffer, ':')) {
        colonInd = (int)(strchr(buffer, ':') - buffer);
        msgSz = atoi(buffer);
      }

      // If there is a , at the end, see if the length is enough
      if (buffer[sizeRec - 1] == ',' && msgSz) {
        if (err && sizeCum + sizeRec >= colonInd + msgSz + 2)
          return err;
        if (sizeRec == colonInd + msgSz + 2) {
          responseStr = &buffer[colonInd + 1];
          responseStr.pop_back();
          return 0;
        }
      }

      // If buffer is now full, reset to beginning to try to get the rest of the
      // message and set error
      if (sizeRec >= SOCK_BUF_SIZE) {
        sizeCum += sizeRec;
        err = SOCKERR_BUF_LENGTH;
        sizeRec = 0;
      }
    }

    if (WSAGetLastError() != WSAEWOULDBLOCK)
      return SOCKERR_RECVING;
    if (SEMTickInterval(startTime) > SOCK_TIMEOUT)
      return SOCKERR_TIMEOUT;
    Sleep(SOCK_RECV_SLEEP);
  }

  // This can't happen but it needs a return
  return SOCKERR_TIMEOUT;
}

// Higher level function takes parameter object, composes message to call method, and
// extracts result object from response
int CCEOSFilter::PackSendReceiveAndUnpack(json::jobject &params, const char *method,
  json::jobject &result)
{
  int err = 0, code = 0;
  std::string commandStr, responseStr;
  json::jobject command, errorObj, dataObj, response;
  command["params"] = params;
  command["jsonrpc"] = "2.0";
  command["id"] = mIdValue;
  command["method"] = method;
  commandStr = (std::string)command;
  err = SendAndReceiveCmd(commandStr, responseStr);
  if (err)
    return err;
  if (!json::jobject::tryparse(responseStr.c_str(), response))
    return SOCKERR_FAIL_PARSE;
  if ((int)response["id"] != mIdValue)
    return SOCKERR_NO_MATCH;
  try {
    if (response.has_key("error")) {
      errorObj = response["error"];
      code = (int)errorObj["code"];
      switch (code) {
      case -32700: err = CEOSERR_FAIL_PARSE; break;
      case -32600: err = CEOSERR_INVALID_RPC; break;
      case -32601: err = CEOSERR_NO_METHOD; break;
      case -32603: err = CEOSERR_BAD_PARAMS; break;
      case -32604: err = CEOSERR_JSONRPC_ERR; break;

      case -32000:
      default:
        err = CEOSERR_SERVER_ERR;

        // Despite documentation, the message was in the response object
        //dataObj = errorObj["data"];
        mCEOSerrMess = (std::string)errorObj["message"];
        break;
      }
      return err;
    }
    result = response["result"];
  }
  catch (json::invalid_key key) {
    return CEOSERR_NO_KEY;
  }
  catch (json::parsing_error error) {
    mCEOSerrMess = error.what();
    return CEOSERR_RESPONSE_PARSE;
  }
  return 0;
}

// Error processing
static const char *messages[] = {"Socket is busy with another command",
"Error sending to socket", "Error receiving from socket",
"Timeout getting response to command", "Response is too long for response buffer",
"Failed to parse the JSON string returned from the CEOS interface",
"Response Id does not match command Id", "Parsing of JSON string passed to CEOS failed",
"The JSON string passed to CEOS interface was not a valid RPC string",
"The requested method/command in CEOS interface is not valid",
"Bad parameters were passed to the CEOS interface",
"An internal error occurred in the CEOS JSON-RPC interface",
"An error occurred executing the command in the CEOS interface: ",
"A key with the expected name was not found in response from CEOS",
"An error occurred parsing the result in response from CEOS: ",
"Unrecognized error"
};

const char *CCEOSFilter::GetErrorString(int errCode)
{
  if (!errCode)
    return NULL;
  B3DCLAMP(errCode, 1, CEOS_UNRECOGNIZED);
  if (errCode == CEOSERR_SERVER_ERR || errCode == CEOSERR_RESPONSE_PARSE)
    mCEOSerrMess = messages[errCode - 1] + mCEOSerrMess;
  else
    mCEOSerrMess = messages[errCode - 1];
  return mCEOSerrMess.c_str();
}

// Get information needed to set mins and maxes: the first real test of communication
int CCEOSFilter::GetFilterInfo(BOOL noSpectrumOffset)
{
  FilterParams *filtParams = mWinApp->GetFilterParams();
  json::jobject params, result, range;
  mNoSpectrumOffset = noSpectrumOffset;
  int err = PackSendReceiveAndUnpack(params, "getInfo", result);
  if (err)
    return err;
  try {
    err = 1;
    range = result["slitWidthRange"];
    filtParams->minWidth = (float)range["min"];
    filtParams->maxWidth = (float)range["max"];
    range = result["highTensionOffsetRange"];
    mMinHToffset = (float)range["min"];
    mMaxHToffset = (float)range["max"];
    filtParams->minLoss = mMinHToffset;
    filtParams->maxLoss = mMaxHToffset;
    if (!noSpectrumOffset) {
      range = result["linerTubeRange"];
      mMinLinerTubeOffset = (float)range["min"];
      mMaxLinerTubeOffset = (float)range["max"];
      filtParams->minLoss += mMinLinerTubeOffset;
      filtParams->maxLoss += mMaxLinerTubeOffset;
    }

    // Didn't work for simulator, make it forgivable error since not used
    err = 0;
    mFilterVersion = (std::string)result["filterVersion"];
  }
  catch (json::invalid_key key) {
    if (err)
      return CEOSERR_NO_KEY;
  }
  return 0;
}

// Return whether imaging or spectroscopy
int CCEOSFilter::GetFilterMode(int &mode)
{
  json::jobject params, result;
  CString str;
  int err = PackSendReceiveAndUnpack(params, "getFilterMode", result);
  if (err)
    return err;
  try {

    // It is esi on simulator and ESI for real
    str = ((std::string)result["mode"]).c_str();
    mode = str.CompareNoCase("esi") != 0 ? 1 : 0;
  }
  catch (json::invalid_key key) {
    return CEOSERR_NO_KEY;
  }
  return 0;
}

// Get energy loss, sum of HT offset and liner tube offset
int CCEOSFilter::GetEnergyLoss(float &offset)
{
  json::jobject params, result, result2;
  int err = PackSendReceiveAndUnpack(params, "getHighTensionOffset", result);
  if (err)
    return err;
  err = PackSendReceiveAndUnpack(params, "getLinerTubeVoltage", result2);
  if (err)
    return err;
  try {
    mLastHToffset = (float)result["value"];
    mLastLinerTubeOffset = (float)result2["value"];
    offset = mLastHToffset + mLastLinerTubeOffset;
  }
  catch (json::invalid_key key) {
    return CEOSERR_NO_KEY;
  }
  return 0;
}

// Set energy loss: change just liner tube if possible, or change just HT if
// NoSpectrumOffset is set
int CCEOSFilter::SetEnergyLoss(float offset)
{
  json::jobject params, result;
  int err;
  float HToffsetToSet = EXTRA_NO_VALUE, linerTubeToSet = EXTRA_NO_VALUE;
  float delta = offset - (mLastHToffset + mLastLinerTubeOffset);
  if (mNoSpectrumOffset) {
    HToffsetToSet = mLastHToffset + delta;
  } else if (mLastLinerTubeOffset + delta >= mMinLinerTubeOffset &&
    mLastLinerTubeOffset + delta <= mMaxLinerTubeOffset) {
    linerTubeToSet = mLastLinerTubeOffset + delta;
  } else {
    HToffsetToSet = mLastHToffset + delta;
    if (HToffsetToSet < mMinHToffset) {
      linerTubeToSet = mLastLinerTubeOffset + (HToffsetToSet - mMinHToffset);
      HToffsetToSet = mMinHToffset;
    } else if (HToffsetToSet > mMaxHToffset) {
      linerTubeToSet = mLastLinerTubeOffset + (mMaxHToffset - HToffsetToSet);
      HToffsetToSet = mMaxHToffset;
    }
  }
  if (HToffsetToSet > EXTRA_VALUE_TEST) {
    params["value"] = HToffsetToSet;
    err = PackSendReceiveAndUnpack(params, "setHighTensionOffset", result);

  }
  if (linerTubeToSet > EXTRA_VALUE_TEST) {
    params["value"] = linerTubeToSet;
    err = PackSendReceiveAndUnpack(params, "setLinerTubeVoltage", result);
  }
  return err;
}

// Get in/out and slit width
int CCEOSFilter::GetSlitState(BOOL &slitIn, float &width)
{
  json::jobject params, result;
  int err = PackSendReceiveAndUnpack(params, "getSlit", result);
  if (err)
    return err;
  try {
    slitIn = ((std::string)result["configuration"]).compare("window") == 0;
    if (result.has_key("width"))
      width = (float)result["width"];
  }
  catch (json::invalid_key key) {
    return CEOSERR_NO_KEY;
  }
  return 0;
}

// Set in/out and slit width
int CCEOSFilter::SetSlitState(BOOL slitIn, float width)
{
  json::jobject params, result;
  int err;
  params["configuration"] = slitIn ? "window" : "out";
  if (slitIn)
    params["width"] = width;
  err = PackSendReceiveAndUnpack(params, "setSlit", result);
  return err;
}

