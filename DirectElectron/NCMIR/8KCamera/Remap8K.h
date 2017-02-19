

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 6.00.0361 */
/* at Fri Jan 09 08:09:41 2009
 */
/* Compiler settings for C:\Yukiko Working\8Kx8K_CCD\ImageTransformCOM\src\Remap8K\Remap8K.idl:
    Oicf, W1, Zp8, env=Win32 (32b run)
    protocol : dce , ms_ext, c_ext, robust
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
//@@MIDL_FILE_HEADING(  )

#pragma warning( disable: 4049 )  /* more than 64k source lines */


/* verify that the <rpcndr.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 475
#endif

#include "rpc.h"
#include "rpcndr.h"

#ifndef __RPCNDR_H_VERSION__
#error this stub requires an updated version of <rpcndr.h>
#endif // __RPCNDR_H_VERSION__

#ifndef COM_NO_WINDOWS_H
#include "windows.h"
#include "ole2.h"
#endif /*COM_NO_WINDOWS_H*/

#ifndef __Remap8K_h__
#define __Remap8K_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

/* Forward Declarations */ 

#ifndef __ICoRemap8K_FWD_DEFINED__
#define __ICoRemap8K_FWD_DEFINED__
typedef interface ICoRemap8K ICoRemap8K;
#endif 	/* __ICoRemap8K_FWD_DEFINED__ */


#ifndef __CoRemap8K_FWD_DEFINED__
#define __CoRemap8K_FWD_DEFINED__

#ifdef __cplusplus
typedef class CoRemap8K CoRemap8K;
#else
typedef struct CoRemap8K CoRemap8K;
#endif /* __cplusplus */

#endif 	/* __CoRemap8K_FWD_DEFINED__ */


/* header files for imported files */
#include "oaidl.h"
#include "ocidl.h"
#include "FSM_MemoryLib.h"
#include "FSM_MultiImageObjectLib.h"

