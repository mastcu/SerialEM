/*
 *  mxmlwrap.h - headers for mxmlwrap index-based calls
 *
 *  Author: David Mastronarde   email: mast@colorado.edu
 */

#ifndef MXMLWRAP_H
#define MXML_WRAP_H

#ifdef __cplusplus
extern "C" {
#endif

  int ixmlReadFile(const char *filename, int withXMLdecl, char **rootElement);
  int ixmlLoadString(const char *string, int withXMLdecl, char **rootElement);
  int ixmlNewNodeList(const char *rootElement);
  int ixmlWriteFile(int xmlInd, const char *filename);
  int ixmlFindElements(int xmlInd, int nodeInd, const char *tag, int *foundInd, 
                       int *numFound);
  int ixmlGetStringValue(int xmlInd, int nodeInd, char **string);
  int ixmlGetIntegerValue(int xmlInd, int nodeInd, int *val);
  int ixmlGetFloatValue(int xmlInd, int nodeInd, float *val);
  int ixmlGetDoubleValue(int xmlInd, int nodeInd, double *val);
  int ixmlGetStringAttribute(int xmlInd, int nodeInd, const char *name, char **string);
  int ixmlGetIntegerAttribute(int xmlInd, int nodeInd, const char *name, int *val);
  int ixmlAddElement(int xmlInd, int nodeInd, const char *tag);
  int ixmlSetStringValue(int xmlInd, int nodeInd, const char *string);
  int ixmlSetIntegerValue(int xmlInd, int nodeInd, int val);
  int ixmlSetFloatValue(int xmlInd, int nodeInd, float val);
  int ixmlSetDoubleValue(int xmlInd, int nodeInd, double val);
  int ixmlAddStringAttribute(int xmlInd, int nodeInd, const char *name, 
                             const char *value);
  int ixmlAddIntegerAttribute(int xmlInd, int nodeInd, const char *name, int value);
  int ixmlPopListIndexes(int xmlInd, int firstInd);
  void ixmlClear(int index);
  const char *ixmlWhitespace_cb(void *nodeVoid, int where);
  void ixmlResetLastLevel();

#ifdef __cplusplus
}
#endif
#endif
