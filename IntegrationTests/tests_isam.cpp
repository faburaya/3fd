#include "stdafx.h"
#include "runtime.h"
#include "isam.h"
#include <map>
#include <list>
#include <array>
#include <chrono>
#include <thread>
#include <future>
#include <random>
#include <iostream>
#include <functional>

#ifdef _3FD_PLATFORM_WINRT
#	include "utils_winrt.h"
#	define FILE_PATH(fileName)	_3fd::utils::WinRTExt::GetFilePathUtf8(fileName, _3fd::utils::WinRTExt::FileLocation::LocalFolder)
#else
#	define FILE_PATH(fileName)	fileName
#endif

namespace _3fd
{
	using std::generate;
	using std::for_each;

	namespace integration_tests
	{
		using namespace _3fd::core;

		void HandleException();

		/// <summary>
		/// Enumerates some columns just to assign a numeric code
		/// to a descriptive type enforced by the compiler.
		/// </summary>
		enum Column
		{
			ColId,
			ColName,
			ColPrice,
			ColBarCode,
			ColFragile,
			ColSpHandling,
			ColAmount,
			ColSequence,
			ColProviders,
			ColExpiration,
			ColDeliveries,
			ColDescription,

			// For the stress tests with historic data:
			ColTimestamp,
			ColValue,
			ColStatus
		};

		/// <summary>
		/// Test fixture for the ISAM module.
		/// </summary>
		class Framework_ISAM_TestCase : public ::testing::Test
		{
		public:

			/// <summary>
			/// Represents a product
			/// </summary>
			struct Product
			{
				unsigned short id;
				string name;
				float price;
				unsigned int amount;
				time_t expiration;
				bool fragile;
				std::array<int, 4> barcode;
				std::vector<string> providers;
				std::vector<time_t> deliveries;
				string description;
			};

			static std::vector<Product> products;

			std::function<bool(isam::RecordReader &)> checkWithReference;
			std::function<bool(const string &)> checkAlso;

