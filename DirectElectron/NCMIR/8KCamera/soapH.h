/* soapH.h
   Generated by gSOAP 2.7.3 from .\webserviceCommands.h
   Copyright (C) 2000-2005, Robert van Engelen, Genivia Inc. All Rights Reserved.
   This part of the software is released under one of the following licenses:
   GPL, the gSOAP public license, or Genivia's license for commercial use.
*/

#ifndef soapH_H
#define soapH_H
#include "soapStub.h"
SOAP_FMAC3 int SOAP_FMAC4 soap_ignore_element(struct soap*);
#ifndef WITH_NOIDREF
SOAP_FMAC3 int SOAP_FMAC4 soap_putindependent(struct soap*);
SOAP_FMAC3 int SOAP_FMAC4 soap_getindependent(struct soap*);
SOAP_FMAC3 void SOAP_FMAC4 soap_markelement(struct soap*, const void*, int);
SOAP_FMAC3 int SOAP_FMAC4 soap_putelement(struct soap*, const void*, const char*, int, int);
SOAP_FMAC3 void * SOAP_FMAC4 soap_getelement(struct soap*, int*);
#endif

#ifndef SOAP_TYPE_byte
#define SOAP_TYPE_byte (2)
#endif
SOAP_FMAC3 void SOAP_FMAC4 soap_default_byte(struct soap*, char *);
SOAP_FMAC3 int SOAP_FMAC4 soap_put_byte(struct soap*, const char *, const char*, const char*);
SOAP_FMAC3 int SOAP_FMAC4 soap_out_byte(struct soap*, const char*, int, const char *, const char*);
SOAP_FMAC3 char * SOAP_FMAC4 soap_get_byte(struct soap*, char *, const char*, const char*);
SOAP_FMAC3 char * SOAP_FMAC4 soap_in_byte(struct soap*, const char*, char *, const char*);

#ifndef SOAP_TYPE_xsd__short
#define SOAP_TYPE_xsd__short (9)
#endif
SOAP_FMAC3 void SOAP_FMAC4 soap_default_xsd__short(struct soap*, short *);
SOAP_FMAC3 int SOAP_FMAC4 soap_put_xsd__short(struct soap*, const short *, const char*, const char*);
SOAP_FMAC3 int SOAP_FMAC4 soap_out_xsd__short(struct soap*, const char*, int, const short *, const char*);
SOAP_FMAC3 short * SOAP_FMAC4 soap_get_xsd__short(struct soap*, short *, const char*, const char*);
SOAP_FMAC3 short * SOAP_FMAC4 soap_in_xsd__short(struct soap*, const char*, short *, const char*);

#ifndef SOAP_TYPE_short
#define SOAP_TYPE_short (8)
#endif
SOAP_FMAC3 void SOAP_FMAC4 soap_default_short(struct soap*, short *);
SOAP_FMAC3 int SOAP_FMAC4 soap_put_short(struct soap*, const short *, const char*, const char*);
SOAP_FMAC3 int SOAP_FMAC4 soap_out_short(struct soap*, const char*, int, const short *, const char*);
SOAP_FMAC3 short * SOAP_FMAC4 soap_get_short(struct soap*, short *, const char*, const char*);
SOAP_FMAC3 short * SOAP_FMAC4 soap_in_short(struct soap*, const char*, short *, const char*);

#ifndef SOAP_TYPE_xsd__int
#define SOAP_TYPE_xsd__int (10)
#endif
SOAP_FMAC3 void SOAP_FMAC4 soap_default_xsd__int(struct soap*, int *);
SOAP_FMAC3 int SOAP_FMAC4 soap_put_xsd__int(struct soap*, const int *, const char*, const char*);
SOAP_FMAC3 int SOAP_FMAC4 soap_out_xsd__int(struct soap*, const char*, int, const int *, const char*);
SOAP_FMAC3 int * SOAP_FMAC4 soap_get_xsd__int(struct soap*, int *, const char*, const char*);
SOAP_FMAC3 int * SOAP_FMAC4 soap_in_xsd__int(struct soap*, const char*, int *, const char*);

#ifndef SOAP_TYPE_int
#define SOAP_TYPE_int (1)
#endif
SOAP_FMAC3 void SOAP_FMAC4 soap_default_int(struct soap*, int *);
SOAP_FMAC3 int SOAP_FMAC4 soap_put_int(struct soap*, const int *, const char*, const char*);
SOAP_FMAC3 int SOAP_FMAC4 soap_out_int(struct soap*, const char*, int, const int *, const char*);
SOAP_FMAC3 int * SOAP_FMAC4 soap_get_int(struct soap*, int *, const char*, const char*);
SOAP_FMAC3 int * SOAP_FMAC4 soap_in_int(struct soap*, const char*, int *, const char*);

#ifndef SOAP_TYPE_xsd__long
#define SOAP_TYPE_xsd__long (13)
#endif
SOAP_FMAC3 void SOAP_FMAC4 soap_default_xsd__long(struct soap*, float *);
SOAP_FMAC3 int SOAP_FMAC4 soap_put_xsd__long(struct soap*, const float *, const char*, const char*);
SOAP_FMAC3 int SOAP_FMAC4 soap_out_xsd__long(struct soap*, const char*, int, const float *, const char*);
SOAP_FMAC3 float * SOAP_FMAC4 soap_get_xsd__long(struct soap*, float *, const char*, const char*);
SOAP_FMAC3 float * SOAP_FMAC4 soap_in_xsd__long(struct soap*, const char*, float *, const char*);

#ifndef SOAP_TYPE_xsd__float
#define SOAP_TYPE_xsd__float (12)
#endif
SOAP_FMAC3 void SOAP_FMAC4 soap_default_xsd__float(struct soap*, float *);
SOAP_FMAC3 int SOAP_FMAC4 soap_put_xsd__float(struct soap*, const float *, const char*, const char*);
SOAP_FMAC3 int SOAP_FMAC4 soap_out_xsd__float(struct soap*, const char*, int, const float *, const char*);
SOAP_FMAC3 float * SOAP_FMAC4 soap_get_xsd__float(struct soap*, float *, const char*, const char*);
SOAP_FMAC3 float * SOAP_FMAC4 soap_in_xsd__float(struct soap*, const char*, float *, const char*);

#ifndef SOAP_TYPE_float
#define SOAP_TYPE_float (11)
#endif
SOAP_FMAC3 void SOAP_FMAC4 soap_default_float(struct soap*, float *);
SOAP_FMAC3 int SOAP_FMAC4 soap_put_float(struct soap*, const float *, const char*, const char*);
SOAP_FMAC3 int SOAP_FMAC4 soap_out_float(struct soap*, const char*, int, const float *, const char*);
SOAP_FMAC3 float * SOAP_FMAC4 soap_get_float(struct soap*, float *, const char*, const char*);
SOAP_FMAC3 float * SOAP_FMAC4 soap_in_float(struct soap*, const char*, float *, const char*);

