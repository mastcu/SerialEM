/* this file contains the actual definitions of */
/* the IIDs and CLSIDs */

/* link this file in with the server and any clients */


/* File created by MIDL compiler version 5.01.0164 */
/* at Mon Jun 12 17:14:21 2006
 */
/* Compiler settings for G:\FSM_ImageData\FSM_MemoryLib\FSM_MemoryLib.idl:
    Oicf (OptLev=i2), W1, Zp8, env=Win32, ms_ext, c_ext
    error checks: allocation ref bounds_check enum stub_data 
*/
//@@MIDL_FILE_HEADING(  )
#ifdef __cplusplus
extern "C"{
#endif 


#ifndef __IID_DEFINED__
#define __IID_DEFINED__

typedef struct _IID
{
    unsigned long x;
    unsigned short s1;
    unsigned short s2;
    unsigned char  c[8];
} IID;

#endif // __IID_DEFINED__

#ifndef CLSID_DEFINED
#define CLSID_DEFINED
typedef IID CLSID;
#endif // CLSID_DEFINED

const IID IID_IFSM_MemoryObject = {0xD9F5A46D,0x4DE5,0x47F7,{0xAE,0xF9,0xC0,0x6F,0xE4,0xA6,0x21,0xD8}};


const IID LIBID_FSM_MEMORYLIBLib = {0xD912488D,0xD6BC,0x429E,{0xB6,0x76,0xEA,0x3D,0xE2,0xB6,0xED,0x40}};


const CLSID CLSID_FSM_MemoryObject = {0x3E87B249,0x8D5E,0x4096,{0x8B,0x20,0x95,0xDB,0x61,0xB2,0xC8,0x7D}};


#ifdef __cplusplus
}
#endif

