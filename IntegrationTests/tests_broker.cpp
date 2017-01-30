#include "stdafx.h"
#include "runtime.h"
#include "broker.h"

namespace _3fd
{
namespace integration_tests
{
    void HandleException();

    TEST(Framework_Broker_TestCase, QueueReaderSetup_Test)
    {
        // Ensures proper initialization/finalization of the framework
        _3fd::core::FrameworkInstance _framework;

        CALL_STACK_TRACE;

        try
        {
            using namespace _3fd::broker;

            QueueReader queueReader(
                Backend::MsSqlServer,
                "Driver={SQL Server Native Client 11.0};Server=(localdb)\\MSSQLLocalDB;Database=SvcBrokerTest;Trusted_Connection=yes;",
                "//SvcBrokerBackend/IntegrationTestService0",
                MessageTypeSpec { 128UL, MessageContentValidation::None },
                0
            );
        }
        catch (...)
        {
            HandleException();
        }
    }

}// end of namespace integration_tests
}// end of namespace _3fd