#ifndef SOAP_TYPE_xsd__double
#define SOAP_TYPE_xsd__double (7)
#endif
SOAP_FMAC3 void SOAP_FMAC4 soap_default_xsd__double(struct soap*, double *);
SOAP_FMAC3 int SOAP_FMAC4 soap_put_xsd__double(struct soap*, const double *, const char*, const char*);
SOAP_FMAC3 int SOAP_FMAC4 soap_out_xsd__double(struct soap*, const char*, int, const double *, const char*);
SOAP_FMAC3 double * SOAP_FMAC4 soap_get_xsd__double(struct soap*, double *, const char*, const char*);
SOAP_FMAC3 double * SOAP_FMAC4 soap_in_xsd__double(struct soap*, const char*, double *, const char*);

#ifndef SOAP_TYPE_double
#define SOAP_TYPE_double (6)
#endif
SOAP_FMAC3 void SOAP_FMAC4 soap_default_double(struct soap*, double *);
SOAP_FMAC3 int SOAP_FMAC4 soap_put_double(struct soap*, const double *, const char*, const char*);
SOAP_FMAC3 int SOAP_FMAC4 soap_out_double(struct soap*, const char*, int, const double *, const char*);
SOAP_FMAC3 double * SOAP_FMAC4 soap_get_double(struct soap*, double *, const char*, const char*);
SOAP_FMAC3 double * SOAP_FMAC4 soap_in_double(struct soap*, const char*, double *, const char*);

#ifndef WITH_NOGLOBAL

#ifndef SOAP_TYPE_SOAP_ENV__Fault
#define SOAP_TYPE_SOAP_ENV__Fault (65)
#endif
SOAP_FMAC3 void SOAP_FMAC4 soap_serialize_SOAP_ENV__Fault(struct soap*, const struct SOAP_ENV__Fault *);
SOAP_FMAC3 void SOAP_FMAC4 soap_default_SOAP_ENV__Fault(struct soap*, struct SOAP_ENV__Fault *);
SOAP_FMAC3 int SOAP_FMAC4 soap_put_SOAP_ENV__Fault(struct soap*, const struct SOAP_ENV__Fault *, const char*, const char*);
SOAP_FMAC3 int SOAP_FMAC4 soap_out_SOAP_ENV__Fault(struct soap*, const char*, int, const struct SOAP_ENV__Fault *, const char*);
SOAP_FMAC3 struct SOAP_ENV__Fault * SOAP_FMAC4 soap_get_SOAP_ENV__Fault(struct soap*, struct SOAP_ENV__Fault *, const char*, const char*);
SOAP_FMAC3 struct SOAP_ENV__Fault * SOAP_FMAC4 soap_in_SOAP_ENV__Fault(struct soap*, const char*, struct SOAP_ENV__Fault *, const char*);

#endif

#ifndef WITH_NOGLOBAL

#ifndef SOAP_TYPE_SOAP_ENV__Detail
#define SOAP_TYPE_SOAP_ENV__Detail (64)
#endif
SOAP_FMAC3 void SOAP_FMAC4 soap_serialize_SOAP_ENV__Detail(struct soap*, const struct SOAP_ENV__Detail *);
SOAP_FMAC3 void SOAP_FMAC4 soap_default_SOAP_ENV__Detail(struct soap*, struct SOAP_ENV__Detail *);
SOAP_FMAC3 int SOAP_FMAC4 soap_put_SOAP_ENV__Detail(struct soap*, const struct SOAP_ENV__Detail *, const char*, const char*);
SOAP_FMAC3 int SOAP_FMAC4 soap_out_SOAP_ENV__Detail(struct soap*, const char*, int, const struct SOAP_ENV__Detail *, const char*);
SOAP_FMAC3 struct SOAP_ENV__Detail * SOAP_FMAC4 soap_get_SOAP_ENV__Detail(struct soap*, struct SOAP_ENV__Detail *, const char*, const char*);
SOAP_FMAC3 struct SOAP_ENV__Detail * SOAP_FMAC4 soap_in_SOAP_ENV__Detail(struct soap*, const char*, struct SOAP_ENV__Detail *, const char*);

#endif

#ifndef WITH_NOGLOBAL

#ifndef SOAP_TYPE_SOAP_ENV__Code
#define SOAP_TYPE_SOAP_ENV__Code (62)
#endif
SOAP_FMAC3 void SOAP_FMAC4 soap_serialize_SOAP_ENV__Code(struct soap*, const struct SOAP_ENV__Code *);
SOAP_FMAC3 void SOAP_FMAC4 soap_default_SOAP_ENV__Code(struct soap*, struct SOAP_ENV__Code *);
SOAP_FMAC3 int SOAP_FMAC4 soap_put_SOAP_ENV__Code(struct soap*, const struct SOAP_ENV__Code *, const char*, const char*);
SOAP_FMAC3 int SOAP_FMAC4 soap_out_SOAP_ENV__Code(struct soap*, const char*, int, const struct SOAP_ENV__Code *, const char*);
SOAP_FMAC3 struct SOAP_ENV__Code * SOAP_FMAC4 soap_get_SOAP_ENV__Code(struct soap*, struct SOAP_ENV__Code *, const char*, const char*);
SOAP_FMAC3 struct SOAP_ENV__Code * SOAP_FMAC4 soap_in_SOAP_ENV__Code(struct soap*, const char*, struct SOAP_ENV__Code *, const char*);

#endif

#ifndef WITH_NOGLOBAL

#ifndef SOAP_TYPE_SOAP_ENV__Header
#define SOAP_TYPE_SOAP_ENV__Header (61)
#endif
SOAP_FMAC3 void SOAP_FMAC4 soap_serialize_SOAP_ENV__Header(struct soap*, const struct SOAP_ENV__Header *);
SOAP_FMAC3 void SOAP_FMAC4 soap_default_SOAP_ENV__Header(struct soap*, struct SOAP_ENV__Header *);
SOAP_FMAC3 int SOAP_FMAC4 soap_put_SOAP_ENV__Header(struct soap*, const struct SOAP_ENV__Header *, const char*, const char*);
SOAP_FMAC3 int SOAP_FMAC4 soap_out_SOAP_ENV__Header(struct soap*, const char*, int, const struct SOAP_ENV__Header *, const char*);
SOAP_FMAC3 struct SOAP_ENV__Header * SOAP_FMAC4 soap_get_SOAP_ENV__Header(struct soap*, struct SOAP_ENV__Header *, const char*, const char*);
SOAP_FMAC3 struct SOAP_ENV__Header * SOAP_FMAC4 soap_in_SOAP_ENV__Header(struct soap*, const char*, struct SOAP_ENV__Header *, const char*);

