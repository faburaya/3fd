#ifndef RPC_TEST_SHARED_H // header guard
#define RPC_TEST_SHARED_H

#include <array>

namespace _3fd
{
    namespace rpc
    {
        /* In this implementation:
            Operate(left, right) = left * right
            ChangeCase(input) = toUpper(input)
        */
        extern const std::array<const char *, 12> objectsUuidsImpl1;

        /* In this implementation:
            Operate(left, right) = left + right
            ChangeCase(input) = toLower(input)
        */
        extern const std::array<const char *, 12> objectsUuidsImpl2;

    }// end of namespace rpc
}// end of namespace _3fd

#endif // end of header guard
