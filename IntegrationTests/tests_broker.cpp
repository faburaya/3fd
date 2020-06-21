//
// Copyright (c) 2020 Part of 3FD project (https://github.com/faburaya/3fd)
// It is FREELY distributed by the author under the Microsoft Public License
// and the observance that it should only be used for the benefit of mankind.
//
#include "pch.h"
#include <3fd/core/runtime.h>
#include <3fd/core/configuration.h>
#include <3fd/broker/broker.h>
#include <3fd/utils/text.h>

#ifdef _WIN32
#   define NANODBC_ENABLE_UNICODE
#endif

#include <nanodbc/nanodbc.h>
#include <algorithm>
#include <array>
#include <chrono>

#define BROKER_DB_NAME      "SvcBrokerTest"
#define BROKER_SVC_VERSION  "/v1_0_0"
#define BROKER_SERVICE_URL  "//" BROKER_DB_NAME "/IntegrationTestService"
#define BROKER_QUEUE_URL    BROKER_SERVICE_URL BROKER_SVC_VERSION "/Queue"

#define UNDER_BROKER_DB_RESETCMD "<< command to reset broker database not found in test configuration! >>"
#define UNDEF_BROKER_DB_CONNSTR  "<< connection string for the broker back-end not found in test configuration! >>"

#ifdef _WIN32
// nanodbc for Windows uses UCS-2
#   define _U(TEXT) L ## TEXT
#else
// nanodbc for POSIX uses UTF-8
#   define _U(TEXT) TEXT
#endif

namespace _3fd
{
namespace integration_tests
{
    void HandleException();

    /* Tests for the broker module using Microsoft SQL Server can be served by a LocalDB instance,
       however, the database must have service broker enabled. In the root of the repository it is
       found 'CreateMsSqlSvcBrokerDatabase.sql', which can be run to create a database that will
       work with these integration tests. */

#ifdef _WIN32
    const char *keyForBrokerDbConnStr("testBrokerMsSqlDbConnStringForWindows");
    const char *keyForBrokerDbResetCmd("testBrokerResetCommandForWindows");
#else
    const char *keyForBrokerDbConnStr("testBrokerMsSqlDbConnStringForLinux");
    const char *keyForBrokerDbResetCmd("testBrokerResetCommandForLinux");
    const char *keyForBrokerDbFixCmd("testBrokerFixDbCommandForLinux");
#endif

    /// <summary>
    /// Test fixture for the broker module.
    /// </summary>
    class BrokerQueueTestCase : public ::testing::Test
    {
    private:

        // Ensures proper initialization/finalization of the framework
        _3fd::core::FrameworkInstance _framework;

    public:

        // provides a connection to the database
        static nanodbc::connection GetDatabaseConnection()
        {
            return nanodbc::connection(
                utils::to_unicode(core::AppConfig::GetSettings().application.GetString(keyForBrokerDbConnStr,
                                                                                       UNDEF_BROKER_DB_CONNSTR))
            );
        }

        // generates text messages to be stored in the broker queue
        static std::vector<std::string> GenerateMessages(uint16_t count)
        {
            std::vector<std::string> messages;
            messages.reserve(count);

            // Generate messages to write into the queue:
            for (uint16_t idx = 0; idx < count; ++idx)
            {
                std::array<char, 32> buffer;
                snprintf(buffer.data(), buffer.size(), "foobar %3d", idx);
                messages.push_back(buffer.data());
            }

            return messages;
        }

        // waits for broker queue operation with a timeout
        static void WaitFor(_3fd::broker::IAsyncDatabaseOperation &op)
        {
            using std::chrono::seconds;
            using std::chrono::milliseconds;

            milliseconds elapsedTime(0);
            milliseconds waitInterval(50);
            
            // Await for end of async read:
            while (op.wait_for(waitInterval) != std::future_status::ready)
            {
                elapsedTime += waitInterval;

                if (elapsedTime > seconds(5))
                    throw core::AppException<std::runtime_error>("Timeout: broker queue operation took too long!");
            }

            op.get();
        }