#endif

#ifndef SOAP_TYPE_ns__SerialGotAcquireToken
#define SOAP_TYPE_ns__SerialGotAcquireToken (58)
#endif
SOAP_FMAC3 void SOAP_FMAC4 soap_serialize_ns__SerialGotAcquireToken(struct soap*, const struct ns__SerialGotAcquireToken *);
SOAP_FMAC3 void SOAP_FMAC4 soap_default_ns__SerialGotAcquireToken(struct soap*, struct ns__SerialGotAcquireToken *);
SOAP_FMAC3 int SOAP_FMAC4 soap_put_ns__SerialGotAcquireToken(struct soap*, const struct ns__SerialGotAcquireToken *, const char*, const char*);
SOAP_FMAC3 int SOAP_FMAC4 soap_out_ns__SerialGotAcquireToken(struct soap*, const char*, int, const struct ns__SerialGotAcquireToken *, const char*);
SOAP_FMAC3 struct ns__SerialGotAcquireToken * SOAP_FMAC4 soap_get_ns__SerialGotAcquireToken(struct soap*, struct ns__SerialGotAcquireToken *, const char*, const char*);
SOAP_FMAC3 struct ns__SerialGotAcquireToken * SOAP_FMAC4 soap_in_ns__SerialGotAcquireToken(struct soap*, const char*, struct ns__SerialGotAcquireToken *, const char*);

#ifndef SOAP_TYPE_ns__SerialGotAcquireTokenResponse
#define SOAP_TYPE_ns__SerialGotAcquireTokenResponse (57)
#endif
SOAP_FMAC3 void SOAP_FMAC4 soap_serialize_ns__SerialGotAcquireTokenResponse(struct soap*, const struct ns__SerialGotAcquireTokenResponse *);
SOAP_FMAC3 void SOAP_FMAC4 soap_default_ns__SerialGotAcquireTokenResponse(struct soap*, struct ns__SerialGotAcquireTokenResponse *);
SOAP_FMAC3 int SOAP_FMAC4 soap_put_ns__SerialGotAcquireTokenResponse(struct soap*, const struct ns__SerialGotAcquireTokenResponse *, const char*, const char*);
SOAP_FMAC3 int SOAP_FMAC4 soap_out_ns__SerialGotAcquireTokenResponse(struct soap*, const char*, int, const struct ns__SerialGotAcquireTokenResponse *, const char*);
SOAP_FMAC3 struct ns__SerialGotAcquireTokenResponse * SOAP_FMAC4 soap_get_ns__SerialGotAcquireTokenResponse(struct soap*, struct ns__SerialGotAcquireTokenResponse *, const char*, const char*);
SOAP_FMAC3 struct ns__SerialGotAcquireTokenResponse * SOAP_FMAC4 soap_in_ns__SerialGotAcquireTokenResponse(struct soap*, const char*, struct ns__SerialGotAcquireTokenResponse *, const char*);

#ifndef SOAP_TYPE_ns__CheckifOk2Acquire
#define SOAP_TYPE_ns__CheckifOk2Acquire (55)
#endif
SOAP_FMAC3 void SOAP_FMAC4 soap_serialize_ns__CheckifOk2Acquire(struct soap*, const struct ns__CheckifOk2Acquire *);
SOAP_FMAC3 void SOAP_FMAC4 soap_default_ns__CheckifOk2Acquire(struct soap*, struct ns__CheckifOk2Acquire *);
SOAP_FMAC3 int SOAP_FMAC4 soap_put_ns__CheckifOk2Acquire(struct soap*, const struct ns__CheckifOk2Acquire *, const char*, const char*);
SOAP_FMAC3 int SOAP_FMAC4 soap_out_ns__CheckifOk2Acquire(struct soap*, const char*, int, const struct ns__CheckifOk2Acquire *, const char*);
SOAP_FMAC3 struct ns__CheckifOk2Acquire * SOAP_FMAC4 soap_get_ns__CheckifOk2Acquire(struct soap*, struct ns__CheckifOk2Acquire *, const char*, const char*);
SOAP_FMAC3 struct ns__CheckifOk2Acquire * SOAP_FMAC4 soap_in_ns__CheckifOk2Acquire(struct soap*, const char*, struct ns__CheckifOk2Acquire *, const char*);

#ifndef SOAP_TYPE_ns__CheckifOk2AcquireResponse
#define SOAP_TYPE_ns__CheckifOk2AcquireResponse (54)
#endif
SOAP_FMAC3 void SOAP_FMAC4 soap_serialize_ns__CheckifOk2AcquireResponse(struct soap*, const struct ns__CheckifOk2AcquireResponse *);
SOAP_FMAC3 void SOAP_FMAC4 soap_default_ns__CheckifOk2AcquireResponse(struct soap*, struct ns__CheckifOk2AcquireResponse *);
SOAP_FMAC3 int SOAP_FMAC4 soap_put_ns__CheckifOk2AcquireResponse(struct soap*, const struct ns__CheckifOk2AcquireResponse *, const char*, const char*);
SOAP_FMAC3 int SOAP_FMAC4 soap_out_ns__CheckifOk2AcquireResponse(struct soap*, const char*, int, const struct ns__CheckifOk2AcquireResponse *, const char*);
SOAP_FMAC3 struct ns__CheckifOk2AcquireResponse * SOAP_FMAC4 soap_get_ns__CheckifOk2AcquireResponse(struct soap*, struct ns__CheckifOk2AcquireResponse *, const char*, const char*);
SOAP_FMAC3 struct ns__CheckifOk2AcquireResponse * SOAP_FMAC4 soap_in_ns__CheckifOk2AcquireResponse(struct soap*, const char*, struct ns__CheckifOk2AcquireResponse *, const char*);

