//gsoap ns service name:	stagecommands
//gsoap ns service style:	rpc
//gsoap ns service encoding:	encoded
//gsoap ns service namespace:	urn:xmethods-delayed-stagecommands
//gsoap ns service location:	http://services.xmethods.net/soap

//Standard definitions
typedef double xsd__double;
typedef short xsd__short;
typedef int xsd__int;
typedef float xsd__float;
typedef float xsd__long;
typedef char* xsd__string;


//struct ns__gettwofloatshortResponse{xsd__float floatone;xsd__float floattwo; xsd__short shortone;};
//struct ns__getfivefloatResponse{xsd__float one;xsd__float two;xsd__float three;xsd__float four;xsd__float five;};



//Controls for the Stage
//int ns__initStageWebservice(xsd__float x,xsd__float y, xsd__int &r);
int ns__getCurrentTiltAngle(int a, float &result);
int ns__setTiltAngle(xsd__float tilt,float &result);
int ns__setXYStagePosition(xsd__float x,xsd__float y, float &r);
int ns__setZ(xsd__float z,float &result);
int ns__getX(xsd__float x,float &result);
int ns__getY(xsd__float y,float &result);
int ns__getZ(xsd__float z,float &result);
int ns__setX(xsd__float x,float &result);
int ns__setY(xsd__float y,float &result);



//Mosaic movements
int ns__NextMosaicMove(xsd__int MoveType,float &result);
int ns__QueryMosaicImages(xsd__int Type,int &result);
int ns__BeginMosaic(xsd__int MosaicType,int &result);
int ns__CheckifOk2Acquire(xsd__int MosaicType,int &result);
int ns__SerialGotAcquireToken(xsd__int confirm,int &result);