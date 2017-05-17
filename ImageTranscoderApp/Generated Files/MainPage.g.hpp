﻿//------------------------------------------------------------------------------
//     This code was generated by a tool.
//
//     Changes to this file may cause incorrect behavior and will be lost if
//     the code is regenerated.
//------------------------------------------------------------------------------
#include "pch.h"

#if defined _DEBUG && !defined DISABLE_XAML_GENERATED_BINDING_DEBUG_OUTPUT
extern "C" __declspec(dllimport) int __stdcall IsDebuggerPresent();
#endif

#include "MainPage.xaml.h"

void ::ImageTranscoderApp::MainPage::InitializeComponent()
{
    if (_contentLoaded)
    {
        return;
    }
    _contentLoaded = true;
    ::Windows::Foundation::Uri^ resourceLocator = ref new ::Windows::Foundation::Uri(L"ms-appx:///MainPage.xaml");
    ::Windows::UI::Xaml::Application::LoadComponent(this, resourceLocator, ::Windows::UI::Xaml::Controls::Primitives::ComponentResourceLocation::Application);
}


/// <summary>
/// Auto generated class for compiled bindings.
/// </summary>
class ImageTranscoderApp::MainPage::MainPage_obj13_Bindings 
    : public ::XamlBindingInfo::XamlBindingsBase<::ImageTranscoderApp::FileListItem>
{
public:
    MainPage_obj13_Bindings()
    {
    }

    void Connect(int __connectionId, ::Platform::Object^ __target)
    {
        switch(__connectionId)
        {
            case 14:
                this->obj14 = safe_cast<::Windows::UI::Xaml::Controls::Image^>(__target);
                break;
            case 15:
                this->obj15 = safe_cast<::Windows::UI::Xaml::Controls::TextBlock^>(__target);
                break;
            case 16:
                this->obj16 = safe_cast<::Windows::UI::Xaml::Controls::TextBlock^>(__target);
                break;
        }
    }

    void ResetTemplate()
    {
    }

    int ProcessBindings(::Windows::UI::Xaml::Controls::ContainerContentChangingEventArgs^ args)
    {
        int nextPhase = -1;
        switch(args->Phase)
        {
            case 0:
                nextPhase = -1;
                this->SetDataRoot(static_cast<::ImageTranscoderApp::FileListItem^>(args->Item));
                if (this->_dataContextChangedToken.Value != 0)
                {
                    safe_cast<::Windows::UI::Xaml::FrameworkElement^>(args->ItemContainer->ContentTemplateRoot)->DataContextChanged -= this->_dataContextChangedToken;
                    this->_dataContextChangedToken.Value = 0;
                }
                this->_isInitialized = true;
                break;
        }
        this->Update_((::ImageTranscoderApp::FileListItem^) args->Item, (1 << args->Phase));
        return nextPhase;
    }
private:
    // Fields for each control that has bindings.
    ::Windows::UI::Xaml::Controls::Image^ obj14;
    ::Windows::UI::Xaml::Controls::TextBlock^ obj15;
    ::Windows::UI::Xaml::Controls::TextBlock^ obj16;

    // Update methods for each path node used in binding steps.
    void Update_(::ImageTranscoderApp::FileListItem^ obj, int phase)
    {
        if (obj != nullptr)
        {
            if ((phase & (NOT_PHASED | (1 << 0))) != 0)
            {
                this->Update_Thumbnail(obj->Thumbnail, phase);
                this->Update_FileExtension(obj->FileExtension, phase);
                this->Update_Description(obj->Description, phase);
            }
        }
    }
    void Update_Thumbnail(::Windows::UI::Xaml::Media::Imaging::BitmapImage^ obj, int phase)
    {
        if((phase & ((1 << 0) | NOT_PHASED )) != 0)
        {
            ::XamlBindingInfo::XamlBindingSetters::Set_Windows_UI_Xaml_Controls_Image_Source(this->obj14, obj, nullptr);
        }
    }
    void Update_FileExtension(::Platform::String^ obj, int phase)
    {
        if((phase & ((1 << 0) | NOT_PHASED )) != 0)
        {
            ::XamlBindingInfo::XamlBindingSetters::Set_Windows_UI_Xaml_Controls_TextBlock_Text(this->obj15, obj, nullptr);
        }
    }
    void Update_Description(::Platform::String^ obj, int phase)
    {
        if((phase & ((1 << 0) | NOT_PHASED )) != 0)
        {
            ::XamlBindingInfo::XamlBindingSetters::Set_Windows_UI_Xaml_Controls_TextBlock_Text(this->obj16, obj, nullptr);
        }
    }
};