#ifndef SOAP_TYPE_ns__BeginMosaic
#define SOAP_TYPE_ns__BeginMosaic (52)
#endif
SOAP_FMAC3 void SOAP_FMAC4 soap_serialize_ns__BeginMosaic(struct soap*, const struct ns__BeginMosaic *);
SOAP_FMAC3 void SOAP_FMAC4 soap_default_ns__BeginMosaic(struct soap*, struct ns__BeginMosaic *);
SOAP_FMAC3 int SOAP_FMAC4 soap_put_ns__BeginMosaic(struct soap*, const struct ns__BeginMosaic *, const char*, const char*);
SOAP_FMAC3 int SOAP_FMAC4 soap_out_ns__BeginMosaic(struct soap*, const char*, int, const struct ns__BeginMosaic *, const char*);
SOAP_FMAC3 struct ns__BeginMosaic * SOAP_FMAC4 soap_get_ns__BeginMosaic(struct soap*, struct ns__BeginMosaic *, const char*, const char*);
SOAP_FMAC3 struct ns__BeginMosaic * SOAP_FMAC4 soap_in_ns__BeginMosaic(struct soap*, const char*, struct ns__BeginMosaic *, const char*);

#ifndef SOAP_TYPE_ns__BeginMosaicResponse
#define SOAP_TYPE_ns__BeginMosaicResponse (51)
#endif
SOAP_FMAC3 void SOAP_FMAC4 soap_serialize_ns__BeginMosaicResponse(struct soap*, const struct ns__BeginMosaicResponse *);
SOAP_FMAC3 void SOAP_FMAC4 soap_default_ns__BeginMosaicResponse(struct soap*, struct ns__BeginMosaicResponse *);
SOAP_FMAC3 int SOAP_FMAC4 soap_put_ns__BeginMosaicResponse(struct soap*, const struct ns__BeginMosaicResponse *, const char*, const char*);
SOAP_FMAC3 int SOAP_FMAC4 soap_out_ns__BeginMosaicResponse(struct soap*, const char*, int, const struct ns__BeginMosaicResponse *, const char*);
SOAP_FMAC3 struct ns__BeginMosaicResponse * SOAP_FMAC4 soap_get_ns__BeginMosaicResponse(struct soap*, struct ns__BeginMosaicResponse *, const char*, const char*);
SOAP_FMAC3 struct ns__BeginMosaicResponse * SOAP_FMAC4 soap_in_ns__BeginMosaicResponse(struct soap*, const char*, struct ns__BeginMosaicResponse *, const char*);

#ifndef SOAP_TYPE_ns__QueryMosaicImages
#define SOAP_TYPE_ns__QueryMosaicImages (49)
#endif
SOAP_FMAC3 void SOAP_FMAC4 soap_serialize_ns__QueryMosaicImages(struct soap*, const struct ns__QueryMosaicImages *);
SOAP_FMAC3 void SOAP_FMAC4 soap_default_ns__QueryMosaicImages(struct soap*, struct ns__QueryMosaicImages *);
SOAP_FMAC3 int SOAP_FMAC4 soap_put_ns__QueryMosaicImages(struct soap*, const struct ns__QueryMosaicImages *, const char*, const char*);
SOAP_FMAC3 int SOAP_FMAC4 soap_out_ns__QueryMosaicImages(struct soap*, const char*, int, const struct ns__QueryMosaicImages *, const char*);
SOAP_FMAC3 struct ns__QueryMosaicImages * SOAP_FMAC4 soap_get_ns__QueryMosaicImages(struct soap*, struct ns__QueryMosaicImages *, const char*, const char*);
SOAP_FMAC3 struct ns__QueryMosaicImages * SOAP_FMAC4 soap_in_ns__QueryMosaicImages(struct soap*, const char*, struct ns__QueryMosaicImages *, const char*);

#ifndef SOAP_TYPE_ns__QueryMosaicImagesResponse
#define SOAP_TYPE_ns__QueryMosaicImagesResponse (48)
#endif
SOAP_FMAC3 void SOAP_FMAC4 soap_serialize_ns__QueryMosaicImagesResponse(struct soap*, const struct ns__QueryMosaicImagesResponse *);
SOAP_FMAC3 void SOAP_FMAC4 soap_default_ns__QueryMosaicImagesResponse(struct soap*, struct ns__QueryMosaicImagesResponse *);
SOAP_FMAC3 int SOAP_FMAC4 soap_put_ns__QueryMosaicImagesResponse(struct soap*, const struct ns__QueryMosaicImagesResponse *, const char*, const char*);
SOAP_FMAC3 int SOAP_FMAC4 soap_out_ns__QueryMosaicImagesResponse(struct soap*, const char*, int, const struct ns__QueryMosaicImagesResponse *, const char*);
SOAP_FMAC3 struct ns__QueryMosaicImagesResponse * SOAP_FMAC4 soap_get_ns__QueryMosaicImagesResponse(struct soap*, struct ns__QueryMosaicImagesResponse *, const char*, const char*);
SOAP_FMAC3 struct ns__QueryMosaicImagesResponse * SOAP_FMAC4 soap_in_ns__QueryMosaicImagesResponse(struct soap*, const char*, struct ns__QueryMosaicImagesResponse *, const char*);

#ifndef SOAP_TYPE_ns__NextMosaicMove
#define SOAP_TYPE_ns__NextMosaicMove (45)
#endif
SOAP_FMAC3 void SOAP_FMAC4 soap_serialize_ns__NextMosaicMove(struct soap*, const struct ns__NextMosaicMove *);
SOAP_FMAC3 void SOAP_FMAC4 soap_default_ns__NextMosaicMove(struct soap*, struct ns__NextMosaicMove *);
SOAP_FMAC3 int SOAP_FMAC4 soap_put_ns__NextMosaicMove(struct soap*, const struct ns__NextMosaicMove *, const char*, const char*);
SOAP_FMAC3 int SOAP_FMAC4 soap_out_ns__NextMosaicMove(struct soap*, const char*, int, const struct ns__NextMosaicMove *, const char*);
SOAP_FMAC3 struct ns__NextMosaicMove * SOAP_FMAC4 soap_get_ns__NextMosaicMove(struct soap*, struct ns__NextMosaicMove *, const char*, const char*);
SOAP_FMAC3 struct ns__NextMosaicMove * SOAP_FMAC4 soap_in_ns__NextMosaicMove(struct soap*, const char*, struct ns__NextMosaicMove *, const char*);

