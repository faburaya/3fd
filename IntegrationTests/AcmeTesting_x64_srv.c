

/* this ALWAYS GENERATED file contains the RPC server stubs */


 /* File created by MIDL compiler version 8.01.0620 */
/* at Tue Jan 19 01:14:07 2038
 */
/* Compiler settings for TestShared\AcmeTesting.idl:
    Oicf, W1, Zp8, env=Win64 (32b run), target_arch=AMD64 8.01.0620 
    protocol : dce , robust
    error checks: allocation ref bounds_check enum stub_data , use_epv
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
/* @@MIDL_FILE_HEADING(  ) */

#if defined(_M_AMD64)


#pragma warning( disable: 4049 )  /* more than 64k source lines */
#if _MSC_VER >= 1200
#pragma warning(push)
#endif

#pragma warning( disable: 4211 )  /* redefine extern to static */
#pragma warning( disable: 4232 )  /* dllimport identity*/
#pragma warning( disable: 4024 )  /* array to pointer mapping*/

#include <string.h>
#include "AcmeTesting_x64.h"

#define TYPE_FORMAT_STRING_SIZE   39                                
#define PROC_FORMAT_STRING_SIZE   151                               
#define EXPR_FORMAT_STRING_SIZE   1                                 
#define TRANSMIT_AS_TABLE_SIZE    0            
#define WIRE_MARSHAL_TABLE_SIZE   0            

typedef struct _AcmeTesting_MIDL_TYPE_FORMAT_STRING
    {
    short          Pad;
    unsigned char  Format[ TYPE_FORMAT_STRING_SIZE ];
    } AcmeTesting_MIDL_TYPE_FORMAT_STRING;

typedef struct _AcmeTesting_MIDL_PROC_FORMAT_STRING
    {
    short          Pad;
    unsigned char  Format[ PROC_FORMAT_STRING_SIZE ];
    } AcmeTesting_MIDL_PROC_FORMAT_STRING;

typedef struct _AcmeTesting_MIDL_EXPR_FORMAT_STRING
    {
    long          Pad;
    unsigned char  Format[ EXPR_FORMAT_STRING_SIZE ];
    } AcmeTesting_MIDL_EXPR_FORMAT_STRING;


static const RPC_SYNTAX_IDENTIFIER  _RpcTransferSyntax = 
{{0x8A885D04,0x1CEB,0x11C9,{0x9F,0xE8,0x08,0x00,0x2B,0x10,0x48,0x60}},{2,0}};

extern const AcmeTesting_MIDL_TYPE_FORMAT_STRING AcmeTesting__MIDL_TypeFormatString;
extern const AcmeTesting_MIDL_PROC_FORMAT_STRING AcmeTesting__MIDL_ProcFormatString;
extern const AcmeTesting_MIDL_EXPR_FORMAT_STRING AcmeTesting__MIDL_ExprFormatString;

/* Standard interface: AcmeTesting, ver. 1.0,
   GUID={0xba209999,0x0c6c,0x11d2,{0x97,0xcf,0x00,0xc0,0x4f,0x8e,0xea,0x45}} */


extern const MIDL_SERVER_INFO AcmeTesting_ServerInfo;

extern const RPC_DISPATCH_TABLE AcmeTesting_v1_0_DispatchTable;

extern const AcmeTesting_v1_0_epv_t DEFAULT_EPV;

static const RPC_SERVER_INTERFACE AcmeTesting___RpcServerInterface =
    {
    sizeof(RPC_SERVER_INTERFACE),
    {{0xba209999,0x0c6c,0x11d2,{0x97,0xcf,0x00,0xc0,0x4f,0x8e,0xea,0x45}},{1,0}},
    {{0x8A885D04,0x1CEB,0x11C9,{0x9F,0xE8,0x08,0x00,0x2B,0x10,0x48,0x60}},{2,0}},
    (RPC_DISPATCH_TABLE*)&AcmeTesting_v1_0_DispatchTable,
    0,
    0,
    (RPC_MGR_EPV*)&DEFAULT_EPV,
    &AcmeTesting_ServerInfo,
    0x04000000
    };
RPC_IF_HANDLE AcmeTesting_v1_0_s_ifspec = (RPC_IF_HANDLE)& AcmeTesting___RpcServerInterface;

extern const MIDL_STUB_DESC AcmeTesting_StubDesc;


#if !defined(__RPC_WIN64__)
#error  Invalid build platform for this stub.
#endif