/// <summary>
/// Auto generated class for compiled bindings.
/// </summary>
class ImageTranscoderApp::MainPage::MainPage_obj1_Bindings 
    : public ::XamlBindingInfo::XamlBindingsBase<::ImageTranscoderApp::MainPage>
{
public:
    MainPage_obj1_Bindings()
    {
    }

    void Connect(int __connectionId, ::Platform::Object^ __target)
    {
        switch(__connectionId)
        {
            case 12:
                this->obj12 = safe_cast<::Windows::UI::Xaml::Controls::ListView^>(__target);
                break;
        }
    }
private:
    // Fields for each control that has bindings.
    ::Windows::UI::Xaml::Controls::ListView^ obj12;

    // Update methods for each path node used in binding steps.
    void Update_(::ImageTranscoderApp::MainPage^ obj, int phase)
    {
        if (obj != nullptr)
        {
            if ((phase & (NOT_PHASED | (1 << 0))) != 0)
            {
                this->Update_InputImages(obj->InputImages, phase);
            }
        }
    }
    void Update_InputImages(::Windows::Foundation::Collections::IObservableVector<::ImageTranscoderApp::FileListItem^>^ obj, int phase)
    {
        if((phase & ((1 << 0) | NOT_PHASED )) != 0)
        {
            ::XamlBindingInfo::XamlBindingSetters::Set_Windows_UI_Xaml_Controls_ItemsControl_ItemsSource(this->obj12, obj, nullptr);
        }
    }
};

void ::ImageTranscoderApp::MainPage::Connect(int __connectionId, ::Platform::Object^ __target)
{
    switch (__connectionId)
    {
        case 2:
            {
                this->wideState = safe_cast<::Windows::UI::Xaml::VisualState^>(__target);
            }
            break;
        case 3:
            {
                this->narrowState = safe_cast<::Windows::UI::Xaml::VisualState^>(__target);
            }
            break;
        case 4:
            {
                this->LayoutRoot = safe_cast<::Windows::UI::Xaml::Controls::Grid^>(__target);
            }
            break;
        case 5:
            {
                this->titleTxtBlock = safe_cast<::Windows::UI::Xaml::Controls::TextBlock^>(__target);
            }
            break;
        case 6:
            {
                this->ContentRoot = safe_cast<::Windows::UI::Xaml::Controls::Grid^>(__target);
            }
            break;
        case 7:
            {
                this->CommandPanel = safe_cast<::Windows::UI::Xaml::Controls::StackPanel^>(__target);
            }
            break;
        case 8:
            {
                this->selImagesButton = safe_cast<::Windows::UI::Xaml::Controls::Button^>(__target);
                (safe_cast<::Windows::UI::Xaml::Controls::Button^>(this->selImagesButton))->Click += ref new ::Windows::UI::Xaml::RoutedEventHandler(this, (void (::ImageTranscoderApp::MainPage::*)
                    (::Platform::Object^, ::Windows::UI::Xaml::RoutedEventArgs^))&MainPage::OnClickSelImagesButton);
            }
            break;
        case 9:
            {
                this->startButton = safe_cast<::Windows::UI::Xaml::Controls::Button^>(__target);
                (safe_cast<::Windows::UI::Xaml::Controls::Button^>(this->startButton))->Click += ref new ::Windows::UI::Xaml::RoutedEventHandler(this, (void (::ImageTranscoderApp::MainPage::*)
                    (::Platform::Object^, ::Windows::UI::Xaml::RoutedEventArgs^))&MainPage::OnClickStartButton);
            }
            break;
        case 10:
            {
                this->qualitySlider = safe_cast<::Windows::UI::Xaml::Controls::Slider^>(__target);
            }
            break;
        case 11:
            {
                this->waitingRing = safe_cast<::Windows::UI::Xaml::Controls::ProgressRing^>(__target);
            }
            break;
        case 12:
            {
                this->filesListView = safe_cast<::Windows::UI::Xaml::Controls::ListView^>(__target);
            }
            break;
    }
    _contentLoaded = true;
}

::Windows::UI::Xaml::Markup::IComponentConnector^ ::ImageTranscoderApp::MainPage::GetBindingConnector(int __connectionId, ::Platform::Object^ __target)
{
    ::XamlBindingInfo::XamlBindings^ bindings = nullptr;
    switch (__connectionId)
    {
        case 1:
            {
                ::Windows::UI::Xaml::Controls::Page^ element1 = safe_cast<::Windows::UI::Xaml::Controls::Page^>(__target);
                MainPage_obj1_Bindings* objBindings = new MainPage_obj1_Bindings();
                objBindings->SetDataRoot(this);
                bindings = ref new ::XamlBindingInfo::XamlBindings(objBindings);
                this->Bindings = bindings;
                element1->Loading += ref new ::Windows::Foundation::TypedEventHandler<::Windows::UI::Xaml::FrameworkElement^, ::Platform::Object^>(bindings, &::XamlBindingInfo::XamlBindings::Loading);
            }
            break;
        case 13:
            {
                ::Windows::UI::Xaml::Controls::StackPanel^ element13 = safe_cast<::Windows::UI::Xaml::Controls::StackPanel^>(__target);
                MainPage_obj13_Bindings* objBindings = new MainPage_obj13_Bindings();
                objBindings->SetDataRoot(element13->DataContext);
                bindings = ref new ::XamlBindingInfo::XamlBindings(objBindings);
                bindings->SubscribeForDataContextChanged(element13);
                ::Windows::UI::Xaml::DataTemplate::SetExtensionInstance(element13, bindings);
            }
            break;
    }
    return bindings;
}