#ifndef SOAP_TYPE_ns__NextMosaicMoveResponse
#define SOAP_TYPE_ns__NextMosaicMoveResponse (44)
#endif
SOAP_FMAC3 void SOAP_FMAC4 soap_serialize_ns__NextMosaicMoveResponse(struct soap*, const struct ns__NextMosaicMoveResponse *);
SOAP_FMAC3 void SOAP_FMAC4 soap_default_ns__NextMosaicMoveResponse(struct soap*, struct ns__NextMosaicMoveResponse *);
SOAP_FMAC3 int SOAP_FMAC4 soap_put_ns__NextMosaicMoveResponse(struct soap*, const struct ns__NextMosaicMoveResponse *, const char*, const char*);
SOAP_FMAC3 int SOAP_FMAC4 soap_out_ns__NextMosaicMoveResponse(struct soap*, const char*, int, const struct ns__NextMosaicMoveResponse *, const char*);
SOAP_FMAC3 struct ns__NextMosaicMoveResponse * SOAP_FMAC4 soap_get_ns__NextMosaicMoveResponse(struct soap*, struct ns__NextMosaicMoveResponse *, const char*, const char*);
SOAP_FMAC3 struct ns__NextMosaicMoveResponse * SOAP_FMAC4 soap_in_ns__NextMosaicMoveResponse(struct soap*, const char*, struct ns__NextMosaicMoveResponse *, const char*);

#ifndef SOAP_TYPE_ns__setY
#define SOAP_TYPE_ns__setY (42)
#endif
SOAP_FMAC3 void SOAP_FMAC4 soap_serialize_ns__setY(struct soap*, const struct ns__setY *);
SOAP_FMAC3 void SOAP_FMAC4 soap_default_ns__setY(struct soap*, struct ns__setY *);
SOAP_FMAC3 int SOAP_FMAC4 soap_put_ns__setY(struct soap*, const struct ns__setY *, const char*, const char*);
SOAP_FMAC3 int SOAP_FMAC4 soap_out_ns__setY(struct soap*, const char*, int, const struct ns__setY *, const char*);
SOAP_FMAC3 struct ns__setY * SOAP_FMAC4 soap_get_ns__setY(struct soap*, struct ns__setY *, const char*, const char*);
SOAP_FMAC3 struct ns__setY * SOAP_FMAC4 soap_in_ns__setY(struct soap*, const char*, struct ns__setY *, const char*);

#ifndef SOAP_TYPE_ns__setYResponse
#define SOAP_TYPE_ns__setYResponse (41)
#endif
SOAP_FMAC3 void SOAP_FMAC4 soap_serialize_ns__setYResponse(struct soap*, const struct ns__setYResponse *);
SOAP_FMAC3 void SOAP_FMAC4 soap_default_ns__setYResponse(struct soap*, struct ns__setYResponse *);
SOAP_FMAC3 int SOAP_FMAC4 soap_put_ns__setYResponse(struct soap*, const struct ns__setYResponse *, const char*, const char*);
SOAP_FMAC3 int SOAP_FMAC4 soap_out_ns__setYResponse(struct soap*, const char*, int, const struct ns__setYResponse *, const char*);
SOAP_FMAC3 struct ns__setYResponse * SOAP_FMAC4 soap_get_ns__setYResponse(struct soap*, struct ns__setYResponse *, const char*, const char*);
SOAP_FMAC3 struct ns__setYResponse * SOAP_FMAC4 soap_in_ns__setYResponse(struct soap*, const char*, struct ns__setYResponse *, const char*);

#ifndef SOAP_TYPE_ns__setX
#define SOAP_TYPE_ns__setX (39)
#endif
SOAP_FMAC3 void SOAP_FMAC4 soap_serialize_ns__setX(struct soap*, const struct ns__setX *);
SOAP_FMAC3 void SOAP_FMAC4 soap_default_ns__setX(struct soap*, struct ns__setX *);
SOAP_FMAC3 int SOAP_FMAC4 soap_put_ns__setX(struct soap*, const struct ns__setX *, const char*, const char*);
SOAP_FMAC3 int SOAP_FMAC4 soap_out_ns__setX(struct soap*, const char*, int, const struct ns__setX *, const char*);
SOAP_FMAC3 struct ns__setX * SOAP_FMAC4 soap_get_ns__setX(struct soap*, struct ns__setX *, const char*, const char*);
SOAP_FMAC3 struct ns__setX * SOAP_FMAC4 soap_in_ns__setX(struct soap*, const char*, struct ns__setX *, const char*);

#ifndef SOAP_TYPE_ns__setXResponse
#define SOAP_TYPE_ns__setXResponse (38)
#endif
SOAP_FMAC3 void SOAP_FMAC4 soap_serialize_ns__setXResponse(struct soap*, const struct ns__setXResponse *);
SOAP_FMAC3 void SOAP_FMAC4 soap_default_ns__setXResponse(struct soap*, struct ns__setXResponse *);
SOAP_FMAC3 int SOAP_FMAC4 soap_put_ns__setXResponse(struct soap*, const struct ns__setXResponse *, const char*, const char*);
SOAP_FMAC3 int SOAP_FMAC4 soap_out_ns__setXResponse(struct soap*, const char*, int, const struct ns__setXResponse *, const char*);
SOAP_FMAC3 struct ns__setXResponse * SOAP_FMAC4 soap_get_ns__setXResponse(struct soap*, struct ns__setXResponse *, const char*, const char*);
SOAP_FMAC3 struct ns__setXResponse * SOAP_FMAC4 soap_in_ns__setXResponse(struct soap*, const char*, struct ns__setXResponse *, const char*);

#ifndef SOAP_TYPE_ns__getZ
#define SOAP_TYPE_ns__getZ (36)
#endif
SOAP_FMAC3 void SOAP_FMAC4 soap_serialize_ns__getZ(struct soap*, const struct ns__getZ *);
SOAP_FMAC3 void SOAP_FMAC4 soap_default_ns__getZ(struct soap*, struct ns__getZ *);
SOAP_FMAC3 int SOAP_FMAC4 soap_put_ns__getZ(struct soap*, const struct ns__getZ *, const char*, const char*);
SOAP_FMAC3 int SOAP_FMAC4 soap_out_ns__getZ(struct soap*, const char*, int, const struct ns__getZ *, const char*);
SOAP_FMAC3 struct ns__getZ * SOAP_FMAC4 soap_get_ns__getZ(struct soap*, struct ns__getZ *, const char*, const char*);
SOAP_FMAC3 struct ns__getZ * SOAP_FMAC4 soap_in_ns__getZ(struct soap*, const char*, struct ns__getZ *, const char*);

#ifndef SOAP_TYPE_ns__getZResponse
#define SOAP_TYPE_ns__getZResponse (35)
#endif
SOAP_FMAC3 void SOAP_FMAC4 soap_serialize_ns__getZResponse(struct soap*, const struct ns__getZResponse *);
SOAP_FMAC3 void SOAP_FMAC4 soap_default_ns__getZResponse(struct soap*, struct ns__getZResponse *);
SOAP_FMAC3 int SOAP_FMAC4 soap_put_ns__getZResponse(struct soap*, const struct ns__getZResponse *, const char*, const char*);
SOAP_FMAC3 int SOAP_FMAC4 soap_out_ns__getZResponse(struct soap*, const char*, int, const struct ns__getZResponse *, const char*);
SOAP_FMAC3 struct ns__getZResponse * SOAP_FMAC4 soap_get_ns__getZResponse(struct soap*, struct ns__getZResponse *, const char*, const char*);
SOAP_FMAC3 struct ns__getZResponse * SOAP_FMAC4 soap_in_ns__getZResponse(struct soap*, const char*, struct ns__getZResponse *, const char*);

