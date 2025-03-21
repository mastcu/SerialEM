/*
 *  mrcfiles.h
 *
 *  Original author: James Kremer
 *  Revised by: David Mastronarde   email: mast@colorado.edu
 *
 *  Copyright (C) 1995-2005 by Boulder Laboratory for 3-Dimensional Electron
 *  Microscopy of Cells ("BL3DEMC") and the Regents of the University of 
 *  Colorado.  See dist/COPYRIGHT for full copyright notice.
 *
 *  $Id$
 */

#ifndef MRCFILES_H
#define MRCFILES_H

#include <stdio.h>
#include <stdlib.h>
#include "hvemtypes.h"
#include "imodconfig.h"

#ifndef FALSE
#define FALSE       0           /*false for boolean*/
#endif
#ifndef TRUE
#define TRUE        1           /*true for boolean*/
#endif

#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif



#define MRC_IDTYPE_MONO   0
#define MRC_IDTYPE_TILT   1
#define MRC_IDTYPE_TILTS  2
#define MRC_IDTYPE_LINA   3
#define MRC_IDTYPE_LINS   4 

#define MRC_SCALE_LINEAR  1
#define MRC_SCALE_POWER   2
#define MRC_SCALE_LOG     3
#define MRC_SCALE_BKG     4

/* DOC_CODE MRC data modes */
/* The modes defined for MRC files in IMOD */
#define MRC_MODE_BYTE          0
#define MRC_MODE_SHORT         1
#define MRC_MODE_FLOAT         2
#define MRC_MODE_COMPLEX_SHORT 3
#define MRC_MODE_COMPLEX_FLOAT 4
#define MRC_MODE_USHORT        6
#define MRC_MODE_HALF_FLOAT    12
#define MRC_MODE_RGB           16
#define MRC_MODE_4BIT          101
/* END_CODE */

#define MRC_EXT_TYPE_NONE      0
#define MRC_EXT_TYPE_SERI      1
#define MRC_EXT_TYPE_AGAR      2
#define MRC_EXT_TYPE_FEI       3
#define MRC_EXT_TYPE_UNKNOWN   4

#define PACKED_4BIT_MODE       1
#define PACKED_HALF_XSIZE      2

#define MRC_RAMP_LIN 1
#define MRC_RAMP_EXP 2
#define MRC_RAMP_LOG 3


#define MRC_LABEL_SIZE         80
#define MRC_NEXTRA             16
#define MRC_NLABELS            10
#define MRC_HEADER_SIZE        1024   /* Length of Header is 1024 Bytes. */
#define MRC_MAXCSIZE           3

#define IIUNIT_SWAPPED       (1l << 0)
#define IIUNIT_BYTES_SIGNED  (1l << 1)
#define IIUNIT_OLD_STYLE     (1l << 2)
#define IIUNIT_NINT_BUG      (1l << 3)
#define IIUNIT_BAD_MAPCRS    (1l << 4)
#define IIUNIT_4BIT_MODE     (1l << 5)
#define IIUNIT_HALF_XSIZE    (1l << 6)
#define IIUNIT_Y_INVERTED    (1l << 7)
#define IIUNIT_HALF_FLOATS   (1l << 8)

typedef struct  /*complex floating number*/
{
  b3dFloat a;
  b3dFloat b;

} ComplexFloat;

typedef struct  /*complex short number*/
{
  b3dInt16 a;
  b3dInt16 b;

} ComplexShort;

