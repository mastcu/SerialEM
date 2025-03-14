/*
 *  istore.h -- General storage element header file.
 *
 *  Author: David Mastronarde   email: mast@colorado.edu
 *
 *  Copyright (C) 1995-2004 by Boulder Laboratory for 3-Dimensional Electron
 *  Microscopy of Cells ("BL3DEMC") and the Regents of the University of 
 *  Colorado.  See dist/COPYRIGHT for full copyright notice.
 *
 * $Id$
 */

#ifndef ISTORE_H
#define ISTORE_H

#include "ilist.h"
#include "imodel.h"

/* DOC_SECTION DEFINES */
/* DOC_CODE Istore definitions */
/* Bits 0-1 of flags have one of these values to indicate type of index, and
   bits 2-3 have one of these values to indicate type of value */
#define GEN_STORE_INT   0
#define GEN_STORE_FLOAT 1
#define GEN_STORE_SHORT 2
#define GEN_STORE_BYTE  3

/* Additional defined flags for general storage */
#define GEN_STORE_NOINDEX  (1l << 4)  /* Index is not a sortable value */
#define GEN_STORE_REVERT   (1l << 5)  /* Revert to default values */
#define GEN_STORE_SURFACE  (1l << 6)  /* Index is a surface # */
#define GEN_STORE_ONEPOINT (1l << 7)  /* Type applies to one point */
#define ISO_STORE_CAP      (1l << 5)
#define ISO_STORE_DELETE   (1l << 6)
#define ISO_STORE_HAS_OUTER (1l << 7)

/* Defined values for type */
#define GEN_STORE_COLOR   1    /* Color change */
#define GEN_STORE_FCOLOR  2    /* Fill color change */
#define GEN_STORE_TRANS   3    /* Transparency change */
#define GEN_STORE_GAP     4    /* Do not connect to next point */
#define GEN_STORE_CONNECT 5    /* A connection number for meshing */
#define GEN_STORE_3DWIDTH 6    /* 3D line width change */
#define GEN_STORE_2DWIDTH 7    /* 2D line width change */
#define GEN_STORE_SYMTYPE 8    /* Symbol type (including open/closed) */
#define GEN_STORE_SYMSIZE 9    /* Symbol size */
#define GEN_STORE_VALUE1  10   /* Arbitrary value */
#define GEN_STORE_MINMAX1 11   /* Min and max of value1 */
#define GEN_STORE_VALUE2  12   /* Arbitrary value 2 */
#define GEN_STORE_MINMAX2 13   /* Min and max of value2 */
#define GEN_STORE_VALUE3  14   /* Arbitrary value 3 */
#define GEN_STORE_MINMAX3 15   /* Min and max of value3 */
#define GEN_STORE_VALUE4  16   /* Arbitrary value 4 */
#define GEN_STORE_MINMAX4 17   /* Min and max of value4 */
#define GEN_STORE_VALUE5  18   /* Arbitrary value 5 */
#define GEN_STORE_MINMAX5 19   /* Min and max of value5 */
#define GEN_STORE_VALUE6  20   /* Arbitrary value 6 */
#define GEN_STORE_MINMAX6 21   /* Min and max of value6 */
#define ISO_STORE_PARAMS  22   /* Model isosurface: flags, bin, smooth, sigma, etc. */
#define ISO_STORE_THRESH  23   /* Model isosurface: thresholds by time index */
#define GEN_STORE_NO_CAP  24   /* Do not cap contour */

/* Defined flags for indicating changes */
#define CHANGED_COLOR     (1l << 0)    /* Color change */
#define CHANGED_FCOLOR    (1l << 1)    /* Fill color change */
#define CHANGED_TRANS     (1l << 2)    /* Transparency change */
#define CHANGED_GAP       (1l << 3)    /* Do not connect to next point */
#define CHANGED_CONNECT   (1l << 4)    /* A connection number for meshing */
#define CHANGED_3DWIDTH   (1l << 5)    /* 3D line width change */
#define CHANGED_2DWIDTH   (1l << 6)    /* 2D line width change */
#define CHANGED_SYMTYPE   (1l << 7)    /* Symbol type */
#define CHANGED_SYMSIZE   (1l << 8)    /* Symbol size */
#define CHANGED_VALUE1    (1l << 9)    /* Arbitrary value */

/* Indexes for isosurface items in the value entry */
#define ISO_STORE_BIN_IND     0
#define ISO_STORE_SMOOTH_IND  1
#define ISO_STORE_OUTER_IND   2
#define ISO_STORE_SIGMA_IND   0
#define ISO_STORE_MINSIZE_IND 1

/* A very handy macro */                          
#define istoreItem(list, index) ((Istore *)ilistItem(list, index))

/* END_CODE */
/* END_SECTION */

