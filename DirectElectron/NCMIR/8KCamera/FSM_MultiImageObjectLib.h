

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 6.00.0361 */
/* at Thu Oct 09 07:55:11 2008
 */
/* Compiler settings for C:\YUKIKO WORKING\FSM_MultiImageObjectLib\FSM_MultiImageObjectLib.idl:
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

#ifndef __FSM_MultiImageObjectLib_h__
#define __FSM_MultiImageObjectLib_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

/* Forward Declarations */ 

#ifndef __IImageObject_FWD_DEFINED__
#define __IImageObject_FWD_DEFINED__
typedef interface IImageObject IImageObject;
#endif 	/* __IImageObject_FWD_DEFINED__ */


#ifndef __ISimpleImage_FWD_DEFINED__
#define __ISimpleImage_FWD_DEFINED__
typedef interface ISimpleImage ISimpleImage;
#endif 	/* __ISimpleImage_FWD_DEFINED__ */


#ifndef __ImageObject_FWD_DEFINED__
#define __ImageObject_FWD_DEFINED__

#ifdef __cplusplus
typedef class ImageObject ImageObject;
#else
typedef struct ImageObject ImageObject;
#endif /* __cplusplus */

#endif 	/* __ImageObject_FWD_DEFINED__ */


#ifndef __SimpleImage_FWD_DEFINED__
#define __SimpleImage_FWD_DEFINED__

#ifdef __cplusplus
typedef class SimpleImage SimpleImage;
#else
typedef struct SimpleImage SimpleImage;
#endif /* __cplusplus */

#endif 	/* __SimpleImage_FWD_DEFINED__ */


/* header files for imported files */
#include "oaidl.h"
#include "ocidl.h"

