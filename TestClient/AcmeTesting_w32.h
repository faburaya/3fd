

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 8.00.0613 */
/* at Tue Jan 19 01:14:07 2038
 */
/* Compiler settings for ..\IntegrationTests\AcmeTesting.idl:
    Oicf, W1, Zp8, env=Win32 (32b run), target_arch=X86 8.00.0613 
    protocol : dce , robust
    error checks: allocation ref bounds_check enum stub_data , use_epv
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
/* @@MIDL_FILE_HEADING(  ) */

#pragma warning( disable: 4049 )  /* more than 64k source lines */


/* verify that the <rpcndr.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 475
#endif

#include "rpc.h"
#include "rpcndr.h"

#ifndef __RPCNDR_H_VERSION__
#error this stub requires an updated version of <rpcndr.h>
#endif /* __RPCNDR_H_VERSION__ */


#ifndef __AcmeTesting_w32_h__
#define __AcmeTesting_w32_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

/* Forward Declarations */ 

#ifdef __cplusplus
extern "C"{
#endif 


#ifndef __AcmeTesting_INTERFACE_DEFINED__
#define __AcmeTesting_INTERFACE_DEFINED__

/* interface AcmeTesting */
/* [version][uuid] */ 

void Multiply( 
    /* [in] */ handle_t IDL_handle,
    /* [in] */ double left,
    /* [in] */ double right,
    /* [out] */ double *result);

typedef /* [public][public] */ struct __MIDL_AcmeTesting_0001
    {
    unsigned short size;
    /* [size_is][string] */ unsigned char *data;
    } 	cstring;

void ToUpperCase( 
    /* [in] */ handle_t IDL_handle,
    /* [string][in] */ unsigned char *input,
    /* [out] */ cstring *output);

void Shutdown( 
    /* [in] */ handle_t IDL_handle);



typedef struct _AcmeTesting_v1_0_epv_t
{
    void ( *Multiply )( 
        /* [in] */ handle_t IDL_handle,
        /* [in] */ double left,
        /* [in] */ double right,
        /* [out] */ double *result);
    void ( *ToUpperCase )( 
        /* [in] */ handle_t IDL_handle,
        /* [string][in] */ unsigned char *input,
        /* [out] */ cstring *output);
    void ( *Shutdown )( 
        /* [in] */ handle_t IDL_handle);
    
    } AcmeTesting_v1_0_epv_t;

extern RPC_IF_HANDLE AcmeTesting_v1_0_c_ifspec;
extern RPC_IF_HANDLE AcmeTesting_v1_0_s_ifspec;
#endif /* __AcmeTesting_INTERFACE_DEFINED__ */

/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


