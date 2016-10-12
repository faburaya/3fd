call "%VS140COMNTOOLS%\VsDevCmd.bat"

cd IntegrationTests
wsutil /noclient /wsdl:calculator.wsdl

cd ..\TestClient
wsutil /noservice /wsdl:IntegrationTests\calculator.wsdl