#ifndef SOAP_TYPE_ns__getY
#define SOAP_TYPE_ns__getY (33)
#endif
SOAP_FMAC3 void SOAP_FMAC4 soap_serialize_ns__getY(struct soap*, const struct ns__getY *);
SOAP_FMAC3 void SOAP_FMAC4 soap_default_ns__getY(struct soap*, struct ns__getY *);
SOAP_FMAC3 int SOAP_FMAC4 soap_put_ns__getY(struct soap*, const struct ns__getY *, const char*, const char*);
SOAP_FMAC3 int SOAP_FMAC4 soap_out_ns__getY(struct soap*, const char*, int, const struct ns__getY *, const char*);
SOAP_FMAC3 struct ns__getY * SOAP_FMAC4 soap_get_ns__getY(struct soap*, struct ns__getY *, const char*, const char*);
SOAP_FMAC3 struct ns__getY * SOAP_FMAC4 soap_in_ns__getY(struct soap*, const char*, struct ns__getY *, const char*);

#ifndef SOAP_TYPE_ns__getYResponse
#define SOAP_TYPE_ns__getYResponse (32)
#endif
SOAP_FMAC3 void SOAP_FMAC4 soap_serialize_ns__getYResponse(struct soap*, const struct ns__getYResponse *);
SOAP_FMAC3 void SOAP_FMAC4 soap_default_ns__getYResponse(struct soap*, struct ns__getYResponse *);
SOAP_FMAC3 int SOAP_FMAC4 soap_put_ns__getYResponse(struct soap*, const struct ns__getYResponse *, const char*, const char*);
SOAP_FMAC3 int SOAP_FMAC4 soap_out_ns__getYResponse(struct soap*, const char*, int, const struct ns__getYResponse *, const char*);
SOAP_FMAC3 struct ns__getYResponse * SOAP_FMAC4 soap_get_ns__getYResponse(struct soap*, struct ns__getYResponse *, const char*, const char*);
SOAP_FMAC3 struct ns__getYResponse * SOAP_FMAC4 soap_in_ns__getYResponse(struct soap*, const char*, struct ns__getYResponse *, const char*);

#ifndef SOAP_TYPE_ns__getX
#define SOAP_TYPE_ns__getX (30)
#endif
SOAP_FMAC3 void SOAP_FMAC4 soap_serialize_ns__getX(struct soap*, const struct ns__getX *);
SOAP_FMAC3 void SOAP_FMAC4 soap_default_ns__getX(struct soap*, struct ns__getX *);
SOAP_FMAC3 int SOAP_FMAC4 soap_put_ns__getX(struct soap*, const struct ns__getX *, const char*, const char*);
SOAP_FMAC3 int SOAP_FMAC4 soap_out_ns__getX(struct soap*, const char*, int, const struct ns__getX *, const char*);
SOAP_FMAC3 struct ns__getX * SOAP_FMAC4 soap_get_ns__getX(struct soap*, struct ns__getX *, const char*, const char*);
SOAP_FMAC3 struct ns__getX * SOAP_FMAC4 soap_in_ns__getX(struct soap*, const char*, struct ns__getX *, const char*);

#ifndef SOAP_TYPE_ns__getXResponse
#define SOAP_TYPE_ns__getXResponse (29)
#endif
SOAP_FMAC3 void SOAP_FMAC4 soap_serialize_ns__getXResponse(struct soap*, const struct ns__getXResponse *);
SOAP_FMAC3 void SOAP_FMAC4 soap_default_ns__getXResponse(struct soap*, struct ns__getXResponse *);
SOAP_FMAC3 int SOAP_FMAC4 soap_put_ns__getXResponse(struct soap*, const struct ns__getXResponse *, const char*, const char*);
SOAP_FMAC3 int SOAP_FMAC4 soap_out_ns__getXResponse(struct soap*, const char*, int, const struct ns__getXResponse *, const char*);
SOAP_FMAC3 struct ns__getXResponse * SOAP_FMAC4 soap_get_ns__getXResponse(struct soap*, struct ns__getXResponse *, const char*, const char*);
SOAP_FMAC3 struct ns__getXResponse * SOAP_FMAC4 soap_in_ns__getXResponse(struct soap*, const char*, struct ns__getXResponse *, const char*);

#ifndef SOAP_TYPE_ns__setZ
#define SOAP_TYPE_ns__setZ (27)
#endif
SOAP_FMAC3 void SOAP_FMAC4 soap_serialize_ns__setZ(struct soap*, const struct ns__setZ *);
SOAP_FMAC3 void SOAP_FMAC4 soap_default_ns__setZ(struct soap*, struct ns__setZ *);
SOAP_FMAC3 int SOAP_FMAC4 soap_put_ns__setZ(struct soap*, const struct ns__setZ *, const char*, const char*);
SOAP_FMAC3 int SOAP_FMAC4 soap_out_ns__setZ(struct soap*, const char*, int, const struct ns__setZ *, const char*);
SOAP_FMAC3 struct ns__setZ * SOAP_FMAC4 soap_get_ns__setZ(struct soap*, struct ns__setZ *, const char*, const char*);
SOAP_FMAC3 struct ns__setZ * SOAP_FMAC4 soap_in_ns__setZ(struct soap*, const char*, struct ns__setZ *, const char*);

#ifndef SOAP_TYPE_ns__setZResponse
#define SOAP_TYPE_ns__setZResponse (26)
#endif
SOAP_FMAC3 void SOAP_FMAC4 soap_serialize_ns__setZResponse(struct soap*, const struct ns__setZResponse *);
SOAP_FMAC3 void SOAP_FMAC4 soap_default_ns__setZResponse(struct soap*, struct ns__setZResponse *);
SOAP_FMAC3 int SOAP_FMAC4 soap_put_ns__setZResponse(struct soap*, const struct ns__setZResponse *, const char*, const char*);
SOAP_FMAC3 int SOAP_FMAC4 soap_out_ns__setZResponse(struct soap*, const char*, int, const struct ns__setZResponse *, const char*);
SOAP_FMAC3 struct ns__setZResponse * SOAP_FMAC4 soap_get_ns__setZResponse(struct soap*, struct ns__setZResponse *, const char*, const char*);
SOAP_FMAC3 struct ns__setZResponse * SOAP_FMAC4 soap_in_ns__setZResponse(struct soap*, const char*, struct ns__setZResponse *, const char*);

