#include "EditableHieghtmap.h"

#include "IconsFontAwesome6.h"
#include "ImGuiExtra.h"
#include "ImGuiUtils.h"
#include "Utils.h"

#include "FastNoiseNodeEditor.h"

namespace Magnum
{
    static constexpr std::uint32_t MaxHeightmapColorMapRes = 512;

    EditableHeightmap::EditableHeightmap()
    {
        SetupSettingsHandlers();
        RegenreateGrid();
    }

    void EditableHeightmap::ResetOffsets()
    {
        auto oldOffset = offset;
        auto mapSize   = GetMapSize();
        offset.x()     = -mapSize.x() / 2;
        offset.y()     = -mapSize.y() / 2;
        if( offset != oldOffset )
            edited = true;
    }

    void EditableHeightmap::RegenreateGrid()
    {
        auto     mapSize = GetMapSize();
        uint32_t VCount  = ( mapSize.y() + 1 ) * ( mapSize.x() + 1 );
        uint32_t ICount  = ( mapSize.y() ) * ( mapSize.x() );
        auto     data    = std::vector<Vector2>( VCount );
        auto     indices = std::vector<uint32_t>( ICount * 6 );
        int      sy      = mapSize.y();
        int      sx      = mapSize.x();
        float    startY  = -( (float)sy ) * 0.5f;
        float    startX  = -( (float)sx ) * 0.5f;


        for( int y = 0; y <= mapSize.y(); ++y )
        {
            for( int x = 0; x <= mapSize.x(); ++x )
            {
                auto vertexId  = (size_t)( y * ( mapSize.x() + 1 ) + x );
                data[vertexId] = Vector2( x + startX, y + startY );
            }
        }
        for( int y = 0; y < mapSize.y(); ++y )
        {
            for( int x = 0; x < mapSize.x(); ++x )
            {
                auto patchId = int( y * mapSize.x() + x ) * 6;
                auto vertId  = int( y * ( mapSize.x() + 1 ) + x );

                indices[patchId + 0] = vertId;
                indices[patchId + 1] = vertId + mapSize.x() + 1;
                indices[patchId + 2] = vertId + 1;

                indices[patchId + 3] = vertId + 1;
                indices[patchId + 4] = vertId + mapSize.x() + 1;
                indices[patchId + 5] = vertId + mapSize.x() + 2;
            }
        }


        heights.resize( 32, VCount );

        xyBuffer = GL::Buffer( GL::Buffer::TargetHint::Array, Containers::ArrayView<Vector2>( data ) );
        heightBuffer =
            GL::Buffer( GL::Buffer::TargetHint::Array, Containers::ArrayView<float>( heights.begin(), VCount ),
                        GL::BufferUsage::DynamicDraw );
        indexBuffer = GL::Buffer( GL::Buffer::TargetHint::Array, Containers::ArrayView<uint32_t>( indices ) );
        mesh        = std::make_unique<GL::Mesh>( GL::MeshPrimitive::Triangles );
        mesh->addVertexBuffer( xyBuffer, 0, Shader::PositionXY {} );
        mesh->addVertexBuffer( heightBuffer, 0, Shader::Height {} );
        mesh->setCount( (uint32_t)indices.size() );
        mesh->setIndexBuffer( GL::Buffer( GL::Buffer::TargetHint::ElementArray, indices ), 0,
                              GL::MeshIndexType::UnsignedInt, 0, (UnsignedInt)data.size() - 1 );
        UpdateHeights();
    }

    void EditableHeightmap::UpdateHeights()
    {
        if( !generator )
            return;
        auto     mapSize = GetMapSize();
        int      sy      = mapSize.y();
        int      sx      = mapSize.x();
        uint32_t VCount  = ( mapSize.y() + 1 ) * ( mapSize.x() + 1 );

        FastNoise::GeneratorInput ctx( heights );

        float startY = -( (float)sy ) * 0.5f + offset.x();
        float startX = -( (float)sx ) * 0.5f + offset.y();

        ctx.start[0]    = (int)startX;
        ctx.start[1]    = (int)startY;
        ctx.size[0]     = ( sx + 1 );
        ctx.size[1]     = ( sy + 1 );
        ctx.seed        = seed;
        ctx.frequency   = frequency;
        ctx.gridSize[0] = gridSize.x();
        ctx.gridSize[1] = gridSize.y();

        generator->GenUniformGrid2D( ctx );

        heightBuffer.setSubData( 0, Containers::ArrayView<float>( heights.begin(), VCount ) );

        heightsDirty = false;

        minMax = ctx.minMax;
    }

