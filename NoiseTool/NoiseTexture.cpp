#include <cstdio>
#include <filesystem>
#include <fstream>

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>

#include <Corrade/Containers/ArrayViewStl.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/ImGuiIntegration/Widgets.h>
#include <Magnum/Math/Functions.h>
#include <Magnum/PixelFormat.h>

#include <FastNoise/Metadata.h>

#include "ImGuiExtra.h"
#include "NoiseTexture.h"


using namespace Magnum;

NoiseTexture::NoiseTexture()
{
    mBuildData.iteration = 0;
    mBuildData.frequency = 0.02f;
    mBuildData.seed = 1337;
    mBuildData.size = { -1, -1 };
    mBuildData.offset = {};
    mBuildData.generationType = GenType_2D;

    mExportBuildData.size = { 4096, 4096 };

    for( size_t i = 0; i < 2; i++ )
    {
        mThreads.emplace_back( GenerateLoopThread, std::ref( mGenerateQueue ), std::ref( mCompleteQueue ) );
    }

    SetupSettingsHandlers();
}

NoiseTexture::~NoiseTexture()
{
    for( auto& thread: mThreads )
    {
        mGenerateQueue.KillThreads();
        thread.join();
    }

    if( mExportThread.joinable() )
    {
        mExportThread.join();
    }
}