			/// <summary>
			/// Set up the test fixture.
			/// </summary>
			virtual void SetUp() override
			{
				try
				{
					if(products.empty())
					{
						products.resize(20);

						const auto now = time(nullptr);
						int i(0);
						auto &p = products;

						p[i].id = i;
						p[i].name = "banana";
						p[i].price = 2.89;
						p[i].amount = 500;
						p[i].expiration = now + 1e6;
						p[i].fragile = true;
						p[i].providers.emplace_back("makro");
						p[i].providers.emplace_back("rodão");
						p[i].deliveries.push_back(now + 1e5);
						p[i].deliveries.push_back(now + 2e5);
						p[i].description = "silver banana";
						std::fill(p[i].barcode.begin(), p[i].barcode.end(), rand());
						++i;

						p[i].id = i;
						p[i].name = "banana";
						p[i].price = 2.59;
						p[i].amount = 500;
						p[i].expiration = now + 1e6;
						p[i].fragile = true;
						p[i].providers.emplace_back("makro");
						p[i].providers.emplace_back("rodão");
						p[i].deliveries.push_back(now + 1e5);
						p[i].deliveries.push_back(now + 2e5);
						p[i].description = "little banana";
						std::fill(p[i].barcode.begin(), p[i].barcode.end(), rand());
						++i;
						
						p[i].id = i;
						p[i].name = "washmachine";
						p[i].price = 789.99;
						p[i].amount = 12;
						p[i].expiration = -1;
						p[i].fragile = false;
						p[i].providers.emplace_back("whirlpool");
						p[i].deliveries.push_back(now + 2e5);
						p[i].deliveries.push_back(now + 3e5);
						p[i].description = "brastemp inox killer wash machine t800";
						std::fill(p[i].barcode.begin(), p[i].barcode.end(), rand());
						++i;
						
						p[i].id = i;
						p[i].name = "pistol";
						p[i].price = 1185.50;
						p[i].amount = 5;
						p[i].expiration = -1;
						p[i].fragile = false;
						p[i].providers.emplace_back("huntersco");
						p[i].providers.emplace_back("armydepot");
						p[i].deliveries.push_back(now + 4e5);
						p[i].description = "glock .18 automatic double coil";
						std::fill(p[i].barcode.begin(), p[i].barcode.end(), rand());
						++i;
						
						p[i].id = i;
						p[i].name = "tire";
						p[i].price = 52.20;
						p[i].amount = 150;
						p[i].expiration = -1;
						p[i].fragile = true;
						p[i].providers.emplace_back("rubberinc");
						p[i].providers.emplace_back("dpaschoal");
						p[i].deliveries.push_back(now + 3e5);
						p[i].deliveries.push_back(now + 4e5);
						p[i].description = "firestone city road tire heavy grip FX3200";
						std::fill(p[i].barcode.begin(), p[i].barcode.end(), rand());
						++i;
						
						p[i].id = i;
						p[i].name = "tire";
						p[i].price = 52.20;
						p[i].amount = 150;
						p[i].expiration = -1;
						p[i].fragile = true;
						p[i].providers.emplace_back("michelin");
						p[i].providers.emplace_back("dpaschoal");
						p[i].deliveries.push_back(now + 3e5);
						p[i].deliveries.push_back(now + 4e5);
						p[i].description = "michelin city road tire rainy days IXR500";
						std::fill(p[i].barcode.begin(), p[i].barcode.end(), rand());
						++i;
						
						p[i].id = i;
						p[i].name = "tomato";
						p[i].price = 1.78;
						p[i].amount = 350;
						p[i].expiration = now + 1e6;
						p[i].fragile = true;
						p[i].providers.emplace_back("makro");
						p[i].providers.emplace_back("rodão");
						p[i].deliveries.push_back(now + 1e5);
						p[i].deliveries.push_back(now + 2e5);
						p[i].deliveries.push_back(now + 3e5);
						p[i].description = "italian tomato";
						std::fill(p[i].barcode.begin(), p[i].barcode.end(), rand());
						++i;
						
						p[i].id = i;
						p[i].name = "rowingmachine";
						p[i].price = 92.25;
						p[i].amount = 12;
						p[i].expiration = -1;
						p[i].fragile = false;
						p[i].providers.emplace_back("caloi");
						p[i].deliveries.push_back(now + 2e5);
						p[i].deliveries.push_back(now + 3e5);
						p[i].description = "caloi super rowing machine sw20k";
						std::fill(p[i].barcode.begin(), p[i].barcode.end(), rand());
						++i;
						
						p[i].id = i;
						p[i].name = "hammer";
						p[i].price = 19.50;
						p[i].amount = 55;
						p[i].expiration = -1;
						p[i].fragile = false;
						p[i].providers.emplace_back("toolsdepot");
						p[i].providers.emplace_back("ironstuff");
						p[i].deliveries.push_back(now + 3e5);
						p[i].description = "conventional hammer for iron nails";
						std::fill(p[i].barcode.begin(), p[i].barcode.end(), rand());
						++i;
						
						p[i].id = i;
						p[i].name = "pcdesktop";
						p[i].price = 555.0;
						p[i].amount = 15;
						p[i].expiration = -1;
						p[i].fragile = false;
						p[i].providers.emplace_back("dellinc");
						p[i].providers.emplace_back("pcdepot");
						p[i].providers.emplace_back("officeco");
						p[i].deliveries.push_back(now + 4e5);
						p[i].deliveries.push_back(now + 8e5);
						p[i].description = "pc dell desktop optiplex 850";
						std::fill(p[i].barcode.begin(), p[i].barcode.end(), rand());
						++i;
						
						p[i].id = i;
						p[i].name = "pcdesktop";
						p[i].price = 539.0;
						p[i].amount = 16;
						p[i].expiration = -1;
						p[i].fragile = false;
						p[i].providers.emplace_back("hpcorp");
						p[i].providers.emplace_back("pcdepot");
						p[i].deliveries.push_back(now + 4e5);
						p[i].deliveries.push_back(now + 8e5);
						p[i].description = "pc hp pavillion desktop hp460";
						std::fill(p[i].barcode.begin(), p[i].barcode.end(), rand());
						++i;

						p[i].id = i;
						p[i].name = "pepino";
						p[i].price = 1.15;
						p[i].amount = 400;
						p[i].expiration = now + 2e6;
						p[i].fragile = true;
						p[i].providers.emplace_back("makro");
						p[i].providers.emplace_back("cheapveggies");
						p[i].deliveries.push_back(now + 1e5);
						p[i].deliveries.push_back(now + 2e5);
						p[i].description = "standard green radioactive pepino";
						std::fill(p[i].barcode.begin(), p[i].barcode.end(), rand());
						++i;
						
						p[i].id = i;
						p[i].name = "microwaveoven";
						p[i].price = 79.50;
						p[i].amount = 52;
						p[i].expiration = -1;
						p[i].fragile = false;
						p[i].providers.emplace_back("electrolux");
						p[i].deliveries.push_back(now + 2e5);
						p[i].deliveries.push_back(now + 3e5);
						p[i].description = "electrolux smart microwave oven zd200 with bluetooth and wifi";
						std::fill(p[i].barcode.begin(), p[i].barcode.end(), rand());
						++i;
						
						p[i].id = i;
						p[i].name = "armymentoys";
						p[i].price = 25.50;
						p[i].amount = 60;
						p[i].expiration = -1;
						p[i].fragile = false;
						p[i].providers.emplace_back("kidsland");
						p[i].providers.emplace_back("toysdepot");
						p[i].deliveries.push_back(now + 1e5);
						p[i].deliveries.push_back(now + 2e5);
						p[i].deliveries.push_back(now + 3e5);
						p[i].description = "army men toys, real action but with men of plastic";
						std::fill(p[i].barcode.begin(), p[i].barcode.end(), rand());
						++i;

						p[i].id = i;
						p[i].name = "ammunition";
						p[i].price = 85.60;
						p[i].amount = 35;
						p[i].expiration = -1;
						p[i].fragile = true;
						p[i].providers.emplace_back("huntersco");
						p[i].providers.emplace_back("militarydepot");
						p[i].deliveries.push_back(now + 3e5);
						p[i].deliveries.push_back(now + 4e5);
						p[i].description = "armor piercing ammo, 37. cal with explosive payload and head of depleted uranium";
						std::fill(p[i].barcode.begin(), p[i].barcode.end(), rand());
						++i;

						p[i].id = i;
						p[i].name = "grapefruit";
						p[i].price = 1.05;
						p[i].amount = 250;
						p[i].expiration = now + 1e6;
						p[i].fragile = true;
						p[i].providers.emplace_back("fruitsco");
						p[i].providers.emplace_back("greendepot");
						p[i].deliveries.push_back(now + 1e5);
						p[i].deliveries.push_back(now + 2e5);
						p[i].description = "grapefruit imported from Florida";
						std::fill(p[i].barcode.begin(), p[i].barcode.end(), rand());
						++i;
						
						p[i].id = i;
						p[i].name = "dinamite";
						p[i].price = 2.25;
						p[i].amount = 600;
						p[i].expiration = -1;
						p[i].fragile = true;
						p[i].providers.emplace_back("nobeldepot");
						p[i].deliveries.push_back(now + 6e5);
						p[i].description = "safe dinamite for civil construction, TNG with standard stabilizer";
						std::fill(p[i].barcode.begin(), p[i].barcode.end(), rand());
						++i;
						
						p[i].id = i;
						p[i].name = "nails";
						p[i].price = 11.99;
						p[i].amount = 400;
						p[i].expiration = -1;
						p[i].fragile = false;
						p[i].providers.emplace_back("ironstuff");
						p[i].providers.emplace_back("toolsdepot");
						p[i].deliveries.push_back(now + 3e5);
						p[i].deliveries.push_back(now + 6e5);
						p[i].description = "conventional iron nails for wood plates";
						std::fill(p[i].barcode.begin(), p[i].barcode.end(), rand());
						++i;
						
						p[i].id = i;
						p[i].name = "pclaptop";
						p[i].price = 665.90;
						p[i].amount = 35;
						p[i].expiration = -1;
						p[i].fragile = false;
						p[i].providers.emplace_back("msidepot");
						p[i].providers.emplace_back("notebookco");
						p[i].providers.emplace_back("officematerials");
						p[i].deliveries.push_back(now + 4e5);
						p[i].deliveries.push_back(now + 8e5);
						p[i].description = "pc msi laptop gs55 high performance with AMD APU";
						std::fill(p[i].barcode.begin(), p[i].barcode.end(), rand());
						++i;
						
						p[i].id = i;
						p[i].name = "pclaptop";
						p[i].price = 705.35;
						p[i].amount = 32;
						p[i].expiration = -1;
						p[i].fragile = false;
						p[i].providers.emplace_back("msidepot");
						p[i].providers.emplace_back("notebookco");
						p[i].deliveries.push_back(now + 4e5);
						p[i].deliveries.push_back(now + 8e5);
						p[i].description = "pc msi laptop gs75x high performance with AMD Radeon R7";
						std::fill(p[i].barcode.begin(), p[i].barcode.end(), rand());
						++i;
					}

					/* Callback used to read a record. This one checks the live record
					against the original reference data: */
					checkWithReference = [this](isam::RecordReader &rec)
					{
						using namespace std::chrono;

						uint16_t id;
						EXPECT_TRUE(rec.ReadFixedSizeValue(ColId, id));
						auto &product = products[id];

						string name;
						EXPECT_TRUE(rec.ReadTextValue(ColName, name));
						EXPECT_EQ(product.name, name);

						EXPECT_TRUE(checkAlso(name)); // further checking

						float price;
						EXPECT_TRUE(rec.ReadFixedSizeValue(ColPrice, price));
						EXPECT_EQ(product.price, price);

						unsigned int amount;
						EXPECT_TRUE(rec.ReadFixedSizeValue(ColAmount, amount));
						EXPECT_EQ(product.amount, amount);

						static std::vector<string> providers;
						rec.ReadTextValues(ColProviders, providers);
						EXPECT_TRUE(std::equal(providers.begin(), providers.end(), product.providers.begin()));

						static std::vector<time_point<system_clock, seconds>> deliveries;
						rec.ReadFixedSizeValues(ColDeliveries, deliveries);
						EXPECT_TRUE(
							std::equal(
								deliveries.begin(),
								deliveries.end(),
								product.deliveries.begin(),
								[](const time_point<system_clock, seconds> &a, time_t b)
								{
									return a.time_since_epoch().count() == b;
								})
						);

						string description;
						EXPECT_TRUE(rec.ReadTextValue(ColDescription, description));
						EXPECT_EQ(product.description, description);

						return true;
					};
				}
				catch(std::exception &ex)
				{
					std::cout << "Thrown exception: " << ex.what() << std::endl;
				}
			}
		};

