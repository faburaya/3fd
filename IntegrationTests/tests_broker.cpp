#include "stdafx.h"
#include "runtime.h"
#include "broker.h"
#include <algorithm>

namespace _3fd
{
namespace integration_tests
{
    void HandleException();
    
    /// <summary>
    /// Tests the setup of a reader for the broker queue.
    /// </summary>
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
                "//SvcBrokerTests/IntegrationTestService0",
                MessageTypeSpec { 128UL, MessageContentValidation::None },
                0
            );

            // Read the empty queue
            auto readOp = queueReader.ReadMessages(256, 0);

            uint16_t elapsedTime(0);
            uint16_t waitInterval(50);

            // Await for end of async read:
            while (!readOp->TryWait(waitInterval))
            {
                elapsedTime += waitInterval;

                if (elapsedTime > 5000)
                    throw core::AppException<std::runtime_error>("Timeout: reading from service broker queue took too long!");
            }

            EXPECT_EQ(0, readOp->GetStepMessageCount(0));
        }
        catch (...)
        {
            HandleException();
        }
    }

    /// <summary>
    /// Tests the setup of a writer for the broker queue.
    /// </summary>
    TEST(Framework_Broker_TestCase, QueueWriterSetup_Test)
    {
        // Ensures proper initialization/finalization of the framework
        _3fd::core::FrameworkInstance _framework;

        CALL_STACK_TRACE;

        try
        {
            using namespace _3fd::broker;

            QueueWriter queueWriter(
                Backend::MsSqlServer,
                "Driver={SQL Server Native Client 11.0};Server=(localdb)\\MSSQLLocalDB;Database=SvcBrokerTest;Trusted_Connection=yes;",
                "//SvcBrokerTests/IntegrationTestService1",
                "//SvcBrokerTests/IntegrationTestService0",
                MessageTypeSpec{ 128UL, MessageContentValidation::None },
                0
            );
        }
        catch (...)
        {
            HandleException();
        }
    }

    /// <summary>
    /// Tests writing into and reading from broker queue.
    /// </summary>
    TEST(Framework_Broker_TestCase, QueueReadWriteInProc_Test)
    {
        // Ensures proper initialization/finalization of the framework
        _3fd::core::FrameworkInstance _framework;

        CALL_STACK_TRACE;

        try
        {
            using namespace _3fd::broker;

            // Setup the writer:
            QueueWriter queueWriter(
                Backend::MsSqlServer,
                "Driver={SQL Server Native Client 11.0};Server=(localdb)\\MSSQLLocalDB;Database=SvcBrokerTest;Trusted_Connection=yes;",
                "//SvcBrokerTests/IntegrationTestService0",
                "//SvcBrokerTests/IntegrationTestService1",
                MessageTypeSpec{ 128UL, MessageContentValidation::None },
                1
            );

            // Generate messages to write into the queue:
            std::vector<string> insertedMessages;
            std::ostringstream oss;
            for (int idx = 0; idx < 100; ++idx)
            {
                oss << "foobar" << idx;
                insertedMessages.push_back(oss.str());
                oss.str("");
            }

            // Write asynchronously
            auto writeOp = queueWriter.WriteMessages(insertedMessages);

            // Setup the reader:
            QueueReader queueReader(
                Backend::MsSqlServer,
                "Driver={SQL Server Native Client 11.0};Server=(localdb)\\MSSQLLocalDB;Database=SvcBrokerTest;Trusted_Connection=yes;",
                "//SvcBrokerTests/IntegrationTestService1",
                MessageTypeSpec{ 128UL, MessageContentValidation::None },
                1
            );

            writeOp.wait(); // wait for write op to finish...

            // Then read the messages back:
            const uint16_t msgCountStepLimit(256);
            auto readOp = queueReader.ReadMessages(msgCountStepLimit, 0);

            std::vector<string> selectedMessages;
            uint32_t msgCount;
            uint16_t waitInterval(50);

            do
            {
                uint16_t elapsedTime(0);

                // Await for end of step:
                while (!readOp->TryWait(waitInterval))
                {
                    elapsedTime += waitInterval;

                    if (elapsedTime > 5000)
                        throw core::AppException<std::runtime_error>("Timeout: reading from service broker queue took too long!");
                }

                msgCount = readOp->GetStepMessageCount(0);

                if (msgCount > 0)
                {
                    auto stepRes = readOp->GetStepResult(0);
                    selectedMessages.insert(selectedMessages.end(), stepRes.begin(), stepRes.end());
                }
                
            } while (msgCount == msgCountStepLimit);

            ASSERT_EQ(insertedMessages.size(), selectedMessages.size());

            EXPECT_TRUE(std::equal(selectedMessages.begin(), insertedMessages.begin(), insertedMessages.end()));
        }
        catch (...)
        {
            HandleException();
        }
    }

}// end of namespace integration_tests
}// end of namespace _3fd
