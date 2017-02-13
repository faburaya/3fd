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
    TEST(Framework_Broker_BasicTestCase, QueueReaderSetup_Test)
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
                "//SvcBrokerTest/IntegrationTestService",
                MessageTypeSpec { 128UL, MessageContentValidation::None }
            );

            // Read the empty queue
            auto readOp = queueReader.ReadMessages(512, 0);
            readOp->Step();

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
    TEST(Framework_Broker_BasicTestCase, QueueWriterSetup_Test)
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
                "//SvcBrokerTest/IntegrationTestService",
                MessageTypeSpec{ 128UL, MessageContentValidation::None }
            );
        }
        catch (...)
        {
            HandleException();
        }
    }

    class Framework_Broker_PerformanceTestCase : public ::testing::TestWithParam<size_t> { };

    /// <summary>
    /// Tests writing into and reading from broker queue.
    /// </summary>
    TEST_P(Framework_Broker_PerformanceTestCase, QueueReadWriteInProc_Test)
    {
        // Ensures proper initialization/finalization of the framework
        _3fd::core::FrameworkInstance _framework;

        CALL_STACK_TRACE;

        try
        {
            using namespace _3fd::broker;

            // Generate messages to write into the queue:
            std::ostringstream oss;
            std::vector<string> insertedMessages;
            for (int idx = 0; idx < GetParam(); ++idx)
            {
                oss << "foobar" << idx;
                insertedMessages.push_back(oss.str());
                oss.str("");
            }

            // Setup the writer:
            QueueWriter queueWriter(
                Backend::MsSqlServer,
                "Driver={SQL Server Native Client 11.0};Server=(localdb)\\MSSQLLocalDB;Database=SvcBrokerTest;Trusted_Connection=yes;",
                "//SvcBrokerTest/IntegrationTestService",
                MessageTypeSpec{ 128UL, MessageContentValidation::None }
            );

            // Write asynchronously
            auto writeOp = queueWriter.WriteMessages(insertedMessages);

            // Setup the reader:
            QueueReader queueReader(
                Backend::MsSqlServer,
                "Driver={SQL Server Native Client 11.0};Server=(localdb)\\MSSQLLocalDB;Database=SvcBrokerTest;Trusted_Connection=yes;",
                "//SvcBrokerTest/IntegrationTestService",
                MessageTypeSpec{ 128UL, MessageContentValidation::None }
            );

            writeOp->Rethrow(); // wait for write op to finish...

            // Then read the messages back:
            std::vector<string> selectedMessages;
            uint32_t msgCount;
            uint16_t waitInterval(50);
            const uint16_t msgCountStepLimit(512);
            auto readOp = queueReader.ReadMessages(msgCountStepLimit, 0);

            do
            {
                readOp->Step();

                // Await for end of step:
                uint16_t elapsedTime(0);
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

            EXPECT_TRUE(std::equal(insertedMessages.begin(), insertedMessages.end(), selectedMessages.begin()));
        }
        catch (...)
        {
            HandleException();
        }
    }

    INSTANTIATE_TEST_CASE_P(QueueReadWriteInProc_Test,
        Framework_Broker_PerformanceTestCase,
        ::testing::Values(256, 512, 1024, 2048));

}// end of namespace integration_tests
}// end of namespace _3fd