        /// <summary>
        /// Set up the test fixture:
        /// Restore database backup to clean-up.
        /// </summary>
        virtual void SetUp() override
        {
            const std::string dbResetCommand =
                core::AppConfig::GetSettings().application.GetString(keyForBrokerDbResetCmd,
																	 UNDER_BROKER_DB_RESETCMD);
			system(dbResetCommand.c_str());

        // linux connects to SQL Server (rather than SQLLocalDB)
        // which needs a fix post backup restore:
#       ifndef _WIN32
            const std::string dbFixCommand =
                core::AppConfig::GetSettings().application.GetString(keyForBrokerDbFixCmd,
																	 UNDER_BROKER_DB_RESETCMD);
			system(dbFixCommand.c_str());
#       endif
        }

        /// <summary>
        /// Tear down the test fixture.
        /// </summary>
        virtual void TearDown() override
        {
        }

    }; // end of class BrokerQueueTestCase

    /// <summary>
    /// Tests the setup of a reader for the broker queue.
    /// </summary>
    TEST_F(BrokerQueueTestCase, QueueReader_SetupTest)
    {
        CALL_STACK_TRACE;

        try
        {
            using namespace _3fd::broker;

            QueueReader queueReader(
                Backend::MsSqlServer,
                core::AppConfig::GetSettings().application.GetString(keyForBrokerDbConnStr, UNDEF_BROKER_DB_CONNSTR),
                BROKER_SERVICE_URL,
                MessageTypeSpec { 128UL, MessageContentValidation::None }
            );
        }
        catch (...)
        {
            HandleException();
        }
    }

    /// <summary>
    /// Tests reading an empty broker queue.
    /// </summary>
    TEST_F(BrokerQueueTestCase, QueueReader_ReadEmptyQueueTest)
    {
        CALL_STACK_TRACE;

        try
        {
            using namespace _3fd::broker;

            QueueReader queueReader(
                Backend::MsSqlServer,
                core::AppConfig::GetSettings().application.GetString(keyForBrokerDbConnStr, UNDEF_BROKER_DB_CONNSTR),
                BROKER_SERVICE_URL,
                MessageTypeSpec{ 128UL, MessageContentValidation::None }
            );

            // Read the empty queue
            std::vector<std::string> retrievedMessages;
            auto readOp = queueReader.ReadMessages(512, 0,
                [&retrievedMessages](std::vector<std::string> &&messages)
                {
                    retrievedMessages = std::move(messages);
                });

            WaitFor(*readOp);

            EXPECT_EQ(0, retrievedMessages.size());
        }
        catch (...)
        {
            HandleException();
        }
    }

    /// <summary>
    /// Tests the setup of a writer for the broker queue.
    /// </summary>
    TEST_F(BrokerQueueTestCase, QueueWriter_SetupTest)
    {
        CALL_STACK_TRACE;

        try
        {
            using namespace _3fd::broker;

            QueueWriter queueWriter(
                Backend::MsSqlServer,
                core::AppConfig::GetSettings().application.GetString(keyForBrokerDbConnStr, UNDEF_BROKER_DB_CONNSTR),
                BROKER_SERVICE_URL,
                MessageTypeSpec{ 128UL, MessageContentValidation::None }
            );
        }
        catch (...)
        {
            HandleException();
        }
    }

    /// <summary>
    /// Tests writing nothing to broker queue.
    /// </summary>
    TEST_F(BrokerQueueTestCase, QueueWriter_WriteZeroMessagesTest)
    {
        CALL_STACK_TRACE;

        try
        {
            using namespace _3fd::broker;

            // Setup the writer:
            QueueWriter queueWriter(
                Backend::MsSqlServer,
                core::AppConfig::GetSettings().application.GetString(keyForBrokerDbConnStr, UNDEF_BROKER_DB_CONNSTR),
                BROKER_SERVICE_URL,
                MessageTypeSpec{ 128UL, MessageContentValidation::None }
            );

            // Write asynchronously:
            auto writeOp = queueWriter.WriteMessages(std::vector<std::string>());

            WaitFor(*writeOp);

            ///////////////////////
            // VERIFY DATABASE:

            auto conn = GetDatabaseConnection();
            auto result = nanodbc::execute(conn, _U("select count(1) from [" BROKER_QUEUE_URL "] WITH(NOLOCK);"));
            ASSERT_TRUE(result.next());
            EXPECT_EQ(0, result.get<int>(0));
        }
        catch (...)
        {
            HandleException();
        }
    }