		std::vector<Framework_ISAM_TestCase::Product> Framework_ISAM_TestCase::products;

		/// <summary>
		/// Tests the setup of an ISAM database.
		/// </summary>
		TEST_F(Framework_ISAM_TestCase, DatabaseSetup_Test)
		{
			using namespace isam;
			
			// Ensures proper initialization/finalization of the framework
			_3fd::core::FrameworkInstance _framework;

			CALL_STACK_TRACE;

			try
			{
				// Create an instance
				Instance instance("tester", ".\\temp\\");
				
				bool dbIsNew, schemaIsReady;
				
				// Open the database if already existent, otherwise, create it:
				auto conn = instance.OpenDatabase(0, FILE_PATH("isam-test.dat"), dbIsNew);

				if(dbIsNew == false)
				{
					auto table = conn.TryOpenTable(schemaIsReady, "products");

					if(schemaIsReady)
						ASSERT_EQ("products", table->GetName());
				}

				if(dbIsNew || schemaIsReady == false) // The database has just been created:
				{
					// Start a transaction to guarantee the schema won't end up half done
					auto transaction = conn.BeginTransaction();

					// Define the table columns:
					std::vector<ITable::ColumnDefinition> columns;
					columns.reserve(11);
					columns.emplace_back("id", DataType::UInt16, true); // not null
					columns.emplace_back("name", DataType::Text, true); // not null
					columns.emplace_back("price", DataType::Float32, true); // not null
					columns.emplace_back("barcode", DataType::GUID, true); // not null
					columns.emplace_back("fragile", DataType::Boolean, true); // not null
					columns.emplace_back("providers", DataType::Text, false, true); // nullable, multi-value
					columns.emplace_back("expiration", DataType::DateTime, false, false, true); // nullable, single value, sparse
					columns.emplace_back("deliveries", DataType::DateTime, false, true, true); // nullable, multi-value, sparse

					uint32_t defAmount = 0;
					columns.emplace_back("amount", DataType::UInt32, true); // not null
					columns.back().default = AsInputParam(defAmount); // default value for amount

					columns.emplace_back("description", DataType::LargeText, true); // not null
					columns.back().codePage = CodePage::Unicode; // code page is 'english' otherwise specified

					columns.emplace_back("sequence", DataType::Int32, true); // not null
					columns.back().autoIncrement = true;

					// Define the indexes:

					std::vector<std::pair<string, Order>> barCodeIdxKeys;
					barCodeIdxKeys.reserve(1);
					barCodeIdxKeys.emplace_back("barcode", Order::Ascending);

					std::vector<std::pair<string, Order>> nameIdxKeys;
					nameIdxKeys.reserve(2);
					nameIdxKeys.emplace_back("name", Order::Ascending);
					nameIdxKeys.emplace_back("id", Order::Descending);

					std::vector<std::pair<string, Order>> idIdxKeys;
					idIdxKeys.reserve(1);
					idIdxKeys.emplace_back("id", Order::Ascending);

					std::vector<ITable::IndexDefinition> indexes;
					indexes.reserve(3);
					indexes.emplace_back("idx-barcode", barCodeIdxKeys, true); // primary, hence unique
					indexes.emplace_back("idx-name", nameIdxKeys); // secondary
					indexes.emplace_back("idx-id", idIdxKeys); // secondary

					// Create the table:
					auto table = conn.CreateTable("products", false, columns, indexes);
					ASSERT_EQ("products", table->GetName());

					// Commit changes to the schema
					transaction.Commit();
				}
				else // Database already existed (and most likely also the schema):
				{
					auto table = conn.OpenTable("products");
					ASSERT_EQ("products", table->GetName());
				}
			}
			catch(...)
			{
				HandleException();
			}
		}

