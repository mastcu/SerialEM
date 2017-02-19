#include "stdafx.h"
#include "SerialEM.h"

static char *TemErrStr(int result, char *buffer) {
 
  int location = result >> 10;
  int error = result & 0x3ff;
  char temp[100];


  switch (location) {
  case 0: sprintf(buffer, "No error"); break;
  case 1: sprintf(buffer, "TEM"); break;
  case 2: sprintf(buffer, "EOS"); break;
  case 3: sprintf(buffer, "Lens"); break;
  case 4: sprintf(buffer, "Def"); break;
  case 5: sprintf(buffer, "Screen"); break;
  case 6: sprintf(buffer, "Camera"); break;
  case 7: sprintf(buffer, "Gonio"); break;
  case 16: sprintf(buffer, "HT"); break;
  case 17: sprintf(buffer, "GUN"); break;
  case 18: sprintf(buffer, "FEG"); break;
  case 19: sprintf(buffer, "Filter"); break;
  case 20: sprintf(buffer, "Stage"); break;
  case 21: sprintf(buffer, "Detector"); break;
  case 22: sprintf(buffer, "Apt"); break;
  case 23: sprintf(buffer, "Scan"); break;
  case 24: sprintf(buffer, "MDS"); break;
  default: sprintf(buffer, "Unknown"); break;
  }

  sprintf(temp, " (%d) : ", location);
  strcat(buffer, temp);

  switch(error) {
 
  case 0: sprintf(temp, "No error"); break;
  case 1: sprintf(temp, "Parameter error"); break;
  case 3: sprintf(temp, "Class is busy"); break; 
  case 10: sprintf(temp, "Timeout"); break;
  case 11: sprintf(temp, "Memory error"); break;
  case 12: sprintf(temp, "NC"); break;
  case 13: sprintf(temp, "EM error"); break;
  case 14: sprintf(temp, "Packet error"); break;
  case 16: sprintf(temp, "GUN type error"); break;
  case 100: sprintf(temp, "Internal parameter error"); break;
  default: sprintf(temp, "Unknown"); break;
  }

  strcat(buffer, temp);
  sprintf(temp, " (%d)", error);
  strcat(buffer, temp);
  return buffer;
}

// This wrapper function takes the result from a JEOL call and posts an error string
// if debug output is on, or logs the call itself if J output is on
short JeolWrap2(short result, char *method = 0) {
  static int serial = 0;
  serial++;
  if (result) {
    char buffer[1024];
    char buffer2[1024];
    TemErrStr(result,buffer2);
    sprintf(buffer,"JEOL call %d returned error status %d [\"%s\"] (%s)\r\n",serial,
      result,buffer2,method ? method : "Unknown method");  
    SEMTrace('1', buffer);
    TRACE(buffer);
  } else
    SEMTrace('J', "JEOL call %d OK (%s)", serial, method ? method : "Unknown method");
  return result;
}
