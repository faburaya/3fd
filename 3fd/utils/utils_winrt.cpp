#include "pch.h"
#include "utils_winrt.h"

#include <shcore.h>
#include <wrl.h>
#include <windows.storage.streams.h>
#include <winrt\Windows.Storage.Streams.h>
#include <winrt\Windows.ApplicationModel.h>
#include <winrt\Windows.UI.Xaml.Controls.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <codecvt>
#include <limits>

#undef min
#undef max

namespace _3fd
{
namespace utils
{
    /// <summary>
    /// Implements <see cref="IStream"/> around an already existing buffer.
    /// </summary>
    /// <seealso cref="IStream"/>
    class ComStreamFromBuffer : public IStream
    {
    private:

        uint8_t * const m_data;

        uint8_t *m_curr;

        const ULONG m_nBytes;

        ULONG m_refCount;

        /// <summary>
        /// Gets an iterator for the end position in the buffer.
        /// </summary>
        /// <returns>A pointer to the position that is 1 byte past the last in the buffer.</returns>
        uint8_t *end() const { return m_data + m_nBytes; }

        /// <summary>
        /// Gets the remaining number of bytes to read from or write into the buffer.
        /// This implementations does not reallocate memory to expand, because it is meant
        /// to use an already existing buffer allocated from any valid store/heap.
        /// </summary>
        /// <returns>The remaining number of bytes to read from or write into the buffer.</returns>
        ULONG GetRemainingNumBytes() const { return static_cast<ULONG>(end() - m_curr); }

    public:

        /// <summary>
        /// Initializes a new instance of the <see cref="ComStreamFromBuffer"/> class.
        /// </summary>
        /// <param name="data">The address of the buffer memory backing this stream.</param>
        /// <param name="nBytes">The size of the buffer in bytes.</param>
        ComStreamFromBuffer(void *data, ULONG nBytes) noexcept
            : m_data(static_cast<uint8_t *> (data))
            , m_curr(static_cast<uint8_t *> (data))
            , m_nBytes(nBytes)
        {}


        /// <summary>
        /// Requests for an interface implementation.
        /// </summary>
        /// <param name="iid">The UUID of the required interface.</param>
        /// <param name="ppvObject">Receives a pointer for an implementation of the requested interface.</param>
        /// <returns>The status as an HRESULT code.</returns>
        virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) override
        {
            if (iid == __uuidof(IUnknown)
                || iid == __uuidof(IStream)
                || iid == __uuidof(ISequentialStream))
            {
                *ppvObject = static_cast<IStream *>(this);
                AddRef();
                return S_OK;
            }
            else
                return E_NOINTERFACE;
        }


        /// <summary>
        /// Increases the reference count for this instance.
        /// </summary>
        /// <returns>The new reference count.</returns>
        virtual ULONG STDMETHODCALLTYPE AddRef() override
        {
            return (ULONG)InterlockedIncrement(&m_refCount);
        }


        /// <summary>
        /// Decrements the reference count for this instance.
        /// </summary>
        /// <returns>The new reference count.</returns>
        virtual ULONG STDMETHODCALLTYPE Release() override
        {
            ULONG count = (ULONG)InterlockedDecrement(&m_refCount);

            if (count == 0)
                delete this;

            return count;
        }


        /// <summary>
        /// Reads a specified number of bytes from the stream object into memory starting at the current seek pointer.
        /// </summary>
        /// <param name="into">A pointer to the buffer which the stream data is read into.</param>
        /// <param name="nBytes">The number of bytes of data to read from this stream object.</param>
        /// <param name="bytesRead">Receives the actual number of bytes read from the stream object.</param>
        /// <returns>The status as an HRESULT code.</returns>
        virtual HRESULT STDMETHODCALLTYPE Read(void *into, ULONG nBytes, ULONG *bytesRead) override
        {
            if (into == nullptr || bytesRead == nullptr)
                return STG_E_INVALIDPOINTER;

            *bytesRead = std::min(GetRemainingNumBytes(), nBytes);
            memcpy(into, m_curr, *bytesRead);
            m_curr += *bytesRead;

            return *bytesRead == nBytes ? S_OK : S_FALSE;
        }