    void EditableHeightmap::Draw( FastNoiseNodeEditor& editor, const Matrix4& transformation, const Matrix4& projection,
                                  const Vector3& cameraPosition )
    {
        if( !generator )
        {
            return;
        }

        Matrix4 transformationProjection = projection * transformation;

        Frustum camFrustum = Frustum::fromMatrix( transformationProjection );
        shader.SetTransformationProjectionMatrix( transformationProjection );


        edited |= ImGui::DragInt( "Seed", &seed );
        edited |= ImGui::DragFloat( "Frequency", &frequency, 0.0005f, 0, 0, "%.4f" );
        if( edited )
            editor.SetEdited();
        edited |= ImGui::DragInt2( "Grid Count", gridCount.data(), 1, 1, 20 );
        edited |= ImGui::DragInt2( "Grid Size", gridSize.data(), 1, 2, std::numeric_limits<int>::max() );
        edited |= ImGui::DragInt2( "Offset", offset.data() );

        if( ImGui::DragFloat( "Heightmap Multiplier", &heightMultiplier, 0.5f ) || firstDraw )
        {
            shader.SetHeightMultiplier( heightMultiplier );
        }

        bool textureChanged = firstDraw;
        bool sunChanged     = firstDraw;
        if( ImGui::ColorEdit3( ICON_FA_SUN " Color", sunColor.data() ) )
        {
            sunChanged = true;
            ImGuiExtra::MarkSettingsDirty();
        }

        if( ImGui::DragFloat( ICON_FA_SUN " Intensity", &sunIntensity, 0.01f, 0, 100000.f ) )
        {
            sunChanged = true;
            ImGuiExtra::MarkSettingsDirty();
        }

        if( ImGui::DragFloat( ICON_FA_SUN " Rotation (phi)", &sunRotation.phi, 0.5f, 0, 360 ) )
        {
            sunChanged = true;
            ImGuiExtra::MarkSettingsDirty();
        }

        if( ImGui::DragFloat( ICON_FA_SUN " Rotation (theta)", &sunRotation.theta, 0.5f, 0, 180 ) )
        {
            sunChanged = true;
            ImGuiExtra::MarkSettingsDirty();
        }

        if( ImGui::DragInt( "Lighting Style", &compressPrec, 1, 1, 10000 ) )
        {
            sunChanged = true;
#ifndef EQUAL_PREC
            edited = true;
#endif
            ImGuiExtra::MarkSettingsDirty();
        }

        if( sunChanged )
        {
            shader.SetSunDirection( sunRotation.toDir() );
            shader.SetSunIntensity( sunColor, sunIntensity );
            shader.SetRenderStyle( compressPrec );
        }

        // color panel
        int   colorIndex   = 0;
        float lastAlpha[3] = { 0 };
        for( auto& [color, active]: strataColorPerHeight )
        {
            for( int cl = 0; cl < 3; cl++ )
            {
                ImGui::PushID( colorIndex++ );
                if( ImGui::ColorEdit4( "", color[cl].data(),
                                       ImGuiColorEditFlags_::ImGuiColorEditFlags_NoInputs |
                                           ImGuiColorEditFlags_AlphaBar ) )
                {
                    if( color[cl].a() < lastAlpha[cl] )
                        color[cl].a() = lastAlpha[cl];
                    lastAlpha[cl]  = color[cl].a();
                    textureChanged = true;
                }
                ImGui::PopID();
                ImGui::SameLine();
            }
            ImGui::SetNextItemWidth( 20 );
            ImGui::PushID( colorIndex++ );
            if( ImGui::Button( ICON_FA_TRASH_CAN ) )
            {
                textureChanged = true;
                active         = false;
            }
            ImGui::PopID();
        }

        if( Button( ICON_FA_LAYER_GROUP, "Add color layer to sample from (x,y,z) (alpha is limiting height)" ) )
        {
            if( strataColorPerHeight.empty() )
                strataColorPerHeight.emplace_back( Color4( 0.2f, 0.2f, 0.2f, 0.0f ), true );
            else
                strataColorPerHeight.emplace_back( strataColorPerHeight.back() );
            textureChanged = true;
        }

        if( textureChanged )
        {
            if( strataColorPerHeight.size() > 0 )
            {
                auto beg = strataColorPerHeight.begin();
                while( beg != strataColorPerHeight.end() )
                {
                    if( !std::get<1>( *beg ) )
                        beg = strataColorPerHeight.erase( beg );
                    else
                        beg++;
                }
            }
            shader.SetHeightColorMap( strataColorPerHeight );
            ImGuiExtra::MarkSettingsDirty();
        }

        if( edited || firstDraw )
        {
            edited = false;
            RegenreateGrid();
            ImGuiExtra::MarkSettingsDirty();
        }

        if( heightsDirty )
        {
            UpdateHeights();
        }

        ImGui::Text( "MinMax: %0.1f, %0.1f Camera Pos: %0.1f, %0.1f, %0.1f", minMax.min, minMax.max, cameraPosition.x(),
                     cameraPosition.y(), cameraPosition.z() );

        shader.draw( *mesh );
        firstDraw = false;
    }