/* DOC_CODE MrcHeader structure */
/* The header structure for MRC files */
typedef struct MRCheader
{
  b3dInt32   nx;         /*  # of Columns                  */
  b3dInt32   ny;         /*  # of Rows                     */
  b3dInt32   nz;         /*  # of Sections.                */
  b3dInt32   mode;       /*  given by #define MRC_MODE...  */

  b3dInt32   nxstart;    /*  Starting point of sub image.  UNSUPPORTED */
  b3dInt32   nystart;
  b3dInt32   nzstart;

  b3dInt32   mx;         /* "Grid size", # of pixels in "unit cell"    */
  b3dInt32   my;         /* Keep the same as nx, ny, nz                */
  b3dInt32   mz;

  b3dFloat   xlen;       /* length of unit cell in Angstroms           */
  b3dFloat   ylen;       /* get scale = xlen/nx ...                    */
  b3dFloat   zlen;

  b3dFloat   alpha;      /* cell angles, ignored, set to 90            */
  b3dFloat   beta;
  b3dFloat   gamma;

  b3dInt32   mapc;       /* map column  1=x,2=y,3=z.     UNSUPPORTED  */
  b3dInt32   mapr;       /* map row     1=x,2=y,3=z.                  */
  b3dInt32   maps;       /* map section 1=x,2=y,3=z.                  */

  b3dFloat   amin;
  b3dFloat   amax;
  b3dFloat   amean;
  
  /* 1/12/12: Removed nsymbt and made ispg be 4 bytes to match standard */
  b3dInt32   ispg;       /* space group number in the standard */

  /* 64 bytes */

  b3dInt32   next;     /* This is nsymbt in the MRC standard */
  b3dInt16   creatid;  /* Used to be creator id, hvem = 1000, now 0 */

  b3dByte    blank[6];
  b3dByte    extType[4]; /* Extended type */
  b3dInt32   nversion;  /* Version number in MRC 2014 standard */
  b3dByte    blank2[16];
  
  b3dInt16   nint;
  b3dInt16   nreal;
  b3dInt16   sub;
  b3dInt16   zfac;

  b3dFloat   min2;
  b3dFloat   max2;
  b3dFloat   min3;
  b3dFloat   max3;
  b3dInt32   imodStamp;
  b3dInt32   imodFlags;

  /*  UINT   extra[MRC_NEXTRA];*/

  /* HVEM extra data */
  /* DNM 3/16/01: divide idtype into two shorts */
  b3dInt16   idtype;
  b3dInt16   lens;
  b3dInt16   nd1;     /* Devide by 100 to get float value. */
  b3dInt16   nd2;
  b3dInt16   vd1;
  b3dInt16   vd2;
  b3dFloat   tiltangles[6];  /* 0,1,2 = original:  3,4,5 = current */

#ifdef OLD_STYLE_HEADER
  /* before 2.6.20 */
  /* DNM 3/16/01: redefine the last three floats as wavelength numbers */
  b3dInt16   nwave;   /* # of wavelengths and values */
  b3dInt16   wave1;
  b3dInt16   wave2;
  b3dInt16   wave3;
  b3dInt16   wave4;
  b3dInt16   wave5;
  b3dFloat   zorg;           /* origin */
  
  b3dFloat   xorg;
  b3dFloat   yorg;
#else
  /* MRC 2000 standard */
  b3dFloat   xorg;
  b3dFloat   yorg;
  b3dFloat   zorg;
  b3dByte    cmap[4];
  b3dByte    stamp[4];
  b3dFloat   rms;
#endif

  b3dInt32 nlabl;
  b3dByte  labels[MRC_NLABELS][MRC_LABEL_SIZE + 1];

  /* Internal data not stored in file header */
  b3dUByte *symops;
  FILE   *fp;
  int    pos;
  struct LoadInfo *li;
  int    headerSize;
  int    sectionSkip;
  int    swapped;
  int    bytesSigned;
  int    yInverted;
  int    iiuFlags;
  int    packed4bits;
  int    halfFloats;

  char *pathname;
  char *filedesc;
  char *userData;
} MrcHeader;
/* END_CODE */


/* to get from index to model coords -> scale -> subtract org -> rotate */
/* rotate by current tilt angles ? */

/* DOC_CODE IloadInfo structure */
/*
 * Used to control loading a subsection of a 3-D data file. 
 */
typedef struct LoadInfo
{

  /* Sub area to load. */
  int xmin;      
  int xmax;
  int ymin;
  int ymax;
  int zmin;
  int zmax;
  int padLeft;     /* Padding at start or end of line when reading or writing a */
  int padRight;    /* subset of the array, in pixels not bytes */
  
  int ramp;      /* Contrast ramp type. */
  int scale;
  int black;     /* Change contrast values. */
  int white;
  
  int axis;      /* 1=x, 2=y, 3=z , 0 = default */
  
  float slope;         /* Scale input by value * slope + offset */
  float offset;
  float smin,smax;     /* Scale range smin-smax in input to 0-255 */
  int contig;          /* Load idata in contigous memory if possible */
  
  int outmin, outmax;  /* clamp values to outmin and outmax after scaling. */
  int mirrorFFT;       /* Return mirrored FFT when scaling to bytes */
  
  int   plist;         /* Size of piece list.         */
  float opx, opy, opz; /* origin of pieces.           */
  float px, py, pz;    /* size of final pieced image. */
  int   pdz;           /* number of zsections that have data. */
  int *pcoords;        /* The piece list.             */

} IloadInfo;
/* END_CODE */


