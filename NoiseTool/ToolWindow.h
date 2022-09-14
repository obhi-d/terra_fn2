
#pragma once
#include <array>
#include <string_view>

#include <Magnum/ImGuiIntegration/Context.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Platform/Sdl2Application.h>
#include <Magnum/SceneGraph/Camera.h>
#include <Magnum/SceneGraph/MatrixTransformation3D.h>
#include <Magnum/SceneGraph/Object.h>

#include <Corrade/Utility/Resource.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/ImGuiIntegration/Context.hpp>
#include <Magnum/Math/Matrix4.h>

namespace Magnum
{
    class ToolWindow
    {
    public:
        using KeyEvent         = Platform::Application::KeyEvent;
        using InputEvent       = Platform::Application::InputEvent;
        using MouseEvent       = Platform::Application::MouseEvent;
        using MouseMoveEvent   = Platform::Application::MouseMoveEvent;
        using MouseScrollEvent = Platform::Application::MouseScrollEvent;
        using TextInputEvent   = Platform::Application::TextInputEvent;
        using ViewportEvent    = Platform::Application::ViewportEvent;

        void relayout()
        {
            _imguiIntegration.relayout( windowSize() );
        }

        void create( Platform::Application& app, std::string_view name, Vector2i size, Vector2 dpi );

        void beginDraw( Platform::Application& );
        void endDraw( Platform::Application& );

        Vector2i framebufferSize() const
        {
            Vector2i size;
            SDL_GL_GetDrawableSize( _window, &size.x(), &size.y() );
            return size;
        }

        Vector2i windowSize() const
        {
            Vector2i size;
            SDL_GetWindowSize( _window, &size.x(), &size.y() );
            return size;
        }

        Vector2 dpiScaling() const
        {
            return _dpiScaling;
        }

        auto id() const
        {
            return _windowID;
        }

        void setSize( int x, int y )
        {
            if( x < 256 )
                return;
            if( y < 256 )
                return;
            SDL_SetWindowSize( _window, x, y );
        }

        void moveWindow( int x, int y )
        {
            if( isMaximized )
                restore();
            _windowLastPos.x() += x;
            _windowLastPos.y() += y;

            SDL_SetWindowPosition( _window, _windowLastPos.x(), _windowLastPos.y() );
        }

        virtual void viewportEvent( ViewportEvent& event );
        virtual void keyPressEvent( KeyEvent& event );
        virtual void keyReleaseEvent( KeyEvent& event );
        virtual void mousePressEvent( MouseEvent& event );
        virtual void mouseReleaseEvent( MouseEvent& event );
        virtual void mouseMoveEvent( MouseMoveEvent& event );
        virtual void mouseScrollEvent( MouseScrollEvent& event );
        virtual void textInputEvent( TextInputEvent& event );

        virtual void handleKeyEvent( KeyEvent::Key key, bool value );

        void drawWindowControls( float xSize );

        void maximize()
        {
            SDL_MaximizeWindow( _window );
            isMaximized = true;
        }

        void restore()
        {
            SDL_RestoreWindow( _window );
            isMaximized = false;
        }

        void hide()
        {
            SDL_HideWindow( _window );
        }

        void show()
        {
            SDL_ShowWindow( _window );
        }

        void destroy()
        {
            if( _window )
            {
                _imguiIntegration.release();
                ImGui::DestroyContext( _imguiContext );
                SDL_DestroyWindow( _window );
                _window = nullptr;
            }
        }
        ~ToolWindow()
        {
            destroy();
        }

    private:
        bool     isMaximized = false;
        Vector2i _windowLastPos;
        // void                      onResize( int w, int h );
        SDL_Window*               _window = nullptr;
        int                       _windowID;
        ImGuiIntegration::Context _imguiIntegration { NoCreate };
        ImGuiContext*             _imguiContext;
        Vector2                   _dpiScaling;
    };
} // namespace Magnum