call "%VS140COMNTOOLS%\VsDevCmd.bat"

cd IntegrationTests
wsutil /noclient /wsdl:..\TestShared\calculator.wsdl

cd ..\TestWwsClient
wsutil /noservice /wsdl:..\TestShared\calculator.wsdl

cd ..