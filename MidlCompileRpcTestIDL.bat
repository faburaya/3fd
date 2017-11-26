call "%VS140COMNTOOLS%\VsDevCmd.bat"

midl /Oicf /use_epv /win32 /client none /out IntegrationTests /h AcmeTesting_w32.h /sstub AcmeTesting_w32_srv.c TestShared\AcmeTesting.idl

midl /Oicf /use_epv /amd64 /client none /out IntegrationTests /h AcmeTesting_x64.h /sstub AcmeTesting_x64_srv.c TestShared\AcmeTesting.idl

midl /Oicf /use_epv /win32 /server none /out TestRpcClient /h AcmeTesting_w32.h /cstub AcmeTesting_w32_cli.c TestShared\AcmeTesting.idl

midl /Oicf /use_epv /amd64 /server none /out TestRpcClient /h AcmeTesting_x64.h /cstub AcmeTesting_x64_cli.c TestShared\AcmeTesting.idl