#ifdef __cplusplus
extern "C"{
#endif 

void * __RPC_USER MIDL_user_allocate(size_t);
void __RPC_USER MIDL_user_free( void * ); 

#ifndef __ICoRemap8K_INTERFACE_DEFINED__
#define __ICoRemap8K_INTERFACE_DEFINED__

/* interface ICoRemap8K */
/* [unique][helpstring][dual][uuid][object] */ 


EXTERN_C const IID IID_ICoRemap8K;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("CFF3F956-C33E-40EE-92E2-359E85340288")
    ICoRemap8K : public IDispatch
    {
    public:
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE RemapAll( void) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE Remap( 
            /* [in] */ short CameraNo) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE SetImageBuffer( 
            /* [in] */ IFSM_MemoryObject *piMemObj) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE SetSimpleImageBuffer( 
            /* [in] */ short bufferNo,
            /* [in] */ ISimpleImage *piSimpleImage) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE SetSrcImageInformation( 
            /* [in] */ short cameraNo,
            /* [in] */ short XUL,
            /* [in] */ short YUL,
            /* [in] */ short XUR,
            /* [in] */ short YUR,
            /* [in] */ short XLL,
            /* [in] */ short YLL,
            /* [in] */ short XLR,
            /* [in] */ short YLR,
            short RefWidth,
            short RefHeight) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE SetSrcImageSize( 
            /* [in] */ short SrcWidth,
            /* [in] */ short SrcHeight) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct ICoRemap8KVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            ICoRemap8K * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            ICoRemap8K * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            ICoRemap8K * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            ICoRemap8K * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            ICoRemap8K * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            ICoRemap8K * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            ICoRemap8K * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *RemapAll )( 
            ICoRemap8K * This);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *Remap )( 
            ICoRemap8K * This,
            /* [in] */ short CameraNo);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *SetImageBuffer )( 
            ICoRemap8K * This,
            /* [in] */ IFSM_MemoryObject *piMemObj);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *SetSimpleImageBuffer )( 
            ICoRemap8K * This,
            /* [in] */ short bufferNo,
            /* [in] */ ISimpleImage *piSimpleImage);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *SetSrcImageInformation )( 
            ICoRemap8K * This,
            /* [in] */ short cameraNo,
            /* [in] */ short XUL,
            /* [in] */ short YUL,
            /* [in] */ short XUR,
            /* [in] */ short YUR,
            /* [in] */ short XLL,
            /* [in] */ short YLL,
            /* [in] */ short XLR,
            /* [in] */ short YLR,
            short RefWidth,
            short RefHeight);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *SetSrcImageSize )( 
            ICoRemap8K * This,
            /* [in] */ short SrcWidth,
            /* [in] */ short SrcHeight);
        
        END_INTERFACE
    } ICoRemap8KVtbl;

    interface ICoRemap8K
    {
        CONST_VTBL struct ICoRemap8KVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ICoRemap8K_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define ICoRemap8K_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define ICoRemap8K_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define ICoRemap8K_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define ICoRemap8K_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define ICoRemap8K_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define ICoRemap8K_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define ICoRemap8K_RemapAll(This)	\
    (This)->lpVtbl -> RemapAll(This)

#define ICoRemap8K_Remap(This,CameraNo)	\
    (This)->lpVtbl -> Remap(This,CameraNo)

#define ICoRemap8K_SetImageBuffer(This,piMemObj)	\
    (This)->lpVtbl -> SetImageBuffer(This,piMemObj)

#define ICoRemap8K_SetSimpleImageBuffer(This,bufferNo,piSimpleImage)	\
    (This)->lpVtbl -> SetSimpleImageBuffer(This,bufferNo,piSimpleImage)

#define ICoRemap8K_SetSrcImageInformation(This,cameraNo,XUL,YUL,XUR,YUR,XLL,YLL,XLR,YLR,RefWidth,RefHeight)	\
    (This)->lpVtbl -> SetSrcImageInformation(This,cameraNo,XUL,YUL,XUR,YUR,XLL,YLL,XLR,YLR,RefWidth,RefHeight)

#define ICoRemap8K_SetSrcImageSize(This,SrcWidth,SrcHeight)	\
    (This)->lpVtbl -> SetSrcImageSize(This,SrcWidth,SrcHeight)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE ICoRemap8K_RemapAll_Proxy( 
    ICoRemap8K * This);


void __RPC_STUB ICoRemap8K_RemapAll_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE ICoRemap8K_Remap_Proxy( 
    ICoRemap8K * This,
    /* [in] */ short CameraNo);


void __RPC_STUB ICoRemap8K_Remap_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE ICoRemap8K_SetImageBuffer_Proxy( 
    ICoRemap8K * This,
    /* [in] */ IFSM_MemoryObject *piMemObj);


void __RPC_STUB ICoRemap8K_SetImageBuffer_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE ICoRemap8K_SetSimpleImageBuffer_Proxy( 
    ICoRemap8K * This,
    /* [in] */ short bufferNo,
    /* [in] */ ISimpleImage *piSimpleImage);


void __RPC_STUB ICoRemap8K_SetSimpleImageBuffer_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE ICoRemap8K_SetSrcImageInformation_Proxy( 
    ICoRemap8K * This,
    /* [in] */ short cameraNo,
    /* [in] */ short XUL,
    /* [in] */ short YUL,
    /* [in] */ short XUR,
    /* [in] */ short YUR,
    /* [in] */ short XLL,
    /* [in] */ short YLL,
    /* [in] */ short XLR,
    /* [in] */ short YLR,
    short RefWidth,
    short RefHeight);


void __RPC_STUB ICoRemap8K_SetSrcImageInformation_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE ICoRemap8K_SetSrcImageSize_Proxy( 
    ICoRemap8K * This,
    /* [in] */ short SrcWidth,
    /* [in] */ short SrcHeight);


void __RPC_STUB ICoRemap8K_SetSrcImageSize_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __ICoRemap8K_INTERFACE_DEFINED__ */



#ifndef __REMAP8KLib_LIBRARY_DEFINED__
#define __REMAP8KLib_LIBRARY_DEFINED__

/* library REMAP8KLib */
/* [helpstring][version][uuid] */ 


EXTERN_C const IID LIBID_REMAP8KLib;

EXTERN_C const CLSID CLSID_CoRemap8K;

#ifdef __cplusplus

class DECLSPEC_UUID("504E087E-618B-40F4-923F-A628398A17B6")
CoRemap8K;
#endif
#endif /* __REMAP8KLib_LIBRARY_DEFINED__ */

/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


