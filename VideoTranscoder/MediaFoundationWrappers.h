#ifndef MEDIAFOUNDATIONWRAPPERS_H // header guard
#define MEDIAFOUNDATIONWRAPPERS_H

#include "3FD\base.h"
#include <string>
#include <mfreadwrite.h>
#include <wrl.h>

namespace application
{
    using std::string;
    using namespace Microsoft::WRL;

    /// <summary>
    /// Uses RAII to initialize and finalize Microsoft Media Foundation library.
    /// </summary>
    class MediaFoundationLib
    {
    public:

        MediaFoundationLib();
        ~MediaFoundationLib();
    };

    /// <summary>
    /// Wraps Media Foundation Source Reader object.
    /// </summary>
    class MFSourceReader : notcopiable
    {
    private:

        ComPtr<IMFAttributes> m_mfAttrStore;
        ComPtr<IMFSourceReader> m_mfSourceReader;

    public:

        MFSourceReader(const string &url);
        ~MFSourceReader();
    };
}

#endif // end of header guard
