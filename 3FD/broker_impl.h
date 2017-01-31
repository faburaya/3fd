#ifndef BROKER_IMPL_H // header guard
#define BROKER_IMPL_H

#include "broker.h"

namespace _3fd
{
namespace broker
{
    const char *ToString(Backend backend);

    const char *ToString(MessageContentValidation msgContentValidation);

}// end of namespace broker
}// end of namespace _3fd

#endif // end of header guard
