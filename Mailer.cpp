// For sending email to an SMTP server
// This was adapted from code by Matthias Brunner at the Austrian Academy of Sciences

#include "stdafx.h"
#include "SerialEM.h"
#include ".\Mailer.h"
#include "BaseSocket.h"

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#define new DEBUG_NEW
#endif

#define CRLF "\r\n"

CMailer::CMailer(void)
{
  mInitialized = false;
  mSendTo = "";
  mMailFrom = "";
  mServer = "";
  mPort = 0;
  mWinApp = (CSerialEMApp *)AfxGetApp();
  mConnectTimeout = 7;
}

CMailer::~CMailer(void)
{
  mInitialized = false;
}

// Initialize mail sending if appropriate; return true if succeeded
bool CMailer::Initialize()
{
  LPHOSTENT lpHostEntry;
  LPSERVENT lpServEntry;
  CString report;
  unsigned int smtpPort;

  if (mServer.IsEmpty() || mMailFrom.IsEmpty())
    return false;

  // Attempt to intialize WinSock (1.1 or later).
  if (SEMInitializeWinsock())
    return false;

  // Lookup email server's IP address.
  lpHostEntry = gethostbyname((LPCTSTR)mServer);
  if(lpHostEntry == NULL) {
    report.Format("Mailer: Cannot find SMTP mail server %s", mServer);
    mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
    return false;
  }

  smtpPort = mPort;
  if (!mPort) {

    // Get the mail service port.
    lpServEntry = getservbyname("mail", 0);

    // Use the SMTP default port if no other port is specified.
    if(lpServEntry == NULL)
      smtpPort = htons(IPPORT_SMTP);
    else
      smtpPort = lpServEntry->s_port;
  }

  // Setup a Socket Address structure.
  mSockAddr.sin_family = AF_INET;
  mSockAddr.sin_port = smtpPort;
  mSockAddr.sin_addr = *((LPIN_ADDR)*lpHostEntry->h_addr_list);
  mInitialized = true;
  return true;
}

