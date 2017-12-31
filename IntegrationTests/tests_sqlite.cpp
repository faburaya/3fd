#include "stdafx.h"
#include "runtime.h"
#include "sqlite.h"
#include "utils.h"
#include <map>
#include <list>
#include <array>
#include <chrono>
#include <thread>
#include <future>
#include <random>

#ifdef _3FD_PLATFORM_WINRT
#    include "utils_winrt.h"
#    include <codecvt>
#endif

namespace _3fd
{
namespace integration_tests
{
    using std::generate;
    using std::for_each;

    using namespace _3fd::core;

    void HandleException();

    /// <summary>
    /// Performs single thread tests on the SQLite module.
    /// </summary>
    TEST(Framework_SQLite_TestCase, SingleThreadUsage_Test)
    {
        using namespace utils;
        using namespace sqlite;

        // Ensures proper initialization/finalization of the framework
#   ifdef _3FD_PLATFORM_WINRT
        core::FrameworkInstance _framework("IntegrationTestsApp.WinRT.UWP");
#   else
        core::FrameworkInstance _framework;
#   endif

        CALL_STACK_TRACE;
            
        try
        {
            // Open/Create database instance:
#ifndef _3FD_PLATFORM_WINRT
            DatabaseConn database("testdb-basic.dat");
#else
            DatabaseConn database = WinRTExt::GetFilePathUtf8(
                "testdb-basic.dat",
                WinRTExt::FileLocation::LocalFolder
            );
#endif
            // Change to WAL mode
            database.CreateStatement("PRAGMA journal_mode=WAL;").Step();
            database.CreateStatement("PRAGMA synchronous=NORMAL;").Step();

            // Create a table
            database.CreateStatement("CREATE TABLE Products (Id INTEGER PRIMARY KEY, Name VARCHAR(25) NOT NULL, Price FLOAT NOT NULL, Description TEXT NULL);").Step();

            // Statement to insert data into the database
            auto insert = database.CreateStatement("INSERT INTO Products (Id, Name, Price, Description) VALUES(@id, @name, @price, @desc);");

            // Bind parameters:
            insert.Bind("@id", 0LL);
            insert.Bind("@name", "Screwdriver");
            insert.Bind("@price", 31.70);
            insert.Bind("@desc", "Flat head");

            // Step into execution
            insert.Step();

            // Bind parameters:
            insert.Reset();
            insert.Bind("@id", 1LL);
            insert.Bind("@name", "Clamp");
            insert.Bind("@price", 12.48);
            insert.Bind("@desc", "Workbench clamp");

            // Next execution step:
            insert.Step();

            // Update a row
            database.CreateStatement("UPDATE Products SET Description = 'Flat head screwdriver' WHERE Id = 0;").Step();

            // Statement to select all rows
            auto select = database.CreateStatement("SELECT * FROM Products;");

            // Step into execution
            select.Step();

            // Check results:
            EXPECT_EQ(0, select.GetColumnValueInteger("Id"));
            EXPECT_EQ("Screwdriver", select.GetColumnValueText("Name"));
            EXPECT_EQ(31.70, select.GetColumnValueFloat64("Price"));
            EXPECT_EQ("Flat head screwdriver", select.GetColumnValueText("Description"));

            // Next execution step:
            select.Step();

            // Check results:
            EXPECT_EQ(1, select.GetColumnValueInteger("Id"));
            EXPECT_EQ("Clamp", select.GetColumnValueText("Name"));
            EXPECT_EQ(12.48, select.GetColumnValueFloat64("Price"));
            EXPECT_EQ("Workbench clamp", select.GetColumnValueText("Description"));

            try
            {
                select.GetColumnValueText("godzilla");
            }
            catch (IAppException &ex)
            {
                Logger::Write(ex, core::Logger::PRIO_ERROR);
            }

            // Reset the SELECT query so as to unlock the database table
            select.Reset();

            // Drop the table
            database.CreateStatement("DROP TABLE Products;").Step();
        }
        catch (...)
        {
            HandleException();
        }
    }