		/// <summary>
		/// Enumerates some indexes just to assign a numeric code to a descriptive type enforced by the compiler.
		/// </summary>
		enum Index { IdxName, IdxBarCode, IdxProvider, IdxID };

		/// <summary>
		/// Tests filling the ISAM database with data.
		/// </summary>
		TEST_F(Framework_ISAM_TestCase, TableDataFill_Test)
		{
			using namespace isam;
			
			// Ensures proper initialization/finalization of the framework
			_3fd::core::FrameworkInstance _framework;

			CALL_STACK_TRACE;

			try
			{
				// Create an instance
				Instance instance("tester", ".\\temp\\");

				// Open the database
				auto conn = instance.OpenDatabase(0, FILE_PATH("isam-test.dat"));

				// Get the table schema
				auto table = conn.OpenTable("products");

				/* Map numeric codes for the column names. The framework uses this 
				approach in order to optimize metadata retrieval keeping in cache: */
				table->MapInt2ColName(ColId, "id");
				table->MapInt2ColName(ColName, "name");
				table->MapInt2ColName(ColPrice, "price");
				table->MapInt2ColName(ColBarCode, "barcode");
				table->MapInt2ColName(ColFragile, "fragile");
				table->MapInt2ColName(ColAmount, "amount");
				table->MapInt2ColName(ColSequence, "sequence");
				table->MapInt2ColName(ColProviders, "providers");
				table->MapInt2ColName(ColExpiration, "expiration");
				table->MapInt2ColName(ColDeliveries, "deliveries");
				table->MapInt2ColName(ColDescription, "description");

				std::list<std::future<void>> asyncCalls;

				const int numThreads(4); // the amount of products must be a mulitple of this number

				for(int tno = 0; tno < numThreads; ++tno)
				{
					int idxBegin = tno * products.size() / numThreads;
					int idxEnd = (tno == numThreads - 1) ? products.size() : (idxBegin + products.size() / numThreads);

					asyncCalls.emplace_back(
						std::async(std::launch::async, [&instance, &table, this] (int idxBegin, int idxEnd)
						{
							try
							{
								CALL_STACK_TRACE;

								// Open the database
								auto conn = instance.OpenDatabase(0, FILE_PATH("isam-test.dat"));

								// Get a table cursor:
								auto cursor = conn.GetCursorFor(table);

								// Begin a transaction
								auto transaction = conn.BeginTransaction();

								// Fill the table with data:
								for(int idx = idxBegin; idx < idxEnd; ++idx)
								{
									auto &prod = products[idx];

									// Start an update to insert a new record in the table
									auto writer = cursor.StartUpdate(TableWriter::Mode::InsertNew);

									// Define the columns values of the record:
									writer.SetColumn(ColId, AsInputParam(prod.id));
									writer.SetColumn(ColName, AsInputParam(prod.name));
									writer.SetColumn(ColPrice, AsInputParam(prod.price));
									writer.SetColumn(ColBarCode, GenericInputParam(prod.barcode.data(), prod.barcode.size() * sizeof prod.barcode[0], DataType::GUID));
									writer.SetColumn(ColFragile, AsInputParam(prod.fragile));
									writer.SetColumn(ColAmount, AsInputParam(prod.amount));

									double expiration1900; // the expiration date must first be converted to fractional days since 1900 so backend can understand

									// The expiration date is a nullable column:
									writer.SetColumn(ColExpiration,
										(prod.expiration >= 0) ? AsInputParam(expiration1900, prod.expiration)
															   : NullParameter(DataType::DateTime)
									);
					
									// The 'providers' is a multi-value column:
									for(auto &provider : prod.providers)
										writer.SetColumn(ColProviders, AsInputParam(provider)); // each call adds a value to the list

									// The expiration date must first be converted to fractional days since 1900 so the backend can use it
									std::vector<double> deliveries(prod.deliveries.size());

									// The 'deliveries' is a multi-value column:
									for(size_t idx = 0; idx < prod.deliveries.size(); ++idx)
										writer.SetColumn(ColDeliveries, AsInputParam(deliveries[idx], prod.deliveries[idx])); // each call adds a value to the list

									// The 'descrition' is a large-text column:
									writer.SetLargeColumn(ColDescription, AsInputParam(prod.description));

									// Save the new record
									writer.Save();
								}

								// Commit the inserted data so the other sessions can see it
								transaction.Commit(false /*lazy commit*/);
							}
							catch(...)
							{
								HandleException();
							}
						}, idxBegin, idxEnd) // end of async
					);
				}// for loop end
				
				// Wait all the asynchronous calls to return:
				for(auto &asyncCall : asyncCalls)
					asyncCall.wait();

				asyncCalls.clear();
				conn.Flush(); // flush all previous (lazy) transactions

				/* Map numeric codes for the indexes names. The framework uses this 
				approach in order to optimize metadata retrieval keeping it in cache: */
				table->MapInt2IdxName(IdxName, "idx-name");
				
				// Get a table cursor
				auto cursor = conn.GetCursorFor(table, true);

				// Begin a transaction
				auto transaction = conn.BeginTransaction();

				// Go through all the added entries and validate them:
				checkAlso = [](const string &) { return true; };
				auto numRecords = cursor.ScanAll(IdxName, checkWithReference);

				EXPECT_EQ(products.size(), numRecords);

				// Commit the inserted data so the other sessions can see it
				transaction.Commit();
			}
			catch(...)
			{
				HandleException();
			}
		}