        /// <summary>
        /// Writes a specified number of bytes into the stream object starting at the current seek pointer.
        /// </summary>
        /// <param name="from">A pointer to the buffer that contains the data that is to be written to the stream.
        /// A valid pointer must be provided for this parameter even when the amount of bytes to write is zero.</param>
        /// <param name="nBytes">The number of bytes of data to attempt to write into the stream. This value can be zero.</param>
        /// <param name="bytesWritten">Optional: receives the actual number of bytes written to the stream object.</param>
        /// <returns>The status as an HRESULT code.</returns>
        virtual HRESULT STDMETHODCALLTYPE Write(const void *from, ULONG nBytes, ULONG *bytesWritten) override
        {
            if (from == nullptr)
                return STG_E_INVALIDPOINTER;

            ULONG wcount = std::min(GetRemainingNumBytes(), nBytes);
            memcpy(m_curr, from, wcount);

            if (bytesWritten != nullptr)
                *bytesWritten = wcount;

            if (wcount == nBytes)
            {
                m_curr += wcount;
                return S_OK;
            }

            return STG_E_MEDIUMFULL;
        }


        /// <summary>
        /// Copies a specified number of bytes read from the current seek pointer in this stream to another stream.
        /// </summary>
        /// <param name="dest">A pointer to the destination stream. It can be a new stream or a clone of the source stream.</param>
        /// <param name="nBytes">The number of bytes to copy from the source stream.</param>
        /// <param name="bytesRead">Optional: a pointer to the location where
        /// this method writes the actual number of bytes read from the source.</param>
        /// <param name="bytesWritten">Optional: a pointer to the location where
        /// this method writes the actual number of bytes written to the destination.</param>
        /// <returns>The status as an HRESULT code.</returns>
        virtual HRESULT STDMETHODCALLTYPE CopyTo(IStream *dest,
                                                 ULARGE_INTEGER nBytes,
                                                 ULARGE_INTEGER *bytesRead,
                                                 ULARGE_INTEGER *bytesWritten) override
        {
            if (dest == nullptr)
                return STG_E_INVALIDPOINTER;

            ULONG wcount(0);

            // first limit the amount of bytes to read by what the integer types in this implementation support...
            auto gcount = static_cast<decltype(m_nBytes)> (
                std::min(
                    static_cast<decltype(nBytes.QuadPart)> (std::numeric_limits<decltype(m_nBytes)>::max()),
                    nBytes.QuadPart
                )
                );

            // ... then limit by the amount of bytes remaining to be read
            gcount = std::min(this->GetRemainingNumBytes(), gcount);

            HRESULT hr = dest->Write(this->m_curr, gcount, &wcount);

            if (SUCCEEDED(hr))
                gcount = wcount;
            else
                gcount = 0;

            this->m_curr += gcount;

            if (bytesRead != nullptr)
                bytesRead->QuadPart = gcount;

            if (bytesWritten != nullptr)
                bytesWritten->QuadPart = wcount;

            return hr;
        }


        /// <summary>
        /// Ensures that any changes made to a stream object open in
        /// transacted mode are reflected in the parent storage object.
        /// </summary>
        /// <param name="grfCommitFlags">Controls how the changes for the stream object are committed.
        /// See the <see cref="STGC"/> enumeration for a definition of these values.</param>
        /// <returns>The status as an HRESULT code.</returns>
        virtual HRESULT STDMETHODCALLTYPE Commit(DWORD grfCommitFlags) override
        {
            if ((STGC_OVERWRITE ^ grfCommitFlags) == 0)
                return S_OK; // do nothing, already committed to memory
            else
                return E_NOTIMPL;
        }

        // This implementation cannot revert data already written to the underlying buffer!
        virtual HRESULT STDMETHODCALLTYPE Revert() override
        {
            return E_NOTIMPL;
        }


        /// <summary>
        /// Creates a new stream object with its own seek pointer that references the same bytes as the original stream.
        /// </summary>
        /// <param name="object">Receives the clone object.</param>
        /// <returns>The status as an HRESULT code.</returns>
        virtual HRESULT STDMETHODCALLTYPE Clone(IStream **object) override
        {
            if (object == nullptr)
                return STG_E_INVALIDPOINTER;

            try
            {
                *object = dbg_new ComStreamFromBuffer(m_data, m_nBytes);
            }
            catch (std::bad_alloc &)
            {
                return STG_E_INSUFFICIENTMEMORY;
            }

            return S_OK;
        }