    /// <summary>
    /// Tests the SQLite connection pool, transactions and concurrent access.
    /// </summary>
    TEST(Framework_SQLite_TestCase, PoolAndTransactions_Test)
    {
        using namespace utils;
        using namespace sqlite;

        // Ensures proper initialization/finalization of the framework
#   ifdef _3FD_PLATFORM_WINRT
        core::FrameworkInstance _framework("IntegrationTestsApp.WinRT.UWP");
#   else
        core::FrameworkInstance _framework;
#   endif

        CALL_STACK_TRACE;

        try
        {
#ifndef _3FD_PLATFORM_WINRT
            string dbFilePath("testdb-basic.dat");
#else
            string dbFilePath = WinRTExt::GetFilePathUtf8(
                "testdb-basic.dat",
                WinRTExt::FileLocation::LocalFolder
            );
#endif
            {// Use WAL mode:
                DatabaseConn database(dbFilePath);
                database.CreateStatement("PRAGMA journal_mode=WAL;").Step();
            }

            // Create database connections pool
            DbConnPool dbConnPool(dbFilePath);

            {// Create the database schema:
                auto conn = dbConnPool.AcquireSQLiteConn(); // acquire database connection

                // Create the 'Products' table
                conn.Get().CreateStatement(
                    "CREATE TABLE Products "
                        "(Id INTEGER PRIMARY KEY, "
                        "Name VARCHAR(25) NOT NULL, "
                        "Price FLOAT NOT NULL, "
                        "Description TEXT NULL);"
                ).Step();
            }// end of scope will release the database connection

            struct Product
            {
                long long id;
                const char *name;
                double price;
                const char *description;

                Product(long long p_id, const char *p_name, double p_price, const char *p_description)
                    : id(p_id), name(p_name), price(p_price), description(p_description) {}
            };

            // The products to be inserted in the table:
            std::array<Product, 20> products =
            {
                Product(0, "Screwdriver", 12.3, "Flat head screwdriver"),
                Product(1, "Clamp", 2.65, "Workbench clamp"),
                Product(2, "Hammer", 63.6, "Hammer of the gods"),
                Product(3, "Wrench", 92.1, "A wrench tool"),
                Product(4, "Screw", 16.8, "Loose screw"),
                Product(5, "Nail", 0.56, "Regular nail"),
                Product(6, "Twisting machine", 65.7, "Not sure if this exists"),
                Product(7, "Turning machine", 79.2, "Not sure of this either"),
                Product(8, "Nail pistol", 656.8, "Air-pistol to shoot nails"),
                Product(9, "Wood", 6.0, "Wood log"),
                Product(10, "Steel cable", 17.4, "Cable of steel"),
                Product(11, "Stainless steel cable", 12.4, "Cable of stainless steel"),
                Product(12, "Network cable", 6.8, "10 m of Network cable with RJ-45 connector"),
                Product(13, "Hook", 1.8, "Metallic hook"),
                Product(14, "Hydrochloric acid", 9.75, "Acid for hard cleaning purposes"),
                Product(15, "Sulfuric acid", 8.8, "Acid for rock cleaning purposes"),
                Product(16, "Optic fiber", 10.0, "20 m of Optic fiber"),
                Product(17, "Motion detector", 175.5, "Security device, motion detector"),
                Product(18, "Heat detector", 166.2, "Security device, head detector"),
                Product(19, "Sand", 1.2, "1 kg of Yellow sand")
            };

            std::list<std::future<void>> asyncCalls;

            // Insert everything in a parallel manner using transactions:
            for (auto &product : products)
            {
                asyncCalls.emplace_back(
                    std::async(std::launch::async, [&dbConnPool, &product]()
                    {
                        // Assign integer values to be later used as codes for queries
                        enum Query { Insert, Update };

                        CALL_STACK_TRACE;

                        // Acquire database connection
                        auto myConn = dbConnPool.AcquireSQLiteConn();

                        // BEGIN TRANSACTION
                        Transaction transaction(myConn);

                        /* Statement to INSERT data into the database. In the first run the prepared query
                        will be cached. Next time it will be retrieved by the assigned integer code. */
                        auto &insert = myConn.Get().CachedStatement(Insert,
                            "INSERT INTO Products "
                                "(Id, Name, Price, Description) "
                                "VALUES (@id, @name, @price, @desc);"
                        );

                        // Bind parameters for the INSERT query:
                        insert.Bind("@id", product.id);
                        insert.Bind("@name", product.name);
                        insert.Bind("@price", product.price);
                        insert.Bind("@desc", product.name);
                        insert.Step();

                        /* Statement to UPDATE data into the database. In the first run the prepared query
                        will be cached. Next time it will be retrieved by the assigned integer code. */
                        auto &update = myConn.Get().CachedStatement(Update,
                            "UPDATE Products "
                                "SET Description = @desc "
                                "WHERE Id = @id;"
                        );

                        // Bind parameters for the UPDATE query:
                        update.Bind("@id", product.id);
                        update.Bind("@desc", product.description);
                        update.Step();

                        // COMMIT TRANSACTION
                        transaction.Commit();
                    })// end of lambda
                );
            }

            // Wait all the asynchronous calls to return:
            for (auto &asyncCall : asyncCalls)
                asyncCall.wait();

            // Throw any exception transported from the asynchronous calls:
            for (auto &asyncCall : asyncCalls)
                asyncCall.get();

            asyncCalls.clear();

            // Acquire database connection
            auto conn = dbConnPool.AcquireSQLiteConn();

            // Statement to select all rows
            auto select = conn.Get().CreateStatement("SELECT * FROM Products ORDER BY Id ASC;");

            // Check results:
            for (auto &product : products)
            {
                select.Step();
                EXPECT_EQ(product.id, select.GetColumnValueInteger("Id"));
                EXPECT_EQ(product.name, select.GetColumnValueText("Name"));
                EXPECT_EQ(product.price, select.GetColumnValueFloat64("Price"));
                EXPECT_EQ(product.description, select.GetColumnValueText("Description"));
            }

            // Reset the SELECT query so as to unlock the database table
            select.Reset();

            // Drop the table
            conn.Get().CreateStatement("DROP TABLE Products;").Step();
        }
        catch (...)
        {
            HandleException();
        }
    }

}// end of namespace integration_tests
}// end of namespace _3fd
