call "%VS140COMNTOOLS%\VsDevCmd.bat"

cd IntegrationTests

midl /osf /Oicf /use_epv /win32 /client none /h AcmeTesting_w32.h /sstub AcmeTesting_w32_srv.c AcmeTesting.idl
midl /osf /Oicf /use_epv /amd64 /client none /h AcmeTesting_x64.h /sstub AcmeTesting_x64_srv.c AcmeTesting.idl

cd ..\TestClient

midl /osf /Oicf /use_epv /win32 /server none /h AcmeTesting_w32.h /cstub AcmeTesting_w32_cli.c ..\IntegrationTests\AcmeTesting.idl
midl /osf /Oicf /use_epv /amd64 /server none /h AcmeTesting_x64.h /cstub AcmeTesting_x64_cli.c ..\IntegrationTests\AcmeTesting.idl

cd ..
