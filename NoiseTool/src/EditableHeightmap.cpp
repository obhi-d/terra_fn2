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
        RegenreateGrid();
    }

    void EditableHeightmap::RegenreateGrid()
    {
        auto& settings = Settings::get();

        auto     mapSize = settings.mapSize();
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
        auto& settings = Settings::get();

        if( !settings.generator() )
            return;

        auto     mapSize = settings.mapSize();
        int      sy      = mapSize.y();
        int      sx      = mapSize.x();
        uint32_t VCount  = ( mapSize.y() + 1 ) * ( mapSize.x() + 1 );

        FastNoise::GeneratorInput ctx( heights );

        float startY = -( (float)sy ) * 0.5f + settings.offset().x();
        float startX = -( (float)sx ) * 0.5f + settings.offset().y();

        ctx.start[0]    = (int)startX;
        ctx.start[1]    = (int)startY;
        ctx.size[0]     = ( sx + 1 );
        ctx.size[1]     = ( sy + 1 );
        ctx.seed        = settings.seed();
        ctx.frequency   = settings.frequency();
        ctx.gridSize[0] = settings.gridSize().x();
        ctx.gridSize[1] = settings.gridSize().y();

        settings.generator()->GenUniformGrid2D( ctx );

        heightBuffer.setSubData( 0, Containers::ArrayView<float>( heights.begin(), VCount ) );

        minMax = ctx.minMax;
    }

    void EditableHeightmap::Draw( FastNoiseNodeEditor& editor, const Matrix4& transformation, const Matrix4& projection,
                                  const Vector3& cameraPosition )
    {
        auto& settings = Settings::get();

        if( !settings.generator() )
            return;

        Matrix4 transformationProjection = projection * transformation;

        Frustum camFrustum = Frustum::fromMatrix( transformationProjection );
        shader.SetTransformationProjectionMatrix( transformationProjection );

        if( settings.versionCheck_heightMultiplier( version ) )
            shader.SetHeightMultiplier( settings.heightMultiplier() );

        if( settings.versionCheck_sunColor( version ) | settings.versionCheck_sunIntensity( version ) )
            shader.SetSunIntensity( settings.sunColor(), settings.sunIntensity() );

        if( settings.versionCheck_sunRotation( version ) )
            shader.SetSunDirection( settings.sunRotation().toDir() );

        if( settings.versionCheck_renderStyle( version ) )
            shader.SetRenderStyle( settings.renderStyle() );

        if( settings.versionCheck_strataColorPerHeight( version ) )
            shader.SetHeightColorMap( settings.strataColorPerHeight() );
        // color panel

        if( settings.versionCheck_grid( version ) )
        {
            RegenreateGrid();
        }

        if( settings.versionCheck_edit( version ) )
        {
            UpdateHeights();
        }

        shader.draw( *mesh );
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

} // namespace Magnum