    /// <summary>
    /// Tests writing some messages to broker queue.
    /// </summary>
    TEST_F(BrokerQueueTestCase, QueueWriter_WriteMessagesTest)
    {
        CALL_STACK_TRACE;

        try
        {
            using namespace _3fd::broker;

            const uint16_t numMessages(10);
            std::vector<std::string> insertedMessages = GenerateMessages(numMessages);

            // Setup the writer:
            QueueWriter queueWriter(
                Backend::MsSqlServer,
                core::AppConfig::GetSettings().application.GetString(keyForBrokerDbConnStr, UNDEF_BROKER_DB_CONNSTR),
                BROKER_SERVICE_URL,
                MessageTypeSpec{ 128UL, MessageContentValidation::None }
            );

            // Write asynchronously:
            auto writeOp = queueWriter.WriteMessages(insertedMessages);

            WaitFor(*writeOp);

            ///////////////////////
            // VERIFY DATABASE:

            auto conn = GetDatabaseConnection();
            auto result = nanodbc::execute(conn, _U("select count(1) from [" BROKER_QUEUE_URL "] WITH(NOLOCK);"));
            ASSERT_TRUE(result.next());
            EXPECT_EQ(numMessages, result.get<int>(0));
        }
        catch (...)
        {
            HandleException();
        }
    }

    /// <summary>
    /// Tests writing messages in the broker queue and then reading them back from it.
    /// </summary>
    TEST_F(BrokerQueueTestCase, QueueReaderAndWriter_ConversationTest)
    {
        CALL_STACK_TRACE;

        try
        {
            using namespace _3fd::broker;

            const uint16_t numMessagesToWrite(64);
            std::vector<std::string> insertedMessages = GenerateMessages(numMessagesToWrite);

            // Setup the writer:
            QueueWriter queueWriter(
                Backend::MsSqlServer,
                core::AppConfig::GetSettings().application.GetString(keyForBrokerDbConnStr, UNDEF_BROKER_DB_CONNSTR),
                BROKER_SERVICE_URL,
                MessageTypeSpec{ 128UL, MessageContentValidation::None }
            );

            // Write asynchronously:
            auto writeOp = queueWriter.WriteMessages(insertedMessages);

            // Setup the reader:
            QueueReader queueReader(
                Backend::MsSqlServer,
                core::AppConfig::GetSettings().application.GetString(keyForBrokerDbConnStr, UNDEF_BROKER_DB_CONNSTR),
                BROKER_SERVICE_URL,
                MessageTypeSpec{ 128UL, MessageContentValidation::None }
            );

            WaitFor(*writeOp);

            // Read the messages back from the queue:
            const uint16_t maxNumMessagesToRead(2 * numMessagesToWrite);
            std::vector<std::string> retrievedMessages;
            auto readOp = queueReader.ReadMessages(maxNumMessagesToRead, 0,
                [&retrievedMessages](std::vector<std::string> &&messages)
                {
                    retrievedMessages = std::move(messages);
                });

            WaitFor(*readOp);

            EXPECT_EQ(insertedMessages.size(), retrievedMessages.size());

            std::sort(retrievedMessages.begin(), retrievedMessages.end());

            EXPECT_TRUE(std::equal(insertedMessages.begin(), insertedMessages.end(),
                                   retrievedMessages.begin(), retrievedMessages.end()));
        }
        catch (...)
        {
            HandleException();
        }
    }

