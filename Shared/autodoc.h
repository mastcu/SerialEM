/*
 * autodoc.h - Header for autodoc.c in libimod
 *
 * Copyright (C) 2006 by Boulder Laboratory for 3-Dimensional Electron
 * Microscopy of Cells ("BL3DEMC") and the Regents of the University of 
 * Colorado.  See dist/COPYRIGHT for full notice.
 *
 * $Id$
 */                                                                           

#ifndef AUTODOC_H
#define AUTODOC_H

#include <stdio.h>

#define ADOC_GLOBAL_NAME "PreData"
#define ADOC_ZVALUE_NAME "ZValue"

#ifdef __cplusplus
extern "C" {
#endif

enum {ADOC_NO_VALUE = 0, ADOC_ONE_INT, ADOC_TWO_INTS, ADOC_THREE_INTS, ADOC_INT_ARRAY,
      ADOC_ONE_FLOAT, ADOC_TWO_FLOATS, ADOC_THREE_FLOATS, ADOC_FLOAT_ARRAY, ADOC_STRING,
      ADOC_ONE_DOUBLE};
int AdocRead(const char *filename);
int AdocOpenImageMetadata(const char *filename, int addMdoc, int *montage,
                          int *numSect, int *sectType);
int AdocGetImageMetaInfo(int *montage, int *numSect, int *sectType);
int AdocNew();
int AdocGetCurrentIndex();
int AdocSetCurrent(int index);
void AdocClear(int index);
void AdocDone();
int AdocWrite(const char *filename);
void AdocRetryWriteOpens(int num);
int AdocPrintToString(char *string, int stringSize, int writeAll);
int AdocAppendSection(const char *filename);
int AdocOrderWriteByValue(const char *typeName);
int AdocAddSection(const char *collName, const char *name);
int AdocInsertSection(const char *typeName, int sectInd, const char *name);
int AdocDeleteSection(const char *typeName, int sectInd);
int AdocChangeSectionName(const char *typeName, int sectInd, const char *newName);
int AdocLookupSection(const char *typeName, const char *name);
int AdocLookupByNameValue(const char *typeName, int nameValue);
int AdocFindInsertIndex(const char *typeName, int nameValue);
int AdocTransferSection(const char *typeName, int sectInd, int toAdocInd, 
                        const char *newName, int byValue);
int AdocSetKeyValue(const char *collName, int sectInd, const char *key, 
                    const char *value);
int AdocDeleteKeyValue(const char *collName, int sectInd, const char *key);
int AdocGetNumCollections();
int AdocGetCollectionName(int collInd, char **string);
int AdocGetSectionName(const char *collName, int sectInd, char **string);
int AdocGetNumberOfSections(const char *collName);
int AdocGetNumberOfKeys(const char *typeName, int sectInd);
int AdocGetKeyByIndex(const char *typeName, int sectInd, int keyInd, char **key);
int AdocGetValTypeAndSize(const char *typeName, int sectInd, const char *key, 
                          int *valType, int *numTokens);
int AdocGetString(const char *collName, int sectInd, const char *key, char **string);
int AdocGetInteger(const char *collName, int sectInd, const char *key, int *val1);
int AdocGetTwoIntegers(const char *collName, int sectInd, const char *key, int *val1,
                       int *val2);
int AdocGetThreeIntegers(const char *collName, int sectInd, const char *key, int *val1,
                         int *val2, int *val3);
int AdocGetIntegerArray(const char *collName, int sectInd, const char *key, int *array,
                        int *numToGet, int arraySize);
int AdocGetFloat(const char *collName, int sectInd, const char *key, float *val1);
int AdocGetDouble(const char *collName, int sectInd, const char *key, double *val1);
int AdocGetTwoFloats(const char *collName, int sectInd, const char *key, float *val1,
                     float *val2);
int AdocGetThreeFloats(const char *collName, int sectInd, const char *key, float *val1,
                       float *val2, float *val3);
int AdocGetFloatArray(const char *collName, int sectInd, const char *key, float *array,
                      int *numToGet, int arraySize);
int AdocSetInteger(const char *typeName, int sectInd, const char *key, int ival);
int AdocSetTwoIntegers(const char *typeName, int sectInd, const char *key, int ival1, 
                       int ival2);
int AdocSetThreeIntegers(const char *typeName, int sectInd, const char *key, int ival1,
                         int ival2, int ival3);
int AdocSetIntegerArray(const char *typeName, int sectInd, const char *key, int *ivals,
                        int numVals);
int AdocSetFloat(const char *typeName, int sectInd, const char *key, float val);
int AdocSetDouble(const char *typeName, int sectInd, const char *key, double val);
int AdocSetTwoFloats(const char *typeName, int sectInd, const char *key, float val1, 
                     float val2);
int AdocSetThreeFloats(const char *typeName, int sectInd, const char *key, float val1,
                       float val2, float val3);
int AdocSetFloatArray(const char *typeName, int sectInd, const char *key, float *vals,
                      int numVals);
int AdocWriteInteger(FILE *fp, const char *key, int ival);
int AdocWriteTwoIntegers(FILE *fp, const char *key, int ival1, int ival2);
int AdocWriteThreeIntegers(FILE *fp, const char *key, int ival1, int ival2, int ival3);
int AdocWriteIntegerArray(FILE *fp, const char *key, int *ivals, int numVals);
int AdocWriteFloat(FILE *fp, const char *key, float val);
int AdocWriteDouble(FILE *fp, const char *key, double val);
int AdocWriteTwoFloats(FILE *fp, const char *key, float val1, float val2);
int AdocWriteThreeFloats(FILE *fp, const char *key, float val1, float val2, float val3);
int AdocWriteFloatArray(FILE *fp, const char *key, float *vals, int numVals);
int AdocWriteKeyValue(FILE *fp, const char *key, const char *value);
int AdocWriteSectionStart(FILE *fp, const char *key, const char *value);
void AdocSetWriteAsXML(int asXML);
int AdocGetWriteAsXML();
int AdocXmlReadStatus(int *sectNotElem, int *sectNoName, int *childNotElem,
                      int *childAttribs, int *valueNotText, int *multipleChilds);
int AdocGetXmlRootElement(char **string);
int AdocSetXmlRootElement(const char *element);

#ifdef __cplusplus
}
#endif
#endif
