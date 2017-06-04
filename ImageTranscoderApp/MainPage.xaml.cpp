//
// MainPage.xaml.cpp
// Implementation of the MainPage class.
//

#include "pch.h"
#include "MainPage.xaml.h"
#include "3FD\callstacktracer.h"
#include "3FD\utils_winrt.h"
#include <functional>

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

using namespace ImageTranscoderApp;

/// <summary>
/// Constructor for the application main page.
/// </summary>
MainPage::MainPage()
{
	InitializeComponent();

    using namespace Windows::Storage::Pickers;

    m_filesPicker = ref new FileOpenPicker();
    m_filesPicker->CommitButtonText = StringReference(L"Add to list");
    m_filesPicker->SuggestedStartLocation = PickerLocationId::PicturesLibrary;
    m_filesPicker->ViewMode = PickerViewMode::Thumbnail;

    // supported image formats:
    m_filesPicker->FileTypeFilter->Append(StringReference(L".tiff"));
    m_filesPicker->FileTypeFilter->Append(StringReference(L".jpeg"));
    m_filesPicker->FileTypeFilter->Append(StringReference(L".jpg"));
    m_filesPicker->FileTypeFilter->Append(StringReference(L".jxr"));
    m_filesPicker->FileTypeFilter->Append(StringReference(L".png"));
    m_filesPicker->FileTypeFilter->Append(StringReference(L".bmp"));

    // default quality
    qualitySlider->Value = 90;
}


/// <summary>
/// Called button to add/select images is clicked,
/// then shows a file picker.
/// </summary>
/// <param name="sender">The event origin.</param>
/// <param name="evArgs">The event arguments.</param>
void MainPage::OnClickSelectImagesButton(Object ^sender, RoutedEventArgs ^evArgs)
{
    using concurrency::task;
    using namespace Windows::Storage;
    using namespace Windows::Storage::FileProperties;
    
    // Launch file picker for selection of images:
    concurrency::create_task(
        m_filesPicker->PickMultipleFilesAsync()
    ).then([this](IVectorView<StorageFile ^> ^selectedFiles)
    {
        static const std::hash<std::wstring> hashText;

        for (auto file : selectedFiles)
        {
            // Skip an already selected file:
            auto filePathHash = hashText(file->Path->Data());
            if (!m_setOfSelFiles.insert(filePathHash).second)
                continue;

            // Get the thumbnail:
            concurrency::create_task(
                file->GetThumbnailAsync(ThumbnailMode::ListView, 64)
            ).then([this, file](StorageItemThumbnail ^thumbnail)
            {
                auto newListItem = ref new FileListItem(file);
                newListItem->Thumbnail = ref new Imaging::BitmapImage();

                // Load thumbnail:
                concurrency::create_task(
                    newListItem->Thumbnail->SetSourceAsync(thumbnail)
                ).then([this, newListItem]()
                {
                    // add new list item to observable vector, so the UI updates to show it
                    InputImages->Append(newListItem);
                });
            });

        }// for loop end
    });
}


/// <summary>
/// Called button to clear selection of images is clicked.
/// </summary>
/// <param name="sender">The event origin.</param>
/// <param name="evArgs">The event arguments.</param>
void MainPage::OnClickClearSelImagesButton(Object ^sender, RoutedEventArgs ^evArgs)
{
    startButton->IsEnabled = false;
    InputImages->Clear(); // clears the list view
    m_setOfSelFiles.clear();
}


using namespace _3fd;
using namespace _3fd::core;


// default parameters for exception notification and logging
static const utils::UwpXaml::ExNotifAndLogParams exHndParams
{
    Platform::StringReference(L"Application error!\n"),
    Platform::StringReference(L"Punch me in the face"),
    core::Logger::PRIO_ERROR
};


/// <summary>
/// Called button to start transcoding is clicked.
/// </summary>
/// <param name="sender">The event origin.</param>
/// <param name="evArgs">The event arguments.</param>
void MainPage::OnClickStartButton(Object ^sender, RoutedEventArgs ^evArgs)
{
    if (InputImages->Size == 0)
        return;

    FrameworkInstance _3fd("ImageTranscoderApp");

    CALL_STACK_TRACE;

    startButton->IsEnabled = false;
    qualitySlider->IsEnabled = false;
    toJxrCheckBox->IsEnabled = false;

    waitingRing->IsActive = true;
    waitingRing->Visibility = Windows::UI::Xaml::Visibility::Visible;

    /* Changes to the view model should happen only here in the STA, so while 
    transcoding takes place, keep track of the files that remain to process (just
    in case the operation fails transcoding one file, leaving others for another
    attempt), and only remove the succesfully processed files from the list view
    when the callback return to STA. */
    
    auto itemsToProcess =
        ref new Platform::Collections::Vector<FileListItem ^>(InputImages->Size);
    
    // make a copy of the items to process:
    for (auto idx = static_cast<int> (InputImages->Size) - 1; idx >= 0; --idx)
        itemsToProcess->SetAt(idx, InputImages->GetAt(idx));

    bool toJXR = toJxrCheckBox->IsChecked->Value;
    auto quality = static_cast<float> (qualitySlider->Value / 100);

    // In order to prevent running out of threads in the pool, process everything in a single callback:
    concurrency::create_task(
        concurrency::create_async([=]()
        {
            CALL_STACK_TRACE;

            auto jpegTranscoder = ref new MyImagingComsWinRT::JpegTranscoder();

            while (itemsToProcess->Size > 0)
            {
                auto item = itemsToProcess->GetAt(itemsToProcess->Size - 1);
                jpegTranscoder->TranscodeSync(item->File, toJXR, quality);
                itemsToProcess->RemoveAtEnd();
            }
        })
    ).then([=](concurrency::task<void> priorTask)
    {
        utils::UwpXaml::CheckActionTask(priorTask, exHndParams);

        InputImages->Clear(); // remove all images from the list view

        // re-add the images that could not be processed:
        for (auto idx = static_cast<int> (itemsToProcess->Size) - 1; idx >= 0; --idx)
        {
            auto item = itemsToProcess->GetAt(idx);
            InputImages->Append(item);
        }

        itemsToProcess->Clear();

        waitingRing->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
        waitingRing->IsActive = false;

        toJxrCheckBox->IsEnabled = true;
        qualitySlider->IsEnabled = true;
        startButton->IsEnabled = true;
    });
}