#ifndef SOAP_TYPE_ns__setXYStagePosition
#define SOAP_TYPE_ns__setXYStagePosition (24)
#endif
SOAP_FMAC3 void SOAP_FMAC4 soap_serialize_ns__setXYStagePosition(struct soap*, const struct ns__setXYStagePosition *);
SOAP_FMAC3 void SOAP_FMAC4 soap_default_ns__setXYStagePosition(struct soap*, struct ns__setXYStagePosition *);
SOAP_FMAC3 int SOAP_FMAC4 soap_put_ns__setXYStagePosition(struct soap*, const struct ns__setXYStagePosition *, const char*, const char*);
SOAP_FMAC3 int SOAP_FMAC4 soap_out_ns__setXYStagePosition(struct soap*, const char*, int, const struct ns__setXYStagePosition *, const char*);
SOAP_FMAC3 struct ns__setXYStagePosition * SOAP_FMAC4 soap_get_ns__setXYStagePosition(struct soap*, struct ns__setXYStagePosition *, const char*, const char*);
SOAP_FMAC3 struct ns__setXYStagePosition * SOAP_FMAC4 soap_in_ns__setXYStagePosition(struct soap*, const char*, struct ns__setXYStagePosition *, const char*);

#ifndef SOAP_TYPE_ns__setXYStagePositionResponse
#define SOAP_TYPE_ns__setXYStagePositionResponse (23)
#endif
SOAP_FMAC3 void SOAP_FMAC4 soap_serialize_ns__setXYStagePositionResponse(struct soap*, const struct ns__setXYStagePositionResponse *);
SOAP_FMAC3 void SOAP_FMAC4 soap_default_ns__setXYStagePositionResponse(struct soap*, struct ns__setXYStagePositionResponse *);
SOAP_FMAC3 int SOAP_FMAC4 soap_put_ns__setXYStagePositionResponse(struct soap*, const struct ns__setXYStagePositionResponse *, const char*, const char*);
SOAP_FMAC3 int SOAP_FMAC4 soap_out_ns__setXYStagePositionResponse(struct soap*, const char*, int, const struct ns__setXYStagePositionResponse *, const char*);
SOAP_FMAC3 struct ns__setXYStagePositionResponse * SOAP_FMAC4 soap_get_ns__setXYStagePositionResponse(struct soap*, struct ns__setXYStagePositionResponse *, const char*, const char*);
SOAP_FMAC3 struct ns__setXYStagePositionResponse * SOAP_FMAC4 soap_in_ns__setXYStagePositionResponse(struct soap*, const char*, struct ns__setXYStagePositionResponse *, const char*);

#ifndef SOAP_TYPE_ns__setTiltAngle
#define SOAP_TYPE_ns__setTiltAngle (21)
#endif
SOAP_FMAC3 void SOAP_FMAC4 soap_serialize_ns__setTiltAngle(struct soap*, const struct ns__setTiltAngle *);
SOAP_FMAC3 void SOAP_FMAC4 soap_default_ns__setTiltAngle(struct soap*, struct ns__setTiltAngle *);
SOAP_FMAC3 int SOAP_FMAC4 soap_put_ns__setTiltAngle(struct soap*, const struct ns__setTiltAngle *, const char*, const char*);
SOAP_FMAC3 int SOAP_FMAC4 soap_out_ns__setTiltAngle(struct soap*, const char*, int, const struct ns__setTiltAngle *, const char*);
SOAP_FMAC3 struct ns__setTiltAngle * SOAP_FMAC4 soap_get_ns__setTiltAngle(struct soap*, struct ns__setTiltAngle *, const char*, const char*);
SOAP_FMAC3 struct ns__setTiltAngle * SOAP_FMAC4 soap_in_ns__setTiltAngle(struct soap*, const char*, struct ns__setTiltAngle *, const char*);

#ifndef SOAP_TYPE_ns__setTiltAngleResponse
#define SOAP_TYPE_ns__setTiltAngleResponse (20)
#endif
SOAP_FMAC3 void SOAP_FMAC4 soap_serialize_ns__setTiltAngleResponse(struct soap*, const struct ns__setTiltAngleResponse *);
SOAP_FMAC3 void SOAP_FMAC4 soap_default_ns__setTiltAngleResponse(struct soap*, struct ns__setTiltAngleResponse *);
SOAP_FMAC3 int SOAP_FMAC4 soap_put_ns__setTiltAngleResponse(struct soap*, const struct ns__setTiltAngleResponse *, const char*, const char*);
SOAP_FMAC3 int SOAP_FMAC4 soap_out_ns__setTiltAngleResponse(struct soap*, const char*, int, const struct ns__setTiltAngleResponse *, const char*);
SOAP_FMAC3 struct ns__setTiltAngleResponse * SOAP_FMAC4 soap_get_ns__setTiltAngleResponse(struct soap*, struct ns__setTiltAngleResponse *, const char*, const char*);
SOAP_FMAC3 struct ns__setTiltAngleResponse * SOAP_FMAC4 soap_in_ns__setTiltAngleResponse(struct soap*, const char*, struct ns__setTiltAngleResponse *, const char*);

#ifndef SOAP_TYPE_ns__getCurrentTiltAngle
#define SOAP_TYPE_ns__getCurrentTiltAngle (18)
#endif
SOAP_FMAC3 void SOAP_FMAC4 soap_serialize_ns__getCurrentTiltAngle(struct soap*, const struct ns__getCurrentTiltAngle *);
SOAP_FMAC3 void SOAP_FMAC4 soap_default_ns__getCurrentTiltAngle(struct soap*, struct ns__getCurrentTiltAngle *);
SOAP_FMAC3 int SOAP_FMAC4 soap_put_ns__getCurrentTiltAngle(struct soap*, const struct ns__getCurrentTiltAngle *, const char*, const char*);
SOAP_FMAC3 int SOAP_FMAC4 soap_out_ns__getCurrentTiltAngle(struct soap*, const char*, int, const struct ns__getCurrentTiltAngle *, const char*);
SOAP_FMAC3 struct ns__getCurrentTiltAngle * SOAP_FMAC4 soap_get_ns__getCurrentTiltAngle(struct soap*, struct ns__getCurrentTiltAngle *, const char*, const char*);
SOAP_FMAC3 struct ns__getCurrentTiltAngle * SOAP_FMAC4 soap_in_ns__getCurrentTiltAngle(struct soap*, const char*, struct ns__getCurrentTiltAngle *, const char*);

