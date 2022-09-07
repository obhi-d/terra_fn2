#include <algorithm>
#include <cmath>

#include <Corrade/Utility/Resource.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/ImGuiIntegration/Context.hpp>
#include <Magnum/Math/Matrix4.h>
#include <imgui.h>

#include "IconsFontAwesome6.h"
#include "ImGuiExtra.h"
#include "NoiseToolApp.h"

#include "FastNoise/ImageData.h"
#include "ImageImporter.h"

using namespace Magnum;

void InitResources() {
#ifdef MAGNUM_BUILD_STATIC
    CORRADE_RESOURCE_INITIALIZE( NoiseTool_RESOURCES )
#endif
}

NoiseToolApp::NoiseToolApp( const Arguments& arguments ) :
    Platform::Application { arguments,
                            Configuration {}
                                .setTitle( "FastNoise2 NoiseTool" )
                                .setSize( Vector2i( 1280, 720 ) )
                                .setWindowFlags( Configuration::WindowFlag::Resizable | Configuration::WindowFlag::Maximized ),
                            GLConfiguration {}
                                .setSampleCount( 4 ) },
    mImGuiIntegrationContext { NoCreate },
    mImGuiContext { ImGui::CreateContext() }
{
    InitResources();

    const Vector2 size = Vector2 { windowSize() } / dpiScaling();

    // Add a font that actually looks acceptable on HiDPI screens.
    {
        ImFontConfig fontConfig;
        fontConfig.FontDataOwnedByAtlas    = false;
        const auto           font          = Utility::Resource { "NoiseTool" }.getRaw( "Font.ttf" );
        const auto           iconFont      = Utility::Resource { "NoiseTool" }.getRaw( "fontawesome.otf" );
        static const ImWchar icon_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
        ImGui::GetIO().Fonts->AddFontFromMemoryTTF( const_cast<char*>( font.data() ), (int)font.size(), 14.0f * framebufferSize().x() / size.x(), &fontConfig );
        ImFontConfig config;
        config.FontDataOwnedByAtlas = false;
        config.MergeMode            = true;
        config.GlyphMinAdvanceX     = 13.0f;
        ImGui::GetIO().Fonts->AddFontFromMemoryTTF( const_cast<char*>( iconFont.data() ), (int)iconFont.size(), 14.0f * framebufferSize().x() / size.x(), &config, icon_ranges );
    }

    auto& io                 = ImGui::GetIO();
    io.IniFilename           = "NoiseTool.ini";
    mImGuiIntegrationContext = ImGuiIntegration::Context( *mImGuiContext, size, windowSize(), framebufferSize() );
    std::memset( io.KeyMap, -1, sizeof( io.KeyMap ) );

    GL::Renderer::enable( GL::Renderer::Feature::DepthTest );

    setSwapInterval( 1 );

    mFrameTime.start();

    mCameraObject.setTransformation( Matrix4::translation( { 20, 20, 20 } ) );
    UpdatePespectiveProjection();

    /* Set up proper blending to be used by ImGui. There's a great chance
       you'll need this exact behavior for the rest of your scene. If not, set
       this only for the drawFrame() call. */
    GL::Renderer::setBlendEquation( GL::Renderer::BlendEquation::Add, GL::Renderer::BlendEquation::Add );
    GL::Renderer::setBlendFunction( GL::Renderer::BlendFunction::SourceAlpha, GL::Renderer::BlendFunction::OneMinusSourceAlpha );

    Debug {} << "FastSIMD detected max CPU SIMD Level:" << FastNoiseNodeEditor::GetSIMDLevelName( FastSIMD::CPUMaxSIMDLevel() );

    mLevelNames = { "Auto" };
    mLevelEnums = { FastSIMD::Level_Null };

    for( int i = 1; i > 0; i <<= 1 )
    {
        FastSIMD::eLevel lvl = (FastSIMD::eLevel)i;
        if( lvl & FastNoise::SUPPORTED_SIMD_LEVELS & FastSIMD::COMPILED_SIMD_LEVELS )
        {
            mLevelNames.emplace_back( FastNoiseNodeEditor::GetSIMDLevelName( lvl ) );
            mLevelEnums.emplace_back( lvl );
        }
    }
}

