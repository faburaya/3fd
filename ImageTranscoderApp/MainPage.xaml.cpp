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
    using namespace Windows::Storage;
    using namespace Windows::Storage::FileProperties;

    concurrency::create_task(
        m_filesPicker->PickMultipleFilesAsync()
    ).then([this](IVectorView<StorageFile ^> ^selectedFiles)
    {
        for (auto file : selectedFiles)
        {
            concurrency::create_task(
                file->GetThumbnailAsync(ThumbnailMode::ListView)
            ).then([this, file](StorageItemThumbnail ^thumbnail)
            {
                auto newListItem = ref new FileListItem(file);
                newListItem->Thumbnail = ref new Imaging::BitmapImage();
                newListItem->Thumbnail->SetSource(thumbnail);
                this->InputImages->Append(newListItem);
            }, concurrency::task_continuation_context::use_current());
        }
    }, concurrency::task_continuation_context::use_current());
}

// Starts transcoding selected images
void MainPage::OnClickStartButton(Object ^sender, RoutedEventArgs ^evArgs)
{

}