    EditableHeightmap::Shader::Shader()
    {
        Utility::Resource noiseToolResources( "NoiseTool" );

        const GL::Version version = GL::Context::current().supportedVersion( { GL::Version::GL300 } );

        GL::Shader vert = CreateShader( version, GL::Shader::Type::Vertex );
        GL::Shader frag = CreateShader( version, GL::Shader::Type::Fragment );

        CORRADE_INTERNAL_ASSERT_OUTPUT( vert.addSource( noiseToolResources.get( "Heightmap.vert" ) ).compile() );
        CORRADE_INTERNAL_ASSERT_OUTPUT( frag.addSource( noiseToolResources.get( "Heightmap.frag" ) ).compile() );

        attachShader( vert );
        attachShader( frag );

        CORRADE_INTERNAL_ASSERT_OUTPUT( link() );

        std::array<std::uint32_t, MaxHeightmapColorMapRes> data;
        data.fill( 0xfffffff );
        ImageView1D heightMapImage( PixelFormat::RGBA8Unorm, { MaxHeightmapColorMapRes }, data );

        for( int32_t i = 0; i < 3; ++i )
        {
            mHeightColors[i] = GL::Texture1D();
            mHeightColors[i].bind( i );
            mHeightColors[i]
                .setStorage( 5, GL::TextureFormat::RGBA8, MaxHeightmapColorMapRes )
                .setSubImage( 0, {}, heightMapImage );
            mHeightColors[i].generateMipmap();

            setUniform( mHeightColorMapUniform[i], i );
        }
    }

    GL::Shader EditableHeightmap::Shader::CreateShader( GL::Version version, GL::Shader::Type type )
    {
        GL::Shader shader( version, type );
        return shader;
    }

    EditableHeightmap::Shader& EditableHeightmap::Shader::SetTransformationProjectionMatrix( const Matrix4& matrix )
    {
        setUniform( mTransformationProjectionMatrixUniform, matrix );
        return *this;
    }

    EditableHeightmap::Shader& EditableHeightmap::Shader::SetSunIntensity( Color3 color, float intensity )
    {
        setUniform( mSunColor, Vector4( color, intensity ) );
        return *this;
    }

    EditableHeightmap::Shader& EditableHeightmap::Shader::SetSunDirection( Vector3 direction )
    {
        setUniform( mSunDirection, -direction );
        return *this;
    }

    EditableHeightmap::Shader& EditableHeightmap::Shader::SetHeightMultiplier( float iHeight )
    {
        setUniform( mHeightmapMultiplier, iHeight );
        return *this;
    }

    EditableHeightmap::Shader& EditableHeightmap::Shader::SetRenderStyle( int spec )
    {
        setUniform( mCompressSpec, (float)spec );
        return *this;
    }

    EditableHeightmap::Shader& EditableHeightmap::Shader::SetHeightColorMap( ColorLayerValue const& colorMap )
    {
        std::array<std::uint32_t, MaxHeightmapColorMapRes> data;

        if( !colorMap.size() )
        {
            data.fill( 0xfcfcfcff );
            ImageView1D heightMapImage( PixelFormat::RGBA8Unorm, { MaxHeightmapColorMapRes }, data );

            for( uint32_t i = 0; i < 3; ++i )
            {
                mHeightColors[i].bind( i );
                mHeightColors[i].setSubImage( 0, {}, heightMapImage );
            }
        }
        else
        {

            for( uint32_t i = 0; i < 3; ++i )
            {
                std::uint32_t index     = 0;
                std::uint32_t lastColor = {};
                for( auto& l: colorMap )
                {
                    auto          color = std::get<0>( l )[i];
                    std::uint32_t r     = ( std::uint8_t )( color.r() * 255.f );
                    std::uint32_t g     = ( std::uint8_t )( color.g() * 255.f );
                    std::uint32_t b     = ( std::uint8_t )( color.b() * 255.f );
                    lastColor           = 255 << 24 | b << 16 | g << 8 | ( r & 0xff );
                    auto lt             = ( std::uint32_t )( color.a() * ( MaxHeightmapColorMapRes ) );
                    while( index < MaxHeightmapColorMapRes && index < lt )
                        data[index++] = lastColor;
                }
                while( index < MaxHeightmapColorMapRes )
                    data[index++] = lastColor;

                ImageView1D heightMapImage( PixelFormat::RGBA8Unorm, { MaxHeightmapColorMapRes }, data );
                mHeightColors[i].bind( i );
                mHeightColors[i].setSubImage( 0, {}, heightMapImage );
                mHeightColors[i].generateMipmap();
            }
        }
        return *this;
    }