NoiseToolApp::~NoiseToolApp()
{
    // Avoid trying to save settings after node editor is already destroyed
    ImGui::SaveIniSettingsToDisk( ImGui::GetIO().IniFilename );
    ImGui::GetIO().IniFilename = nullptr;
}

void NoiseToolApp::drawEvent()
{
    GL::defaultFramebuffer.clear( GL::FramebufferClear::Color | GL::FramebufferClear::Depth );

    mImGuiIntegrationContext.newFrame();

    /* Enable text input, if needed */
    if( ImGui::GetIO().WantTextInput && !isTextInputActive() )
        startTextInput();
    else if( !ImGui::GetIO().WantTextInput && isTextInputActive() )
        stopTextInput();

    {
        if( ImGui::Button( "Reset State" ) )
        {
            ImGui::ClearIniSettings();
            mNodeEditor.~FastNoiseNodeEditor();
            new( &mNodeEditor ) FastNoiseNodeEditor();
            ImGui::SaveIniSettingsToDisk( ImGui::GetIO().IniFilename );
        }

        if( ImGui::ColorEdit3( "Clear Color", mClearColor.data() ) )
            GL::Renderer::setClearColor( mClearColor );

        ImGui::Checkbox( "Backface Culling", &mBackFaceCulling );

        ImGui::Text( "Application average %.3f ms/frame (%.1f FPS)",
                     1000.0 / Double( ImGui::GetIO().Framerate ), Double( ImGui::GetIO().Framerate ) );

        if( ImGui::Combo( "Max SIMD Level", &mMaxSIMDLevel, mLevelNames.data(), (int)mLevelEnums.size() ) ||
            ImGuiExtra::ScrollCombo( &mMaxSIMDLevel, (int)mLevelEnums.size() ) )
        {
            FastSIMD::eLevel newLevel = mLevelEnums[mMaxSIMDLevel];
            mNodeEditor.SetSIMDLevel( newLevel );
        }
    }

    // Update camera pos
    Vector3 cameraVelocity( 0 );
    if( mKeyDown[Key_W] || mKeyDown[Key_Up] )
    {
        cameraVelocity.z() -= 1.0f;
    }
    if( mKeyDown[Key_S] || mKeyDown[Key_Down] )
    {
        cameraVelocity.z() += 1.0f;
    }
    if( mKeyDown[Key_A] || mKeyDown[Key_Left] )
    {
        cameraVelocity.x() -= 1.0f;
    }
    if( mKeyDown[Key_D] || mKeyDown[Key_Right] )
    {
        cameraVelocity.x() += 1.0f;
    }
    if( mKeyDown[Key_Q] || mKeyDown[Key_PgDn] )
    {
        cameraVelocity.y() -= 1.0f;
    }
    if( mKeyDown[Key_E] || mKeyDown[Key_PgUp] )
    {
        cameraVelocity.y() += 1.0f;
    }
    if( mKeyDown[Key_RShift] || mKeyDown[Key_LShift] )
    {
        cameraVelocity *= 4.0f;
    }

    cameraVelocity *= mFrameTime.previousFrameDuration() * 80.0f;

    if( !cameraVelocity.isZero() )
    {
        Matrix4 transform = mCameraObject.transformation();
        transform.translation() += transform.rotation() * cameraVelocity;
        mCameraObject.setTransformation( transform );
    }

    if( mBackFaceCulling )
    {
        GL::Renderer::enable( GL::Renderer::Feature::FaceCulling );
    }

    mNodeEditor.Draw( mCamera.cameraMatrix(), mCamera.projectionMatrix(), mCameraObject.transformation().translation() );

    /* Set appropriate states. If you only draw ImGui, it is sufficient to
       just enable blending and scissor test in the constructor. */
    GL::Renderer::enable( GL::Renderer::Feature::Blending );
    GL::Renderer::enable( GL::Renderer::Feature::ScissorTest );
    GL::Renderer::disable( GL::Renderer::Feature::DepthTest );
    GL::Renderer::disable( GL::Renderer::Feature::FaceCulling );

    mImGuiIntegrationContext.updateApplicationCursor( *this );
    mImGuiIntegrationContext.drawFrame();

    /* Reset state. Only needed if you want to draw something else with
       different state after. */
    GL::Renderer::enable( GL::Renderer::Feature::DepthTest );
    GL::Renderer::disable( GL::Renderer::Feature::ScissorTest );
    GL::Renderer::disable( GL::Renderer::Feature::Blending );

    swapBuffers();
    redraw();
    mFrameTime.nextFrame();
}

