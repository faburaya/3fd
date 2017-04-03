#ifndef METADATACOPIER_H // header guard
#define METADATACOPIER_H

#include <vector>
#include <string>
#include <cinttypes>
#include <wtypes.h>

namespace application
{
    using std::string;

    class MetadataMapCases;
    class MetadataItems;

    /// <summary>
    /// Handles how to copy metadata from one image file to another.
    /// </summary>
    class MetadataCopier
    {
    private:

        MetadataMapCases *m_mapCases;
        MetadataItems *m_items;

    public:

        MetadataCopier(const string &cfgFilePath);

        ~MetadataCopier();
    };

}// end of namespace application

#endif // end of header guard