    void EditableHeightmap::SetupSettingsHandlers()
    {
        ImGuiSettingsHandler editorSettings;
        editorSettings.TypeName   = "NoiseToolEditableHeightmap";
        editorSettings.TypeHash   = ImHashStr( editorSettings.TypeName );
        editorSettings.UserData   = this;
        editorSettings.WriteAllFn = []( ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* outBuf ) {
            auto* _ = (EditableHeightmap*)handler->UserData;
            outBuf->appendf( "\n[%s][Settings]\n", handler->TypeName );

            outBuf->appendf( "frequency=%f\n", _->frequency );
            outBuf->appendf( "heightmap_multiplier=%f\n", _->heightMultiplier );
            outBuf->appendf( "seed=%d\n", _->seed );
            outBuf->appendf( "color=%d\n", (int)_->sunColor.toSrgbInt() );
            outBuf->appendf( "offset=%d:%d\n", _->offset.x(), _->offset.y() );
            outBuf->appendf( "grid_size=%d:%d\n", _->gridSize[0], _->gridSize[1] );
            outBuf->appendf( "grid_count=%d:%d\n", _->gridCount[0], _->gridCount[1] );
            outBuf->appendf( "sun_rotation=%f:%f\n", _->sunRotation.theta, _->sunRotation.phi );
            outBuf->appendf( "draw_style=%d\n", _->compressPrec );
            outBuf->appendf( "sun_intensity=%f\n", _->sunIntensity );
            for( auto& [color, active]: _->strataColorPerHeight )
            {
                if( active )
                {
                    outBuf->appendf( "strata_color=%d:%d:%d\n", (int)color[0].toSrgbAlphaInt(),
                                     (int)color[1].toSrgbAlphaInt(), (int)color[2].toSrgbAlphaInt() );
                }
            }
        };
        editorSettings.ReadOpenFn = []( ImGuiContext* ctx, ImGuiSettingsHandler* handler, const char* name ) -> void* {
            if( strcmp( name, "Settings" ) == 0 )
            {
                return handler->UserData;
            }

            return nullptr;
        };
        editorSettings.ReadLineFn = []( ImGuiContext* ctx, ImGuiSettingsHandler* handler, void* entry,
                                        const char* line ) {
            auto* _ = (EditableHeightmap*)handler->UserData;

            sscanf( line, "frequency=%f", &_->frequency );
            sscanf( line, "heightmap_multiplier=%f", &_->heightMultiplier );
            sscanf( line, "seed=%d", &_->seed );
            sscanf( line, "offset=%d:%d", &_->offset.x(), &_->offset.y() );
            sscanf( line, "grid_size=%d:%d", &_->gridSize[0], &_->gridSize[1] );
            sscanf( line, "grid_count=%d:%d", &_->gridCount[0], &_->gridCount[1] );
            sscanf( line, "sun_rotation=%f:%f", &_->sunRotation.theta, &_->sunRotation.phi );
            sscanf( line, "draw_style=%d", &_->compressPrec );
            sscanf( line, "sun_intensity=%f", &_->sunIntensity );

            int i     = 0;
            int cl[3] = { 0 };
            if( sscanf( line, "color=%d", &i ) == 1 )
            {
                _->sunColor = Color3::fromSrgb( i );
            }
            else if( sscanf( line, "strata_color=%d:%d:%d", &cl[0], &cl[1], &cl[2] ) == 3 )
            {
                _->strataColorPerHeight.emplace_back( Color4x3 { Color4::fromSrgbAlpha( cl[0] ),
                                                                 Color4::fromSrgbAlpha( cl[1] ),
                                                                 Color4::fromSrgbAlpha( cl[2] ) },
                                                      true );
            }
        };
        ImGuiExtra::AddOrReplaceSettingsHandler( editorSettings );
    }

} // namespace Magnum