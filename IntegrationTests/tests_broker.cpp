#include "stdafx.h"
#include "runtime.h"
#include "broker.h"
#include <algorithm>

namespace _3fd
{
namespace integration_tests
{
    void HandleException();

    /* Tests for the broker module using Microsoft SQL Server can be served by a LocalDB instance,
       however, the database must have service broker enabled. In the root of the repository it is
       found 'CreateMsSqlSvcBrokerDatabase.sql', which can be run to create a database that will
       work with these integration tests. */
    
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

            EXPECT_TRUE(readOp->Commit(0));
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

    /// <summary>
    /// Tests rollback of read/write operations in broker queue.
    /// </summary>
    TEST(Framework_Broker_BasicTestCase, QueueRollback_Test)
    {
        // Ensures proper initialization/finalization of the framework
        _3fd::core::FrameworkInstance _framework;

        CALL_STACK_TRACE;

        try
        {
            using namespace _3fd::broker;

            const uint16_t numMessages(256);
            std::vector<string> insertedMessages;
            insertedMessages.reserve(numMessages);

            // Generate messages to write into the queue:
            std::ostringstream oss;
            for (int idx = 0; idx < numMessages; ++idx)
            {
                oss << "foobar" << std::setw(4) << idx;
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

            EXPECT_TRUE(writeOp->Rollback(0)); // rollback the insertions

            // Read the queue that is supposed to be empty
            auto readOp = queueReader.ReadMessages(numMessages, 0);
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

            EXPECT_TRUE(readOp->Commit(0));

            // Write again asynchronously
            auto writeOp2 = queueWriter.WriteMessages(insertedMessages);

            writeOp2->Rethrow(); // wait for write op to finish...

            EXPECT_TRUE(writeOp2->Commit(0)); // commit the insertions

            // Now read the messages for the first time:

            std::vector<string> selectedMessages;

            uint32_t msgCount;
            auto readOp2 = queueReader.ReadMessages(numMessages, 0);

            while (true)
            {
                readOp2->Step();

                // Await for end of step:
                uint16_t elapsedTime(0);
                while (!readOp2->TryWait(waitInterval))
                {
                    elapsedTime += waitInterval;

                    if (elapsedTime > 5000)
                        throw core::AppException<std::runtime_error>("Timeout: reading from service broker queue took too long!");
                }

                msgCount = readOp2->GetStepMessageCount(0);

                if (msgCount > 0)
                {
                    auto stepRes = readOp2->GetStepResult(0);
                    selectedMessages.insert(selectedMessages.end(), stepRes.begin(), stepRes.end());
                }
                else
                    break;
            }

            EXPECT_EQ(insertedMessages.size(), selectedMessages.size());

            std::sort(selectedMessages.begin(), selectedMessages.end());

            EXPECT_TRUE(std::equal(insertedMessages.begin(), insertedMessages.end(), selectedMessages.begin()));

            EXPECT_TRUE(readOp2->Rollback(0)); // rollback to keep messages in the queue...

            // ... and read the messages for the second time:

            selectedMessages.clear();
            auto readOp3 = queueReader.ReadMessages(numMessages, 0);

            while (true)
            {
                readOp3->Step();

                // Await for end of step:
                uint16_t elapsedTime(0);
                while (!readOp3->TryWait(waitInterval))
                {
                    elapsedTime += waitInterval;

                    if (elapsedTime > 5000)
                        throw core::AppException<std::runtime_error>("Timeout: reading from service broker queue took too long!");
                }

                msgCount = readOp3->GetStepMessageCount(0);

                if (msgCount > 0)
                {
                    auto stepRes = readOp3->GetStepResult(0);
                    selectedMessages.insert(selectedMessages.end(), stepRes.begin(), stepRes.end());
                }
                else
                    break;
            }

            EXPECT_TRUE(readOp3->Commit(0)); // commit to remove the messages from the queue

            EXPECT_EQ(insertedMessages.size(), selectedMessages.size());

            std::sort(selectedMessages.begin(), selectedMessages.end());

            EXPECT_TRUE(std::equal(insertedMessages.begin(), insertedMessages.end(), selectedMessages.begin()));
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
    TEST_P(Framework_Broker_PerformanceTestCase, QueueReadWrite_Test)
    {
        // Ensures proper initialization/finalization of the framework
        _3fd::core::FrameworkInstance _framework;

        CALL_STACK_TRACE;

        try
        {
            using namespace _3fd::broker;

            std::ostringstream oss;
            std::vector<string> insertedMessages;
            insertedMessages.reserve(GetParam());

            // Generate messages to write into the queue:
            for (int idx = 0; idx < GetParam(); ++idx)
            {
                oss << "foobar" << std::setw(4) << idx;
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
            
            EXPECT_TRUE(writeOp->Commit(0));

            // Then read the messages back:
            
            std::vector<string> selectedMessages;

            uint32_t msgCount;
            const uint16_t waitInterval(50);
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

            EXPECT_TRUE(readOp->Commit(0));

            ASSERT_EQ(insertedMessages.size(), selectedMessages.size());

            std::sort(selectedMessages.begin(), selectedMessages.end());

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
