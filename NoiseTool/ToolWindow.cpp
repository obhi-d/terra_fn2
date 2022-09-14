
#include "ToolWindow.h"
#include "IconsFontAwesome6.h"
#include "ImGuiUtils.h"
#include "NoiseToolApp.h"

void Magnum::ToolWindow::create( Platform::Application& app, std::string_view name, Vector2i size, Vector2 dpi )
{
    _dpiScaling = dpi;

    const Vector2i scaledWindowSize = size * _dpiScaling;
    _window                         = SDL_CreateWindow(
                                name.data(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, scaledWindowSize.x(), scaledWindowSize.y(),
                                SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE | SDL_WINDOW_BORDERLESS );

    if( _window )
        _windowID = SDL_GetWindowID( _window );

    _imguiContext = ImGui::CreateContext();
    ImGui::SetCurrentContext( _imguiContext );
    NoiseToolApp::addFont( windowSize(), framebufferSize(), dpiScaling() );

    _imguiIntegration =
        ImGuiIntegration::Context( *_imguiContext, Vector2( scaledWindowSize ), windowSize(), framebufferSize() );
    auto& io = ImGui::GetIO();
    std::memset( io.KeyMap, -1, sizeof( io.KeyMap ) );
    SDL_GetWindowPosition( _window, &_windowLastPos.x(), &_windowLastPos.y() );
}

void Magnum::ToolWindow::beginDraw( Platform::Application& app )
{
    SDL_GL_MakeCurrent( _window, app.glContext() );
    GL::defaultFramebuffer.setViewport( { {}, framebufferSize() } );
    GL::defaultFramebuffer.clear( GL::FramebufferClear::Color );
    _imguiIntegration.newFrame();
    // glfwMakeContextCurrent( _window );
    // GL::defaultFramebuffer.clear( GL::FramebufferClear::Color | GL::FramebufferClear::Depth );
    // _imguiIntegration.newFrame();
}

void Magnum::ToolWindow::endDraw( Platform::Application& app )
{
    _imguiIntegration.drawFrame();
    SDL_GL_SwapWindow( _window );
}

void Magnum::ToolWindow::viewportEvent( ViewportEvent& event )
{
    _dpiScaling = event.dpiScaling();
    _imguiIntegration.relayout( Vector2 { windowSize() } / event.dpiScaling(), windowSize(), framebufferSize() );
}

void Magnum::ToolWindow::handleKeyEvent( KeyEvent::Key key, bool value )
{
    ImGui::SetCurrentContext( _imguiIntegration.context() );
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
        return;
    }
}
void Magnum::ToolWindow::keyPressEvent( KeyEvent& event )
{
    handleKeyEvent( event.key(), true );
}
void Magnum::ToolWindow::keyReleaseEvent( KeyEvent& event )
{
    handleKeyEvent( event.key(), false );
}
void Magnum::ToolWindow::mousePressEvent( MouseEvent& event )
{
    _imguiIntegration.handleMousePressEvent( event );
}
void Magnum::ToolWindow::mouseReleaseEvent( MouseEvent& event )
{
    _imguiIntegration.handleMouseReleaseEvent( event );
}
void Magnum::ToolWindow::mouseMoveEvent( MouseMoveEvent& event )
{
    _imguiIntegration.handleMouseMoveEvent( event );
    ImGuiIO& io = ImGui::GetIO();

    int    x, y;
    Uint32 button = SDL_GetGlobalMouseState( &x, &y );

    if( ( ( (MouseMoveEvent::Button)button ) & MouseMoveEvent::Button::Left ) )
        io.AddMouseButtonEvent( 0, true );
    else
        io.AddMouseButtonEvent( 0, false );
}
void Magnum::ToolWindow::mouseScrollEvent( MouseScrollEvent& event )
{
    _imguiIntegration.handleMouseScrollEvent( event );
}
void Magnum::ToolWindow::textInputEvent( TextInputEvent& event )
{
    _imguiIntegration.handleTextInputEvent( event );
}

void Magnum::ToolWindow::drawWindowControls( float xSize )
{
    auto higlightColor = ImGui::GetColorU32( ImVec4( 0.6f, 0.2f, 0.2f, 1.0f ) );

    ImGui::SameLine();
    if( IconButton( ICON_FA_CIRCLE_XMARK, xSize - 30, ImVec2( 20, 20 ), higlightColor ) )
        hide();
}