		/// <summary>
		/// Tests searching the ISAM database.
		/// </summary>
		TEST_F(Framework_ISAM_TestCase, TableSearch_Test)
		{
			using namespace isam;
			
			// Ensures proper initialization/finalization of the framework
			_3fd::core::FrameworkInstance _framework;

			CALL_STACK_TRACE;

			try
			{
				// Create an instance
				Instance instance("tester", ".\\temp\\");

				// Open the database
				auto conn = instance.OpenDatabase(0, FILE_PATH("isam-test.dat"));

				// Get the table schema
				auto table = conn.OpenTable("products");

				/* Map numeric codes for the column names. The framework uses this 
				approach in order to optimize metadata retrieval keeping in cache: */
				table->MapInt2ColName(ColId, "id");
				table->MapInt2ColName(ColName, "name");
				table->MapInt2ColName(ColPrice, "price");
				table->MapInt2ColName(ColAmount, "amount");
				table->MapInt2ColName(ColProviders, "providers");
				table->MapInt2ColName(ColDeliveries, "deliveries");
				table->MapInt2ColName(ColDescription, "description");

				/* Map numeric codes for the indexes names. The framework uses this 
				approach in order to optimize metadata retrieval keeping it in cache: */
				table->MapInt2IdxName(IdxName, "idx-name");
				
				// Create an index key through which the table will be scanned:
				std::vector<GenericInputParam> keys;
				keys.push_back(AsInputParam("pc"));
				
				// Get a table cursor:
				auto cursor = conn.GetCursorFor(table, true);

				/* This test is only going to read the table, but you use 
				a transaction in order to have snapshot isolation */
				auto transaction = conn.BeginTransaction();

				// Further checking:
				checkAlso = [](const string &prodName) { return prodName.substr(0, 2) != "pc"; };
				
				// Find an entry and from that point scan until the end of the index:
				auto numRecords = cursor.ScanFrom(
					IdxName, keys, 
					TableCursor::IndexKeyMatch::PrefixWildcard, 
					TableCursor::ComparisonOperator::GreaterThan,
					checkWithReference
				);

				EXPECT_GT(numRecords, 0);

				// Find an entry and from that point scan backwards until the beginning of the index:
				numRecords = cursor.ScanFrom(
					IdxName, keys, 
					TableCursor::IndexKeyMatch::PrefixWildcard, 
					TableCursor::ComparisonOperator::LessThan, 
					checkWithReference,
					true // backwards
				);

				EXPECT_GT(numRecords, 0);

				// Finalize the transaction with nothing to commit
				transaction.Commit();
			}
			catch(...)
			{
				HandleException();
			}
		}

		/// <summary>
		/// Tests searching the ISAM database using ranges.
		/// </summary>
		TEST_F(Framework_ISAM_TestCase, TableSearchRange_Test)
		{
			using namespace isam;

			// Ensures proper initialization/finalization of the framework
			_3fd::core::FrameworkInstance _framework;

			CALL_STACK_TRACE;

			try
			{
				// Create an instance
				Instance instance("tester", ".\\temp\\");

				// Open the database
				auto conn = instance.OpenDatabase(0, FILE_PATH("isam-test.dat"));

				// Get the table schema
				auto table = conn.OpenTable("products");

				/* Map numeric codes for the column names. The framework uses this
				approach in order to optimize metadata retrieval keeping in cache: */
				table->MapInt2ColName(ColId, "id");
				table->MapInt2ColName(ColName, "name");
				table->MapInt2ColName(ColPrice, "price");
				table->MapInt2ColName(ColAmount, "amount");
				table->MapInt2ColName(ColProviders, "providers");
				table->MapInt2ColName(ColDeliveries, "deliveries");
				table->MapInt2ColName(ColDescription, "description");

				/* Map numeric codes for the indexes names. The framework uses this
				approach in order to optimize metadata retrieval keeping it in cache: */
				table->MapInt2IdxName(IdxName, "idx-name");

				// Get a table cursor:
				auto cursor = conn.GetCursorFor(table, true);

				/* This test is only going to read the table, but you use
				a transaction in order to have snapshot isolation */
				auto transaction = conn.BeginTransaction();

				// Create an index range through which the table will be scanned:
				TableCursor::IndexRangeDefinition range;
				range.indexCode = IdxName;

				range.beginKey.colsVals.push_back(AsInputParam("pc"));
				range.beginKey.typeMatch = TableCursor::IndexKeyMatch::PrefixWildcard;
				range.beginKey.comparisonOper = TableCursor::ComparisonOperator::GreaterThanOrEqualTo;

				range.endKey.colsVals.push_back(AsInputParam("pc"));
				range.endKey.typeMatch = TableCursor::IndexKeyMatch::PrefixWildcard;
				range.endKey.isUpperLimit = true;
				range.endKey.isInclusive = true;

				// Further checking:
				checkAlso = [](const string &prodName) { return prodName.substr(0, 2) == "pc"; };

				// Scan the range forward:
				auto numRecords1 = cursor.ScanRange(range, checkWithReference);
				EXPECT_GT(numRecords1, 0);

				// Change the keys so as to revert the direction:

				range.beginKey.typeMatch = TableCursor::IndexKeyMatch::PrefixWildcard;
				range.beginKey.comparisonOper = TableCursor::ComparisonOperator::LessThanOrEqualTo;

				range.endKey.typeMatch = TableCursor::IndexKeyMatch::PrefixWildcard;
				range.endKey.isUpperLimit = false;
				range.endKey.isInclusive = true;

				// Scan the range backward:
				auto numRecords2 = cursor.ScanRange(range, checkWithReference);
				EXPECT_GT(numRecords2, 0);

				// Going forward or backward must produce the same count:
				EXPECT_EQ(numRecords2, numRecords1);

				// Finalize the transaction with nothing to commit
				transaction.Commit();
			}
			catch (...)
			{
				HandleException();
			}
		}