#ifndef SOAP_TYPE_ns__getCurrentTiltAngleResponse
#define SOAP_TYPE_ns__getCurrentTiltAngleResponse (17)
#endif
SOAP_FMAC3 void SOAP_FMAC4 soap_serialize_ns__getCurrentTiltAngleResponse(struct soap*, const struct ns__getCurrentTiltAngleResponse *);
SOAP_FMAC3 void SOAP_FMAC4 soap_default_ns__getCurrentTiltAngleResponse(struct soap*, struct ns__getCurrentTiltAngleResponse *);
SOAP_FMAC3 int SOAP_FMAC4 soap_put_ns__getCurrentTiltAngleResponse(struct soap*, const struct ns__getCurrentTiltAngleResponse *, const char*, const char*);
SOAP_FMAC3 int SOAP_FMAC4 soap_out_ns__getCurrentTiltAngleResponse(struct soap*, const char*, int, const struct ns__getCurrentTiltAngleResponse *, const char*);
SOAP_FMAC3 struct ns__getCurrentTiltAngleResponse * SOAP_FMAC4 soap_get_ns__getCurrentTiltAngleResponse(struct soap*, struct ns__getCurrentTiltAngleResponse *, const char*, const char*);
SOAP_FMAC3 struct ns__getCurrentTiltAngleResponse * SOAP_FMAC4 soap_in_ns__getCurrentTiltAngleResponse(struct soap*, const char*, struct ns__getCurrentTiltAngleResponse *, const char*);

#ifndef WITH_NOGLOBAL

#ifndef SOAP_TYPE_PointerToSOAP_ENV__Detail
#define SOAP_TYPE_PointerToSOAP_ENV__Detail (66)
#endif
SOAP_FMAC3 void SOAP_FMAC4 soap_serialize_PointerToSOAP_ENV__Detail(struct soap*, struct SOAP_ENV__Detail *const*);
SOAP_FMAC3 int SOAP_FMAC4 soap_put_PointerToSOAP_ENV__Detail(struct soap*, struct SOAP_ENV__Detail *const*, const char*, const char*);
SOAP_FMAC3 int SOAP_FMAC4 soap_out_PointerToSOAP_ENV__Detail(struct soap*, const char *, int, struct SOAP_ENV__Detail *const*, const char *);
SOAP_FMAC3 struct SOAP_ENV__Detail ** SOAP_FMAC4 soap_get_PointerToSOAP_ENV__Detail(struct soap*, struct SOAP_ENV__Detail **, const char*, const char*);
SOAP_FMAC3 struct SOAP_ENV__Detail ** SOAP_FMAC4 soap_in_PointerToSOAP_ENV__Detail(struct soap*, const char*, struct SOAP_ENV__Detail **, const char*);

#endif

#ifndef WITH_NOGLOBAL

#ifndef SOAP_TYPE_PointerToSOAP_ENV__Code
#define SOAP_TYPE_PointerToSOAP_ENV__Code (63)
#endif
SOAP_FMAC3 void SOAP_FMAC4 soap_serialize_PointerToSOAP_ENV__Code(struct soap*, struct SOAP_ENV__Code *const*);
SOAP_FMAC3 int SOAP_FMAC4 soap_put_PointerToSOAP_ENV__Code(struct soap*, struct SOAP_ENV__Code *const*, const char*, const char*);
SOAP_FMAC3 int SOAP_FMAC4 soap_out_PointerToSOAP_ENV__Code(struct soap*, const char *, int, struct SOAP_ENV__Code *const*, const char *);
SOAP_FMAC3 struct SOAP_ENV__Code ** SOAP_FMAC4 soap_get_PointerToSOAP_ENV__Code(struct soap*, struct SOAP_ENV__Code **, const char*, const char*);
SOAP_FMAC3 struct SOAP_ENV__Code ** SOAP_FMAC4 soap_in_PointerToSOAP_ENV__Code(struct soap*, const char*, struct SOAP_ENV__Code **, const char*);

#endif

#ifndef SOAP_TYPE_xsd__string
#define SOAP_TYPE_xsd__string (14)
#endif
SOAP_FMAC3 void SOAP_FMAC4 soap_default_xsd__string(struct soap*, char **);
SOAP_FMAC3 void SOAP_FMAC4 soap_serialize_xsd__string(struct soap*, char *const*);
SOAP_FMAC3 int SOAP_FMAC4 soap_put_xsd__string(struct soap*, char *const*, const char*, const char*);
SOAP_FMAC3 int SOAP_FMAC4 soap_out_xsd__string(struct soap*, const char*, int, char*const*, const char*);
SOAP_FMAC3 char ** SOAP_FMAC4 soap_get_xsd__string(struct soap*, char **, const char*, const char*);
SOAP_FMAC3 char * * SOAP_FMAC4 soap_in_xsd__string(struct soap*, const char*, char **, const char*);

#ifndef SOAP_TYPE__QName
#define SOAP_TYPE__QName (5)
#endif
SOAP_FMAC3 void SOAP_FMAC4 soap_default__QName(struct soap*, char **);
SOAP_FMAC3 void SOAP_FMAC4 soap_serialize__QName(struct soap*, char *const*);
SOAP_FMAC3 int SOAP_FMAC4 soap_put__QName(struct soap*, char *const*, const char*, const char*);
SOAP_FMAC3 int SOAP_FMAC4 soap_out__QName(struct soap*, const char*, int, char*const*, const char*);
SOAP_FMAC3 char ** SOAP_FMAC4 soap_get__QName(struct soap*, char **, const char*, const char*);
SOAP_FMAC3 char * * SOAP_FMAC4 soap_in__QName(struct soap*, const char*, char **, const char*);

#ifndef SOAP_TYPE_string
#define SOAP_TYPE_string (3)
#endif
SOAP_FMAC3 void SOAP_FMAC4 soap_default_string(struct soap*, char **);
SOAP_FMAC3 void SOAP_FMAC4 soap_serialize_string(struct soap*, char *const*);
SOAP_FMAC3 int SOAP_FMAC4 soap_put_string(struct soap*, char *const*, const char*, const char*);
SOAP_FMAC3 int SOAP_FMAC4 soap_out_string(struct soap*, const char*, int, char*const*, const char*);
SOAP_FMAC3 char ** SOAP_FMAC4 soap_get_string(struct soap*, char **, const char*, const char*);
SOAP_FMAC3 char * * SOAP_FMAC4 soap_in_string(struct soap*, const char*, char **, const char*);

#endif

/* End of soapH.h */
