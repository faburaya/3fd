//
// MainPage.xaml.h
// Declaration of the MainPage class.
//

#pragma once

#include "3FD\runtime.h"
#include "MainPage.g.h"

namespace ImageTranscoderApp
{
    using namespace Windows;
    using namespace Platform;

    /// <summary>
    /// Represents an item in a list of files.
    /// </summary>
    public ref class FileListItem sealed
    {
    public:

        property Storage::StorageFile ^File;

        property String ^Description
        {
            String ^get() { return File->Path; }
        }

        FileListItem(Storage::StorageFile ^file) { File = file; }
    };

    using Foundation::Collections::IObservableVector;

	/// <summary>
	/// An empty page that can be used on its own or navigated to within a Frame.
	/// </summary>
	public ref class MainPage sealed
	{
    private:

        _3fd::core::FrameworkInstance m_3fdInstance;

        Storage::Pickers::FileOpenPicker ^m_filesPicker;
        
        IObservableVector<FileListItem ^> ^m_inputImages;

	public:

		MainPage();
        
        property IObservableVector<FileListItem ^> ^InputImages
        {
            IObservableVector<FileListItem ^> ^get()
            {
                if (m_inputImages != nullptr)
                    return m_inputImages;
                else
                    return m_inputImages = ref new Platform::Collections::Vector<FileListItem ^>();
            }
        }

        void OnClickSelImagesButton(Object ^sender, Windows::UI::Xaml::RoutedEventArgs ^evArgs);

        void OnClickStartButton(Object ^sender, Windows::UI::Xaml::RoutedEventArgs ^evArgs);
	};
}