		/// <summary>
		/// Tests searching the ISAM database using intersection of ranges.
		/// </summary>
		TEST_F(Framework_ISAM_TestCase, TableIndexIntersection_Test)
		{
			using namespace isam;

			// Ensures proper initialization/finalization of the framework
			_3fd::core::FrameworkInstance _framework;

			CALL_STACK_TRACE;

			try
			{
				// Create an instance
				Instance instance("tester", ".\\temp\\");

				// Open the database
				auto conn = instance.OpenDatabase(0, FILE_PATH("isam-test.dat"));

				// Get the table schema
				auto table = conn.OpenTable("products");

				/* Map numeric codes for the column names. The framework uses this
				approach in order to optimize metadata retrieval keeping in cache: */
				table->MapInt2ColName(ColId, "id");
				table->MapInt2ColName(ColName, "name");
				table->MapInt2ColName(ColPrice, "price");
				table->MapInt2ColName(ColAmount, "amount");
				table->MapInt2ColName(ColProviders, "providers");
				table->MapInt2ColName(ColDeliveries, "deliveries");
				table->MapInt2ColName(ColDescription, "description");

				/* Map numeric codes for the indexes names. The framework uses this
				approach in order to optimize metadata retrieval keeping it in cache: */
				table->MapInt2IdxName(IdxID, "idx-id");
				table->MapInt2IdxName(IdxName, "idx-name");

				// Get a table cursor:
				auto cursor = conn.GetCursorFor(table, true);

				/* This test is only going to read the table, but you use
				a transaction in order to have snapshot isolation */
				auto transaction = conn.BeginTransaction();

				// Create some index ranges to intersect:
				std::vector<TableCursor::IndexRangeDefinition> ranges(2);

				// Range 1: second half of the set
				unsigned short idStart = 0;// products.size() / 2;
				unsigned short idEnd = products.size() - 1;

				ranges[0].indexCode = IdxID;
				ranges[0].beginKey.colsVals.push_back(AsInputParam(idStart));
				ranges[0].beginKey.typeMatch = TableCursor::IndexKeyMatch::Regular;
				ranges[0].beginKey.comparisonOper = TableCursor::ComparisonOperator::GreaterThanOrEqualTo;

				ranges[0].endKey.colsVals.push_back(AsInputParam(idEnd));
				ranges[0].endKey.typeMatch = TableCursor::IndexKeyMatch::Regular;
				ranges[0].endKey.isUpperLimit = true;
				ranges[0].endKey.isInclusive = true;

				// Range 2: the PC's again

				ranges[1].indexCode = IdxName;
				ranges[1].beginKey.colsVals.push_back(AsInputParam("pc"));
				ranges[1].beginKey.typeMatch = TableCursor::IndexKeyMatch::PrefixWildcard;
				ranges[1].beginKey.comparisonOper = TableCursor::ComparisonOperator::LessThanOrEqualTo;

				ranges[1].endKey.colsVals.push_back(AsInputParam("pc"));
				ranges[1].endKey.typeMatch = TableCursor::IndexKeyMatch::PrefixWildcard;
				ranges[1].endKey.isUpperLimit = false;
				ranges[1].endKey.isInclusive = true;

				// Further checking:
				checkAlso = [](const string &prodName) { return prodName.substr(0, 2) == "pc"; };

				// Intersect the ranges:
				auto numRecords = cursor.ScanIntersection(ranges, checkWithReference);
				EXPECT_GT(numRecords, 0);

				// Finalize the transaction with nothing to commit
				transaction.Commit();
			}
			catch (...)
			{
				HandleException();
			}
		}

		/// <summary>
		/// Tests updating the ISAM database.
		/// </summary>
		TEST_F(Framework_ISAM_TestCase, TableDataUpdate_Test)
		{
			using namespace isam;
			
			// Ensures proper initialization/finalization of the framework
			_3fd::core::FrameworkInstance _framework;

			CALL_STACK_TRACE;

			try
			{
				// Create an instance
				Instance instance("tester", ".\\temp\\");

				// Open the database
				auto conn = instance.OpenDatabase(0, FILE_PATH("isam-test.dat"));

				// Get the table schema
				auto table = conn.OpenTable("products");

				/* Map numeric codes for the column names. The framework uses this 
				approach in order to optimize metadata retrieval keeping in cache: */
				table->MapInt2ColName(ColId, "id");
				table->MapInt2ColName(ColName, "name");
				table->MapInt2ColName(ColPrice, "price");
				table->MapInt2ColName(ColAmount, "amount");
				table->MapInt2ColName(ColProviders, "providers");
				table->MapInt2ColName(ColDeliveries, "deliveries");
				table->MapInt2ColName(ColDescription, "description");

				/* Map numeric codes for the indexes names. The framework uses this 
				approach in order to optimize metadata retrieval keeping it in cache: */
				table->MapInt2IdxName(IdxName, "idx-name");
				
				// Create an index range through which the table will be scanned:
				TableCursor::IndexRangeDefinition range;
				range.indexCode = IdxName;

				range.beginKey.colsVals.push_back(AsInputParam("pc"));
				range.beginKey.typeMatch = TableCursor::IndexKeyMatch::PrefixWildcard;
				range.beginKey.comparisonOper = TableCursor::ComparisonOperator::GreaterThanOrEqualTo;
				
				range.endKey.colsVals.push_back(AsInputParam("pc"));
				range.endKey.typeMatch = TableCursor::IndexKeyMatch::PrefixWildcard;
				range.endKey.isUpperLimit = true;
				range.endKey.isInclusive = true;
				
				// Get a table cursor:
				auto cursor = conn.GetCursorFor(table);

				// Begin a transaction
				auto transaction = conn.BeginTransaction();

				// Go though all the added entries and update the records with new values:
				auto numRecords1 = cursor.ScanRange(range, [this, &cursor] (RecordReader &rec)
				{
					uint16_t id;
					EXPECT_TRUE(rec.ReadFixedSizeValue(ColId, id));
					auto &product = products[id];

					// Start an update to update a record in the table
					auto writer = cursor.StartUpdate(TableWriter::Mode::Replace);

					// Define some new values for the record:

					product.price *= 2;
					writer.SetColumn(ColPrice, AsInputParam(product.price));

					product.amount /= 2;
					writer.SetColumn(ColAmount, AsInputParam(product.amount));

					/* The 'providers' is a multi-value column. The last value will be removed, 
					a new provider will be added, and the removed value will be put back: */

					auto lastVal = std::move(product.providers.back());
					writer.RemoveValueFromMVColumn(ColProviders, product.providers.size());
					product.providers.pop_back();

					auto newProvider = "cheaptechlc";
					writer.SetColumn(ColProviders, AsInputParam(newProvider)); // each call adds a value to the list
					product.providers.push_back(newProvider);

					writer.SetColumn(ColProviders, AsInputParam(lastVal));
					product.providers.push_back(std::move(lastVal));

					// The expiration date must be in fractional days since 1900 so the backend can use it
					std::vector<double> deliveries(product.deliveries.size());

					for(int idx = 0; idx < product.deliveries.size(); ++idx)
					{
						product.deliveries[idx] += 86400; // increase 1 day in the delivery date

						writer.SetColumn(
							ColDeliveries,
							AsInputParam(deliveries[idx], product.deliveries[idx]),
							idx + 1
						);
					}

					// Append something to the description text:
					static const string descSuffix(" (updated)");
					product.description += descSuffix;
					writer.SetLargeColumnAppend(ColDescription, AsInputParam(descSuffix));

					// Save the new record
					writer.Save();
					return true;
				});

				EXPECT_GT(numRecords1, 0);

				// Go though all the updated entries and validate them:
				auto numRecords2 = cursor.ScanRange(range, [this] (RecordReader &rec)
				{
					using namespace std::chrono;

					uint16_t id;
					EXPECT_TRUE(rec.ReadFixedSizeValue(ColId, id));
					auto &product = products[id];

					float price;
					EXPECT_TRUE(rec.ReadFixedSizeValue(ColPrice, price));
					EXPECT_EQ(product.price, price);

					unsigned int amount;
					EXPECT_TRUE(rec.ReadFixedSizeValue(ColAmount, amount));
					EXPECT_EQ(product.amount, amount);

					static std::vector<string> providers;
					rec.ReadTextValues(ColProviders, providers);
					EXPECT_TRUE( std::equal(providers.begin(), providers.end(), product.providers.begin()) );

					static std::vector<time_point<system_clock, seconds>> deliveries;
					rec.ReadFixedSizeValues(ColDeliveries, deliveries);
					EXPECT_TRUE(
						std::equal(deliveries.begin(), 
								   deliveries.end(),
								   product.deliveries.begin(),
								   [] (const time_point<system_clock, seconds> &a, time_t b)
								   {
									   return a.time_since_epoch().count() == b;
								   })
					);

					string description;
					EXPECT_TRUE(rec.ReadTextValue(ColDescription, description));
					EXPECT_EQ(product.description, description);

					return true;
				});

				EXPECT_GT(numRecords2, 0);

				EXPECT_EQ(numRecords2, numRecords1);

				// Commit the inserted data so the other sessions can see it
				transaction.Commit();
			}
			catch(...)
			{
				HandleException();
			}
		}