bool NoiseToolApp::handleKeyEvent( KeyEvent::Key key, bool value )
{
    ImGui::SetCurrentContext( mImGuiIntegrationContext.context() );

    ImGuiIO& io = ImGui::GetIO();

    switch( key )
    {
    /* LCOV_EXCL_START */
    case KeyEvent::Key::LeftShift:
    case KeyEvent::Key::RightShift:
        io.KeyShift = value;
        break;
    case KeyEvent::Key::LeftCtrl:
    case KeyEvent::Key::RightCtrl:
        io.KeyCtrl = value;
        break;
    case KeyEvent::Key::LeftAlt:
    case KeyEvent::Key::RightAlt:
        io.KeyAlt = value;
        break;
    case KeyEvent::Key::LeftSuper:
    case KeyEvent::Key::RightSuper:
        io.KeySuper = value;
        break;
    case KeyEvent::Key::Tab:
        ImGui::GetIO().AddKeyEvent( ImGuiKey_Tab, value );
        break;
    case KeyEvent::Key::Up:
        ImGui::GetIO().AddKeyEvent( ImGuiKey_UpArrow, value );
        break;
    case KeyEvent::Key::Down:
        ImGui::GetIO().AddKeyEvent( ImGuiKey_DownArrow, value );
        break;
    case KeyEvent::Key::Left:
        ImGui::GetIO().AddKeyEvent( ImGuiKey_LeftArrow, value );
        break;
    case KeyEvent::Key::Right:
        ImGui::GetIO().AddKeyEvent( ImGuiKey_RightArrow, value );
        break;
    case KeyEvent::Key::Home:
        ImGui::GetIO().AddKeyEvent( ImGuiKey_Home, value );
        break;
    case KeyEvent::Key::End:
        ImGui::GetIO().AddKeyEvent( ImGuiKey_End, value );
        break;
    case KeyEvent::Key::PageUp:
        ImGui::GetIO().AddKeyEvent( ImGuiKey_PageUp, value );
        break;
    case KeyEvent::Key::PageDown:
        ImGui::GetIO().AddKeyEvent( ImGuiKey_PageDown, value );
        break;
    case KeyEvent::Key::Enter:
    case KeyEvent::Key::NumEnter:
        ImGui::GetIO().AddKeyEvent( ImGuiKey_Enter, value );
        break;
    case KeyEvent::Key::Esc:
        ImGui::GetIO().AddKeyEvent( ImGuiKey_Escape, value );
        break;
    case KeyEvent::Key::Space:
        ImGui::GetIO().AddKeyEvent( ImGuiKey_Space, value );
        break;
    case KeyEvent::Key::Backspace:
        ImGui::GetIO().AddKeyEvent( ImGuiKey_Backspace, value );
        break;
    case KeyEvent::Key::Delete:
        ImGui::GetIO().AddKeyEvent( ImGuiKey_Delete, value );
        break;
    case KeyEvent::Key::A:
        ImGui::GetIO().AddKeyEvent( ImGuiKey_A, value );
        break;
    case KeyEvent::Key::C:
        ImGui::GetIO().AddKeyEvent( ImGuiKey_C, value );
        break;
    case KeyEvent::Key::V:
        ImGui::GetIO().AddKeyEvent( ImGuiKey_V, value );
        break;
    case KeyEvent::Key::X:
        ImGui::GetIO().AddKeyEvent( ImGuiKey_X, value );
        break;
    case KeyEvent::Key::Y:
        ImGui::GetIO().AddKeyEvent( ImGuiKey_Y, value );
        break;
    case KeyEvent::Key::Z:
        ImGui::GetIO().AddKeyEvent( ImGuiKey_Z, value );
        break;
    /* LCOV_EXCL_STOP */

    /* Unknown key, do nothing */
    default:
        return false;
    }

    return io.WantCaptureKeyboard;
}

void NoiseToolApp::viewportEvent( ViewportEvent& event )
{
    GL::defaultFramebuffer.setViewport( { {}, event.framebufferSize() } );

    UpdatePespectiveProjection();

    mImGuiIntegrationContext.relayout( Vector2 { event.windowSize() } / event.dpiScaling(), event.windowSize(), event.framebufferSize() );
}