void NoiseTexture::Draw()
{
    TextureData texData;
    if( mCompleteQueue.Pop( texData ) )
    {
        if( mCurrentIteration < texData.iteration )
        {
            mCurrentIteration = texData.iteration;
            ImageView2D noiseImage( PixelFormat::RGBA8Srgb, texData.size, texData.textureData );
            SetPreviewTexture( noiseImage );
        }
        texData.Free();
    }

    ImGui::SetNextWindowSize( ImVec2( 768, 768 ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowPos( ImVec2( 1143, 305 ), ImGuiCond_FirstUseEver );
    if( ImGui::Begin( "Texture Preview", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse ) )
    {
        // ImGui::Text( "Min: %0.6f Max: %0.6f", mMinMax.min, mMinMax.max );

        ImGui::PushItemWidth( 82.0f );
        bool edited = false;

        edited |= ImGui::Combo( "Generation Type", reinterpret_cast<int*>( &mBuildData.generationType ), GenTypeStrings );
        edited |= ImGuiExtra::ScrollCombo( reinterpret_cast<int*>( &mBuildData.generationType ), GenType_Count );

        ImVec2 contentSize = ImGui::GetContentRegionAvail();
        ImGui::SameLine();

        Vector2i texSize = { mBuildData.size.x(), mBuildData.size.y() };

        if( ImGui::DragInt2( "Size", texSize.data(), 2, 4, 8192 ) )
        {
            ImVec2 delta( Vector2 { texSize - mBuildData.size } );

            ImVec2 windowSize = ImGui::GetWindowSize();

            windowSize += delta;
            contentSize += delta;

            ImGui::SetWindowSize( windowSize );
        }
        ImGui::SameLine();

        edited |= ImGui::DragInt( "Seed", &mBuildData.seed );
        ImGui::SameLine();

        edited |= ImGui::DragFloat( "Frequency", &mBuildData.frequency, 0.001f );


        if( exportProgress.load() > 0 )
        {
            status = "Export progress: ";
            status += std::to_string( exportProgress.load() );
            status += "%";
        }

        if( mBuildData.generator && ImGui::Button( "Export BMP" ) )
        {
            auto size = mExportBuildData.size;
            mExportBuildData = mBuildData;
            mExportBuildData.size = size;
            ImGui::OpenPopup( "Export BMP" );
            status = "Exporting BMP";
        }

        ImGui::SameLine();

        if( mBuildData.generator && ImGui::Button( "Export RAW" ) )
        {
            auto size = mExportBuildData.size;
            auto path = mExportBuildData.path;
            mExportBuildData = mBuildData;
            mExportBuildData.size = size;
            mExportBuildData.path = path;
            ImGui::OpenPopup( "Export RAW" );
            status = "Exporting RAW terrain data";
        }

        ImGui::SameLine();

        if( mBuildData.generator && ImGui::Button( "Wait For Export" ) )
        {
            if( mExportThread.joinable() )
            {
                mExportThread.join();
            }

            status = "Export finished.";
        }

        ImGui::SameLine();
        ImGui::Text( "Status: %s", status.c_str() );

        ImGui::PopItemWidth();

        if( contentSize.x >= 1 && contentSize.y >= 1 &&
            ( edited || mBuildData.size.x() != (int)contentSize.x || mBuildData.size.y() != (int)contentSize.y ) )
        {
            Vector2i newSize = { (int)contentSize.x, (int)contentSize.y };

            mBuildData.offset.xy() -= Vector2( newSize - mBuildData.size ) / 2;
            mBuildData.size = newSize;
            ReGenerate( mBuildData.generator );
        }

        if( edited )
        {
            ImGuiExtra::MarkSettingsDirty();
        }

        ImGui::PushStyleColor( ImGuiCol_Button, 0 );
        ImGui::PushStyleColor( ImGuiCol_ButtonActive, 0 );
        ImGui::PushStyleColor( ImGuiCol_ButtonHovered, 0 );
        ImGuiIntegration::imageButton( mNoiseTexture, Vector2( mNoiseTexture.imageSize( 0 ) ), { {}, Vector2 { 1 } }, 0 );
        ImGui::PopStyleColor( 3 );

        if( ImGui::IsItemHovered() )
        {
            Vector4 oldOffset = mBuildData.offset;

            if( mBuildData.generationType != GenType_2DTiled && ImGui::IsMouseDragging( ImGuiMouseButton_Left ) )
            {
                Vector2 dragDelta( ImGui::GetMouseDragDelta( ImGuiMouseButton_Left ) );
                ImGui::ResetMouseDragDelta( ImGuiMouseButton_Left );

                mBuildData.offset.x() -= dragDelta.x();
                mBuildData.offset.y() += dragDelta.y();
            }
            else if( ( mBuildData.generationType == GenType_3D || mBuildData.generationType == GenType_4D ) && ImGui::IsMouseDragging( ImGuiMouseButton_Right ) )
            {
                Vector2 dragDelta( ImGui::GetMouseDragDelta( ImGuiMouseButton_Right ) );
                ImGui::ResetMouseDragDelta( ImGuiMouseButton_Right );

                mBuildData.offset.z() -= dragDelta.x();

                if( mBuildData.generationType == GenType_4D )
                {
                    mBuildData.offset.w() -= dragDelta.y();
                }
            }

            if( oldOffset != mBuildData.offset )
            {
                ReGenerate( mBuildData.generator );
            }
        }

        DoExport();
    }
    ImGui::End();
}

void NoiseTexture::DoExport()
{
    DoExportBMP();
    DoExportRAW();
}

void NoiseTexture::DoExportRAW()
{
    if( ImGui::BeginPopupModal( "Export RAW", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings ) )
    {
        ImGui::PushItemWidth( 200.0f );

        if( ImGui::DragInt2( "Size", mExportBuildData.size.data(), 2, 4, 8192 * 4 ) )
        {
            ImGuiExtra::MarkSettingsDirty();
        }

        if( ImGui::DragInt2( "Planes", mExportBuildData.numberOfPlanes.data(), 2, 1, 256 ) )
        {
            ImGuiExtra::MarkSettingsDirty();
        }

        if( ImGui::InputText( "Path", mExportBuildData.path.data(), mExportBuildData.path.size() ) )
        {
            ImGuiExtra::MarkSettingsDirty();
        }

        if( ImGui::Button( "Export (async)" ) )
        {
            ImGui::CloseCurrentPopup();

            float relativeScale = (float)mExportBuildData.size.sum() / mBuildData.size.sum();

            mExportBuildData.frequency /= relativeScale;
            mExportBuildData.offset *= relativeScale;

            if( mExportThread.joinable() )
            {
                mExportThread.join();
            }
            mExportThread = std::thread( [buildData = mExportBuildData, this]() {
                auto const startOffset = buildData.offset;
                auto buffer = std::vector<std::uint16_t>( buildData.size.x() * buildData.size.y() );
                std::filesystem::path path = buildData.path.data();
                std::error_code ec;
                std::filesystem::create_directories( path, ec );
                exportProgress = 0;
                auto step = 100 / ( buildData.numberOfPlanes.y() * buildData.numberOfPlanes.x() );
                for( int y = 0; y < buildData.numberOfPlanes.y(); ++y )
                {
                    for( int x = 0; x < buildData.numberOfPlanes.x(); ++x )
                    {
                        auto copy = buildData;
                        auto offset = buildData.offset + Magnum::Vector4( (float)x * buildData.size.x(), (float)y * buildData.size.y(), 0, 0 );

                        BuildTerrainDataRAW( buffer, copy, offset );

                        std::string filename = "terrain_";
                        filename += std::to_string( x );
                        filename += 'x';
                        filename += std::to_string( y );
                        filename += ".raw";

                        std::ofstream file( path / filename, std::ofstream::binary | std::ofstream::out | std::ofstream::trunc );
                        file.write( reinterpret_cast<char*>( buffer.data() ), buffer.size() * 2 );
                        exportProgress.fetch_add( step );
                    }
                }
            } );
        }
        ImGui::SameLine();
        if( ImGui::Button( "Cancel" ) )
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::PopItemWidth();
        ImGui::EndPopup();
    }
}

void NoiseTexture::DoExportBMP()
{

    if( ImGui::BeginPopupModal( "Export BMP", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings ) )
    {
        ImGui::PushItemWidth( 82.0f );
        if( ImGui::DragInt2( "Size", mExportBuildData.size.data(), 2, 4, 8192 * 4 ) )
        {
            ImGuiExtra::MarkSettingsDirty();
        }

        if( ImGui::Button( "Export (async)" ) )
        {
            ImGui::CloseCurrentPopup();

            float relativeScale = (float)mExportBuildData.size.sum() / mBuildData.size.sum();

            mExportBuildData.frequency /= relativeScale;
            mExportBuildData.offset *= relativeScale;

            if( mExportThread.joinable() )
            {
                mExportThread.join();
            }
            mExportThread = std::thread( [buildData = mExportBuildData, this]() {
                exportProgress = 0;
                auto data = BuildTexture<FastNoise::ConvertRGBA8>( buildData );

                const char* nodeName = buildData.generator->GetMetadata().name;
                std::string filename = nodeName;
                filename += ".bmp";

                // Iterate through file names if filename exists
                for( int i = 1; i < 1024; i++ )
                {
                    if( !std::filesystem::exists( filename.c_str() ) )
                    {
                        break;
                    }
                    filename = nodeName;
                    filename += '_' + std::to_string( i ) + ".bmp";
                }

                std::ofstream file( filename.c_str(), std::ofstream::binary | std::ofstream::out | std::ofstream::trunc );

                if( file.is_open() )
                {
                    struct BmpHeader
                    {
                        // File header (14)
                        // char b = 'B';
                        // char m = 'M';
                        uint32_t fileSize;
                        uint32_t reserved = 0;
                        uint32_t dataOffset = 14u + 12u + ( 256u * 3u );
                        // Bmp Info Header (12)
                        uint32_t headerSize = 12u;
                        uint16_t sizeX;
                        uint16_t sizeY;
                        uint16_t colorPlanes = 1u;
                        uint16_t bitDepth = 8u;
                    };

                    int paddedSizeX = buildData.size.x();
                    int padding = paddedSizeX % 4;
                    if( padding )
                    {
                        padding = 4 - padding;
                        paddedSizeX += padding;
                    }

                    BmpHeader header;
                    header.fileSize = header.dataOffset + (uint32_t)( paddedSizeX * buildData.size.y() );
                    header.sizeX = (uint16_t)buildData.size.x();
                    header.sizeY = (uint16_t)buildData.size.y();

                    file << 'B' << 'M';
                    file.write( reinterpret_cast<char*>( &header ), sizeof( BmpHeader ) );

                    // Colour map
                    for( int i = 0; i < 256; i++ )
                    {
                        Vector3ub b3( (uint8_t)i );
                        file.write( reinterpret_cast<char*>( b3.data() ), 3 );
                    }

                    int xIdx = padding ? buildData.size.x() : 0;

                    for( uint32_t pix: data.textureData )
                    {
                        file.write( reinterpret_cast<char*>( &pix ), 1 );

                        if( --xIdx == 0 )
                        {
                            xIdx = buildData.size.x();

                            Vector3ub b3( 0 );
                            file.write( reinterpret_cast<char*>( b3.data() ), padding );
                        }
                    }

                    file.close();
                    exportProgress = 100;
                }
            } );
        }

        if( ImGui::Button( "Cancel" ) )
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::PopItemWidth();
        ImGui::EndPopup();
    }
}

void NoiseTexture::SetPreviewTexture( ImageView2D& imageView )
{
    mNoiseTexture = GL::Texture2D();
    mNoiseTexture.setStorage( 1, GL::TextureFormat::RGBA8, imageView.size() )
        .setSubImage( 0, {}, imageView );
}

void NoiseTexture::ReGenerate( FastNoise::SmartNodeArg<> generator )
{
    mBuildData.generator = generator;
    mBuildData.iteration++;

    mGenerateQueue.Clear();

    if( mBuildData.size.x() <= 0 || mBuildData.size.y() <= 0 )
    {
        return;
    }

    if( generator )
    {
        mGenerateQueue.Push( mBuildData );
        return;
    }

    std::array<uint32_t, 16 * 16> blankTex = {};

    ImageView2D noiseImage( PixelFormat::RGBA8Unorm, { 16, 16 }, blankTex );
    mCurrentIteration = mBuildData.iteration;

    SetPreviewTexture( noiseImage );
}

void NoiseTexture::BuildTerrainDataRAW( std::vector<std::uint16_t>& buffer, const BuildData& buildData, Magnum::Vector4 offset )
{
    static thread_local std::vector<float> noiseData;
    noiseData.resize( (size_t)buildData.size.x() * buildData.size.y() );

    FastNoise::OutputMinMax minMax;
    auto gen = buildData.generator;

    switch( buildData.generationType )
    {
    case GenType_2D:
        minMax = gen->GenUniformGrid2D( noiseData.data(),
                                        (int)buildData.offset.x(), (int)buildData.offset.y(),
                                        buildData.size.x(), buildData.size.y(),
                                        buildData.frequency, buildData.seed );
        break;

    case GenType_2DTiled:
        minMax = gen->GenTileable2D( noiseData.data(),
                                     buildData.size.x(), buildData.size.y(),
                                     buildData.frequency, buildData.seed );
        break;

    case GenType_3D:
        minMax = gen->GenUniformGrid3D( noiseData.data(),
                                        (int)buildData.offset.x(), (int)buildData.offset.y(), (int)buildData.offset.z(),
                                        buildData.size.x(), buildData.size.y(), 1,
                                        buildData.frequency, buildData.seed );
        break;

    case GenType_4D:
        minMax = gen->GenUniformGrid4D( noiseData.data(),
                                        (int)buildData.offset.x(), (int)buildData.offset.y(), (int)buildData.offset.z(), (int)buildData.offset.w(),
                                        buildData.size.x(), buildData.size.y(), 1, 1,
                                        buildData.frequency, buildData.seed );
        break;
    case GenType_Count:
        break;
    }

    auto ratio = 1.0f / ( minMax.max - minMax.min );
    for( std::size_t pix = 0; pix < buffer.size(); ++pix )
        buffer[pix] = static_cast<std::uint16_t>( ( noiseData[pix] - minMax.min ) * ratio * 65535.0f );
}

template<typename Wrapper>
NoiseTexture::TextureData NoiseTexture::BuildTexture( const BuildData& buildData )
{
    static thread_local std::vector<float> noiseData;
    noiseData.resize( (size_t)buildData.size.x() * buildData.size.y() );

    auto gen = FastNoise::New<Wrapper>( buildData.generator->GetSIMDLevel() );
    gen->SetSource( buildData.generator );

    FastNoise::OutputMinMax minMax;

    switch( buildData.generationType )
    {
    case GenType_2D:
        minMax = gen->GenUniformGrid2D( noiseData.data(),
                                        (int)buildData.offset.x(), (int)buildData.offset.y(),
                                        buildData.size.x(), buildData.size.y(),
                                        buildData.frequency, buildData.seed );
        break;

    case GenType_2DTiled:
        minMax = gen->GenTileable2D( noiseData.data(),
                                     buildData.size.x(), buildData.size.y(),
                                     buildData.frequency, buildData.seed );
        break;

    case GenType_3D:
        minMax = gen->GenUniformGrid3D( noiseData.data(),
                                        (int)buildData.offset.x(), (int)buildData.offset.y(), (int)buildData.offset.z(),
                                        buildData.size.x(), buildData.size.y(), 1,
                                        buildData.frequency, buildData.seed );
        break;

    case GenType_4D:
        minMax = gen->GenUniformGrid4D( noiseData.data(),
                                        (int)buildData.offset.x(), (int)buildData.offset.y(), (int)buildData.offset.z(), (int)buildData.offset.w(),
                                        buildData.size.x(), buildData.size.y(), 1, 1,
                                        buildData.frequency, buildData.seed );
        break;
    case GenType_Count:
        break;
    }

    return TextureData( buildData.iteration, buildData.size, minMax, noiseData );
}

void NoiseTexture::GenerateLoopThread( GenerateQueue<BuildData>& generateQueue, CompleteQueue<TextureData>& completeQueue )
{
    while( true )
    {
        BuildData buildData = generateQueue.Pop();

        if( generateQueue.ShouldKillThread() )
        {
            return;
        }

        TextureData texData = BuildTexture<FastNoise::ConvertRGBA8>( buildData );

        if( !completeQueue.Push( texData ) )
        {
            texData.Free();
        }
    }
}

void NoiseTexture::SetupSettingsHandlers()
{
    ImGuiSettingsHandler editorSettings;
    editorSettings.TypeName = "NoiseToolNoiseTexture";
    editorSettings.TypeHash = ImHashStr( editorSettings.TypeName );
    editorSettings.UserData = this;
    editorSettings.WriteAllFn = []( ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* outBuf ) {
        auto* noiseTexture = (NoiseTexture*)handler->UserData;
        outBuf->appendf( "\n[%s][Settings]\n", handler->TypeName );

        outBuf->appendf( "frequency=%f\n", noiseTexture->mBuildData.frequency );
        outBuf->appendf( "seed=%d\n", noiseTexture->mBuildData.seed );
        outBuf->appendf( "gen_type=%d\n", (int)noiseTexture->mBuildData.generationType );
        outBuf->appendf( "export_size=%d:%d\n", noiseTexture->mExportBuildData.size.x(), noiseTexture->mExportBuildData.size.y() );
        outBuf->appendf( "pane_size=%d:%d\n", noiseTexture->mExportBuildData.numberOfPlanes.x(), noiseTexture->mExportBuildData.numberOfPlanes.y() );
        outBuf->appendf( "path=%s\n", noiseTexture->mExportBuildData.path.data() );
    };
    editorSettings.ReadOpenFn = []( ImGuiContext* ctx, ImGuiSettingsHandler* handler, const char* name ) -> void* {
        if( strcmp( name, "Settings" ) == 0 )
        {
            return handler->UserData;
        }

        return nullptr;
    };
    editorSettings.ReadLineFn = []( ImGuiContext* ctx, ImGuiSettingsHandler* handler, void* entry, const char* line ) {
        auto* noiseTexture = (NoiseTexture*)handler->UserData;

        sscanf( line, "frequency=%f", &noiseTexture->mBuildData.frequency );
        sscanf( line, "seed=%d", &noiseTexture->mBuildData.seed );
        sscanf( line, "gen_type=%d", (int*)&noiseTexture->mBuildData.generationType );
        sscanf( line, "export_size=%d:%d", &noiseTexture->mExportBuildData.size.x(), &noiseTexture->mExportBuildData.size.y() );
        sscanf( line, "plane_size=%d:%d", &noiseTexture->mExportBuildData.numberOfPlanes.x(), &noiseTexture->mExportBuildData.numberOfPlanes.y() );
        sscanf( line, "path=%s", noiseTexture->mExportBuildData.path.data() );
    };

    ImGuiExtra::AddOrReplaceSettingsHandler( editorSettings );
}