#ifdef __cplusplus
extern "C"{
#endif 

void * __RPC_USER MIDL_user_allocate(size_t);
void __RPC_USER MIDL_user_free( void * ); 

/* interface __MIDL_itf_FSM_MultiImageObjectLib_0000 */
/* [local] */ 

struct Rect
    {
    unsigned long ulLeft;
    unsigned long ulTop;
    unsigned long ulRight;
    unsigned long ulBottom;
    } ;
struct SimpleImageParameters
    {
    unsigned long ulNumberOfFrame;
    unsigned long ulNumberOfChannel;
    unsigned long ulWidth;
    unsigned long ulHeight;
    unsigned long ulBitDepth;
    } ;


extern RPC_IF_HANDLE __MIDL_itf_FSM_MultiImageObjectLib_0000_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_FSM_MultiImageObjectLib_0000_v0_0_s_ifspec;

#ifndef __IImageObject_INTERFACE_DEFINED__
#define __IImageObject_INTERFACE_DEFINED__

/* interface IImageObject */
/* [unique][helpstring][dual][uuid][object] */ 


EXTERN_C const IID IID_IImageObject;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("29EA315F-FABA-43B6-92EC-2B2439AF960F")
    IImageObject : public IDispatch
    {
    public:
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE Allocate( 
            /* [in] */ int ImageNo,
            /* [in] */ long width,
            /* [in] */ long height,
            /* [in] */ int depth) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE Free( void) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE GetDataPtr( 
            /* [in] */ int ImageNo,
            /* [out] */ unsigned long **plData) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_ImageWidth( 
            /* [in] */ int ImageNo,
            /* [retval][out] */ long *pVal) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_ImageHeight( 
            /* [in] */ int ImageNo,
            /* [retval][out] */ long *pVal) = 0;
        
        virtual /* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_ImageDepth( 
            /* [in] */ int ImageNo,
            /* [retval][out] */ short *pVal) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IImageObjectVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IImageObject * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IImageObject * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IImageObject * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IImageObject * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IImageObject * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IImageObject * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IImageObject * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *Allocate )( 
            IImageObject * This,
            /* [in] */ int ImageNo,
            /* [in] */ long width,
            /* [in] */ long height,
            /* [in] */ int depth);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *Free )( 
            IImageObject * This);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *GetDataPtr )( 
            IImageObject * This,
            /* [in] */ int ImageNo,
            /* [out] */ unsigned long **plData);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_ImageWidth )( 
            IImageObject * This,
            /* [in] */ int ImageNo,
            /* [retval][out] */ long *pVal);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_ImageHeight )( 
            IImageObject * This,
            /* [in] */ int ImageNo,
            /* [retval][out] */ long *pVal);
        
        /* [helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_ImageDepth )( 
            IImageObject * This,
            /* [in] */ int ImageNo,
            /* [retval][out] */ short *pVal);
        
        END_INTERFACE
    } IImageObjectVtbl;

    interface IImageObject
    {
        CONST_VTBL struct IImageObjectVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IImageObject_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IImageObject_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IImageObject_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IImageObject_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IImageObject_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IImageObject_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IImageObject_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IImageObject_Allocate(This,ImageNo,width,height,depth)	\
    (This)->lpVtbl -> Allocate(This,ImageNo,width,height,depth)

#define IImageObject_Free(This)	\
    (This)->lpVtbl -> Free(This)

#define IImageObject_GetDataPtr(This,ImageNo,plData)	\
    (This)->lpVtbl -> GetDataPtr(This,ImageNo,plData)

#define IImageObject_get_ImageWidth(This,ImageNo,pVal)	\
    (This)->lpVtbl -> get_ImageWidth(This,ImageNo,pVal)

#define IImageObject_get_ImageHeight(This,ImageNo,pVal)	\
    (This)->lpVtbl -> get_ImageHeight(This,ImageNo,pVal)

#define IImageObject_get_ImageDepth(This,ImageNo,pVal)	\
    (This)->lpVtbl -> get_ImageDepth(This,ImageNo,pVal)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IImageObject_Allocate_Proxy( 
    IImageObject * This,
    /* [in] */ int ImageNo,
    /* [in] */ long width,
    /* [in] */ long height,
    /* [in] */ int depth);


void __RPC_STUB IImageObject_Allocate_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IImageObject_Free_Proxy( 
    IImageObject * This);


void __RPC_STUB IImageObject_Free_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IImageObject_GetDataPtr_Proxy( 
    IImageObject * This,
    /* [in] */ int ImageNo,
    /* [out] */ unsigned long **plData);


void __RPC_STUB IImageObject_GetDataPtr_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IImageObject_get_ImageWidth_Proxy( 
    IImageObject * This,
    /* [in] */ int ImageNo,
    /* [retval][out] */ long *pVal);


void __RPC_STUB IImageObject_get_ImageWidth_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IImageObject_get_ImageHeight_Proxy( 
    IImageObject * This,
    /* [in] */ int ImageNo,
    /* [retval][out] */ long *pVal);


void __RPC_STUB IImageObject_get_ImageHeight_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE IImageObject_get_ImageDepth_Proxy( 
    IImageObject * This,
    /* [in] */ int ImageNo,
    /* [retval][out] */ short *pVal);


void __RPC_STUB IImageObject_get_ImageDepth_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IImageObject_INTERFACE_DEFINED__ */


#ifndef __ISimpleImage_INTERFACE_DEFINED__
#define __ISimpleImage_INTERFACE_DEFINED__

/* interface ISimpleImage */
/* [unique][helpstring][dual][uuid][object] */ 


EXTERN_C const IID IID_ISimpleImage;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("9BE4FD23-C61B-4211-8220-DF55F9BC621C")
    ISimpleImage : public IDispatch
    {
    public:
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE AllocateSimpleImageBuffer( 
            /* [in] */ struct SimpleImageParameters tParam) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE FreeImageBuffer( void) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE IsImageBufferAllocated( 
            /* [out] */ BOOLEAN *bIsAllocated) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE GetSimpleImageBufferParameters( 
            /* [out] */ struct SimpleImageParameters *ptParam) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE SetROI( 
            /* [in] */ struct Rect tRect) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE GetROI( 
            /* [in] */ struct Rect *ptRect) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE GetDataPtr( 
            /* [out] */ unsigned long **pDataPtr) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE GetImagePtr( 
            /* [out] */ unsigned long **pImagePtr) = 0;
        
        virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE GetFramePtr( 
            /* [in] */ unsigned long ulFrameNumber,
            /* [out] */ unsigned long **pulFramePtr) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct ISimpleImageVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            ISimpleImage * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            ISimpleImage * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            ISimpleImage * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            ISimpleImage * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            ISimpleImage * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            ISimpleImage * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            ISimpleImage * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *AllocateSimpleImageBuffer )( 
            ISimpleImage * This,
            /* [in] */ struct SimpleImageParameters tParam);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *FreeImageBuffer )( 
            ISimpleImage * This);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *IsImageBufferAllocated )( 
            ISimpleImage * This,
            /* [out] */ BOOLEAN *bIsAllocated);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *GetSimpleImageBufferParameters )( 
            ISimpleImage * This,
            /* [out] */ struct SimpleImageParameters *ptParam);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *SetROI )( 
            ISimpleImage * This,
            /* [in] */ struct Rect tRect);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *GetROI )( 
            ISimpleImage * This,
            /* [in] */ struct Rect *ptRect);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *GetDataPtr )( 
            ISimpleImage * This,
            /* [out] */ unsigned long **pDataPtr);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *GetImagePtr )( 
            ISimpleImage * This,
            /* [out] */ unsigned long **pImagePtr);
        
        /* [helpstring][id] */ HRESULT ( STDMETHODCALLTYPE *GetFramePtr )( 
            ISimpleImage * This,
            /* [in] */ unsigned long ulFrameNumber,
            /* [out] */ unsigned long **pulFramePtr);
        
        END_INTERFACE
    } ISimpleImageVtbl;

    interface ISimpleImage
    {
        CONST_VTBL struct ISimpleImageVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ISimpleImage_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define ISimpleImage_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define ISimpleImage_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define ISimpleImage_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define ISimpleImage_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define ISimpleImage_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define ISimpleImage_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define ISimpleImage_AllocateSimpleImageBuffer(This,tParam)	\
    (This)->lpVtbl -> AllocateSimpleImageBuffer(This,tParam)

#define ISimpleImage_FreeImageBuffer(This)	\
    (This)->lpVtbl -> FreeImageBuffer(This)

#define ISimpleImage_IsImageBufferAllocated(This,bIsAllocated)	\
    (This)->lpVtbl -> IsImageBufferAllocated(This,bIsAllocated)

#define ISimpleImage_GetSimpleImageBufferParameters(This,ptParam)	\
    (This)->lpVtbl -> GetSimpleImageBufferParameters(This,ptParam)

#define ISimpleImage_SetROI(This,tRect)	\
    (This)->lpVtbl -> SetROI(This,tRect)

#define ISimpleImage_GetROI(This,ptRect)	\
    (This)->lpVtbl -> GetROI(This,ptRect)

#define ISimpleImage_GetDataPtr(This,pDataPtr)	\
    (This)->lpVtbl -> GetDataPtr(This,pDataPtr)

#define ISimpleImage_GetImagePtr(This,pImagePtr)	\
    (This)->lpVtbl -> GetImagePtr(This,pImagePtr)

#define ISimpleImage_GetFramePtr(This,ulFrameNumber,pulFramePtr)	\
    (This)->lpVtbl -> GetFramePtr(This,ulFrameNumber,pulFramePtr)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE ISimpleImage_AllocateSimpleImageBuffer_Proxy( 
    ISimpleImage * This,
    /* [in] */ struct SimpleImageParameters tParam);


void __RPC_STUB ISimpleImage_AllocateSimpleImageBuffer_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE ISimpleImage_FreeImageBuffer_Proxy( 
    ISimpleImage * This);


void __RPC_STUB ISimpleImage_FreeImageBuffer_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE ISimpleImage_IsImageBufferAllocated_Proxy( 
    ISimpleImage * This,
    /* [out] */ BOOLEAN *bIsAllocated);


void __RPC_STUB ISimpleImage_IsImageBufferAllocated_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE ISimpleImage_GetSimpleImageBufferParameters_Proxy( 
    ISimpleImage * This,
    /* [out] */ struct SimpleImageParameters *ptParam);


void __RPC_STUB ISimpleImage_GetSimpleImageBufferParameters_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE ISimpleImage_SetROI_Proxy( 
    ISimpleImage * This,
    /* [in] */ struct Rect tRect);


void __RPC_STUB ISimpleImage_SetROI_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE ISimpleImage_GetROI_Proxy( 
    ISimpleImage * This,
    /* [in] */ struct Rect *ptRect);


void __RPC_STUB ISimpleImage_GetROI_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE ISimpleImage_GetDataPtr_Proxy( 
    ISimpleImage * This,
    /* [out] */ unsigned long **pDataPtr);


void __RPC_STUB ISimpleImage_GetDataPtr_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE ISimpleImage_GetImagePtr_Proxy( 
    ISimpleImage * This,
    /* [out] */ unsigned long **pImagePtr);


void __RPC_STUB ISimpleImage_GetImagePtr_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][id] */ HRESULT STDMETHODCALLTYPE ISimpleImage_GetFramePtr_Proxy( 
    ISimpleImage * This,
    /* [in] */ unsigned long ulFrameNumber,
    /* [out] */ unsigned long **pulFramePtr);


void __RPC_STUB ISimpleImage_GetFramePtr_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __ISimpleImage_INTERFACE_DEFINED__ */



#ifndef __FSM_MULTIIMAGEOBJECTLIBLib_LIBRARY_DEFINED__
#define __FSM_MULTIIMAGEOBJECTLIBLib_LIBRARY_DEFINED__

/* library FSM_MULTIIMAGEOBJECTLIBLib */
/* [helpstring][version][uuid] */ 


EXTERN_C const IID LIBID_FSM_MULTIIMAGEOBJECTLIBLib;

EXTERN_C const CLSID CLSID_ImageObject;

#ifdef __cplusplus

class DECLSPEC_UUID("E32A3833-BF51-4ECC-BA7C-DE1F7EFB2A6E")
ImageObject;
#endif

EXTERN_C const CLSID CLSID_SimpleImage;

#ifdef __cplusplus

class DECLSPEC_UUID("EB88546C-AA74-4C0F-A442-668A03CCD176")
SimpleImage;
#endif
#endif /* __FSM_MULTIIMAGEOBJECTLIBLib_LIBRARY_DEFINED__ */

/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