struct TiltInfo
{

  float *tilt;
  float  axis_x;
  float  axis_y;
  float  axis_z;
  float  scale_x;
  float  scale_y;
  float  scale_z;
  float  alpha;
  float  beta;
  float  gamma;

};

typedef unsigned short imnp_uint16;
typedef unsigned int imnp_uint32;

union FloatBits
{
  float f;
  imnp_uint32 fbits;
};

#ifdef __cplusplus
extern "C" {
#endif


/******************************** Header functions **************************/
int mrc_head_read (FILE *fin,  MrcHeader *hdata);
int mrc_head_write(FILE *fout, MrcHeader *hdata);
void mrcInitOutputHeader(MrcHeader *hdata);
int mrcCopyExtraHeader(MrcHeader *hin, MrcHeader *hout);
int mrcReadExtraHeader(MrcHeader *hin, unsigned char **extData);
int mrcWriteExtraHeader(MrcHeader *hout, unsigned char *extData, int next);
void mrcCopyValidExtendedType(MrcHeader *hin, MrcHeader *hout);
int mrc_head_label(MrcHeader *hdata, const char *label);
void mrcFillLabelString(const char *label, void *outLabel);
int mrcPrintLabelString(MrcHeader *hdata, int labelInd);
int mrc_head_new  (MrcHeader *hdata, int x, int y, int z, int mode);
int mrc_byte_mmm  (MrcHeader *hdata, unsigned char **idata);
int mrc_head_label_cp(MrcHeader *hin, MrcHeader *hout);
int mrc_test_size(MrcHeader *hdata);
int mrcGetStandardVersion(MrcHeader *hdata);
void fixTitlePadding(char *label);
int sizeCanBe4BitK2SuperRes(int nx, int ny);
int mrcGetExtendedType(MrcHeader *hdata, int *version);

void mrc_get_scale(MrcHeader *h, float *xs, float *ys, float *zs);
void mrc_set_scale(MrcHeader *h, double x, double y, double z);
void mrc_coord_cp(MrcHeader *hout, MrcHeader *hin);
	  

/************************* Write image data functions ************************/
int mrc_write_byte (FILE *fout, MrcHeader *hdata, unsigned char **data);
int mrc_write_idata(FILE *fout, MrcHeader *hdata, void *data[]);
int mrc_data_new   (FILE *fout, MrcHeader *hdata);
int mrc_write_slice(void *buf, FILE *fout, MrcHeader *hdata, 
		    int slice, char axis);
int parallelWriteSlice(void *buf, FILE *fout, MrcHeader *hdata, int slice);
int mrcWriteFFT(const char *filename, float *fft, int nxReal, int nyReal, int ifScale);

/************************ Read image data functions **************************/
void *mrc_mread_slice(FILE *fin, MrcHeader *hdata,
		      int slice, char axis);
int mrc_read_slice(void *buf, FILE *fin, MrcHeader *hdata, 
		   int slice, char axis);
int mrcReadFloatSlice(b3dFloat *buf, MrcHeader *hdata, int slice);

  unsigned char **mrcGetDataMemory(IloadInfo *li, size_t xysize,
                                   int zsize, int pixsize);
  void mrcFreeDataMemory(unsigned char **idata, int contig, int zsize);
  float mrcGetComplexScale();
  void mrcComplexSminSmax(float inMin, float inMax, float *outMin, 
                           float *outMax);
  void mrcMirrorSource(int nx, int ny, int imageX, int imageY, int *fileX,
                       int *fileY);
void mrcContrastScaling(MrcHeader *hdata, float smin, float smax, int black,
                        int white, int ramptype, float *slope, float *offset);

unsigned char **read_mrc_byte(FILE *fin, MrcHeader *hdata, 
			      IloadInfo *li);
unsigned char **mrc_read_byte(FILE *fin, MrcHeader *hdata, 
			      IloadInfo *li,
			      void (*func)(const char *));

  /* Functions in mrcsec.c */
int mrcReadSectionByte(MrcHeader *hdata, IloadInfo *li, unsigned char *buf, int z);
int mrcReadZByte(MrcHeader *hdata, IloadInfo *li, unsigned char *buf, int z);
int mrcReadYByte(MrcHeader *hdata, IloadInfo *li, unsigned char *buf, int y);
int mrcReadSectionUShort(MrcHeader *hdata, IloadInfo *li, unsigned char *buf, int z);
int mrcReadZUShort(MrcHeader *hdata, IloadInfo *li, unsigned char *buf, int z);
int mrcReadYUShort(MrcHeader *hdata, IloadInfo *li, unsigned char *buf, int y);
int mrcReadZ(MrcHeader *hdata, IloadInfo *li, unsigned char *buf, int cz);
int mrcReadY(MrcHeader *hdata, IloadInfo *li, unsigned char *buf, int cy);
int mrcReadSection(MrcHeader *hdata, IloadInfo *li, unsigned char *buf, int z);
int mrcReadSectionFloat(MrcHeader *hdata, IloadInfo *li, b3dFloat *buf, int z);
int mrcReadYFloat(MrcHeader *hdata, IloadInfo *li, b3dFloat *buf, int z);
int mrcReadZFloat(MrcHeader *hdata, IloadInfo *li, b3dFloat *buf, int z);
int mrcWriteZ(MrcHeader *hdata, IloadInfo *li, unsigned char *buf, int z);
int mrcWriteZFloat(MrcHeader *hdata, IloadInfo *li, b3dFloat *buf, int z);

/* misc stdio functions */
int  loadtilts(struct TiltInfo *ti, MrcHeader *hdata);
int  getloadinfo(MrcHeader *hdata,  IloadInfo *li); 
int  mrc_init_li(IloadInfo *li, MrcHeader *hd);
int  mrc_plist_li(IloadInfo *li, MrcHeader *hdata, const char *fname);
int  mrc_plist_load(IloadInfo *li, MrcHeader *hdata, FILE *fin);
int  mrc_plist_proc(IloadInfo *li, int nx, int ny, int nz);
int  mrc_plist_create(IloadInfo *li, int nx, int ny, int nz, int nfx, 
		      int nfy, int ovx, int ovy);
int  iiPlistLoadF(FILE *fin, IloadInfo *li, int nx, int ny, int nz);
int  iiPlistLoad(const char *filename, IloadInfo *li, int nx, int ny, int nz);
int iiPlistFromMetadata(const char *filename, int addMdoc, IloadInfo *li, int nx, 
                        int ny, int nz);
int iiPlistFromAutodoc(int adocIndex, int clearOnDone,  IloadInfo *li, int nx, int ny, 
                       int nz, int montage, int numSect, int sectType);
void iiPlistSetAdocCoordType(int value);
void mrc_liso(MrcHeader *hdata, IloadInfo *li);
int mrc_fix_li(IloadInfo *li, int nx, int ny, int nz);
int get_loadinfo(MrcHeader *hdata, IloadInfo *li);
unsigned char *get_byte_map(float slope, float offset, int outmin, int outmax, 
                            int bytesSigned);
unsigned char *get_short_map(float slope, float offset, int outmin, int outmax,
			     int ramptype, int swapbytes, int signedint);


/************************ Internal functions *********************************/
int getfilename(char *name, char *prompt);
void mrc_default_status(const char *string);
int mrc_getdcsize(int mode, int *dsize, int *csize);
void mrc_swap_shorts(b3dInt16 *data, int amt);
void mrc_swap_longs(b3dInt32 *data, int amt);
void mrc_swap_floats(b3dFloat *data, int amt);
void mrc_swap_header(MrcHeader *hdata);
void mrc_set_cmap_stamp(MrcHeader *hdata);
void imnp_halfbuf_to_floats(void *halfBuf, float *floatBuf, int numVals);
void imnp_floatbuf_to_halfs(float *floatBuf, void *halfBuf, int numVals);
imnp_uint16 imnp_floatbits_to_halfbits(imnp_uint32 f);
imnp_uint32 imnp_halfbits_to_floatbits(imnp_uint16 h);

#ifdef __cplusplus
}
#endif
#endif