void NoiseToolApp::keyPressEvent( KeyEvent& event )
{
    if( handleKeyEvent( event.key(), true ) )
        return;

    HandleKeyEvent( event.key(), true );
}

void NoiseToolApp::keyReleaseEvent( KeyEvent& event )
{
    if( handleKeyEvent( event.key(), false ) )
        return;

    HandleKeyEvent( event.key(), false );
}

void NoiseToolApp::HandleKeyEvent( KeyEvent::Key key, bool value )
{
    switch( key )
    {
    case KeyEvent::Key::W:
        mKeyDown[Key_W] = value;
        break;
    case KeyEvent::Key::S:
        mKeyDown[Key_S] = value;
        break;
    case KeyEvent::Key::A:
        mKeyDown[Key_A] = value;
        break;
    case KeyEvent::Key::D:
        mKeyDown[Key_D] = value;
        break;
    case KeyEvent::Key::Q:
        mKeyDown[Key_Q] = value;
        break;
    case KeyEvent::Key::E:
        mKeyDown[Key_E] = value;
        break;
    case KeyEvent::Key::Up:
        mKeyDown[Key_Up] = value;
        break;
    case KeyEvent::Key::Down:
        mKeyDown[Key_Down] = value;
        break;
    case KeyEvent::Key::Left:
        mKeyDown[Key_Left] = value;
        break;
    case KeyEvent::Key::Right:
        mKeyDown[Key_Right] = value;
        break;
    case KeyEvent::Key::PageDown:
        mKeyDown[Key_PgDn] = value;
        break;
    case KeyEvent::Key::PageUp:
        mKeyDown[Key_PgUp] = value;
        break;
    case KeyEvent::Key::LeftShift:
        mKeyDown[Key_LShift] = value;
        break;
    case KeyEvent::Key::RightShift:
        mKeyDown[Key_RShift] = value;
        break;
    default:
        break;
    }
}

void NoiseToolApp::mousePressEvent( MouseEvent& event )
{
    if( mImGuiIntegrationContext.handleMousePressEvent( event ) )
        return;
    if( event.button() != MouseEvent::Button::Left )
        return;

    event.setAccepted();
}

void NoiseToolApp::mouseReleaseEvent( MouseEvent& event )
{
    if( mImGuiIntegrationContext.handleMouseReleaseEvent( event ) )
        return;

    event.setAccepted();
}

void NoiseToolApp::mouseScrollEvent( MouseScrollEvent& event )
{
    if( mImGuiIntegrationContext.handleMouseScrollEvent( event ) )
    {
        /* Prevent scrolling the page */
        event.setAccepted();
        return;
    }
}

void NoiseToolApp::mouseMoveEvent( MouseMoveEvent& event )
{
    if( mImGuiIntegrationContext.handleMouseMoveEvent( event ) )
        return;
    if( !( event.buttons() & MouseMoveEvent::Button::Left ) )
        return;

    constexpr float mouseSensitivity = 0.22f;
    Vector2         angleDelta       = Vector2( event.relativePosition() ) * mouseSensitivity;

    if( !angleDelta.isZero() )
    {
        mLookAngle.x() = std::fmod( mLookAngle.x() - angleDelta.x(), 360.0f );
        mLookAngle.y() = std::clamp( mLookAngle.y() - angleDelta.y(), -89.f, 89.f );

        const Vector3 translation = mCameraObject.transformation().translation();
        const Matrix4 rotation    = Matrix4::rotationY( Deg { mLookAngle.x() } ) * Matrix4::rotationX( Deg { mLookAngle.y() } );

        mCameraObject.setTransformation( Matrix4::lookAt( translation, translation - rotation.rotationNormalized() * Vector3::zAxis(), Vector3::yAxis() ) );
    }

    event.setAccepted();
}

void NoiseToolApp::textInputEvent( TextInputEvent& event )
{
    if( mImGuiIntegrationContext.handleTextInputEvent( event ) )
        return;
}

void NoiseToolApp::UpdatePespectiveProjection()
{
    mCamera.setProjectionMatrix( Matrix4::perspectiveProjection( Deg( 70.0f ), Vector2 { windowSize() }.aspectRatio(), 2.0f, 3500.0f ) );
}


MAGNUM_APPLICATION_MAIN( NoiseToolApp )
