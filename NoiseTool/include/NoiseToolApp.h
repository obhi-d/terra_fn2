#pragma once

#include "FastNoiseNodeEditor.h"
#include "ToolWindow.h"

namespace Magnum
{
    class NoiseToolApp : public Platform::Application
    {
    public:
        explicit NoiseToolApp( const Arguments& arguments );
        ~NoiseToolApp();

        static void addFont( Vector2i windowSize, Vector2i framebufferSize, Vector2 dpiScaling );

        ImGuiContext* getImGuiContext()
        {
            return mImGuiContextMain;
        }

    private:
        void recomputeCamera();
        void quit();
        void createEditorWindows();
        void drawEvent() override;
        void viewportEvent( ViewportEvent& event ) override;

        bool handleKeyEvent( KeyEvent::Key, bool );

        void keyPressEvent( KeyEvent& event ) override;
        void keyReleaseEvent( KeyEvent& event ) override;
        void mousePressEvent( MouseEvent& event ) override;
        void mouseReleaseEvent( MouseEvent& event ) override;
        void mouseMoveEvent( MouseMoveEvent& event ) override;
        void mouseScrollEvent( MouseScrollEvent& event ) override;
        void textInputEvent( TextInputEvent& event ) override;
        void anyEvent( SDL_Event& ) override;

        void UpdatePespectiveProjection();
        void HandleKeyEvent( KeyEvent::Key key, bool value );

        SceneGraph::Object<SceneGraph::MatrixTransformation3D> mCameraObject;
        SceneGraph::Camera3D                                   mCamera { mCameraObject };
        Vector2                                                mLookAngle { 0 };
        Timeline                                               mFrameTime;

        Color3                        mClearColor { 0.122f };
        int                           mMaxSIMDLevel = 0;
        std::vector<const char*>      mLevelNames;
        std::vector<FastSIMD::eLevel> mLevelEnums;

        ImGuiIntegration::Context mImGuiIntegrationContext { NoCreate };
        ImGuiContext*             mImGuiContextMain;
        FastNoiseNodeEditor       mNodeEditor;
        ToolWindow                mNodes;

        enum Key
        {
            Key_W,
            Key_A,
            Key_S,
            Key_D,
            Key_Q,
            Key_E,
            Key_Left,
            Key_Right,
            Key_Up,
            Key_Down,
            Key_PgUp,
            Key_PgDn,
            Key_LShift,
            Key_RShift,
            Key_Ctrl,
            Key_V,
            Key_C,
            Key_Count
        };

        float mCamDefaultLookAtAngle = 45.0f;

        float mFOV       = 70.0f;
        float mFarPlane  = 5000.0f;
        float mNearPlane = 1.0f;

        int                         mWindowID           = 0;
        std::array<bool, Key_Count> mKeyDown            = {};
        bool                        mBackFaceCulling    = false;
        bool                        mSkipDraw           = false;
        bool                        mExternalNodeEditor = true;
        bool                        mShowTexturePreview = true;
        bool                        mShowFPS            = false;
    };
} // namespace Magnum
