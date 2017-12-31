//
// MainPage.xaml.cpp
// Implementation of the MainPage class.
//

#include "pch.h"
#include "MainPage.xaml.h"
#include "utils_winrt.h"
#include <sstream>
#include <codecvt>
#include <cstdio>

using namespace UnitTestsApp_WinRT_UWP;

using namespace Platform;
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

MainPage::MainPage()
{
    InitializeComponent();
}

void MainPage::OnClickRunButton(Object ^sender, RoutedEventArgs ^evArgs)
{
    mainTextBlock->Text = L"";
    runButton->IsEnabled = false;
    waitingRing->Visibility = Windows::UI::Xaml::Visibility::Visible;
    waitingRing->IsActive = true;

    using namespace Windows::Storage;

    auto asyncOpGetStdOut =
        ApplicationData::Current->LocalFolder->GetFileAsync(L"test-report.txt");

    // Run the tests using the gtest port:
    concurrency::create_task([asyncOpGetStdOut]()
    {
        RUN_ALL_TESTS();
        fclose(stdout);
        auto stdOutFile = _3fd::utils::WinRTExt::WaitForAsync(asyncOpGetStdOut);
        return FileIO::ReadTextAsync(stdOutFile);
    })
    // Print the tests results in the app main page:
    .then([this](concurrency::task<String ^> &asyncOpReadFile)
    {
        try
        {
            mainTextBlock->Text = asyncOpReadFile.get();
        }
        catch (_3fd::core::IAppException &ex)
        {
            std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;

            mainTextBlock->Text =
                ref new String(
                    transcoder.from_bytes(ex.ToString()).c_str()
                );
        }
        catch (std::exception &ex)
        {
            std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
            std::wostringstream woss;
            woss << L"Standard exception: " << transcoder.from_bytes(ex.what());

            mainTextBlock->Text = ref new String(woss.str().c_str());
        }
        catch (Platform::Exception ^ex)
        {
            std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
            std::wostringstream woss;
            woss << L"Windows Runtime exception: "
                << transcoder.from_bytes(_3fd::core::WWAPI::GetDetailsFromWinRTEx(ex));

            mainTextBlock->Text = ref new String(woss.str().c_str());
        }

        waitingRing->IsActive = false;
        waitingRing->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
    });
}
