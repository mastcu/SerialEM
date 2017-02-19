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
  SetMember(CString, MailFrom)
  SetMember(CString, Server)
  SetMember(int, Port)
  GetSetMember(CString, SendTo)
  GetMember(bool, Initialized)

private:
  CString mServer;
  int mPort;
  CString mMailFrom;
  CString mSendTo;
  SOCKADDR_IN mSockAddr;
  bool mInitialized;
  CSerialEMApp *mWinApp;

  void Check(int iStatus, char *szFunction);
};