    /// <summary>
    /// Tests writing messages in the broker queue and then reading them back from it,
    /// but in several consecutive steps.
    /// </summary>
    TEST_F(BrokerQueueTestCase, QueueReaderAndWriter_ConversationWithReadingStepsTest)
    {
        CALL_STACK_TRACE;

        try
        {
            using namespace _3fd::broker;

            const struct {
                uint16_t toWrite = 64;
                uint16_t toReadPerStep = 16;
            } numMessages;

            std::vector<std::string> insertedMessages = GenerateMessages(numMessages.toWrite);

            // Setup the writer:
            QueueWriter queueWriter(
                Backend::MsSqlServer,
                core::AppConfig::GetSettings().application.GetString(keyForBrokerDbConnStr, UNDEF_BROKER_DB_CONNSTR),
                BROKER_SERVICE_URL,
                MessageTypeSpec{ 128UL, MessageContentValidation::None }
            );

            // Write asynchronously:
            auto writeOp = queueWriter.WriteMessages(insertedMessages);

            // Setup the reader:
            QueueReader queueReader(
                Backend::MsSqlServer,
                core::AppConfig::GetSettings().application.GetString(keyForBrokerDbConnStr, UNDEF_BROKER_DB_CONNSTR),
                BROKER_SERVICE_URL,
                MessageTypeSpec{ 128UL, MessageContentValidation::None }
            );

            WaitFor(*writeOp);

            std::vector<std::unique_ptr<IAsyncDatabaseOperation>> steps;
            std::vector<std::string> retrievedMessages;
            retrievedMessages.reserve(numMessages.toWrite);

            // Read the messages back from the queue in several steps:
            for (int step = 0; step <= numMessages.toWrite / numMessages.toReadPerStep; ++step)
            {
                auto readOp = queueReader.ReadMessages(numMessages.toReadPerStep, 0,
                    [&retrievedMessages](std::vector<std::string> &&messages)
                    {
                        // accumulate the received messages:
                        for (auto &message : messages)
                        {
                            retrievedMessages.push_back(std::move(message));
                        }
                    });

                WaitFor(*readOp); // wait for every step to finish
            }

            EXPECT_EQ(insertedMessages.size(), retrievedMessages.size());

            std::sort(retrievedMessages.begin(), retrievedMessages.end());

            EXPECT_TRUE(std::equal(insertedMessages.begin(), insertedMessages.end(),
                                   retrievedMessages.begin(), retrievedMessages.end()));
        }
        catch (...)
        {
            HandleException();
        }
    }

    /// <summary>
    /// Tests writing messages in the broker queue and then reading them back from it,
    /// but in CONCURRENT steps
    /// </summary>
    TEST_F(BrokerQueueTestCase, QueueReaderAndWriter_ConversationWithReadingStepsAndConcurrencyTest)
    {
        CALL_STACK_TRACE;

        try
        {
            using namespace _3fd::broker;

            const struct
            {
                uint16_t toWrite = 64;
                uint16_t toReadPerStep = 16;
            } numMessages;

            std::vector<std::string> insertedMessages = GenerateMessages(numMessages.toWrite);

            // Setup the writer:
            QueueWriter queueWriter(
                Backend::MsSqlServer,
                core::AppConfig::GetSettings().application.GetString(keyForBrokerDbConnStr, UNDEF_BROKER_DB_CONNSTR),
                BROKER_SERVICE_URL,
                MessageTypeSpec{ 128UL, MessageContentValidation::None }
            );

            // Write asynchronously:
            auto writeOp = queueWriter.WriteMessages(insertedMessages);

            // Setup the reader:
            QueueReader queueReader(
                Backend::MsSqlServer,
                core::AppConfig::GetSettings().application.GetString(keyForBrokerDbConnStr, UNDEF_BROKER_DB_CONNSTR),
                BROKER_SERVICE_URL,
                MessageTypeSpec{ 128UL, MessageContentValidation::None }
            );

            WaitFor(*writeOp);

            std::vector<std::unique_ptr<IAsyncDatabaseOperation>> stepOps;
            std::vector<std::string> retrievedMessages;
            retrievedMessages.reserve(numMessages.toWrite);

            // Read the messages back from the queue in several steps:
            for (int step = 0; step <= numMessages.toWrite / numMessages.toReadPerStep; ++step)
            {
                auto readOp = queueReader.ReadMessages(numMessages.toReadPerStep, 0,
                    [&retrievedMessages](std::vector<std::string> &&messages)
                    {
                        // accumulate the received messages:
                        for (auto &message : messages)
                        {
                            retrievedMessages.push_back(std::move(message));
                        }
                    });

                stepOps.push_back(std::move(readOp)); // do not wait, let the steps compete for resources
            }

            // wait for all steps to finish:
            for (auto &operation : stepOps)
            {
                WaitFor(*operation);
            }

            EXPECT_EQ(insertedMessages.size(), retrievedMessages.size());

            std::sort(retrievedMessages.begin(), retrievedMessages.end());

            EXPECT_TRUE(std::equal(insertedMessages.begin(), insertedMessages.end(),
                                   retrievedMessages.begin(), retrievedMessages.end()));
        }
        catch (...)
        {
            HandleException();
        }
    }

}// end of namespace integration_tests
}// end of namespace _3fd
