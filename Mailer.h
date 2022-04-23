#pragma once

#include <winsock2.h>

class CSerialEMApp;

class CMailer
{
public:
  CMailer(void);
  ~CMailer(void);
  bool Initialize();
  bool SendMail(CString subject, CString message);
  SetMember(CString, MailFrom);
  SetMember(CString, Server);
  SetMember(int, Port);
  GetSetMember(CString, SendTo);
  GetMember(bool, Initialized);
  void SetNextEmailAddress(CString &addr, bool addTo) { mNextEmailAddress = addr; addTo = mAddToAddress ; };

private:
  CString mServer;
  int mPort;
  CString mMailFrom;
  CString mSendTo;
  SOCKADDR_IN mSockAddr;
  bool mInitialized;
  CSerialEMApp *mWinApp;
  bool mAddToAddress;
  CString mNextEmailAddress;

  void Check(int iStatus, char *szFunction);
};
