/* soapstagecommandsProxy.h
   Generated by gSOAP 2.7.3 from .\webserviceCommands.h
   Copyright (C) 2000-2005, Robert van Engelen, Genivia Inc. All Rights Reserved.
   This part of the software is released under one of the following licenses:
   GPL, the gSOAP public license, or Genivia's license for commercial use.
*/

#ifndef soapstagecommands_H
#define soapstagecommands_H
#include "soapH.h"
class stagecommands
{   public:
	struct soap *soap;
	const char *endpoint;
	stagecommands()
	{ soap = soap_new(); endpoint = "http://services.xmethods.net/soap"; if (soap && !soap->namespaces) { static const struct Namespace namespaces[] = 
{
	{"SOAP-ENV", "http://schemas.xmlsoap.org/soap/envelope/", "http://www.w3.org/*/soap-envelope", NULL},
	{"SOAP-ENC", "http://schemas.xmlsoap.org/soap/encoding/", "http://www.w3.org/*/soap-encoding", NULL},
	{"xsi", "http://www.w3.org/2001/XMLSchema-instance", "http://www.w3.org/*/XMLSchema-instance", NULL},
	{"xsd", "http://www.w3.org/2001/XMLSchema", "http://www.w3.org/*/XMLSchema", NULL},
	{"ns", "urn:xmethods-delayed-stagecommands", NULL, NULL},
	{NULL, NULL, NULL, NULL}
};
	soap->namespaces = namespaces; } };
	virtual ~stagecommands() { if (soap) { soap_destroy(soap); soap_end(soap); soap_done(soap); free((void*)soap); } };
	virtual int ns__getCurrentTiltAngle(int a, float &result) { return soap ? soap_call_ns__getCurrentTiltAngle(soap, endpoint, NULL, a, result) : SOAP_EOM; };
	virtual int ns__setTiltAngle(float tilt, float &result) { return soap ? soap_call_ns__setTiltAngle(soap, endpoint, NULL, tilt, result) : SOAP_EOM; };
	virtual int ns__setXYStagePosition(float x, float y, float &r) { return soap ? soap_call_ns__setXYStagePosition(soap, endpoint, NULL, x, y, r) : SOAP_EOM; };
	virtual int ns__setZ(float z, float &result) { return soap ? soap_call_ns__setZ(soap, endpoint, NULL, z, result) : SOAP_EOM; };
	virtual int ns__getX(float x, float &result) { return soap ? soap_call_ns__getX(soap, endpoint, NULL, x, result) : SOAP_EOM; };
	virtual int ns__getY(float y, float &result) { return soap ? soap_call_ns__getY(soap, endpoint, NULL, y, result) : SOAP_EOM; };
	virtual int ns__getZ(float z, float &result) { return soap ? soap_call_ns__getZ(soap, endpoint, NULL, z, result) : SOAP_EOM; };
	virtual int ns__setX(float x, float &result) { return soap ? soap_call_ns__setX(soap, endpoint, NULL, x, result) : SOAP_EOM; };
	virtual int ns__setY(float y, float &result) { return soap ? soap_call_ns__setY(soap, endpoint, NULL, y, result) : SOAP_EOM; };
	virtual int ns__NextMosaicMove(int MoveType, float &result) { return soap ? soap_call_ns__NextMosaicMove(soap, endpoint, NULL, MoveType, result) : SOAP_EOM; };
	virtual int ns__QueryMosaicImages(int Type, int &result) { return soap ? soap_call_ns__QueryMosaicImages(soap, endpoint, NULL, Type, result) : SOAP_EOM; };
	virtual int ns__BeginMosaic(int MosaicType, int &result) { return soap ? soap_call_ns__BeginMosaic(soap, endpoint, NULL, MosaicType, result) : SOAP_EOM; };
	virtual int ns__CheckifOk2Acquire(int MosaicType, int &result) { return soap ? soap_call_ns__CheckifOk2Acquire(soap, endpoint, NULL, MosaicType, result) : SOAP_EOM; };
	virtual int ns__SerialGotAcquireToken(int confirm, int &result) { return soap ? soap_call_ns__SerialGotAcquireToken(soap, endpoint, NULL, confirm, result) : SOAP_EOM; };
};
#endif
