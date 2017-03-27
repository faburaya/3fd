#ifndef METADATACOPIER_H // header guard
#define METADATACOPIER_H

#include <vector>
#include <string>
#include <memory>
#include <cinttypes>
#include <wtypes.h>

namespace application
{
    using std::string;

    class MetadataMapCases;

    /// <summary>
    /// Handles how to copy metadata from one image file to another.
    /// </summary>
    class MetadataCopier
    {
    private:

        std::unique_ptr<MetadataMapCases> m_mapCases;

    public:

        MetadataCopier(const string &cfgFilePath);

        ~MetadataCopier();
    };

}// end of namespace application

#endif // end of header guard