        /// <summary>
        /// Seeks the specified n bytes.
        /// </summary>
        /// <param name="nBytes">The displacement to be added to the location indicated by
        /// the <c>origin</c> parameter. If <c>origin</c> is STREAM_SEEK_SET, this is
        /// interpreted as an unsigned value rather than a signed value.</param>
        /// <param name="origin">The origin for the displacement. The origin can be the
        /// beginning of the file (<c>STREAM_SEEK_SET</c>), the current seek pointer
        /// (<c>STREAM_SEEK_CUR</c>), or the end of the file (<c>STREAM_SEEK_END</c>). For
        /// more information about values, see the <see cref="STREAM_SEEK"/> enumeration.</param>
        /// <param name="offset">Optional: a pointer to the location where this method writes
        /// the value of the new seek pointer from the beginning of the stream.</param>
        /// <returns>The status as an HRESULT code.</returns>
        virtual HRESULT STDMETHODCALLTYPE Seek(LARGE_INTEGER nBytes,
                                               DWORD origin,
                                               ULARGE_INTEGER *offset) override
        {
            switch (origin)
            {
            case STREAM_SEEK_SET:
                m_curr = m_data + nBytes.QuadPart;
                break;
            case STREAM_SEEK_CUR:
                m_curr += nBytes.QuadPart;
                break;
            case STREAM_SEEK_END:
                m_curr = end() + nBytes.QuadPart;
                break;
            default:
                return STG_E_INVALIDFUNCTION;
            }

            // moved out of bounds?
            if (m_curr < m_data || m_curr > end())
                return STG_E_INVALIDFUNCTION;

            if (offset != nullptr)
                offset->QuadPart = m_curr - m_data;

            return S_OK;
        }


        // This implementation cannot resize the underlying buffer!
        virtual HRESULT STDMETHODCALLTYPE SetSize(ULARGE_INTEGER) override
        {
            return E_NOTIMPL;
        }

        // This implementation cannot provide stats!
        virtual HRESULT STDMETHODCALLTYPE Stat(STATSTG *, DWORD) override
        {
            return E_NOTIMPL;
        }

        // This implementation cannot lock/unlock regions of the underlying buffer!
        virtual HRESULT STDMETHODCALLTYPE LockRegion(ULARGE_INTEGER, ULARGE_INTEGER, DWORD) override
        {
            return E_NOTIMPL;
        }

        // This implementation cannot lock/unlock regions of the underlying buffer!
        virtual HRESULT STDMETHODCALLTYPE UnlockRegion(ULARGE_INTEGER, ULARGE_INTEGER, DWORD) override
        {
            return E_NOTIMPL;
        }
    };


    ////////////////////////
    // WinRTExt Struct
    ////////////////////////


    /// <summary>
    /// Creates a random access stream from an already existing buffer.
    /// </summary>
    /// <param name="data">The address of the buffer memory backing this stream.</param>
    /// <param name="nBytes">The size of the buffer in bytes.</param>
    /// <returns>An object implementing <see cref="winrt::Windows::Storage::Streams::IRandomAccessStream"/>.</returns>
    WinRTExt::IRandomAccessStream WinRTExt::CreateRandomAccessStreamFromBuffer(void *data, ULONG nBytes)
    {
        try
        {
            Microsoft::WRL::ComPtr<::IStream> bufStream = dbg_new ComStreamFromBuffer(data, nBytes);

            winrt::com_ptr<ABI::Windows::Storage::Streams::IRandomAccessStream> abiObject;
            winrt::check_hresult(
                CreateRandomAccessStreamOverStream(bufStream.Get(),
                                                    BSOS_PREFERDESTINATIONSTREAM,
                                                    __uuidof(abiObject),
                                                    abiObject.put_void())
            );
            
            // see https://docs.microsoft.com/de-de/windows/uwp/cpp-and-winrt-apis/interop-winrt-abi
            IRandomAccessStream randomAccessStream;
            winrt::check_hresult(
                abiObject->QueryInterface(reinterpret_cast<const IID &>(winrt::guid_of<IRandomAccessStream>()),
                                          winrt::put_abi(randomAccessStream))
            );

            return randomAccessStream;
        }
        catch (winrt::hresult_error &ex)
        {
            std::ostringstream oss;
            oss << "Failed to create stream from buffer - " << core::WWAPI::GetDetailsFromWinRTEx(ex);
            throw core::AppException<std::runtime_error>(oss.str());
        }

    }