/* DOC_CODE StoreUnion union */
/* Union of types for the general storage structure Istore */
typedef union store_type {
  b3dInt32 i;
  b3dFloat f;
  b3dUInt16 us[2];
  b3dInt16 s[2];
  b3dUByte b[4];
} StoreUnion;
/* END_CODE */

/* DOC_CODE Istore structure */
/* The storage structure */
typedef struct Mod_Store 
{
  b3dInt16 type;            /* Type of information */
  b3dUInt16 flags;          /* Flags for data types, modifiers */
  StoreUnion index;         /* Item index in simplest usage */
  StoreUnion value;         /* Item value in simplest usage */
} Istore;
/* END_CODE */

/* DOC_CODE DrawProps structure */
/* The drawing property structure */
typedef struct draw_properties
{
  float red, green, blue;            /* Keep as floats so they are GL-ready */
  float fillRed, fillGreen, fillBlue;
  int trans;
  int connect;
  int gap;
  int linewidth;
  int linewidth2;
  int symtype;
  int symflags;
  int symsize;
  float value1;
  int valskip;    /* Flag that gap is set because of skipping lo/hi values */
  int noCap;
} DrawProps;
/* END_CODE */

#ifdef __cplusplus
extern "C" {
#endif

  int imodWriteStore(Ilist *list, int id, FILE *fout);
  Ilist *imodReadStore(FILE *fin, int *error);
  void istoreSort(Ilist *list);
  int istoreInsert(Ilist **listp, Istore *store);
  int istoreLookup(Ilist *list, int index, int *after);
  void istoreDump(Ilist *list);
  double istoreChecksum(Ilist *list);
  int istoreBreakChanges(Ilist *list, int index, int psize);
  int istoreFindBreak(Ilist *list, int index);
  void istoreShiftIndex(Ilist *list, int ptIndex, int startScan, int amount);
  int istoreDeletePoint(Ilist *list, int index, int psize);
  void istoreDeleteContSurf(Ilist *list, int index, int surfFlag);
  int istoreBreakContour(Icont *cont, Icont *ncont, int p1, int p2);
  int istoreInvert(Ilist **listp, int psize);
  int istoreExtractChanges(Ilist *olist, Ilist **nlistp, int indStart, 
                           int indEnd, int newStart, int psize);
  int istoreCopyNonIndex(Ilist *olist, Ilist **nlistp);
  int istoreCopyContSurfItems(Ilist *olist, Ilist **nlistp, int indFrom,
                            int indTo, int surfFlag);
  int istoreCountContSurfItems(Ilist *list, int index, int surfFlag);
  Istore *istoreNextObjItem(Ilist *list, int co, int surf, int first);
  int istoreInsertChange(Ilist **listp, Istore *store);
  int istoreEndChange(Ilist *list, int type, int index);
  int istoreClearChange(Ilist *list, int type, int index);
  void istoreClearRange(Ilist *list, int type, int start, int end);
  void istoreCleanEnds(Ilist *list);
  int istoreAddOneIndexItem(Ilist **listp, Istore *store);
  int istoreClearOneIndexItem(Ilist *list, int type, int index, int surfFlag);

  void istoreDefaultDrawProps(Iobj *obj, DrawProps *props);
  int istoreContSurfDrawProps(Ilist *list, DrawProps *defProps, 
                              DrawProps *contProps, int co, int surf, 
                              int *contState, int *surfState);
  int istoreListPointProps(Ilist *list, DrawProps *contProps,
                           DrawProps *ptProps, int pt);
  int istoreFirstChangeIndex(Ilist *list);
  int istoreNextChange(Ilist *list, DrawProps *defProps,
                       DrawProps *ptProps, int *stateFlags, int *changeFlags);
  int istorePointDrawProps(Iobj *obj, DrawProps *contProps, DrawProps *ptProps,
                           int co, int pt);
  int istoreCountItems(Ilist *list, int type, int stop);
  int istoreCountObjectItems(Iobj *obj, int type, int doCont, int doMesh,
                               int stop);
  int istoreTransStateMatches(Ilist *list, int state);
  int istoreRetainPoint(Ilist *list, int index);
  int istoreGenerateItems(Ilist **listp, DrawProps *props, int flags, 
                          int index, int genFlags);
  int istoreGenPointItems(Ilist *clist, DrawProps *contProps, int contState, 
                          int ptInd, Ilist **mlistp, int meshInd, 
                          int genFlags);
  int istorePointIsGap(Ilist *list, int index);
  int istoreConnectNumber(Ilist *list, int index);
  int istoreSkipToIndex(Ilist *list, int index);
  int istoreAddMinMax(Ilist **list, int type, float min, float max);
  int istoreGetMinMax(Ilist *list, int size, int type, float *min, float *max);
  int istoreFindAddMinMax1(Iobj *obj);
  int istoreFindAddMinMax(Iobj *obj, int type);

#ifdef __cplusplus
}
#endif

#endif