static const AcmeTesting_MIDL_PROC_FORMAT_STRING AcmeTesting__MIDL_ProcFormatString =
    {
        0,
        {

	/* Procedure Operate */

			0x0,		/* 0 */
			0x4a,		/* Old Flags:  DCE mem package, */
/*  2 */	NdrFcLong( 0x0 ),	/* 0 */
/*  6 */	NdrFcShort( 0x0 ),	/* 0 */
/*  8 */	NdrFcShort( 0x20 ),	/* X64 Stack size/offset = 32 */
/* 10 */	0x32,		/* FC_BIND_PRIMITIVE */
			0x0,		/* 0 */
/* 12 */	NdrFcShort( 0x0 ),	/* X64 Stack size/offset = 0 */
/* 14 */	NdrFcShort( 0x20 ),	/* 32 */
/* 16 */	NdrFcShort( 0x24 ),	/* 36 */
/* 18 */	0x40,		/* Oi2 Flags:  has ext, */
			0x3,		/* 3 */
/* 20 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 22 */	NdrFcShort( 0x0 ),	/* 0 */
/* 24 */	NdrFcShort( 0x0 ),	/* 0 */
/* 26 */	NdrFcShort( 0x0 ),	/* 0 */
/* 28 */	NdrFcShort( 0x28 ),	/* 40 */

	/* Parameter left */

/* 30 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 32 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 34 */	0xc,		/* FC_DOUBLE */
			0x0,		/* 0 */

	/* Parameter right */

/* 36 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 38 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 40 */	0xc,		/* FC_DOUBLE */
			0x0,		/* 0 */

	/* Parameter result */

/* 42 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 44 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 46 */	0xc,		/* FC_DOUBLE */
			0x0,		/* 0 */

	/* Procedure ChangeCase */

/* 48 */	0x0,		/* 0 */
			0x4b,		/* Old Flags:  full ptr, DCE mem package, */
/* 50 */	NdrFcLong( 0x0 ),	/* 0 */
/* 54 */	NdrFcShort( 0x1 ),	/* 1 */
/* 56 */	NdrFcShort( 0x18 ),	/* X64 Stack size/offset = 24 */
/* 58 */	0x32,		/* FC_BIND_PRIMITIVE */
			0x0,		/* 0 */
/* 60 */	NdrFcShort( 0x0 ),	/* X64 Stack size/offset = 0 */
/* 62 */	NdrFcShort( 0x0 ),	/* 0 */
/* 64 */	NdrFcShort( 0x0 ),	/* 0 */
/* 66 */	0x43,		/* Oi2 Flags:  srv must size, clt must size, has ext, */
			0x2,		/* 2 */
/* 68 */	0xa,		/* 10 */
			0x7,		/* Ext Flags:  new corr desc, clt corr check, srv corr check, */
/* 70 */	NdrFcShort( 0x1 ),	/* 1 */
/* 72 */	NdrFcShort( 0x1 ),	/* 1 */
/* 74 */	NdrFcShort( 0x0 ),	/* 0 */
/* 76 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter input */

/* 78 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 80 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 82 */	NdrFcShort( 0x12 ),	/* Type Offset=18 */

	/* Parameter output */

/* 84 */	NdrFcShort( 0x4113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=16 */
/* 86 */	NdrFcShort( 0x10 ),	/* X64 Stack size/offset = 16 */
/* 88 */	NdrFcShort( 0x12 ),	/* Type Offset=18 */

	/* Procedure WriteOnStorage */

/* 90 */	0x0,		/* 0 */
			0x4a,		/* Old Flags:  DCE mem package, */
/* 92 */	NdrFcLong( 0x0 ),	/* 0 */
/* 96 */	NdrFcShort( 0x2 ),	/* 2 */
/* 98 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 100 */	0x32,		/* FC_BIND_PRIMITIVE */
			0x0,		/* 0 */
/* 102 */	NdrFcShort( 0x0 ),	/* X64 Stack size/offset = 0 */
/* 104 */	NdrFcShort( 0x0 ),	/* 0 */
/* 106 */	NdrFcShort( 0x0 ),	/* 0 */
/* 108 */	0x40,		/* Oi2 Flags:  has ext, */
			0x0,		/* 0 */
/* 110 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 112 */	NdrFcShort( 0x0 ),	/* 0 */
/* 114 */	NdrFcShort( 0x0 ),	/* 0 */
/* 116 */	NdrFcShort( 0x0 ),	/* 0 */
/* 118 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Procedure Shutdown */

/* 120 */	0x0,		/* 0 */
			0x4a,		/* Old Flags:  DCE mem package, */
/* 122 */	NdrFcLong( 0x0 ),	/* 0 */
/* 126 */	NdrFcShort( 0x3 ),	/* 3 */
/* 128 */	NdrFcShort( 0x8 ),	/* X64 Stack size/offset = 8 */
/* 130 */	0x32,		/* FC_BIND_PRIMITIVE */
			0x0,		/* 0 */
/* 132 */	NdrFcShort( 0x0 ),	/* X64 Stack size/offset = 0 */
/* 134 */	NdrFcShort( 0x0 ),	/* 0 */
/* 136 */	NdrFcShort( 0x0 ),	/* 0 */
/* 138 */	0x40,		/* Oi2 Flags:  has ext, */
			0x0,		/* 0 */
/* 140 */	0xa,		/* 10 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 142 */	NdrFcShort( 0x0 ),	/* 0 */
/* 144 */	NdrFcShort( 0x0 ),	/* 0 */
/* 146 */	NdrFcShort( 0x0 ),	/* 0 */
/* 148 */	NdrFcShort( 0x0 ),	/* 0 */

			0x0
        }
    };

static const AcmeTesting_MIDL_TYPE_FORMAT_STRING AcmeTesting__MIDL_TypeFormatString =
    {
        0,
        {
			NdrFcShort( 0x0 ),	/* 0 */
/*  2 */	
			0x11, 0xc,	/* FC_RP [alloced_on_stack] [simple_pointer] */
/*  4 */	0xc,		/* FC_DOUBLE */
			0x5c,		/* FC_PAD */
/*  6 */	
			0x11, 0x0,	/* FC_RP */
/*  8 */	NdrFcShort( 0xa ),	/* Offset= 10 (18) */
/* 10 */	
			0x22,		/* FC_C_CSTRING */
			0x44,		/* FC_STRING_SIZED */
/* 12 */	0x17,		/* Corr desc:  field pointer, FC_USHORT */
			0x0,		/*  */
/* 14 */	NdrFcShort( 0x0 ),	/* 0 */
/* 16 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 18 */	
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 20 */	NdrFcShort( 0x10 ),	/* 16 */
/* 22 */	NdrFcShort( 0x0 ),	/* 0 */
/* 24 */	NdrFcShort( 0x6 ),	/* Offset= 6 (30) */
/* 26 */	0x6,		/* FC_SHORT */
			0x42,		/* FC_STRUCTPAD6 */
/* 28 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 30 */	
			0x14, 0x0,	/* FC_FP */
/* 32 */	NdrFcShort( 0xffea ),	/* Offset= -22 (10) */
/* 34 */	
			0x11, 0x4,	/* FC_RP [alloced_on_stack] */
/* 36 */	NdrFcShort( 0xffee ),	/* Offset= -18 (18) */

			0x0
        }
    };

static const unsigned short AcmeTesting_FormatStringOffsetTable[] =
    {
    0,
    48,
    90,
    120
    };


static const MIDL_STUB_DESC AcmeTesting_StubDesc = 
    {
    (void *)& AcmeTesting___RpcServerInterface,
    NdrRpcSsDefaultAllocate,
    NdrRpcSsDefaultFree,
    0,
    0,
    0,
    0,
    0,
    AcmeTesting__MIDL_TypeFormatString.Format,
    1, /* -error bounds_check flag */
    0x50002, /* Ndr library version */
    0,
    0x801026c, /* MIDL Version 8.1.620 */
    0,
    0,
    0,  /* notify & notify_flag routine table */
    0x1, /* MIDL flag */
    0, /* cs routines */
    0,   /* proxy/server info */
    0
    };

static const RPC_DISPATCH_FUNCTION AcmeTesting_table[] =
    {
    NdrServerCall2,
    NdrServerCall2,
    NdrServerCall2,
    NdrServerCall2,
    0
    };
static const RPC_DISPATCH_TABLE AcmeTesting_v1_0_DispatchTable = 
    {
    4,
    (RPC_DISPATCH_FUNCTION*)AcmeTesting_table
    };

static const AcmeTesting_v1_0_epv_t DEFAULT_EPV = 
    {
    Operate,
    ChangeCase,
    WriteOnStorage,
    Shutdown
    };

static const SERVER_ROUTINE AcmeTesting_ServerRoutineTable[] = 
    {
    (SERVER_ROUTINE)Operate,
    (SERVER_ROUTINE)ChangeCase,
    (SERVER_ROUTINE)WriteOnStorage,
    (SERVER_ROUTINE)Shutdown
    };

static const MIDL_SERVER_INFO AcmeTesting_ServerInfo = 
    {
    &AcmeTesting_StubDesc,
    AcmeTesting_ServerRoutineTable,
    AcmeTesting__MIDL_ProcFormatString.Format,
    AcmeTesting_FormatStringOffsetTable,
    0,
    0,
    0,
    0};
#if _MSC_VER >= 1200
#pragma warning(pop)
#endif


#endif /* defined(_M_AMD64)*/

