#pragma once

class CDoseModulator
{
public:
  CDoseModulator();
  ~CDoseModulator();
  int Initialize(CString &errStr);
  int SetDutyPercent(float pct, CString &errStr);
  int GetDutyPercent(float & pct, CString & errStr);
  GetMember(float, LastDutyPctSet);
  float GetRecentDutyPercent();

 private:
  int RunCommand(const char *urlOpt, const char *data, CString &errStr, CString &output);
  bool mIsInitialized;
  CSerialEMApp *mWinApp;
  int mLastFrequencySet;
  float mLastDutyPctSet;
  double mDutyPercentTime;

};