		/// <summary>
		/// Tests changing the database schema and then some of its content.
		/// </summary>
		TEST_F(Framework_ISAM_TestCase, TableSchemaChange_Test)
		{
			using namespace isam;
			
			// Ensures proper initialization/finalization of the framework
			_3fd::core::FrameworkInstance _framework;

			CALL_STACK_TRACE;

			try
			{
				// Create an instance
				Instance instance("tester", ".\\temp\\");

				// Open the database
				auto conn = instance.OpenDatabase(0, FILE_PATH("isam-test.dat"));

				// Get the table schema
				auto table = conn.OpenTable("products");
				
				/* Map numeric codes for the column names. The framework uses this 
				approach in order to optimize metadata retrieval keeping in cache: */
				table->MapInt2ColName(ColId, "id");
				table->MapInt2ColName(ColSequence, "sequence");
				table->MapInt2ColName(ColProviders, "providers");
				table->MapInt2ColName(ColDescription, "description");

				// Remove column for whether fragile:
				table->DeleteColumn("fragile");

				// Rename the column for sequence code:
				table->RenameColumn("sequence", "sequenceid");

				// Create a new column for special handling:
				table->AddColumn(ITable::ColumnDefinition("sphandling", DataType::Text, false, true)); // nullable, multi-value
				table->MapInt2ColName(ColSpHandling, "sphandling");

				// Remove index for product name:
				table->DeleteIndex("idx-name");

				// Create a new index for the providers:
				std::vector<std::pair<string, Order>> idxKeyCols;
				idxKeyCols.emplace_back("providers", Order::Ascending);
				std::vector<ITable::IndexDefinition> idxDefs;
				idxDefs.emplace_back("providersidx", idxKeyCols, false, false);
				table->CreateIndexes(idxDefs);

				/* Map numeric codes for the indexes names. The framework uses this 
				approach in order to optimize metadata retrieval keeping it in cache: */
				table->MapInt2IdxName(IdxProvider, "providersidx");
				table->MapInt2IdxName(IdxBarCode, "idx-barcode");
				
				// Get a table cursor
				auto cursor = conn.GetCursorFor(table);

				// Begin a transaction
				auto transaction = conn.BeginTransaction();

				// Go through all the records and update some of them:
				auto numRecords = cursor.ScanAll(IdxBarCode, [this, &cursor] (RecordReader &rec)
				{
					uint16_t id;
					EXPECT_TRUE(rec.ReadFixedSizeValue(ColId, id));
					auto &product = products[id];

					// Test if the column with a changed name is still okay:
					int sequenceId;
					EXPECT_TRUE(rec.ReadFixedSizeValue(ColSequence, sequenceId));

					// The column 'fragile' no longer exists, but the new column 'sphandling' can take care of it:
					if(product.fragile)
					{
						auto writer = cursor.StartUpdate(TableWriter::Mode::Replace);
						writer.SetColumn(ColSpHandling, AsInputParam("fragile"));
						writer.Save();
					}

					return true;
				});

				EXPECT_EQ(products.size(), numRecords);

				auto newProvider("ceasa");
				auto oldProvider("makro");

				// Create an index range through which the table will be scanned:
				TableCursor::IndexRangeDefinition range;
				range.indexCode = IdxProvider;

				range.beginKey.colsVals.push_back(AsInputParam(oldProvider));
				range.beginKey.typeMatch = TableCursor::IndexKeyMatch::PrefixWildcard;
				range.beginKey.comparisonOper = TableCursor::ComparisonOperator::GreaterThanOrEqualTo;

				range.endKey.colsVals.push_back(AsInputParam(oldProvider));
				range.endKey.typeMatch = TableCursor::IndexKeyMatch::PrefixWildcard;
				range.endKey.isUpperLimit = true;
				range.endKey.isInclusive = true;

				// Go through the new index for providers and replace one provider by another:
				numRecords = cursor.ScanRange(range,
					// Callback:
					[this, &cursor, oldProvider, newProvider] (RecordReader &rec)
					{
						uint16_t id;
						EXPECT_TRUE(rec.ReadFixedSizeValue(ColId, id));
						auto &product = products[id];

						// Get a list of providers for the products:
						static std::vector<string> providers;
						rec.ReadTextValues(ColProviders, providers);
						auto iter = std::find(providers.begin(), providers.end(), oldProvider);
						EXPECT_NE(providers.end(), iter); // if the scan implementation is right, the given provider is expected to be there

						// Replace the given provider by a new one:
						if(providers.end() != iter)
						{
							auto tagSeqFoundProvider = std::distance(providers.begin(), iter) + 1;
							auto writer = cursor.StartUpdate(TableWriter::Mode::Replace);
							writer.SetColumn(ColProviders, AsInputParam(newProvider), tagSeqFoundProvider);
							writer.Save();
							std::replace(product.providers.begin(), product.providers.end(), oldProvider, newProvider);
						}

						return true;
					}
				);

				EXPECT_GT(numRecords, 0);

				// Go through all the records and validate the changes made:
				numRecords = cursor.ScanAll(IdxBarCode, [this, &cursor] (RecordReader &rec)
				{
					uint16_t id;
					EXPECT_TRUE(rec.ReadFixedSizeValue(ColId, id));
					auto &product = products[id];

					// The 'fragile' are now identifiable by the new column 'sphandling':
					if(product.fragile)
					{
						string spHandling;
						EXPECT_TRUE(rec.ReadTextValue(ColSpHandling, spHandling));
						EXPECT_EQ("fragile", spHandling);
					}

					// Check if the content for 'providers' is still consistent:
					static std::vector<string> providers;
					rec.ReadTextValues(ColProviders, providers);
					EXPECT_TRUE( std::equal(providers.begin(), providers.end(), product.providers.begin()) );

					return true;
				});

				EXPECT_EQ(products.size(), numRecords);

				// Commit the inserted data so the other sessions can see it
				transaction.Commit();
			}
			catch(...)
			{
				HandleException();
			}
		}

