#ifndef METADATACOPIER_H // header guard
#define METADATACOPIER_H

#include <vector>
#include <string>
#include <mutex>
#include <memory>
#include <cinttypes>
#include <wincodec.h>
#include <wtypes.h>
#include <wrl.h>

namespace application
{
    using std::string;

    using namespace Microsoft::WRL;

    class MetadataMapCases;
    class MetadataItems;

    /// <summary>
    /// Handles how to copy metadata from one image file to another.
    /// </summary>
    class MetadataCopier
    {
    private:

        ComPtr<IWICImagingFactory> m_wicImagingFactory;

        MetadataMapCases *m_mapCases;
        MetadataItems *m_items;

        MetadataCopier(const string &cfgFilePath);

        static std::mutex singletonCreationMutex;
        static std::unique_ptr<MetadataCopier> uniqueInstance;

    public:

        static MetadataCopier &GetInstance();

        static void Finalize();

        ~MetadataCopier();

        void Copy(IWICMetadataQueryReader *from, IWICMetadataQueryWriter *to);
    };

}// end of namespace application

#endif // end of header guard