    /// <summary>
    /// Gets the path for a given directory in the sandbox.
    /// </summary>
    static winrt::hstring GetPath(WinRTExt::FileLocation where)
    {
        using namespace winrt::Windows::Storage;

        switch (where)
        {
        case WinRTExt::FileLocation::InstallFolder:
            return winrt::Windows::ApplicationModel::Package::Current().InstalledLocation().Path();

        case WinRTExt::FileLocation::LocalFolder:
            return ApplicationData::Current().LocalFolder().Path();

        case WinRTExt::FileLocation::TempFolder:
            return ApplicationData::Current().TemporaryFolder().Path();

        case WinRTExt::FileLocation::RoamingFolder:
            return ApplicationData::Current().RoamingFolder().Path();

        default:
            _ASSERTE(false);
            return winrt::hstring();
        }
    }

    /// <summary>
    /// Gets the path of a specified location of the
    /// sandboxed storage system of WinRT platform.
    /// </summary>
    /// <param name="where">The location.</param>
    /// <returns>The location path, UTF-8 encoded.</returns>
    std::string WinRTExt::GetPathUtf8(FileLocation where)
    {
        return winrt::to_string(GetPath(where) + L"\\");
    }

    /// <summary>
    /// Gets the path of a file in the specified location
    /// of the sandboxed storage system of WinRT platform.
    /// The files does not need to exist. Only the path is generated.
    /// </summary>
    /// <param name="fileName">Name of the file.</param>
    /// <param name="where">The location where to look for the file.</param>
    /// <returns>The complete path of the file (UTF-8 encoded) in the given location.</returns>
    std::string WinRTExt::GetFilePathUtf8(std::string_view fileName, WinRTExt::FileLocation where)
    {
        std::ostringstream oss;
        oss << winrt::to_string(GetPath(where)) << '\\' << fileName;
        return oss.str();
    }

    /// <summary>
    /// Gets the path of a file in the specified location
    /// of the sandboxed storage system of WinRT platform.
    /// If the specified file does not exist, it is created.
    /// </summary>
    /// <param name="fileName">Name of the file.</param>
    /// <param name="where">The location where to look for the file.</param>
    /// <returns>The complete path of the file (UTF-8 encoded) in the given location.</returns>
    std::string WinRTExt::GetFilePathUtf8(std::wstring_view fileName, WinRTExt::FileLocation where)
    {
        return winrt::to_string(GetPath(where) + L'\\' + fileName);
    }

    /// <summary>
    /// Determines whether the current thread is the application main STA thread.
    /// </summary>
    /// <returns>
    /// <c>true</c> if the current thread is the application main STA thread or if information
    /// regarding the thread apartment could not be retrieved, otherwise, <c>false</c>.
    /// </returns>
    bool WinRTExt::IsCurrentThreadASTA()
    {
        APTTYPE aptType;
        APTTYPEQUALIFIER aptTypeQualifier;

        auto hres = CoGetApartmentType(&aptType, &aptTypeQualifier);

        _ASSERTE(hres != E_INVALIDARG);

        if (hres != S_OK)
        {
            std::ostringstream oss;
            oss << "COM API error: could not get apartment information from current thread"
                    " - CoGetApartmentType returned "
                << core::WWAPI::GetHResultLabel(hres);

            throw core::AppException<std::runtime_error>(oss.str());
        }

        return (aptType == APTTYPE_STA || aptType == APTTYPE_MAINSTA);
    }


    ///////////////////////
    // UwpXaml Struct
    ///////////////////////


    /// <summary>
    /// Notifies the user about an error (using a dialog) and writes the event to the log.
    /// </summary>
    /// <param name="title">The title of the dialog.</param>
    /// <param name="content">The error message.</param>
    /// <param name="closeButtonText">The close button text.</param>
    void UwpXaml::Notify(std::string_view title,
                         std::string_view content,
                         std::string_view closeButtonText)
    {
        winrt::Windows::UI::Xaml::Controls::ContentDialog dialog;
        dialog.Title(winrt::box_value(winrt::to_hstring(title)));
        dialog.Content(winrt::box_value(winrt::to_hstring(content)));
        dialog.SecondaryButtonText(winrt::to_hstring(closeButtonText));
        dialog.ShowAsync();
    }

