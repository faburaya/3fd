#ifndef METADATACOPIER_H // header guard
#define METADATACOPIER_H

#include <map>
#include <vector>
#include <string>

namespace application
{
    using std::string;

    /// <summary>
    /// Handles how to copy metadata from one image file to another.
    /// </summary>
    class MetadataCopier
    {
    private:

    public:

        MetadataCopier(const string &cfgFilePath);
    };

}// end of namespace application

#endif // end of header guard
