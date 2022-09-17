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
#include "ImGuiUtils.h"
#include "NoiseTexture.h"

#include "IconsFontAwesome6.h"
#include "ImGuiFileDialog.h"

using namespace Magnum;

NoiseTexture::NoiseTexture()
{
    mBuildData.iteration = 0;
    mBuildData.frequency = 0.02f;
    mBuildData.seed      = 1337;
    mBuildData.size      = { -1, -1 };
    mBuildData.gridSize  = { 126, 126 };
    mBuildData.offset    = {};


    mExportBuildData = mBuildData;
    SetupSettingsHandlers();
}

NoiseTexture::~NoiseTexture()
{
    if( mExportTask.valid() )
        mExportTask.wait();
    // mWorkerThread.join();
}

void NoiseTexture::Draw( FastNoiseNodeEditor* iParent )
{
    if( mTexData.valid() )
    {
        mTexData.wait();
        auto texData = mTexData.get();
        if( mCurrentIteration < texData.iteration )
        {
            mCurrentIteration = texData.iteration;
            ImageView2D noiseImage( PixelFormat::RGBA8Srgb, texData.size, texData.textureData );
            SetPreviewTexture( noiseImage );
        }
        texData.Free();
    }

    bool regen    = false;
    auto GridSize = iParent->GetMeshGridSize();
    if( GridSize != mBuildData.gridSize )
    {
        regen                     = true;
        mBuildData.gridSize       = GridSize;
        mExportBuildData.gridSize = GridSize;
    }

    ImGui::SetNextWindowSize( ImVec2( 768, 768 ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowPos( ImVec2( 1143, 305 ), ImGuiCond_FirstUseEver );
    if( ImGui::Begin( "Texture Preview", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse ) )
    {
        ImGui::PushItemWidth( 82.0f );
        bool edited = false;

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


        auto exportProg = mExportProgress.load();
        if( exportProg > 0 )
        {
            mStatus = "Export progress: ";
            mStatus += std::to_string( mExportProgress.load() );
            mStatus += "%";
        }

        ImGui::SameLine();

        if( mBuildData.generator && Button( ICON_FA_BULLSEYE, "Recenter the preview texture" ) )
        {
            mBuildData.offset.x() = -contentSize.x / 2;
            mBuildData.offset.y() = -contentSize.y / 2;

            regen = true;
        }

        ImGui::SameLine();
        if( mBuildData.generator && Button( ICON_FA_FILE_EXPORT, "Export the texture as PNG (Grayscale 16bit)" ) )
        {
            mExportBuildData = mBuildData;
            ImGui::OpenPopup( "Export PNG" );
            mStatus = "Exporting PNG terrain data";
            if( iParent )
                iParent->AddHistoryRecord();
        }

        /*
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
        */

        ImGui::SameLine();

        if( mBuildData.generator && exportProg > 0 && exportProg != 100 )
        {
            ImGui::ProgressBar( exportProg / 100.f );
        }
        else if( exportProg == 100 && mExportTask.valid() )
        {
            if( mExportTask.valid() )
            {
                mExportTask.wait();
            }
            mExportProgress = 0;
            mStatus         = "Export finished.";
        }

        ImGui::SameLine();
        ImGui::Text( "Status: %s", mStatus.c_str() );

        ImGui::PopItemWidth();

        if( contentSize.x >= 1 && contentSize.y >= 1 &&
            ( edited || mBuildData.size.x() != (int)contentSize.x || mBuildData.size.y() != (int)contentSize.y ) )
        {
            Vector2i newSize = { (int)contentSize.x, (int)contentSize.y };

            mBuildData.offset -= Vector2( newSize - mBuildData.size ) / 2;
            mBuildData.size = newSize;
            regen           = true;
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

        ImDrawList* DrawList = ImGui::GetWindowDrawList();

        if( ImGui::IsItemHovered() )
        {

            auto oldOffset = mBuildData.offset;

            if( ImGui::IsMouseDragging( ImGuiMouseButton_Left ) )
            {
                Vector2 dragDelta( ImGui::GetMouseDragDelta( ImGuiMouseButton_Left ) );
                ImGui::ResetMouseDragDelta( ImGuiMouseButton_Left );

                mBuildData.offset.x() -= dragDelta.x();
                mBuildData.offset.y() -= dragDelta.y();
            }

            if( oldOffset != mBuildData.offset )
            {
                regen = true;
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


        auto size = contentSize;

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

    if( regen )
        ReGenerate( mBuildData.generator );
}

void NoiseTexture::DoExport( Vector2i grid )
{
    DoExportPNG();
}

void NoiseTexture::DoExportPNG()
{
    if( ImGui::BeginPopupModal( "Export PNG", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings ) )
    {

        ImGui::PushItemWidth( 200.0f );


        if( ImGui::DragInt2( "Grid Size", mExportBuildData.gridSize.data(), 2, 1, 8129 ) )
        {
            ImGuiExtra::MarkSettingsDirty();
        }

        if( ImGui::DragInt2( "Grid Count", mExportBuildData.gridCount.data(), 2, 1, 256 ) )
        {
            ImGuiExtra::MarkSettingsDirty();
        }

        if( ImGui::DragInt2( "Grid Start", mExportBuildData.gridStart.data(), 2, 1, 8129 ) )
        {
            ImGuiExtra::MarkSettingsDirty();
        }

        if( Magnum::Button( "Browse", "Select a folder to export to." ) )
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
            mExportTask           = std::async( std::launch::async, [buildData = mExportBuildData, this]() {
                // compute size
                auto buffer =
                    std::vector<std::uint16_t>( ( buildData.gridSize.x() + 1 ) * ( buildData.gridSize.y() + 1 ) );

                FastNoise::Buffer fsBuffer;

                std::filesystem::path path = buildData.path.data();
                std::error_code       ec;
                std::filesystem::create_directories( path, ec );
                mExportProgress = 0;

                auto step = 100 / ( buildData.gridCount.y() * buildData.gridCount.x() );
                for( int vy = 0; vy < buildData.gridCount.y(); ++vy )
                {
                    for( int vx = 0; vx < buildData.gridCount.x(); ++vx )
                    {
                        int gx = ( vx + buildData.gridStart.x() );
                        int gy = ( vy + buildData.gridStart.y() );
                        int x  = ( gx * buildData.gridSize.x() ) - ( buildData.gridSize.x() / 2 );
                        int y  = ( gy * buildData.gridSize.y() ) - ( buildData.gridSize.y() / 2 );

                        BuildTerrainDataRAW( buffer, fsBuffer, buildData, Vector2i( x, y ) );

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
                        filename += std::to_string( gx );
                        filename += "_y";
                        filename += std::to_string( gy );
                        filename += ".png";
                        filename   = ( path / filename ).string();
                        FILE* file = fopen( filename.c_str(), "wb" );
                        png_init_io( png_ptr, file );
                        png_set_IHDR( png_ptr, info_ptr, (uint32_t)buildData.gridSize.x() + 1,
                                                (uint32_t)buildData.gridSize.y() + 1, 16, PNG_COLOR_TYPE_GRAY, PNG_INTERLACE_NONE,
                                                PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT );
                        png_write_info( png_ptr, info_ptr );
                        png_set_swap( png_ptr );
                        auto row_pointers = std::vector<png_bytep>( buildData.gridSize.y() + 1 );
                        auto start        = buffer.data();
                        for( int i = 0; i <= buildData.gridSize.y(); ++i )
                        {
                            row_pointers[i] = (png_bytep)start;
                            start += ( buildData.gridSize.x() + 1 );
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

void NoiseTexture::SetPreviewTexture( ImageView2D& imageView )
{
    mNoiseTexture = GL::Texture2D();
    mNoiseTexture.setStorage( 1, GL::TextureFormat::RGBA8, imageView.size() ).setSubImage( 0, {}, imageView );
}

void NoiseTexture::ReGenerate( FastNoise::SmartNodeArg<> generator )
{
    mBuildData.generator = generator;
    mBuildData.iteration++;

    if( mBuildData.size.x() > 0 && mBuildData.size.y() > 0 && generator )
    {

        mTexData = std::async( std::launch::async, [buildData = mBuildData, this]() -> TextureData {
            return BuildTexture<FastNoise::ConvertRGBA8>(
                buildData.generator, buildData.iteration, buildData.texBuffer, buildData.gridSize, buildData.size,
                Vector2i( buildData.offset ), buildData.frequency, buildData.seed );
        } );
    }
}

template<typename Wrapper>
NoiseTexture::TextureData NoiseTexture::BuildTexture( FastNoise::SmartNode<const FastNoise::Generator> generator,
                                                      uint64_t iter, FastNoise::Buffer& buffer,
                                                      Magnum::Vector2i gridSize, Magnum::Vector2i size,
                                                      Magnum::Vector2i offset, float freq, int seed )
{
    buffer.resize( 32, (size_t)( size.x() * size.y() ) );

    auto gen = FastNoise::New<Wrapper>( generator->GetSIMDLevel() );
    gen->SetSource( generator );

    auto context        = FastNoise::GeneratorInput( buffer );
    context.frequency   = freq;
    context.seed        = seed;
    context.start[0]    = offset.x();
    context.start[1]    = offset.y();
    context.size[0]     = size.x();
    context.size[1]     = size.y();
    context.gridSize[0] = gridSize.x();
    context.gridSize[1] = gridSize.y();


    gen->GenUniformGrid2D( context );

    return TextureData( iter, size, context.minMax, buffer );
}

void NoiseTexture::BuildTerrainDataRAW( std::vector<std::uint16_t>& buffer, FastNoise::Buffer& out,
                                        const ExportData& buildData, Magnum::Vector2i offset )
{

    auto  data   = BuildTexture<FastNoise::ConvertRAW16>( buildData.generator, 0, out, buildData.gridSize,
                                                       buildData.gridSize + Vector2i( 1, 1 ), offset,
                                                       buildData.frequency, buildData.seed );
    auto* floats = (float*)out.begin();
    for( std::size_t pix = 0; pix < buffer.size(); ++pix )
        buffer[pix] = static_cast<std::uint16_t>( floats[pix] );
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
        outBuf->appendf( "export_grid_size=%d:%d\n", noiseTexture->mExportBuildData.gridSize.x(),
                         noiseTexture->mExportBuildData.gridSize.y() );
        outBuf->appendf( "export_grid_start=%d:%d\n", noiseTexture->mExportBuildData.gridStart.x(),
                         noiseTexture->mExportBuildData.gridStart.y() );
        outBuf->appendf( "export_grid_count=%d:%d\n", noiseTexture->mExportBuildData.gridCount.x(),
                         noiseTexture->mExportBuildData.gridCount.y() );
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
        sscanf( line, "export_grid_size=%d:%d", &noiseTexture->mExportBuildData.gridSize.x(),
                &noiseTexture->mExportBuildData.gridSize.y() );
        sscanf( line, "export_grid_start=%d:%d", &noiseTexture->mExportBuildData.gridStart.x(),
                &noiseTexture->mExportBuildData.gridStart.y() );
        sscanf( line, "export_grid_count=%d:%d", &noiseTexture->mExportBuildData.gridCount.x(),
                &noiseTexture->mExportBuildData.gridCount.y() );
        char name[256] = {};
        if( sscanf( line, "path=%s", name ) == 1 )
            noiseTexture->mExportBuildData.path = name;
        else if( sscanf( line, "name=%s", name ) == 1 )
            noiseTexture->mName = name;
    };

    ImGuiExtra::AddOrReplaceSettingsHandler( editorSettings );
}
