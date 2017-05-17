//
// MainPage.xaml.cpp
// Implementation of the MainPage class.
//

#include "pch.h"
#include "MainPage.xaml.h"
#include "3FD\callstacktracer.h"
#include "3FD\utils_winrt.h"
#include <functional>

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

// default parameters for exception notification and logging
static const utils::UwpXaml::ExNotifAndLogParams exHndParams
{
    Platform::StringReference(L"Application error!"),
    Platform::StringReference(L"Punch me in the face"),
    core::Logger::PRIO_ERROR
};

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

    using concurrency::task;
    using namespace Windows::Storage;
    using namespace Windows::Storage::FileProperties;

    // Launch file picker for selection of images:
    concurrency::create_task(
        m_filesPicker->PickMultipleFilesAsync()
    ).then([this](task<IVectorView<StorageFile ^> ^> priorTask)
    {
        CALL_STACK_TRACE;

        auto selectedFiles = utils::UwpXaml::GetTaskRetAndHndEx(priorTask, exHndParams);

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
            ).then([this, file](task<StorageItemThumbnail ^> priorTask)
            {
                CALL_STACK_TRACE;

                auto thumbnail = utils::UwpXaml::GetTaskRetAndHndEx(priorTask, exHndParams);

                auto newListItem = ref new FileListItem(file);
                newListItem->Thumbnail = ref new Imaging::BitmapImage();

                // Load thumbnail:
                concurrency::create_task(
                    newListItem->Thumbnail->SetSourceAsync(thumbnail)
                ).then([this, newListItem](task<void> priorTask)
                {
                    CALL_STACK_TRACE;

                    utils::UwpXaml::CheckActionTask(priorTask, exHndParams);

                    // add new list item to observable vector, so the UI updates to show it
                    this->InputImages->Append(newListItem);
                });
            });
        }
    });
}

// Starts transcoding selected images
void MainPage::OnClickStartButton(Object ^sender, RoutedEventArgs ^evArgs)
{

}