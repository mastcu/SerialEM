#ifndef FILEOPTIONS_H
#define FILEOPTIONS_H

#include "MontageParam.h"
#include "EMimageExtra.h"
class KImageStore;

#define TRUNCATE_UNSIGNED 0
#define DIVIDE_UNSIGNED 1
#define SHIFT_UNSIGNED 2
#define TRUNCATE_SIGNED 0
#define SHIFT_SIGNED 1
#define COMPRESS_NONE 1
#define COMPRESS_LZW 5
#define COMPRESS_JPEG 7
#define COMPRESS_ZIP 8
#define COPY_FROM_MASTER 0
#define COPY_TO_MASTER 1
#define SIGNED_SHIFTED 3
#define OPEN_TS_EXT ".openTS"

// File option structure
// Remember to initialize new values in GainRefMaker if necessary
struct  FileOptions {
  int mode;
  int typext;
  int maxSec;
  BOOL useMdoc;
  BOOL montageInMdoc;
  float pctTruncLo;
  float pctTruncHi;
  int unsignOpt;
  int signToUnsignOpt;
  int fileType;
  int compression;
  int hdfCompression;
  int jpegQuality;
  int TIFFallowed;
  BOOL separateForMont;
  BOOL montUseMdoc;
  BOOL leaveExistingMdoc;
  int montFileType;
  int refCount;
  int navID;
  BOOL useMont(void) {return separateForMont && ((typext & MONTAGE_MASK) || montageInMdoc);};
  BOOL isMontage(void) {return (typext & MONTAGE_MASK) || montageInMdoc;};
};

struct StoreData
{
  KImageStore *store;       // The KStore item
  FileOptions fileOpt;   // File options
  MontParam *montParam;   // Pointer to montage params or NULL
  bool montage;           // flag if montaging
  int protectNum;         // Original file index if protected, -1 if not
  int saveOnNewMap;       // Flag to autosave when new map (-1 not asked yet)
};

#endif
