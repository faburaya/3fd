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

using namespace UnitTestsApp_WinRT_Windows;

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

// The Blank Page item template is documented at http://go.microsoft.com/fwlink/?LinkId=234238

MainPage::MainPage()
{
	InitializeComponent();
}

void MainPage::OnClickRunButton(Object ^sender, RoutedEventArgs ^evArgs)
{
	mainTextBlock->Text = "";
	runButton->IsEnabled = false;
	waitingRing->Visibility = Windows::UI::Xaml::Visibility::Visible;
	waitingRing->IsActive = true;
	
	using namespace Windows::Storage;

	auto asyncOpCreateStdOut =
		ApplicationData::Current->LocalFolder
			->CreateFileAsync(L"test-report.txt", CreationCollisionOption::OpenIfExists);

	auto asyncOpCreateStdErr =
		ApplicationData::Current->LocalFolder
			->CreateFileAsync(L"gtest-errors.txt", CreationCollisionOption::OpenIfExists);

	concurrency::create_task([asyncOpCreateStdOut, asyncOpCreateStdErr]()
	{
		auto stdOutFile = _3fd::utils::WinRTExt::WaitForAsync(asyncOpCreateStdOut);

		auto stdErrFile = _3fd::utils::WinRTExt::WaitForAsync(asyncOpCreateStdErr);

		// Replace standard output & error for TXT files:
		_wfreopen(stdOutFile->Path->Data(), L"w", stdout);
		_wfreopen(stdErrFile->Path->Data(), L"w", stderr);

		auto appExePath =
			Windows::ApplicationModel::Package::Current->InstalledLocation->Path
			+ StringReference(L"\\")
			+ Windows::ApplicationModel::Package::Current->DisplayName
			+ StringReference(L".exe");

		int argc(1);
		wchar_t *argv[] =
		{
			const_cast<wchar_t *> (appExePath->Data()),
			nullptr
		};

		testing::InitGoogleTest(&argc, argv);
		RUN_ALL_TESTS();

		fclose(stdout);
		fflush(stderr);

		return FileIO::ReadTextAsync(stdOutFile);
	})
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
					transcoder.from_bytes(ex.ToPrettyString()).c_str()
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
		runButton->IsEnabled = true;
	});
}