    /// <summary>
    /// Notifies the user about an exception (using a dialog) and writes the event to the log.
    /// </summary>
    /// <param name="ex">The exception.</param>
    /// <param name="title">The dialog title.</param>
    /// <param name="closeButtonText">The close button text.</param>
    /// <param name="logEntryPrio">The priority for the log entry.</param>
    void UwpXaml::NotifyAndLog(const std::exception &ex,
                               std::string_view title,
                               std::string_view closeButtonText,
                               core::Logger::Priority logEntryPrio)
    {
        std::ostringstream oss;
        oss << "Generic exception: " << ex.what();
        std::string utf8content = oss.str();
        Notify(title, utf8content, closeButtonText);
        core::Logger::Write(utf8content, logEntryPrio);
    }

    /// <summary>
    /// Notifies the user about an exception (using a dialog) and writes the event to the log.
    /// </summary>
    /// <param name="ex">The exception.</param>
    /// <param name="title">The dialog title.</param>
    /// <param name="closeButtonText">The close button text.</param>
    /// <param name="logEntryPrio">The priority for the log entry.</param>
    void UwpXaml::NotifyAndLog(const winrt::hresult_error &ex,
                               std::string_view title,
                               std::string_view closeButtonText,
                               core::Logger::Priority logEntryPrio)
    {
        std::ostringstream oss;
        oss << "HRESULT error code 0x" << std::hex << ex.code() << ": ";

        std::array<char, 256> buffer;
        strncpy(buffer.data(), winrt::to_string(ex.message()).data(), buffer.size());
        buffer[buffer.size() - 1] = '\0';

        auto token = strtok(buffer.data(), "\r\n");
        while (true)
        {
            oss << token;

            token = strtok(nullptr, "\r\n");
            if (token != nullptr)
                oss << " - ";
            else
                break;
        }

        std::string message = oss.str();
        Notify(title, message, closeButtonText);
        core::Logger::Write(message, logEntryPrio);
    }

    /// <summary>
    /// Notifies the user about an exception (using a dialog) and writes the event to the log.
    /// </summary>
    /// <param name="ex">The exception.</param>
    /// <param name="title">The dialog title.</param>
    /// <param name="closeButtonText">The close button text.</param>
    /// <param name="logEntryPrio">The priority for the log entry.</param>
    void UwpXaml::NotifyAndLog(const core::IAppException &ex,
                               std::string_view title,
                               std::string_view closeButtonText,
                               core::Logger::Priority logEntryPrio)
    {
        std::ostringstream oss;
        oss << ex.What() << "\n\n" << ex.Details();
        Notify(title, oss.str(), closeButtonText);
        core::Logger::Write(ex.ToString(), logEntryPrio);
    }

    /// <summary>
    /// Receives an asynchronous action to handle an eventual thrown
    /// exception by notifying with a dialog and logging the event.
    /// </summary>
    /// <param name="task">The task.</param>
    /// <param name="exHndParams">The parameters for notification and logging of the event.</param>
    /// <returns>
    /// <c>STATUS_OKAY</c> if the task finished without throwing an exception, otherwise, <c>STATUS_FAIL</c>.
    /// </returns>
    bool UwpXaml::CheckActionTask(const concurrency::task<void> &task,
                                  std::string_view title,
                                  std::string_view closeButtonText,
                                  core::Logger::Priority logEntryPrio)
    {
        try
        {
            task.get();
            return STATUS_OKAY;
        }
        catch (winrt::hresult_error &ex)
        {
            NotifyAndLog(ex, title, closeButtonText, logEntryPrio);
        }
        catch (core::IAppException &ex)
        {
            NotifyAndLog(ex, title, closeButtonText, logEntryPrio);
        }
        catch (std::exception &ex)
        {
            NotifyAndLog(ex, title, closeButtonText, logEntryPrio);
        }
        catch (...)
        {
            _ASSERTE(false); // extend implementation to handle more exception types!
        }

        return STATUS_FAIL;
    }

} // end of namespace utils
} // end of namespace _3fd
