#include "stdafx.h"
#include "utils_winrt.h"
#include "exceptions.h"
#include "logger.h"
#include <shcore.h>
#include <windows.storage.streams.h>

#include <array>
#include <limits>
#include <cassert>
#include <codecvt>
#include <algorithm>

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
        ULONG GetRemainingNumBytes() const { return end() - m_curr; }

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

            *object = new (std::nothrow) ComStreamFromBuffer(m_data, m_nBytes);

            return *object != nullptr ? S_OK : STG_E_INSUFFICIENTMEMORY;
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
    /// <returns>An object implementing <see cref="Windows::Storage::Streams::IRandomAccessStream"/>.</returns>
    Windows::Storage::Streams::IRandomAccessStream ^
    WinRTExt::CreateRandomAccessStreamFromBuffer(void *data, ULONG nBytes)
    {
        using namespace Microsoft::WRL;
        using namespace ABI::Windows::Storage::Streams;

        ComPtr<IStream> bufStream = new ComStreamFromBuffer(data, nBytes);

        ComPtr<IRandomAccessStream> winrtStream;

        HRESULT hr = CreateRandomAccessStreamOverStream(bufStream.Get(),
                                                        BSOS_PREFERDESTINATIONSTREAM,
                                                        IID_PPV_ARGS(winrtStream.GetAddressOf()));
        core::WWAPI::RaiseHResultException(hr,
            "Failed to create stream from buffer",
            "CreateRandomAccessStreamOverStream");

        return reinterpret_cast<Windows::Storage::Streams::IRandomAccessStream ^> (winrtStream.Get());
    }


    /// <summary>
    /// Gets the path of a specified location of the
    /// sandboxed storage system of WinRT platform.
    /// </summary>
    /// <param name="where">The location.</param>
    /// <returns>The location path, UTF-8 encoded.</returns>
    std::string WinRTExt::GetPathUtf8(FileLocation where)
    {
        using namespace Windows::Storage;

        Platform::String ^path;

        switch (where)
        {
        case WinRTExt::FileLocation::InstallFolder:
            path = Windows::ApplicationModel::Package::Current->InstalledLocation + L"\\";
            break;

        case WinRTExt::FileLocation::LocalFolder:
            path = ApplicationData::Current->LocalFolder->Path + L"\\";
            break;

        case WinRTExt::FileLocation::TempFolder:
            path = ApplicationData::Current->TemporaryFolder->Path + L"\\";
            break;

        case WinRTExt::FileLocation::RoamingFolder:
            path = ApplicationData::Current->RoamingFolder->Path + L"\\";
            break;

        default:
            _ASSERTE(false);
        }

        std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
        return transcoder.to_bytes(path->Data());
    }

    // Private implementation for 'GetFilePath'
    static std::string GetFilePathUtf8Impl(
        Platform::String ^utf16FileName,
        WinRTExt::FileLocation where,
        std::wstring_convert<std::codecvt_utf8<wchar_t>> &transcoder)
    {
        using namespace Windows::Storage;

        StorageFolder ^folder;
        
        switch (where)
        {
        case WinRTExt::FileLocation::InstallFolder:
            folder = Windows::ApplicationModel::Package::Current->InstalledLocation;
            break;

        case WinRTExt::FileLocation::LocalFolder:
            folder = ApplicationData::Current->LocalFolder;
            break;

        case WinRTExt::FileLocation::TempFolder:
            folder = ApplicationData::Current->TemporaryFolder;
            break;

        case WinRTExt::FileLocation::RoamingFolder:
            folder = ApplicationData::Current->RoamingFolder;
            break;

        default:
            break;
        }

        std::wostringstream woss;
        woss << folder->Path->Data() << L'\\' << utf16FileName->Data();
        return transcoder.to_bytes(woss.str());
    }

    /// <summary>
    /// Gets the path of a file in the specified location
    /// of the sandboxed storage system of WinRT platform.
    /// The files does not need to exist. Only the path is generated.
    /// </summary>
    /// <param name="fileName">Name of the file.</param>
    /// <param name="where">The location where to look for the file.</param>
    /// <returns>The complete path of the file (UTF-8 encoded) in the given location.</returns>
    std::string WinRTExt::GetFilePathUtf8(const std::string &fileName, WinRTExt::FileLocation where)
    {
        std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
        auto utf16FileName = ref new Platform::String(transcoder.from_bytes(fileName).c_str());
        return GetFilePathUtf8Impl(utf16FileName, where, transcoder);
    }

    /// <summary>
    /// Gets the path of a file in the specified location
    /// of the sandboxed storage system of WinRT platform.
    /// If the specified file does not exist, it is created.
    /// </summary>
    /// <param name="fileName">Name of the file.</param>
    /// <param name="where">The location where to look for the file.</param>
    /// <returns>The complete path of the file (UTF-8 encoded) in the given location.</returns>
    std::string WinRTExt::GetFilePathUtf8(const char *fileName, WinRTExt::FileLocation where)
    {
        std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
        auto utf16FileName = ref new Platform::String(transcoder.from_bytes(fileName).c_str());
        return GetFilePathUtf8Impl(utf16FileName, where, transcoder);
    }

    /// <summary>
    /// Gets the path of a file in the specified location
    /// of the sandboxed storage system of WinRT platform.
    /// If the specified file does not exist, it is created.
    /// </summary>
    /// <param name="fileName">Name of the file.</param>
    /// <param name="where">The location where to look for the file.</param>
    /// <returns>The complete path of the file (UTF-8 encoded) in the given location.</returns>
    std::string WinRTExt::GetFilePathUtf8(const wchar_t *fileName, WinRTExt::FileLocation where)
    {
        std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
        Platform::StringReference utf16FileName(fileName);
        return GetFilePathUtf8Impl(utf16FileName, where, transcoder);
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

    // Translates a WinRT exception to a framework exception
    core::AppException<std::runtime_error> WinRTExt::TranslateAsyncWinRTEx(Platform::Exception ^ex)
    {
        std::ostringstream oss;
        oss << "Windows Runtime asynchronous call reported an error: "
            << core::WWAPI::GetDetailsFromWinRTEx(ex);

        return core::AppException<std::runtime_error>(oss.str());
    }


    /// <summary>
    /// Waits for an asynchronous WinRT action in the application UI STA thread.
    /// </summary>
    /// <param name="asyncAction">The asynchronous WinRT action to wait for.</param>
    void WinRTExt::WaitForAsync(IAsyncAction ^asyncAction)
    {
        using Windows::Foundation::AsyncStatus;

        try
        {
            /* If callback is completed, just exit. If callback execution
            is not finished, awaiting for completion is allowed as long as
            the current thread is not in app UI STA: */
            if (asyncAction->Status != AsyncStatus::Started || !IsCurrentThreadASTA())
            {
                try
                {
                    concurrency::create_task(asyncAction).get();
                    return;
                }
                catch (Platform::Exception ^ex)
                {
                    throw TranslateAsyncWinRTEx(ex);
                }
            }

            /* Otherwise, await for completion the way done below, which is the easiest
            that works in the app UI STA thread, and transports any eventual exception: */

            std::promise<void> resultPromise;
            auto resultFuture = resultPromise.get_future();

            // Set delegate for completion event:
            asyncAction->Completed = ref new AsyncActionCompletedHandler(
                // Handler for completion:
                [&resultPromise](decltype(asyncAction) action, AsyncStatus status)
                {
                    try
                    {
                        // Get result and fulfill the promise:
                        action->GetResults();
                        action->Close();
                        resultPromise.set_value();
                    }
                    catch (Platform::Exception ^ex) // transport exception:
                    {
                        auto appEx = std::make_exception_ptr(TranslateAsyncWinRTEx(ex));
                        resultPromise.set_exception(appEx);
                    }
                }
            );

            resultFuture.get(); // waits for completion and throws any transported exception
        }
        catch (core::IAppException &)
        {
            throw; // just forward exceptions for errors that have already been handled
        }
        catch (std::future_error &ex)
        {
            std::ostringstream oss;
            oss << "Failed to wait for WinRT asynchronous action: "
                << core::StdLibExt::GetDetailsFromFutureError(ex);

            throw core::AppException<std::runtime_error>(oss.str());
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Generic error when waiting for WinRT asynchronous action: " << ex.what();
            throw core::AppException<std::runtime_error>(oss.str());
        }
        catch (Platform::Exception ^ex)
        {
            std::ostringstream oss;
            oss << "Generic failure when preparing to wait for Windows Runtime asynchronous action: "
                << core::WWAPI::GetDetailsFromWinRTEx(ex);

            throw core::AppException<std::runtime_error>(oss.str());
        }
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
    void UwpXaml::Notify(Platform::String ^title,
                         Platform::String ^content,
                         Platform::String ^closeButtonText)
    {
        auto dialog = ref new Windows::UI::Xaml::Controls::ContentDialog();
        dialog->Title = title;
        dialog->Content = content;
        dialog->SecondaryButtonText = closeButtonText;
        dialog->ShowAsync();
    }

    /// <summary>
    /// Notifies the user about an exception (using a dialog) and writes the event to the log.
    /// </summary>
    /// <param name="ex">The exception.</param>
    /// <param name="title">The dialog title.</param>
    /// <param name="closeButtonText">The close button text.</param>
    /// <param name="logEntryPrio">The priority for the log entry.</param>
    void UwpXaml::NotifyAndLog(std::exception &ex,
                               Platform::String ^title,
                               Platform::String ^closeButtonText,
                               core::Logger::Priority logEntryPrio)
    {
        std::ostringstream oss;
        oss << "Generic exception: " << ex.what();
        auto utf8content = oss.str();

        std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
        auto wcontent = ref new Platform::String(
            transcoder.from_bytes(utf8content).c_str()
        );

        Notify(title, wcontent, closeButtonText);

        core::Logger::Write(utf8content, logEntryPrio);
    }

    /// <summary>
    /// Notifies the user about an exception (using a dialog) and writes the event to the log.
    /// </summary>
    /// <param name="ex">The exception.</param>
    /// <param name="title">The dialog title.</param>
    /// <param name="closeButtonText">The close button text.</param>
    /// <param name="logEntryPrio">The priority for the log entry.</param>
    void UwpXaml::NotifyAndLog(Platform::Exception ^ex,
                               Platform::String ^title,
                               Platform::String ^closeButtonText,
                               core::Logger::Priority logEntryPrio)
    {
        std::wostringstream woss;
        woss << L"HRESULT error code 0x" << std::hex << ex->HResult << L": ";

        std::array<wchar_t, 256> buffer;
        wcsncpy(buffer.data(), ex->Message->Data(), buffer.size());
        buffer[buffer.size() - 1] = L'\0';

        auto token = wcstok(buffer.data(), L"\r\n");
        while (true)
        {
            woss << token;

            token = wcstok(nullptr, L"\r\n");
            if (token != nullptr)
                woss << L" - ";
            else
                break;
        }

        auto message = woss.str();
        Notify(title, ref new Platform::String(message.c_str()), closeButtonText);

        std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
        core::Logger::Write(transcoder.to_bytes(message), logEntryPrio);
    }

    /// <summary>
    /// Notifies the user about an exception (using a dialog) and writes the event to the log.
    /// </summary>
    /// <param name="ex">The exception.</param>
    /// <param name="title">The dialog title.</param>
    /// <param name="closeButtonText">The close button text.</param>
    /// <param name="logEntryPrio">The priority for the log entry.</param>
    void UwpXaml::NotifyAndLog(core::IAppException &ex,
                               Platform::String ^title,
                               Platform::String ^closeButtonText,
                               core::Logger::Priority logEntryPrio)
    {
        std::ostringstream oss;
        oss << ex.What() << "\n\n" << ex.Details();

        std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;

        auto content = ref new Platform::String(
            transcoder.from_bytes(oss.str()).c_str()
        );

        Notify(title, content, closeButtonText);
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
    bool UwpXaml::CheckActionTask(const concurrency::task<void> &task, const ExNotifAndLogParams &exHndParams)
    {
        try
        {
            task.get();
            return STATUS_OKAY;
        }
        catch (Platform::Exception ^ex)
        {
            NotifyAndLog(ex, exHndParams.title, exHndParams.closeButtonText, exHndParams.logEntryPrio);
        }
        catch (core::IAppException &ex)
        {
            NotifyAndLog(ex, exHndParams.title, exHndParams.closeButtonText, exHndParams.logEntryPrio);
        }
        catch (std::exception &ex)
        {
            NotifyAndLog(ex, exHndParams.title, exHndParams.closeButtonText, exHndParams.logEntryPrio);
        }
        catch (...)
        {
            _ASSERTE(false); // extend implementation to handle more exception types!
        }

        return STATUS_FAIL;
    }

} // end of namespace utils
} // end of namespace _3fd
