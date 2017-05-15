//
// MainPage.xaml.cpp
// Implementation of the MainPage class.
//

#include "pch.h"
#include "MainPage.xaml.h"
#include "3FD\callstacktracer.h"
#include "3FD\utils_winrt.h"

using namespace _3fd;
using namespace _3fd::core;
using namespace ImageTranscoderApp;

using namespace Platform;
using namespace Platform::Collections;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Navigation;

// The Blank Page item template is documented at http://go.microsoft.com/fwlink/?LinkId=402352&clcid=0x409

#define DEFAULT_EXCEPTION_HANDLER(EX) utils::UwpXaml::NotifyAndLog((EX), StringReference(L"Application error!"), StringReference(L"Puch me in the face"));

MainPage::MainPage()
{
	InitializeComponent();

    using namespace Windows::Storage::Pickers;

    m_filesPicker = ref new FileOpenPicker();
    m_filesPicker->CommitButtonText = StringReference(L"Add to list");
    m_filesPicker->SuggestedStartLocation = PickerLocationId::ComputerFolder;
    m_filesPicker->ViewMode = PickerViewMode::Thumbnail;

    m_filesPicker->FileTypeFilter->Append(StringReference(L".tiff"));
    m_filesPicker->FileTypeFilter->Append(StringReference(L".jpeg"));
    m_filesPicker->FileTypeFilter->Append(StringReference(L".jpg"));
    m_filesPicker->FileTypeFilter->Append(StringReference(L".jxr"));
    m_filesPicker->FileTypeFilter->Append(StringReference(L".png"));
    m_filesPicker->FileTypeFilter->Append(StringReference(L".bmp"));

    // default quality
    qualitySlider->Value = 90;
}

// Shows the files picker
void MainPage::OnClickSelImagesButton(Object ^sender, RoutedEventArgs ^evArgs)
{
    CALL_STACK_TRACE;

    try
    {
        using namespace Windows::Storage;

        concurrency::create_task(m_filesPicker->PickMultipleFilesAsync()).then([this](IVectorView<StorageFile ^> ^selectedImages)
        {
            CALL_STACK_TRACE;

            try
            {
                for (auto file : selectedImages)
                    InputImages->Append(ref new FileListItem(file));
            }
            catch (Exception ^ex)
            {
                DEFAULT_EXCEPTION_HANDLER(ex);
            }
        });
    }
    catch (Exception ^ex)
    {
        DEFAULT_EXCEPTION_HANDLER(ex);
    }
    catch (std::exception &ex)
    {
        DEFAULT_EXCEPTION_HANDLER(ex);
    }
}

// Starts transcoding selected images
void MainPage::OnClickStartButton(Object ^sender, RoutedEventArgs ^evArgs)
{

}