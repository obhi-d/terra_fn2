#include <cstdio>
#include <filesystem>
#include <fstream>

#include <png.h>

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>

#include <Corrade/Containers/ArrayViewStl.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/ImGuiIntegration/Widgets.h>
#include <Magnum/Math/Functions.h>
#include <Magnum/PixelFormat.h>

#include <FastNoise/Metadata.h>

#include "FastNoiseNodeEditor.h"
#include "ImGuiExtra.h"
#include "NoiseTexture.h"

#include "IconsFontAwesome6.h"
#include "ImGuiFileDialog.h"

using namespace Magnum;

NoiseTexture::NoiseTexture()
{
    mBuildData.iteration      = 0;
    mBuildData.frequency      = 0.02f;
    mBuildData.seed           = 1337;
    mBuildData.size           = { -1, -1 };
    mBuildData.offset         = {};
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

void NoiseTexture::Draw( FastNoiseNodeEditor* iParent )
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

    auto GridSize = iParent->GetMeshGridSize();

    ImGui::SetNextWindowSize( ImVec2( 768, 768 ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowPos( ImVec2( 1143, 305 ), ImGuiCond_FirstUseEver );
    if( ImGui::Begin( "Texture Preview", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse ) )
    {
        ImGui::PushItemWidth( 82.0f );
        bool edited = false;

        edited |=
            ImGui::Combo( "Generation Type", reinterpret_cast<int*>( &mBuildData.generationType ), GenTypeStrings );
        edited |= ImGuiExtra::ScrollCombo( reinterpret_cast<int*>( &mBuildData.generationType ), GenType_Count );

        ImVec2 contentSize = ImGui::GetContentRegionAvail();
        ImGui::SameLine();

        Vector2i texSize = { mBuildData.size.x(), mBuildData.size.y() };

        if( ImGui::DragInt2( ICON_FA_HAND, texSize.data(), 2, 4, 8192 ) )
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


        if( mExportProgress.load() > 0 )
        {
            mStatus = "Export progress: ";
            mStatus += std::to_string( mExportProgress.load() );
            mStatus += "%";
        }

        ImGui::SameLine();

        if( mBuildData.generator && ImGui::Button( ICON_FA_BULLSEYE ) )
        {
            mBuildData.offset.x() = -contentSize.x / 2;
            mBuildData.offset.y() = -contentSize.y / 2;

            ReGenerate( mBuildData.generator );
        }

        ImGui::SameLine();
        if( mBuildData.generator && ImGui::Button( ICON_FA_FILE_EXPORT ) )
        {
            auto size                       = mExportBuildData.size;
            auto path                       = mExportBuildData.path;
            auto nbPlanes                   = mExportBuildData.numberOfPlanes;
            mExportBuildData                = mBuildData;
            mExportBuildData.size           = size;
            mExportBuildData.path           = path;
            mExportBuildData.numberOfPlanes = nbPlanes;
            ImGui::OpenPopup( "Export PNG" );
            mStatus = "Exporting PNG terrain data";
            if( iParent )
                iParent->AddHistoryRecord();
        }

        ImGui::SameLine();
        if( mBuildData.generator && ImGui::Button( ICON_FA_FILE_IMAGE ) )
        {
            auto size                       = mExportBuildData.size;
            auto path                       = mExportBuildData.path;
            auto nbPlanes                   = mExportBuildData.numberOfPlanes;
            mExportBuildData                = mBuildData;
            mExportBuildData.size           = size;
            mExportBuildData.path           = path;
            mExportBuildData.numberOfPlanes = nbPlanes;
            ImGui::OpenPopup( "Export BMP" );
            mStatus = "Exporting BMP terrain data";
            if( iParent )
                iParent->AddHistoryRecord();
        }

        ImGui::SameLine();

        if( mBuildData.generator && ImGui::Button( ICON_FA_CLOCK ) )
        {
            if( mExportThread.joinable() )
            {
                mExportThread.join();
            }

            mStatus = "Export finished.";
        }

        ImGui::SameLine();
        ImGui::Text( "Status: %s", mStatus.c_str() );

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

        auto ImagePos    = ImGui::GetCursorScreenPos();
        auto RelImagePos = ImGui::GetCursorPos();

        ImGuiIntegration::imageButton( mNoiseTexture, Vector2( mNoiseTexture.imageSize( 0 ) ),
                                       { { 0.0f, 1.0f }, Vector2 { 1.0f, 0.0f } }, 0 );
        ImGui::PopStyleColor( 3 );

        if( ImGui::IsItemHovered() )
        {

            Vector4 oldOffset = mBuildData.offset;

            if( ImGui::IsMouseDragging( ImGuiMouseButton_Left ) )
            {
                Vector2 dragDelta( ImGui::GetMouseDragDelta( ImGuiMouseButton_Left ) );
                ImGui::ResetMouseDragDelta( ImGuiMouseButton_Left );

                mBuildData.offset.x() -= dragDelta.x();
                mBuildData.offset.y() -= dragDelta.y();
            }

            if( oldOffset != mBuildData.offset )
            {
                ReGenerate( mBuildData.generator );
            }

            auto mouse = ImGui::GetMousePos();
            auto delta = mouse - ImagePos;

            auto CoordX = ( mBuildData.offset.x() + delta.x );
            auto CoordY = ( mBuildData.offset.y() + delta.y );


            auto PlaneX = (int)( ( ( std::abs( CoordX ) + GridSize.x() * 0.5f ) / GridSize.x() ) );
            auto PlaneY = (int)( ( ( std::abs( CoordY ) + GridSize.y() * 0.5f ) / GridSize.y() ) );

            if( CoordX < 0 )
                PlaneX = -PlaneX;
            if( CoordY < 0 )
                PlaneY = -PlaneY;

            ImGui::SetTooltip( "(%d, %d) : (%d, %d)", (int)PlaneX, (int)PlaneY, (int)( CoordX ), (int)CoordY );
        }


        ImDrawList* DrawList = ImGui::GetWindowDrawList();
        auto        size     = contentSize;

        // vertical grids
        auto getMod = []( auto offset, auto size ) {
            if( offset < 0 )
                return std::abs( (int)( offset - ( size / 2 ) ) % (int)size );
            else
                return (int)size - std::abs( (int)( offset + ( size / 2 ) ) % (int)size );
        };
        auto start = getMod( mBuildData.offset.x(), GridSize.x() );
        while( start < size.x )
        {
            DrawList->AddLine( ImVec2( ImagePos.x + start, ImagePos.y ),
                               ImVec2( ImagePos.x + start, ImagePos.y + size.y ),
                               ImGui::GetColorU32( ImGuiCol_TextDisabled ) );
            start += GridSize.x();
        }

        // horizontal grids
        start = getMod( mBuildData.offset.y(), GridSize.y() );
        while( start < size.y )
        {
            DrawList->AddLine( ImVec2( ImagePos.x, ImagePos.y + start ),
                               ImVec2( ImagePos.x + size.x, ImagePos.y + start ),
                               ImGui::GetColorU32( ImGuiCol_TextDisabled ) );
            start += GridSize.y();
        }

        DoExport( GridSize );
    }
    ImGui::End();
}

void NoiseTexture::DoExport( Vector2i grid )
{
    mExportBuildData.size   = grid;
    mExportBuildData.offset = Magnum::Vector4( -mExportBuildData.size.x() / 2, -mExportBuildData.size.y() / 2, 0, 0 );
    DoExportBMP();
    // DoExportRAW();
    DoExportPNG();
}

void NoiseTexture::DoExportPNG()
{
    if( ImGui::BeginPopupModal( "Export PNG", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings ) )
    {

        ImGui::PushItemWidth( 200.0f );


        if( ImGui::DragInt2( "Plane Count", mExportBuildData.numberOfPlanes.data(), 2, 1, 256 ) )
        {
            ImGuiExtra::MarkSettingsDirty();
        }

        if( ImGui::DragInt2( "Plane Offset", mExportBuildData.plane.data(), 2, 1, 256 ) )
        {
            ImGuiExtra::MarkSettingsDirty();
        }

        if( ImGui::Button( "Browse" ) )
            ImGuiFileDialog::Instance()->OpenDialog( "BrowseFileDlgKey", "Raw", nullptr, mExportBuildData.path );

        if( ImGuiFileDialog::Instance()->Display( "BrowseFileDlgKey", 32, ImVec2 { 600, 400 } ) )
        {
            if( ImGuiFileDialog::Instance()->IsOk() )
            {
                mExportBuildData.path = ImGuiFileDialog::Instance()->GetFilePathName();
                ImGuiExtra::MarkSettingsDirty();
            }
            ImGuiFileDialog::Instance()->Close();
        }

        ImGui::SameLine();
        ImGui::Text( mExportBuildData.path.data() );

        if( ImGui::Button( "Export (async)" ) )
        {
            ImGui::CloseCurrentPopup();

            mExportBuildData.name = mName;

            if( mExportThread.joinable() )
            {
                mExportThread.join();
            }

            mExportThread = std::thread( [buildData = mExportBuildData, this]() {
                auto const startOffset = buildData.offset;
                auto       buffer      = std::vector<std::uint16_t>( buildData.size.x() * buildData.size.y() );

                std::filesystem::path path = buildData.path.data();
                std::error_code       ec;
                std::filesystem::create_directories( path, ec );
                mExportProgress = 0;

                auto step = 100 / ( buildData.numberOfPlanes.y() * buildData.numberOfPlanes.x() );
                for( int vy = 0; vy < buildData.numberOfPlanes.y(); ++vy )
                {
                    for( int vx = 0; vx < buildData.numberOfPlanes.x(); ++vx )
                    {
                        int  x      = vx + buildData.plane.x();
                        int  y      = vy + buildData.plane.y();
                        auto copy   = buildData;
                        copy.offset = Magnum::Vector4( (float)x * buildData.size.x() - buildData.size.x() / 2,
                                                       (float)y * buildData.size.y() - buildData.size.y() / 2, 0, 0 );

                        copy.plane.x() = x;
                        copy.plane.y() = y;

                        BuildTerrainDataRAW( buffer, copy, Magnum::Vector4( 0.0f ) );

                        png_structp png_ptr =
                            png_create_write_struct( PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr );
                        if( !png_ptr )
                            return;
                        png_infop info_ptr = png_create_info_struct( png_ptr );
                        if( !info_ptr )
                        {
                            png_destroy_write_struct( &png_ptr, (png_infopp)NULL );
                            return;
                        }

                        if( setjmp( png_jmpbuf( png_ptr ) ) )
                        {
                            png_destroy_write_struct( &png_ptr, &info_ptr );
                            return;
                        }


                        std::string filename = buildData.name;
                        if( filename.empty() )
                            filename = "untitled";
                        filename += "_x";
                        filename += std::to_string( x );
                        filename += "_y";
                        filename += std::to_string( y );
                        filename += ".png";
                        filename   = ( path / filename ).string();
                        FILE* file = fopen( filename.c_str(), "wb" );
                        png_init_io( png_ptr, file );
                        png_set_IHDR( png_ptr, info_ptr, (uint32_t)copy.size.x(), (uint32_t)copy.size.y(), 16,
                                      PNG_COLOR_TYPE_GRAY, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                                      PNG_FILTER_TYPE_DEFAULT );
                        png_write_info( png_ptr, info_ptr );
                        png_set_swap( png_ptr );
                        auto row_pointers = std::vector<png_bytep>( copy.size.y() );
                        auto start        = buffer.data();
                        for( int i = 0; i < copy.size.y(); ++i )
                        {
                            row_pointers[i] = (png_bytep)start;
                            start += copy.size.x();
                        }
                        png_write_image( png_ptr, row_pointers.data() );
                        png_write_end( png_ptr, info_ptr );
                        png_destroy_write_struct( &png_ptr, &info_ptr );
                        fclose( file );
                        mExportProgress.fetch_add( step );
                    }
                }
                mExportProgress = 100;
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

        if( ImGui::Button( "Browse" ) )
            ImGuiFileDialog::Instance()->OpenDialog( "BrowseFileDlgKey", "Raw", nullptr, "." );

        if( ImGuiFileDialog::Instance()->Display( "BrowseFileDlgKey", 32, ImVec2 { 600, 400 } ) )
        {
            if( ImGuiFileDialog::Instance()->IsOk() )
            {
                mExportBuildData.path = ImGuiFileDialog::Instance()->GetFilePathName();
                ImGuiExtra::MarkSettingsDirty();
            }
            ImGuiFileDialog::Instance()->Close();
        }

        ImGui::SameLine();
        ImGui::Text( mExportBuildData.path.data() );

        if( ImGui::Button( "Export (async)" ) )
        {
            ImGui::CloseCurrentPopup();

            float relativeScale = (float)mExportBuildData.size.sum() / mBuildData.size.sum();

            mExportBuildData.frequency /= relativeScale;
            mExportBuildData.offset *= relativeScale;
            mExportBuildData.name = mName;

            if( mExportThread.joinable() )
            {
                mExportThread.join();
            }

            mExportThread = std::thread( [buildData = mExportBuildData, this]() {
                auto const startOffset = buildData.offset;
                auto       buffer      = std::vector<std::uint16_t>( buildData.size.x() * buildData.size.y() );

                std::filesystem::path path = buildData.path.data();
                std::error_code       ec;
                std::filesystem::create_directories( path, ec );
                mExportProgress = 0;

                auto step = 100 / ( buildData.numberOfPlanes.y() * buildData.numberOfPlanes.x() );
                for( int y = 0; y < buildData.numberOfPlanes.y(); ++y )
                {
                    for( int x = 0; x < buildData.numberOfPlanes.x(); ++x )
                    {
                        auto copy   = buildData;
                        auto offset = buildData.offset +
                            Magnum::Vector4( (float)x * buildData.size.x(), (float)y * buildData.size.y(), 0, 0 );

                        copy.plane.x() = x;
                        copy.plane.y() = y;

                        BuildTerrainDataRAW( buffer, copy, offset );

                        std::string filename = buildData.name;
                        filename += "_x";
                        filename += std::to_string( x );
                        filename += "_y";
                        filename += std::to_string( y );
                        filename += ".raw";

                        std::ofstream file( path / filename,
                                            std::ofstream::binary | std::ofstream::out | std::ofstream::trunc );
                        file.write( reinterpret_cast<char*>( buffer.data() ), buffer.size() * 2 );
                        mExportProgress.fetch_add( step );
                    }
                }
                mExportProgress = 100;
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

            mExportBuildData.name = mName;

            if( mExportThread.joinable() )
            {
                mExportThread.join();
            }
            mExportThread = std::thread( [buildData = mExportBuildData, this]() {
                mExportProgress = 0;
                auto data       = BuildTexture<FastNoise::ConvertRGBA8>( buildData, Vector4() );

                std::filesystem::path filename;
                if( !buildData.path.empty() )
                    filename = buildData.path;
                filename /= buildData.name;
                filename += ".bmp";

                // Iterate through file names if filename exists


                std::ofstream file( filename, std::ofstream::binary | std::ofstream::out | std::ofstream::trunc );

                if( file.is_open() )
                {
                    struct BmpHeader
                    {
                        // File header (14)
                        // char b = 'B';
                        // char m = 'M';
                        uint32_t fileSize;
                        uint32_t reserved   = 0;
                        uint32_t dataOffset = 14u + 12u + ( 256u * 3u );
                        // Bmp Info Header (12)
                        uint32_t headerSize = 12u;
                        uint16_t sizeX;
                        uint16_t sizeY;
                        uint16_t colorPlanes = 1u;
                        uint16_t bitDepth    = 8u;
                    };

                    int paddedSizeX = buildData.size.x();
                    int padding     = paddedSizeX % 4;
                    if( padding )
                    {
                        padding = 4 - padding;
                        paddedSizeX += padding;
                    }

                    BmpHeader header;
                    header.fileSize = header.dataOffset + (uint32_t)( paddedSizeX * buildData.size.y() );
                    header.sizeX    = (uint16_t)buildData.size.x();
                    header.sizeY    = (uint16_t)buildData.size.y();

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
                    mExportProgress = 100;
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
    mNoiseTexture.setStorage( 1, GL::TextureFormat::RGBA8, imageView.size() ).setSubImage( 0, {}, imageView );
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
template<typename Wrapper>
NoiseTexture::TextureData NoiseTexture::BuildTexture( const BuildData& buildData, Magnum::Vector4 offset )
{
    static thread_local FastNoise::Buffer noiseData;
    noiseData.resize( 32, (size_t)buildData.size.x() * buildData.size.y() );

    auto gen = FastNoise::New<Wrapper>( buildData.generator->GetSIMDLevel() );
    gen->SetSource( buildData.generator );

    auto context       = FastNoise::Generator::Context( noiseData );
    context.planeId[0] = buildData.plane.x();
    context.planeId[1] = buildData.plane.y();
    offset += buildData.offset;
    switch( buildData.generationType )
    {
    case GenType_2D:

        gen->GenUniformGrid2D( context, (int)offset.x(), (int)offset.y(), buildData.size.x(), buildData.size.y(),
                               buildData.frequency, buildData.seed );
        break;
    case GenType_Count:
        break;
    }

    return TextureData( buildData.iteration, buildData.size, context.minMax, noiseData );
}

void NoiseTexture::BuildTerrainDataRAW( std::vector<std::uint16_t>& buffer, const BuildData& buildData,
                                        Magnum::Vector4 offset )
{
    auto  data   = BuildTexture<FastNoise::ConvertRAW16>( buildData, offset );
    auto* floats = (float*)data.copy.begin();
    for( std::size_t pix = 0; pix < buffer.size(); ++pix )
        buffer[pix] = static_cast<std::uint16_t>( floats[pix] );
}

void NoiseTexture::BuildTerrainDataRAW( std::vector<std::uint8_t>& buffer, const BuildData& buildData,
                                        Magnum::Vector4 offset )
{
    auto  data   = BuildTexture<FastNoise::ConvertRAW8>( buildData, offset );
    auto* floats = (float*)data.copy.begin();
    for( std::size_t pix = 0; pix < buffer.size(); ++pix )
        buffer[pix] = static_cast<std::uint8_t>( floats[pix] );
}
void NoiseTexture::GenerateLoopThread( GenerateQueue<BuildData>&   generateQueue,
                                       CompleteQueue<TextureData>& completeQueue )
{
    while( true )
    {
        BuildData buildData = generateQueue.Pop();

        if( generateQueue.ShouldKillThread() )
        {
            return;
        }

        TextureData texData = BuildTexture<FastNoise::ConvertRGBA8>( buildData, Vector4() );

        if( !completeQueue.Push( texData ) )
        {
            texData.Free();
        }
    }
}

void NoiseTexture::SetupSettingsHandlers()
{
    ImGuiSettingsHandler editorSettings;
    editorSettings.TypeName   = "NoiseToolNoiseTexture";
    editorSettings.TypeHash   = ImHashStr( editorSettings.TypeName );
    editorSettings.UserData   = this;
    editorSettings.WriteAllFn = []( ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* outBuf ) {
        auto* noiseTexture = (NoiseTexture*)handler->UserData;
        outBuf->appendf( "\n[%s][Settings]\n", handler->TypeName );

        outBuf->appendf( "frequency=%f\n", noiseTexture->mBuildData.frequency );
        outBuf->appendf( "seed=%d\n", noiseTexture->mBuildData.seed );
        outBuf->appendf( "gen_type=%d\n", (int)noiseTexture->mBuildData.generationType );
        outBuf->appendf( "export_size=%d:%d\n", noiseTexture->mExportBuildData.size.x(),
                         noiseTexture->mExportBuildData.size.y() );
        outBuf->appendf( "pane_size=%d:%d\n", noiseTexture->mExportBuildData.numberOfPlanes.x(),
                         noiseTexture->mExportBuildData.numberOfPlanes.y() );
        outBuf->appendf( "pane_offset=%d:%d\n", noiseTexture->mExportBuildData.plane.x(),
                         noiseTexture->mExportBuildData.plane.y() );
        outBuf->appendf( "path=%s\n", noiseTexture->mExportBuildData.path.c_str() );
        outBuf->appendf( "name=%s\n", noiseTexture->mName.c_str() );
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
        sscanf( line, "export_size=%d:%d", &noiseTexture->mExportBuildData.size.x(),
                &noiseTexture->mExportBuildData.size.y() );
        sscanf( line, "plane_size=%d:%d", &noiseTexture->mExportBuildData.numberOfPlanes.x(),
                &noiseTexture->mExportBuildData.numberOfPlanes.y() );
        sscanf( line, "plane_offset=%d:%d", &noiseTexture->mExportBuildData.plane.x(),
                &noiseTexture->mExportBuildData.plane.y() );
        char name[256] = {};
        if( sscanf( line, "path=%s", name ) == 1 )
            noiseTexture->mExportBuildData.path = name;
        else if( sscanf( line, "name=%s", name ) == 1 )
            noiseTexture->mName = name;
    };

    ImGuiExtra::AddOrReplaceSettingsHandler( editorSettings );
}