bool CMailer::SendMail(CString subject, CString message)
{
  CString report, fromAddress, sendTo = mSendTo;
  char *username;
  bool acceptedForDelivery = false;
  fd_set write, err;
  SOCKET hServer;
  char szBuffer[4096], szMsgLine[1024];
  TIMEVAL timeout;
  u_long iMode = 1;
  int retVal;

  // Just return silently if not initialized
  if (!mInitialized)
    return false;

  if (!mNextEmailAddress.IsEmpty()) {
    if (mAddToAddress) {
      if (!mSendTo.IsEmpty())
        sendTo += ", ";
      sendTo += mNextEmailAddress;
    } else {
      sendTo = mNextEmailAddress;
    }
  }

  if (sendTo.IsEmpty()) {
    mWinApp->AppendToLog("Mailer: No address to send to has been defined",
      LOG_OPEN_IF_CLOSED);
    return false;
  }

   // Get the from address completed
  fromAddress = mMailFrom;
  if (fromAddress.Find("@") <= 0) {
    if (fromAddress.Find("@") < 0)
      fromAddress = CString("@") + fromAddress;
    username = getenv("USERNAME");
    if (username)
      fromAddress = CString(username) + fromAddress;
    else
      fromAddress = CString("SerialEM") + fromAddress;
  }

 // Create a TCP/IP socket, no specific protocol.
  hServer = socket(PF_INET, SOCK_STREAM, 0);
  if(hServer == INVALID_SOCKET) {
    mWinApp->AppendToLog("Mailer: Cannot open mail server socket", LOG_OPEN_IF_CLOSED);
    return false;
  }

  // Set the socket nonblocking and start connecting the Socket.
  ioctlsocket(hServer, FIONBIO, &iMode);
  retVal = connect(hServer, (PSOCKADDR)&mSockAddr, sizeof(mSockAddr));

  // Check for the expected return from a non-blocking connect attempt
  if (retVal != 0 && (retVal != SOCKET_ERROR ||  WSAGetLastError() != WSAEWOULDBLOCK) ) {
    mWinApp->AppendToLog("Mailer: Error connecting to Server socket",
      LOG_OPEN_IF_CLOSED);
    return false;
  }

  // Restore to blocking mode
  iMode = 0;
  ioctlsocket(hServer, FIONBIO, &iMode);

  // Check for write connection to socket with gievn timeout
  FD_ZERO(&write);
  FD_ZERO(&err);
  FD_SET(hServer, &write);
  FD_SET(hServer, &err);
  timeout.tv_sec = mConnectTimeout;
  timeout.tv_usec = 0;

  select(0, NULL, &write, &err, &timeout);
  if (!FD_ISSET(hServer, &write)) {
    report.Format("Mailer: Failed to connect to Server socket within %d seconds",
      mConnectTimeout);
    mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
    return false;
  }

  try {

    // Receive initial response from SMTP server.
    Check(recv(hServer, szBuffer, sizeof(szBuffer), 0), "recv() Reply");

    // Send HELO server.com
    sprintf(szMsgLine, "HELO %s%s", (LPCTSTR)mServer, CRLF);
    Check(send(hServer, szMsgLine, (int)strlen(szMsgLine), 0), "send() HELO");
    Check(recv(hServer, szBuffer, sizeof(szBuffer), 0), "recv() HELO");

    // Send MAIL FROM: <sender@mydomain.com>
    sprintf(szMsgLine, "MAIL FROM:<%s>%s", (LPCTSTR)fromAddress, CRLF);
    Check(send(hServer, szMsgLine, (int)strlen(szMsgLine), 0), "send() MAIL FROM");
    Check(recv(hServer, szBuffer, sizeof(szBuffer), 0), "recv() MAIL FROM");

    // Send RCPT TO: <receiver@domain.com>
    int curPos= 0;

    report = sendTo.Tokenize(", ",curPos);
    while (!report.IsEmpty()) {
      sprintf(szMsgLine, "RCPT TO:<%s>%s", (LPCTSTR)report, CRLF);
      Check(send(hServer, szMsgLine, (int)strlen(szMsgLine), 0), "send() RCPT TO");
      Check(recv(hServer, szBuffer, sizeof(szBuffer), 0), "recv() RCPT TO");
      report = sendTo.Tokenize(", ",curPos);
    }

    // Send DATA.
    sprintf(szMsgLine, "DATA%s", CRLF);
    Check(send(hServer, szMsgLine, (int)strlen(szMsgLine), 0), "send() DATA");
    Check(recv(hServer, szBuffer, sizeof(szBuffer), 0), "recv() DATA");

    sprintf(szMsgLine, "%s%s%s%s%s%s%s%s%s", "To: ", (LPCTSTR)sendTo, CRLF, "Subject: ",
      (LPCTSTR)subject, CRLF, CRLF, (LPCTSTR)message, CRLF);
    Check(send(hServer, szMsgLine, (int)strlen(szMsgLine), 0), "send() message-line");

    // Send blank line and a period.
    sprintf(szMsgLine, "%s.%s", CRLF, CRLF);
    Check(send(hServer, szMsgLine, (int)strlen(szMsgLine), 0), "send() end-message");
    Check(recv(hServer, szBuffer, sizeof(szBuffer), 0), "recv() end-message");

    acceptedForDelivery = (strncmp("250", szBuffer, B3DMIN(3, strlen(szBuffer))) == 0);

    // Send QUIT.
    sprintf(szMsgLine, "QUIT%s", CRLF);
    Check(send(hServer, szMsgLine, (int)strlen(szMsgLine), 0), "send() QUIT");
    Check(recv(hServer, szBuffer, sizeof(szBuffer), 0), "recv() QUIT");

    if (!acceptedForDelivery) {
      mWinApp->AppendToLog("Mailer: Mail not accepted for delivery", LOG_OPEN_IF_CLOSED);
      mWinApp->AppendToLog(subject, LOG_OPEN_IF_CLOSED);
      mWinApp->AppendToLog(message, LOG_OPEN_IF_CLOSED);
    }
  }
  catch (int) {
  }

  // Close server socket
  closesocket(hServer);

  return acceptedForDelivery;
}

// Basic error checking for send() and recv() functions.
void CMailer::Check(int iStatus, char *szFunction)
{
  CString report;
  if(iStatus != SOCKET_ERROR && iStatus != 0)
    return;
  report.Format("Error during call to %s: status %d - error %d", szFunction, iStatus,
    GetLastError());
  mWinApp->AppendToLog(report, LOG_OPEN_IF_CLOSED);
  throw 1;
}

