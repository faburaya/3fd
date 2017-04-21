#ifndef RPC_TEST_SHARED_H // header guard
#define RPC_TEST_SHARED_H

#include "rpc_helpers.h"
#include <gtest/gtest-param-test.h>
#include <array>

// Choose the scenario uncommenting one of the following macros:

#define SCENARIO_SINGLE_BOX_LOCAL_SEC
//#define SCENARIO_SINGLE_BOX_AD_SEC
//#define SCENARIO_REMOTE_WITH_AD_SEC

namespace _3fd
{
namespace integration_tests
{
    /* In this implementation:
        Operate(left, right) = left * right
        ChangeCase(input) = toUpper(input)
    */
    extern const std::array<const char *, 20> objectsUuidsImpl1;

    /* In this implementation:
        Operate(left, right) = left + right
        ChangeCase(input) = toLower(input)
    */
    extern const std::array<const char *, 20> objectsUuidsImpl2;

    /// <summary>
    /// A set of options to be used as variable parameters of test cases
    /// for RPC module with Schannel SSP in both server & client sides.
    /// </summary>
    struct SchannelTestOptions
    {
        const char *objectUUID1;
        const char *objectUUID2;
        rpc::AuthenticationLevel authenticationLevel;
        bool useStrongSec;
    };

    const auto paramsForSchannelTests = ::testing::Values(
        SchannelTestOptions{ objectsUuidsImpl1[16], objectsUuidsImpl2[16], rpc::AuthenticationLevel::Privacy, false },
        SchannelTestOptions{ objectsUuidsImpl1[17], objectsUuidsImpl2[17], rpc::AuthenticationLevel::Privacy, true },
        SchannelTestOptions{ objectsUuidsImpl1[18], objectsUuidsImpl2[18], rpc::AuthenticationLevel::Integrity, false },
        SchannelTestOptions{ objectsUuidsImpl1[19], objectsUuidsImpl2[19], rpc::AuthenticationLevel::Integrity, true }
    );

}// end of namespace integration_tests
}// end of namespace _3fd

#endif // end of header guard
