// TestClient.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

int wmain(int argc, wchar_t *argv[])
{
    std::cout << "Running main() from \'TestClient.cpp\'\n";
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}