		/// <summary>
		/// Tests erasing all the records and then removing the table from the ISAM database.
		/// </summary>
		TEST_F(Framework_ISAM_TestCase, TableRemoval_Test)
		{
			using namespace isam;
			
			// Ensures proper initialization/finalization of the framework
			_3fd::core::FrameworkInstance _framework;

			CALL_STACK_TRACE;

			try
			{
				// Create an instance
				Instance instance("tester", ".\\temp\\");

				// Open the database
				auto conn = instance.OpenDatabase(0, FILE_PATH("isam-test.dat"));
				{
					// Get the table schema
					auto table = conn.OpenTable("products");

					/* Map numeric codes for the indexes names. The framework uses this 
					approach in order to optimize metadata retrieval keeping it in cache: */
					table->MapInt2IdxName(IdxBarCode, "idx-barcode");
					
					// Get a table cursor
					auto cursor = conn.GetCursorFor(table);

					// Begin a transaction
					auto transaction = conn.BeginTransaction();

					// Go though all the records and remove each one of them:
					auto numRecords = cursor.ScanAll(IdxBarCode, [this, &cursor] (RecordReader &rec)
					{
						cursor.DeleteCurrentRecord();
						return true;
					});

					EXPECT_EQ(products.size(), numRecords);
				
					// Commit the inserted data so the other sessions can see it
					transaction.Commit();
				}
				
				// Remove the table from the schema
				conn.DeleteTable("products");
			}
			catch(...)
			{
				HandleException();
			}
		}

		/// <summary>
		/// Tests stressing an ISAM database with a heavy load of historic data.
		/// </summary>
		TEST_F(Framework_ISAM_TestCase, HistDataStress_Test)
		{
			using namespace isam;

			// Ensures proper initialization/finalization of the framework
			_3fd::core::FrameworkInstance _framework;

			CALL_STACK_TRACE;

			try
			{
				// Create an instance
				Instance instance("tester", ".\\temp\\", 4, 256);

				bool dbIsNew, schemaAlreadyExistent(false);

				// Open the database if already existent, otherwise, create it:
				auto conn = instance.OpenDatabase(0, FILE_PATH("isam-stress.dat"), dbIsNew);
				{
					if (dbIsNew == false)
						conn.TryOpenTable(schemaAlreadyExistent, "history");

					if (schemaAlreadyExistent)
						conn.DeleteTable("history");

					// Start a transaction to guarantee the schema won't end up half done
					auto transaction1 = conn.BeginTransaction();

					// Define the table for historic data:
					std::vector<ITable::ColumnDefinition> columns;
					columns.reserve(3);
					columns.emplace_back("timestamp", DataType::DateTime, true); // not null
					columns.emplace_back("value", DataType::Float64, true); // not null
					columns.emplace_back("status", DataType::UByte, true); // not null
					columns.back().default = AsInputParam((uint8_t)0); // default value for status

					// Define the timestamp-index:
					std::vector<ITable::IndexDefinition> indexes;
					std::vector<std::pair<string, Order>> timestampIdxKeys;
					timestampIdxKeys.emplace_back("timestamp", Order::Ascending);
					indexes.emplace_back("timestamp", timestampIdxKeys, true); // primary, hence unique

					// Create the table with its columns and index:
					auto table = conn.CreateTable("history", false, columns, indexes);

					// Commit changes to the schema
					transaction1.Commit(false /*lazy commit*/);

					/* Map numeric codes for the column names. The framework uses this
					approach in order to optimize metadata retrieval keeping in cache: */
					table->MapInt2ColName(ColTimestamp, "timestamp");
					table->MapInt2ColName(ColValue, "value");
					table->MapInt2ColName(ColStatus, "status");

					// Get a table cursor:
					auto cursor = conn.GetCursorFor(table);

					// Begin a transaction
					auto transaction2 = conn.BeginTransaction();

					auto stime = time(nullptr); // start timestamp

					// Fill the table with data:
					for (int count = 0; count < 4e4; ++count)
					{
						// Start an update to insert a new record in the table
						auto writer = cursor.StartUpdate(TableWriter::Mode::InsertNew);

						// Define the columns values of the record:
						double daysSince1900; // the timestamp must first be converted to fractional days since 1900 so backend can understand
						writer.SetColumn(ColTimestamp, AsInputParam(daysSince1900, stime + count));
						writer.SetColumn(ColValue, AsInputParam(rand() / 10.0));

						// Save the new record
						writer.Save();
					}

					// Commit the inserted data
					transaction2.Commit();
				}

				// Remove table from the schema
				conn.DeleteTable("history");
			}
			catch (...)
			{
				HandleException();
			}
		}

	}// end of namespace integration_tests
}// end of namespace _3fd
