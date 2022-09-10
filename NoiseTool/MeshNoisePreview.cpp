#include <algorithm>
#include <cmath>
#include <thread>

#include <Corrade/Containers/ArrayViewStl.h>
#include <Corrade/Utility/Resource.h>
#include <Magnum/GL/GL.h>
#include <Magnum/GL/Texture.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/ImGuiIntegration/Widgets.h>
#include <Magnum/ImageView.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Frustum.h>
#include <Magnum/Math/Functions.h>
#include <Magnum/Math/Intersection.h>
#include <Magnum/Math/Matrix4.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Shaders/Implementation/CreateCompatibilityShader.h>

#include "IconsFontAwesome6.h"
#include "ImGuiExtra.h"
#include "MeshNoisePreview.h"

using namespace Magnum;

#define EQUAL_PREC

#ifdef EQUAL_PREC
constexpr int MaxRenderStyle = 10000;
#else
constexpr int MaxRenderStyle = 16;
#endif

auto msign( Vector2 v )
{
    return Vector2( ( v.x() >= 0.0f ) ? 1.0f : -1.0f,
                    ( v.y() >= 0.0 ) ? 1.0f : -1.0f );
}

auto CompressNormal( Vector3 nor, std::uint32_t CompressionPrec ) -> float
{
    nor /= ( std::abs( nor.x() ) + std::abs( nor.y() ) + std::abs( nor.z() ) );
    nor.xy()  = ( nor.z() >= 0.0 ) ? nor.xy() : ( ( Vector2( 1.0f ) - abs( Vector2( nor.y(), nor.x() ) ) ) * msign( nor.xy() ) );
    Vector2 v = Vector2( 0.5 ) + Vector2( 0.5 ) * nor.xy();

    std::uint32_t mu = ( 1u << CompressionPrec ) - 1u;
    Vector2ui     d  = Vector2ui( floor( clamp( v, -1.0f, 1.0f ) * float( mu ) + Vector2( 0.5 ) ) );
    mu               = ( d.y() << CompressionPrec ) | d.x();
    return *(float*)( &mu );
};

uint32_t packSnorm2x16( Vector2 const& v )
{
    union
    {
        signed short in[2];
        uint32_t     out;
    } u;

    Vector2 result( round( clamp( v, -1.0f, 1.0f ) * 32767.0f ) );

    u.in[0] = result[0];
    u.in[1] = result[1];

    return u.out;
}

auto CompressNormal( Vector3 nor ) -> float
{
    nor /= ( std::abs( nor.x() ) + std::abs( nor.y() ) + std::abs( nor.z() ) );
    nor.xy()   = ( nor.z() >= 0.0 ) ? nor.xy() : ( ( Vector2( 1.0f ) - abs( Vector2( nor.y(), nor.x() ) ) ) * msign( nor.xy() ) );
    auto value = packSnorm2x16( nor.xy() );
    return *(float*)( &value );
};

MeshNoisePreview::MeshNoisePreview() :
    mVoxelShader( Voxel3D {} )
{
    mShader                        = &mMeshShader;
    mBuildData.frequency           = 0.005f;
    mBuildData.seed                = 1338;
    mBuildData.isoSurface          = 0.0f;
    mBuildData.heightmapMultiplier = 100.0f;
    mBuildData.sunIntensity        = 1.0f;
    mBuildData.meshType            = MeshType_Voxel3D;

    uint32_t threadCount = std::max( 2u, std::thread::hardware_concurrency() );

    threadCount -= threadCount / 4;

    for( uint32_t i = 0; i < threadCount; i++ )
    {
        mThreads.emplace_back( GenerateLoopThread, std::ref( mGenerateQueue ), std::ref( mCompleteQueue ) );
    }

    SetupSettingsHandlers();
}

MeshNoisePreview::~MeshNoisePreview()
{
    for( auto& thread: mThreads )
    {
        mGenerateQueue.KillThreads();
        thread.join();
    }
}

void MeshNoisePreview::ReGenerate( FastNoise::SmartNodeArg<> generator )
{
    mRequiresRegen       = true;
    mLoadRange           = 200.0f;
    mBuildData.generator = generator;
    mBuildData.pos       = Vector3i( 0 );

    mMinMax    = {};
    mMinAirY   = INFINITY;
    mMaxSolidY = -INFINITY;

    mRegisteredChunkPositions.clear();
    mChunks.clear();
    mGenerateQueue.Clear();
    mBuildData.genVersion = mCompleteQueue.IncVersion();

    mShader = ( mBuildData.meshType == MeshType_Voxel3D ) ? &mVoxelShader : &mMeshShader;

    Chunk::MeshData meshData;
    while( mCompleteQueue.Pop( meshData ) )
    {
        meshData.Free();
    }
}

void MeshNoisePreview::Draw( const Matrix4& transformation, const Matrix4& projection, const Vector3& cameraPosition, const Vector2i& offset )
{
    if( ImGui::Checkbox( "Generate Mesh Preview", &mEnabled ) )
    {
        ReGenerate( mBuildData.generator );
        ImGuiExtra::MarkSettingsDirty();
    }

    if( !mBuildData.generator || !mEnabled )
    {
        return;
    }

    UpdateChunkQueues( cameraPosition );

    Matrix4 transformationProjection = projection * transformation;

    Frustum camFrustum = Frustum::fromMatrix( transformationProjection );
    mShader->SetTransformationProjectionMatrix( transformationProjection );

    mTriCount              = 0;
    mMeshesCount           = 0;
    uint32_t drawnTriCount = 0;

    for( Chunk& chunk: mChunks )
    {
        if( GL::Mesh* mesh = chunk.GetMesh() )
        {
            int32_t meshTriCount = mesh->count();

            mTriCount += meshTriCount;
            mMeshesCount++;

            bool drawObject = true;
            if( mBuildData.meshType != MeshType_LimitedHeightmap2D )
            {
                Vector3 posf( chunk.GetPos() );
                Range3D bbox( posf, posf + Vector3( Chunk::SIZE + 1 ) );

                if( mBuildData.meshType == MeshType_Heightmap2D || mBuildData.meshType == MeshType_LimitedHeightmap2D )
                {
                    bbox.min().y() = mMinMax.min;
                    bbox.max().y() = mMinMax.max;
                }

                drawObject = Math::Intersection::rangeFrustum( bbox, camFrustum );
            }
            // always draw
            if( drawObject )
            {
                drawnTriCount += meshTriCount;
                mShader->draw( *mesh );
            }
        }
    }
    mTriCount /= 3;

    bool edited = false;
    edited |= ImGui::Combo( "Mesh Type", reinterpret_cast<int*>( &mBuildData.meshType ), MeshTypeStrings );
    edited |= ImGuiExtra::ScrollCombo( reinterpret_cast<int*>( &mBuildData.meshType ), MeshType_Count );

    bool textureChanged = false;
    bool sunChanged     = false;
    if( ImGui::ColorEdit3( ICON_FA_SUN " Color", mBuildData.sunColor.data() ) )
    {
        sunChanged = true;
        ImGuiExtra::MarkSettingsDirty();
    }

    if( ImGui::DragFloat( ICON_FA_SUN " Intensity", &mBuildData.sunIntensity, 0.01f, 0, 100000.f ) )
    {
        sunChanged = true;
        ImGuiExtra::MarkSettingsDirty();
    }

    if( ImGui::DragFloat( ICON_FA_SUN " Rotation (phi)", &mBuildData.sunRotation.phi, 0.5f, 0, 360 ) )
    {
        sunChanged = true;
        ImGuiExtra::MarkSettingsDirty();
    }

    if( ImGui::DragFloat( ICON_FA_SUN " Rotation (theta)", &mBuildData.sunRotation.theta, 0.5f, 0, 180 ) )
    {
        sunChanged = true;
        ImGuiExtra::MarkSettingsDirty();
    }

    if( ImGui::DragInt( "Lighting Style", &mBuildData.compressPrec, 1, 1, MaxRenderStyle ) )
    {
        sunChanged = true;
#ifndef EQUAL_PREC
        edited = true;
#endif
        ImGuiExtra::MarkSettingsDirty();
    }

    if( sunChanged )
    {
        mShader->SetSunDirection( mBuildData.sunRotation.toDir() );
        mShader->SetSunIntensity( mBuildData.sunColor, mBuildData.sunIntensity );
        mShader->SetRenderStyle( mBuildData.compressPrec );
    }

    if( mBuildData.meshType == MeshType_LimitedHeightmap2D )
    {
        // color panel
        int colorIndex = 0;
        for( auto& [color, level, active]: mBuildData.strataColorPerHeight )
        {
            ImGui::PushID( colorIndex++ );
            if( ImGui::ColorEdit3( "", color.data() ) )
            {
                textureChanged = true;
            }
            ImGui::PopID();
            ImGui::SameLine();
            ImGui::SetNextItemWidth( 40 );
            ImGui::PushID( colorIndex++ );
            if( ImGui::DragFloat( "", &level, 0.0005f, 0.0f, 1.0f, "%.3f" ) )
            {
                level          = std::clamp( level, 0.0f, 1.0f );
                textureChanged = true;
            }
            ImGui::PopID();
            ImGui::SameLine();
            ImGui::SetNextItemWidth( 20 );
            ImGui::PushID( colorIndex++ );
            if( ImGui::Button( ICON_FA_DELETE_LEFT ) )
            {
                textureChanged = true;
                active         = false;
            }
            ImGui::PopID();
        }

        if( ImGui::Button( ICON_FA_PLUS " Add Color Layer" ) )
        {
            if( mBuildData.strataColorPerHeight.empty() )
                mBuildData.strataColorPerHeight.emplace_back( Color3( 0.2f, 0.2f, 0.2f ), .0f, true );
            else
                mBuildData.strataColorPerHeight.emplace_back( mBuildData.strataColorPerHeight.back() );
            textureChanged = true;
        }
    }

    if( textureChanged )
    {
        UpdateHeightTexture();
        ImGuiExtra::MarkSettingsDirty();
    }

    edited |= ImGui::DragInt( "Seed", &mBuildData.seed );
    edited |= ImGui::DragFloat( "Frequency", &mBuildData.frequency, 0.0005f, 0, 0, "%.4f" );
    if( mBuildData.meshType == MeshType_LimitedHeightmap2D )
    {
        edited |= ImGui::DragInt( "Planes X", &mBuildData.heightmapPlanes[0] );
        edited |= ImGui::DragInt( "Planes Y", &mBuildData.heightmapPlanes[1] );
        edited |= ImGui::DragInt( "Size X", &mBuildData.heightmapSize[0] );
        edited |= ImGui::DragInt( "Size Y", &mBuildData.heightmapSize[1] );
    }
    if( mBuildData.meshType == MeshType_Heightmap2D || mBuildData.meshType == MeshType_LimitedHeightmap2D )
    {
        if( ImGui::DragFloat( "Heightmap Multiplier", &mBuildData.heightmapMultiplier, 0.5f ) )
            mShader->SetHeightMultiplier( mBuildData.heightmapMultiplier );
    }
    else
    {
        edited |= ImGui::DragFloat( "Iso Surface", &mBuildData.isoSurface, 0.02f );
    }

    edited |= ( mBuildData.meshType == MeshType_LimitedHeightmap2D ) && offset != mBuildData.offset;
    mBuildData.offset = offset;
    if( edited )
    {
        ReGenerate( mBuildData.generator );
        ImGuiExtra::MarkSettingsDirty();
    }

    float triLimitMil = (float)mTriLimit / 1000000.0f;
    if( ImGui::DragFloat( "Triangle Limit", &triLimitMil, 1, 10.0f, 300.0f, "%0.1fM" ) )
    {
        mTriLimit = (uint32_t)( triLimitMil * 1000000 );
        ImGuiExtra::MarkSettingsDirty();
    }

    ImGui::Text( "Triangle Count: %0.1fM (%0.1fM)", mTriCount / 1000000.0f, drawnTriCount / 3000000.0f );
    ImGui::Text( "Voxel Count: %0.1fM", ( mChunks.size() * Chunk::SIZE * Chunk::SIZE * Chunk::SIZE ) / 1000000.0 );
    ImGui::Text( "Loaded Chunks: %zu (%d)", mChunks.size(), mMeshesCount );

    size_t generateCount = mGenerateQueue.Count();
    ImGui::Text( "Meshing Chunks: %zu (%zu)", mRegisteredChunkPositions.size() - mChunks.size() - generateCount, generateCount );
    ImGui::Text( "Chunk Load Range: %0.1f", mLoadRange );
    ImGui::Text( "Generated Min (%0.6f) : Max (%0.6f)", mMinMax.min, mMinMax.max );

    if( mBuildData.meshType != MeshType_Heightmap2D && mBuildData.meshType != MeshType_LimitedHeightmap2D )
    {
        ImGui::Text( "Min Air Y (%0.1f) : Max Solid Y (%0.1f)", mMinAirY, mMaxSolidY );
    }

    ImGui::Text( "Camera Pos: %0.1f, %0.1f, %0.1f", cameraPosition.x(), cameraPosition.y(), cameraPosition.z() );

    if( mBuildData.meshType != MeshType_LimitedHeightmap2D )
        UpdateChunksForPosition( cameraPosition );
    else
        CreateChunksForStaticHeightMap( mRequiresRegen && mEnabled );
    mRequiresRegen = false;
}

float MeshNoisePreview::GetLoadRangeModifier()
{
    return std::min( 0.01f, (float)( 1000 / std::pow( std::min( 1000.0f, mLoadRange ), 1.5 ) ) );
}

void MeshNoisePreview::CreateChunksForStaticHeightMap( bool regen )
{
    if( regen )
    {
        mBuildData.pos.x() = mBuildData.offset.x();
        mBuildData.pos.z() = mBuildData.offset.y();
        mGenerateQueue.Clear();
        mGenerateQueue.Push( mBuildData );
    }
}

void MeshNoisePreview::UpdateChunkQueues( const Vector3& position )
{
    size_t queueCount = mCompleteQueue.Count();

    if( mTriCount > mTriLimit ) // Reduce load range if over tri limit
    {
        mLoadRange = std::max( mLoadRange * ( 1 - GetLoadRangeModifier() ), Chunk::SIZE * 1.5f );
    }

    StartTimer();
    Vector3i chunkPos = Vector3i( position - Vector3( Chunk::SIZE / 2.0f ) );

    size_t newChunks = 0;
    if( queueCount )
    {
        Chunk::MeshData meshData;

        while( GetTimerDurationMs() < 14 && mCompleteQueue.Pop( meshData ) )
        {
            mMinMax << meshData.minMax;
            mMinAirY   = std::min( mMinAirY, meshData.minAirY );
            mMaxSolidY = std::max( mMaxSolidY, meshData.maxSolidY );

            mChunks.emplace_back( meshData );
            newChunks++;
        }
        mAvgNewChunks += ( newChunks - mAvgNewChunks ) * 0.01f;
    }

    std::sort( mChunks.begin(), mChunks.end(),
               [chunkPos]( const Chunk& a, const Chunk& b ) {
                   return ( chunkPos - a.GetPos() ).dot() < ( chunkPos - b.GetPos() ).dot();
               } );


    if( mBuildData.meshType == MeshType_LimitedHeightmap2D )
        return;

    // Unload further chunk if out of load range
    size_t deletedChunks = 0;
    while( !mChunks.empty() )
    {
        Vector3i backChunkPos = mChunks.back().GetPos();
        float    unloadRange  = mLoadRange * 1.1f;
        if( GetTimerDurationMs() < 15 && ( chunkPos - backChunkPos ).dot() > unloadRange * unloadRange )
        {
            mRegisteredChunkPositions.erase( backChunkPos );
            mChunks.pop_back();
            deletedChunks++;
        }
        else
        {
            break;
        }
    }

    // ImGui::Text( " Queued Chunks: %zu", queueCount );
    // ImGui::Text( "    New Chunks: %zu (%0.1f)", newChunks, mAvgNewChunks );
    // ImGui::Text( "Deleted Chunks: %zu", deletedChunks );

    // Increase load range if queue is not full
    if( (double)mTriCount < mTriLimit * 0.85 && ( mRegisteredChunkPositions.size() - mChunks.size() ) < mThreads.size() * mAvgNewChunks )
    {
        mLoadRange = std::min( mLoadRange * ( 1 + GetLoadRangeModifier() ), 3000.0f );
    }
}

void MeshNoisePreview::UpdateChunksForPosition( Vector3 position )
{
    // StartTimer();
    int chunkRange = (int)ceilf( mLoadRange / Chunk::SIZE );

    position -= Vector3( Chunk::SIZE * 0.5f );
    Vector3i positionI = Vector3i( position );

    Vector3i chunkCenter = ( positionI / Chunk::SIZE ) * Chunk::SIZE;

    std::vector<Vector3i> chunkPositions;
    Vector3i              chunkPos;
    int                   loadRangeSq = (int)( mLoadRange * mLoadRange );

    int staggerShift = std::min( 5, (int)( ( loadRangeSq * (int64_t)mLoadRange ) / 1000000000 ) );
    int staggerCount = ( 1 << staggerShift ) - 1;

    for( int x = -chunkRange; x <= chunkRange; x++ )
    {
        if( ( x & staggerCount ) != ( mStaggerCheck & staggerCount ) )
        {
            continue;
        }

        chunkPos.x() = x * Chunk::SIZE + chunkCenter.x();

        for( int y = -chunkRange; y <= chunkRange; y++ )
        {
            if( mBuildData.meshType == MeshType_Heightmap2D || mBuildData.meshType == MeshType_LimitedHeightmap2D )
            {
                positionI.y() = 0;
                chunkPos.y()  = 0;
                y             = chunkRange;
            }
            else
            {
                chunkPos.y() = y * Chunk::SIZE + chunkCenter.y();
            }

            for( int z = -chunkRange; z <= chunkRange; z++ )
            {
                chunkPos.z() = z * Chunk::SIZE + chunkCenter.z();


                if( ( positionI - chunkPos ).dot() <= loadRangeSq &&
                    !mRegisteredChunkPositions.contains( chunkPos ) )
                {
                    chunkPositions.push_back( chunkPos );
                }
            }
        }
    }

    mStaggerCheck++;

    std::sort( chunkPositions.begin(), chunkPositions.end(), [positionI]( const Vector3i& a, const Vector3i& b ) {
        return ( positionI - a ).dot() < ( positionI - b ).dot();
    } );

    for( const Vector3i& pos: chunkPositions )
    {
        mBuildData.pos = pos;
        mRegisteredChunkPositions.insert( pos );

        if( mGenerateQueue.Push( mBuildData ) >= mThreads.size() * 16 )
        {
            break;
        }
    }

    // ImGui::Text( "UpdateChunksForPosition(%d) Ms: %.2f", staggerShift, GetTimerDurationMs() );
}

void MeshNoisePreview::GenerateLoopThread( GenerateQueue<Chunk::BuildData>& generateQueue, CompleteQueue<Chunk::MeshData>& completeQueue )
{
    while( true )
    {
        Chunk::BuildData buildData = generateQueue.Pop();

        if( generateQueue.ShouldKillThread() )
        {
            return;
        }

        Chunk::MeshData meshData = Chunk::BuildMeshData( buildData );

        if( !completeQueue.Push( meshData, buildData.genVersion ) )
        {
            meshData.Free();
        }
    }
}

MeshNoisePreview::Chunk::MeshData MeshNoisePreview::Chunk::BuildMeshData( const BuildData& buildData )
{
    thread_local static FastNoise::Buffer       densityValues( SIZE_GEN * SIZE_GEN * SIZE_GEN );
    thread_local static std::vector<VertexData> vertexData;
    thread_local static std::vector<uint32_t>   indicies;

    vertexData.clear();
    indicies.clear();

    switch( buildData.meshType )
    {
    case MeshType_Voxel3D:
        return BuildVoxel3DMesh( buildData, densityValues, vertexData, indicies );

    case MeshType_Heightmap2D:
        return BuildHeightMap2DMesh( buildData, densityValues, vertexData, indicies );

    case MeshType_LimitedHeightmap2D:
        return BuildHeightMap2DMesh( buildData, vertexData, indicies );

    case MeshType_Count:
        break;
    }

    return MeshData( buildData.pos, {}, vertexData, indicies );
}

MeshNoisePreview::Chunk::MeshData MeshNoisePreview::Chunk::BuildVoxel3DMesh( const BuildData& buildData, FastNoise::Buffer& density, std::vector<VertexData>& vertexData, std::vector<uint32_t>& indicies )
{
    FastNoise::Generator::Context ctx( density, { buildData.pos.x() - 1, buildData.pos.y() - 1, buildData.pos.z() - 1, 0 } );
    buildData.generator->GenUniformGrid3D( ctx,
                                           buildData.pos.x() - 1, buildData.pos.y() - 1, buildData.pos.z() - 1,
                                           SIZE_GEN, SIZE_GEN, SIZE_GEN, buildData.frequency, buildData.seed );
    auto                    densityValues = density.begin();
    FastNoise::OutputMinMax minMax        = ctx.minMax;
    float                   minAir        = INFINITY;
    float                   maxSolid      = -INFINITY;

#if FASTNOISE_CALC_MIN_MAX
    if( minMax.min > buildData.isoSurface )
    {
        minAir = (float)buildData.pos.y();
    }
    else if( minMax.max < buildData.isoSurface )
    {
        maxSolid = (float)buildData.pos.y() - 1.0f + SIZE;
    }
    else
#endif
    {
        Vector3 light = LIGHT_DIR.normalized() * ( 1.0f - AMBIENT_LIGHT ) + Vector3( AMBIENT_LIGHT );

        float xLight = std::abs( light.x() );
        float yLight = std::abs( light.y() );
        float zLight = std::abs( light.z() );

        constexpr int32_t STEP_X = 1;
        constexpr int32_t STEP_Y = SIZE_GEN;
        constexpr int32_t STEP_Z = SIZE_GEN * SIZE_GEN;

        int32_t noiseIdx = STEP_X + STEP_Y + STEP_Z;

        for( uint32_t z = 0; z < SIZE; z++ )
        {
            float zf = z + (float)buildData.pos.z();

            for( uint32_t y = 0; y < SIZE; y++ )
            {
                float yf = y + (float)buildData.pos.y();

                for( uint32_t x = 0; x < SIZE; x++ )
                {
                    float xf = x + (float)buildData.pos.x();

                    if( densityValues[noiseIdx] <= buildData.isoSurface ) // Is Solid?
                    {
                        maxSolid = std::max( yf, maxSolid );

                        if( densityValues[noiseIdx + STEP_X] > buildData.isoSurface ) // Right
                        {
                            AddQuadAO( vertexData, indicies, densityValues, buildData.isoSurface, noiseIdx, STEP_X, STEP_Y, STEP_Z, xLight,
                                       Vector3( xf + 1, yf, zf ), Vector3( xf + 1, yf + 1, zf ), Vector3( xf + 1, yf + 1, zf + 1 ), Vector3( xf + 1, yf, zf + 1 ) );
                        }

                        if( densityValues[noiseIdx - STEP_X] > buildData.isoSurface ) // Left
                        {
                            AddQuadAO( vertexData, indicies, densityValues, buildData.isoSurface, noiseIdx, -STEP_X, -STEP_Y, STEP_Z, 1.0f - xLight,
                                       Vector3( xf, yf + 1, zf ), Vector3( xf, yf, zf ), Vector3( xf, yf, zf + 1 ), Vector3( xf, yf + 1, zf + 1 ) );
                        }

                        if( densityValues[noiseIdx + STEP_Y] > buildData.isoSurface ) // Up
                        {
                            AddQuadAO( vertexData, indicies, densityValues, buildData.isoSurface, noiseIdx, STEP_Y, STEP_Z, STEP_X, yLight,
                                       Vector3( xf, yf + 1, zf ), Vector3( xf, yf + 1, zf + 1 ), Vector3( xf + 1, yf + 1, zf + 1 ), Vector3( xf + 1, yf + 1, zf ) );
                        }

                        if( densityValues[noiseIdx - STEP_Y] > buildData.isoSurface ) // Down
                        {
                            AddQuadAO( vertexData, indicies, densityValues, buildData.isoSurface, noiseIdx, -STEP_Y, -STEP_Z, STEP_X, 1.0f - yLight,
                                       Vector3( xf, yf, zf + 1 ), Vector3( xf, yf, zf ), Vector3( xf + 1, yf, zf ), Vector3( xf + 1, yf, zf + 1 ) );
                        }

                        if( densityValues[noiseIdx + STEP_Z] > buildData.isoSurface ) // Forward
                        {
                            AddQuadAO( vertexData, indicies, densityValues, buildData.isoSurface, noiseIdx, STEP_Z, STEP_X, STEP_Y, zLight,
                                       Vector3( xf, yf, zf + 1 ), Vector3( xf + 1, yf, zf + 1 ), Vector3( xf + 1, yf + 1, zf + 1 ), Vector3( xf, yf + 1, zf + 1 ) );
                        }

                        if( densityValues[noiseIdx - STEP_Z] > buildData.isoSurface ) // Back
                        {
                            AddQuadAO( vertexData, indicies, densityValues, buildData.isoSurface, noiseIdx, -STEP_Z, -STEP_X, STEP_Y, 1.0f - zLight,
                                       Vector3( xf + 1, yf, zf ), Vector3( xf, yf, zf ), Vector3( xf, yf + 1, zf ), Vector3( xf + 1, yf + 1, zf ) );
                        }
                    }
                    else
                    {
                        minAir = std::min( yf, minAir );
                    }
                    noiseIdx++;
                }

                noiseIdx += STEP_X * 2;
            }

            noiseIdx += STEP_Y * 2;
        }
    }

    return MeshData( buildData.pos, minMax, vertexData, indicies, minAir, maxSolid );
}

void MeshNoisePreview::Chunk::AddQuadAO( std::vector<VertexData>& verts, std::vector<uint32_t>& indicies, const float* density, float isoSurface,
                                         int32_t idx, int32_t facingOffset, int32_t offsetA, int32_t offsetB, float light, Vector3 pos00, Vector3 pos01, Vector3 pos11, Vector3 pos10 )
{
    int32_t facingIdx = idx + facingOffset;

    int32_t sideA0 = density[facingIdx - offsetA] <= isoSurface;
    int32_t sideA1 = density[facingIdx + offsetA] <= isoSurface;
    int32_t sideB0 = density[facingIdx - offsetB] <= isoSurface;
    int32_t sideB1 = density[facingIdx + offsetB] <= isoSurface;

    int32_t corner00 = ( sideA0 & sideB0 ) || density[facingIdx - offsetA - offsetB] <= isoSurface;
    int32_t corner01 = ( sideA0 & sideB1 ) || density[facingIdx - offsetA + offsetB] <= isoSurface;
    int32_t corner10 = ( sideA1 & sideB0 ) || density[facingIdx + offsetA - offsetB] <= isoSurface;
    int32_t corner11 = ( sideA1 & sideB1 ) || density[facingIdx + offsetA + offsetB] <= isoSurface;

    constexpr float aoAdjust = AO_STRENGTH / 3.0f;

    float ao00 = (float)( sideA0 + sideB0 + corner00 ) * aoAdjust;
    float ao01 = (float)( sideA1 + sideB0 + corner10 ) * aoAdjust;
    float ao10 = (float)( sideA0 + sideB1 + corner01 ) * aoAdjust;
    float ao11 = (float)( sideA1 + sideB1 + corner11 ) * aoAdjust;

    float densityLightShift = 1 - ( isoSurface - density[idx] ) * 2;
    light *= densityLightShift * densityLightShift;

    uint32_t vertIdx = (uint32_t)verts.size();
    verts.emplace_back( pos00, ( 1.0f - ao00 ) * light );
    verts.emplace_back( pos01, ( 1.0f - ao01 ) * light );
    verts.emplace_back( pos10, ( 1.0f - ao10 ) * light );
    verts.emplace_back( pos11, ( 1.0f - ao11 ) * light );

    // Rotate tris to give best visuals for AO lighting
    uint32_t triRotation = ( ao00 + ao11 > ao01 + ao10 ) * 2;
    indicies.push_back( vertIdx );
    indicies.push_back( vertIdx + 3 - triRotation );
    indicies.push_back( vertIdx + 2 );
    indicies.push_back( vertIdx + 3 );
    indicies.push_back( vertIdx + triRotation );
    indicies.push_back( vertIdx + 1 );
}

MeshNoisePreview::Chunk::MeshData MeshNoisePreview::Chunk::BuildHeightMap2DMesh( const BuildData& buildData, std::vector<VertexData>& vertexData, std::vector<uint32_t>& indicies )
{


    int                           sizeX   = std::max( 1, buildData.heightmapSize[0] );
    int                           sizeY   = std::max( 1, buildData.heightmapSize[1] );
    int                           nbVertX = sizeX + 1;
    int                           nbVertY = sizeY + 1;
    FastNoise::Buffer             density( 32, nbVertX * nbVertY );
    FastNoise::Generator::Context ctx( density, { buildData.offset.x(), buildData.offset.y(), 0, 0 } );
    FastNoise::OutputMinMax       minMax;
    ctx.totalPlanes[0] = buildData.heightmapPlanes[0];
    ctx.totalPlanes[1] = buildData.heightmapPlanes[1];
    for( int py = 0; py < buildData.heightmapPlanes[1]; ++py )
    {
        ctx.planeId[1] = py;
        for( int px = 0; px < buildData.heightmapPlanes[0]; ++px )
        {
            auto offset    = buildData.offset + Magnum::Vector2i( px * sizeX, (float)py * sizeY );
            ctx.planeId[0] = px;
            buildData.generator->GenUniformGrid2D( ctx,
                                                   offset.x(), offset.y(),
                                                   nbVertX, nbVertY, buildData.frequency, buildData.seed );
            auto densityValues = density.begin();

            minMax << ctx.minMax;
            int32_t STEP_X = 1;
            int32_t STEP_Y = nbVertX;

            // Vector3 sunLight = LIGHT_DIR.normalized() * ( 1.0f - AMBIENT_LIGHT ) + Vector3( AMBIENT_LIGHT );

            int32_t noiseIdx = 0;

            for( int32_t y = 0; y < buildData.heightmapSize[1]; y++ )
            {
                float yf = ( y + offset.y() );

                for( int32_t x = 0; x < buildData.heightmapSize[0]; x++ )
                {
                    float   xf = x + offset.x();
                    Vector3 v00( xf, densityValues[noiseIdx], yf );
                    Vector3 v01( xf, densityValues[noiseIdx + STEP_Y], yf + 1 );
                    Vector3 v10( xf + 1, densityValues[noiseIdx + STEP_X], yf );
                    Vector3 v11( xf + 1, densityValues[noiseIdx + STEP_X + STEP_Y], yf + 1 );

                    uint32_t triRotation = 2 * ( ( v00 + v11 ).dot() < ( v01 + v10 ).dot() );

                    // Normal for quad
                    Vector3 normal[4] = {};
                    normal[0] += Math::cross( v10 - v11, v00 - v11 ).normalized();
                    normal[3 - triRotation] += normal[0];
                    normal[2] += normal[0];
                    normal[3] += Math::cross( v01 - v00, v11 - v00 ).normalized();
                    normal[triRotation] += normal[3];
                    normal[1] += normal[3];

#ifdef HAS_TRUE_NORMAL
                    uint32_t vertIdx = (uint32_t)vertexData.size();
                    vertexData.emplace_back( v00, CompressNormal( normal[0].normalized(), buildData.compressPrec ), normal[0].normalized() );
                    vertexData.emplace_back( v01, CompressNormal( normal[1].normalized(), buildData.compressPrec ), normal[1].normalized() );
                    vertexData.emplace_back( v10, CompressNormal( normal[2].normalized(), buildData.compressPrec ), normal[2].normalized() );
                    vertexData.emplace_back( v11, CompressNormal( normal[3].normalized(), buildData.compressPrec ), normal[3].normalized() );

#else
                    uint32_t vertIdx = (uint32_t)vertexData.size();
#ifdef EQUAL_PREC
                    vertexData.emplace_back( v00, CompressNormal( normal[0].normalized() ) );
                    vertexData.emplace_back( v01, CompressNormal( normal[1].normalized() ) );
                    vertexData.emplace_back( v10, CompressNormal( normal[2].normalized() ) );
                    vertexData.emplace_back( v11, CompressNormal( normal[3].normalized() ) );
#else
                    vertexData.emplace_back( v00, CompressNormal( normal[0].normalized(), buildData.compressPrec ) );
                    vertexData.emplace_back( v01, CompressNormal( normal[1].normalized(), buildData.compressPrec ) );
                    vertexData.emplace_back( v10, CompressNormal( normal[2].normalized(), buildData.compressPrec ) );
                    vertexData.emplace_back( v11, CompressNormal( normal[3].normalized(), buildData.compressPrec ) );
#endif

#endif
                    // Slice quad along longest split

                    indicies.push_back( vertIdx );
                    indicies.push_back( vertIdx + 3 - triRotation );
                    indicies.push_back( vertIdx + 2 );
                    indicies.push_back( vertIdx + 3 );
                    indicies.push_back( vertIdx + triRotation );
                    indicies.push_back( vertIdx + 1 );

                    noiseIdx++;
                }

                noiseIdx += STEP_X;
            }
        }
    }

    return MeshData( buildData.pos, minMax, vertexData, indicies );
}

MeshNoisePreview::Chunk::MeshData MeshNoisePreview::Chunk::BuildHeightMap2DMesh( const BuildData& buildData, FastNoise::Buffer& density, std::vector<VertexData>& vertexData, std::vector<uint32_t>& indicies )
{
    constexpr uint32_t            SIZE_GEN_HEIGHTMAP = SIZE + 1;
    FastNoise::Generator::Context ctx( density, { buildData.pos.x(), buildData.pos.z(), 0, 0 } );
    auto                          densityValues = density.begin();

    buildData.generator->GenUniformGrid2D( ctx,
                                           buildData.pos.x(), buildData.pos.z(),
                                           SIZE_GEN_HEIGHTMAP, SIZE_GEN_HEIGHTMAP, buildData.frequency, buildData.seed );
    FastNoise::OutputMinMax minMax = ctx.minMax;
    constexpr int32_t       STEP_X = 1;
    constexpr int32_t       STEP_Y = SIZE_GEN_HEIGHTMAP;

    Vector3 sunLight = LIGHT_DIR.normalized() * ( 1.0f - AMBIENT_LIGHT ) + Vector3( AMBIENT_LIGHT );

    int32_t noiseIdx = 0;

    for( uint32_t y = 0; y < SIZE; y++ )
    {
        float yf = y + (float)buildData.pos.z();

        for( uint32_t x = 0; x < SIZE; x++ )
        {
            float xf = x + (float)buildData.pos.x();

            Vector3 v00( xf, densityValues[noiseIdx], yf );
            Vector3 v01( xf, densityValues[noiseIdx + STEP_Y], yf + 1 );
            Vector3 v10( xf + 1, densityValues[noiseIdx + STEP_X], yf );
            Vector3 v11( xf + 1, densityValues[noiseIdx + STEP_X + STEP_Y], yf + 1 );

            uint32_t triRotation = 2 * ( ( v00 + v11 ).dot() < ( v01 + v10 ).dot() );

            // Normal for quad
            // Normal for quad
            Vector3 normal[4] = {};
            normal[0] += Math::cross( v10 - v11, v00 - v11 ).normalized();
            normal[3 - triRotation] += normal[0];
            normal[2] += normal[0];
            normal[3] += Math::cross( v01 - v00, v11 - v00 ).normalized();
            normal[triRotation] += normal[3];
            normal[1] += normal[3];

            uint32_t vertIdx = (uint32_t)vertexData.size();
            vertexData.emplace_back( v00, CompressNormal( normal[0].normalized(), buildData.compressPrec ) );
            vertexData.emplace_back( v01, CompressNormal( normal[1].normalized(), buildData.compressPrec ) );
            vertexData.emplace_back( v10, CompressNormal( normal[2].normalized(), buildData.compressPrec ) );
            vertexData.emplace_back( v11, CompressNormal( normal[3].normalized(), buildData.compressPrec ) );

            // Slice quad along longest split
            indicies.push_back( vertIdx );
            indicies.push_back( vertIdx + 3 - triRotation );
            indicies.push_back( vertIdx + 2 );
            indicies.push_back( vertIdx + 3 );
            indicies.push_back( vertIdx + triRotation );
            indicies.push_back( vertIdx + 1 );

            noiseIdx++;
        }

        noiseIdx += STEP_X;
    }

    return MeshData( buildData.pos, minMax, vertexData, indicies );
}

MeshNoisePreview::Chunk::Chunk( MeshData& meshData )
{
    mPos = meshData.pos;

    if( !meshData.vertexData.empty() )
    {
        // https://doc.magnum.graphics/magnum/classMagnum_1_1GL_1_1Mesh.html

        mMesh = std::make_unique<GL::Mesh>( GL::MeshPrimitive::Triangles );

#ifdef HAS_TRUE_NORMAL
        mMesh->addVertexBuffer( GL::Buffer( GL::Buffer::TargetHint::Array, meshData.vertexData ), 0, VertexLightShader::PositionLight {}, VertexLightShader::Normal {} );
#else
        mMesh->addVertexBuffer( GL::Buffer( GL::Buffer::TargetHint::Array, meshData.vertexData ), 0, VertexLightShader::PositionLight {} );
#endif
        if( meshData.indicies.empty() )
        {
            mMesh->setCount( (int)meshData.vertexData.size() );
        }
        else
        {
            mMesh->setCount( (Int)meshData.indicies.size() );
            mMesh->setIndexBuffer( GL::Buffer( GL::Buffer::TargetHint::ElementArray, meshData.indicies ), 0, GL::MeshIndexType::UnsignedInt, 0, (UnsignedInt)meshData.vertexData.size() - 1 );
        }
    }

    meshData.Free();
}

MeshNoisePreview::VertexLightShader::VertexLightShader( Voxel3D )
{
    ContinueDefaultBuild( Type::Default );
}

MeshNoisePreview::VertexLightShader::VertexLightShader()
{
    ContinueDefaultBuild( Type::CompressedNormals );
}

void MeshNoisePreview::VertexLightShader::ContinueDefaultBuild( Type type )
{
    Utility::Resource noiseToolResources( "NoiseTool" );

#ifndef MAGNUM_TARGET_GLES
    const GL::Version version = GL::Context::current().supportedVersion( { GL::Version::GL320, GL::Version::GL310, GL::Version::GL300, GL::Version::GL210 } );
#else
    const GL::Version version = GL::Context::current().supportedVersion( { GL::Version::GLES300, GL::Version::GLES200 } );
#endif

    GL::Shader vert = CreateShader( version, GL::Shader::Type::Vertex, type );
    GL::Shader frag = CreateShader( version, GL::Shader::Type::Fragment, type );

    CORRADE_INTERNAL_ASSERT_OUTPUT(
        vert.addSource( noiseToolResources.get( "VertexLight.vert" ) ).compile() );
    CORRADE_INTERNAL_ASSERT_OUTPUT(
        frag.addSource( noiseToolResources.get( "VertexLight.frag" ) ).compile() );

    attachShader( vert );
    attachShader( frag );
    /* ES3 has this done in the shader directly */
#if !defined( MAGNUM_TARGET_GLES ) || defined( MAGNUM_TARGET_GLES2 )
#ifndef MAGNUM_TARGET_GLES
    if( !GL::Context::current().isExtensionSupported<GL::Extensions::ARB::explicit_attrib_location>( version ) )
#endif
    {
        bindAttributeLocation( PositionLight::Location, "positionLight" );
    }
#endif

    CORRADE_INTERNAL_ASSERT_OUTPUT( link() );

#ifndef MAGNUM_TARGET_GLES
    if( !GL::Context::current().isExtensionSupported<GL::Extensions::ARB::explicit_uniform_location>( version ) )
#endif
    {
        mTransformationProjectionMatrixUniform = uniformLocation( "transformationProjectionMatrix" );
        mHeightColorMapUniform                 = uniformLocation( "heightColorMap" );
        mHeightMultiplierUniform               = uniformLocation( "heightMultiplier" );
        mSunColor                              = uniformLocation( "sunColor" );
        mSunDirection                          = uniformLocation( "sunDirection" );
        mCompressSpec                          = uniformLocation( "compressSpec" );
    }

    /* Set defaults in OpenGL ES (for desktop they are set in shader code itself) */
#ifdef MAGNUM_TARGET_GLES
    SetTransformationProjectionMatrix( Matrix4 {} );
    SetColorTint( Color3 { 1.0f } );
#endif

    std::array<std::uint32_t, MaxHeightmapColorMapRes> data;
    data.fill( 0xfcfcfcff );
    ImageView1D heightMapImage( PixelFormat::RGBA8Unorm, { MaxHeightmapColorMapRes }, data );

    mHeightColors = GL::Texture1D();
    mHeightColors.setStorage( 1, GL::TextureFormat::RGBA8, MaxHeightmapColorMapRes )
        .setSubImage( 0, {}, heightMapImage );
    mHeightColors.bind( 0 );
    setUniform( mHeightMultiplierUniform, 0 );
}

GL::Shader MeshNoisePreview::VertexLightShader::CreateShader( GL::Version version, GL::Shader::Type type, Type iType )
{
    GL::Shader shader( version, type );

#ifndef MAGNUM_TARGET_GLES
    if( GL::Context::current().isExtensionDisabled<GL::Extensions::ARB::explicit_attrib_location>( version ) )
        shader.addSource( "#define DISABLE_GL_ARB_explicit_attrib_location\n" );
    if( GL::Context::current().isExtensionDisabled<GL::Extensions::ARB::shading_language_420pack>( version ) )
        shader.addSource( "#define DISABLE_GL_ARB_shading_language_420pack\n" );
    if( GL::Context::current().isExtensionDisabled<GL::Extensions::ARB::explicit_uniform_location>( version ) )
        shader.addSource( "#define DISABLE_GL_ARB_explicit_uniform_location\n" );
#endif

#ifndef MAGNUM_TARGET_GLES2
    if( type == GL::Shader::Type::Vertex && GL::Context::current().isExtensionDisabled<GL::Extensions::MAGNUM::shader_vertex_id>( version ) )
        shader.addSource( "#define DISABLE_GL_MAGNUM_shader_vertex_id\n" );
#endif

/* My Android emulator (running on NVidia) doesn't define GL_ES
       preprocessor macro, thus *all* the stock shaders fail to compile */
/** @todo remove this when Android emulator is sane */
#ifdef CORRADE_TARGET_ANDROID
    shader.addSource( "#ifndef GL_ES\n#define GL_ES 1\n#endif\n" );
#endif

    if( iType == Type::CompressedNormals )
        shader.addSource( "#define HAS_COMPRESSED_NORMALS\n" );
#ifdef EQUAL_PREC
    shader.addSource( "#define EQUAL_PREC\n" );
#endif
#ifdef HAS_TRUE_NORMAL
    if( iType == Type::CompressedNormals )
        shader.addSource( "#define HAS_TRUE_NORMAL\n" );
#endif

    return shader;
}

MeshNoisePreview::VertexLightShader& MeshNoisePreview::VertexLightShader::SetTransformationProjectionMatrix( const Matrix4& matrix )
{
    setUniform( mTransformationProjectionMatrixUniform, matrix );
    return *this;
}

MeshNoisePreview::VertexLightShader& MeshNoisePreview::VertexLightShader::SetHeightMultiplier( float iHeightMul )
{
    setUniform( mHeightMultiplierUniform, iHeightMul );
    return *this;
}

MeshNoisePreview::VertexLightShader& MeshNoisePreview::VertexLightShader::SetSunIntensity( Color3 color, float intensity )
{
    setUniform( mSunColor, Vector4( color, intensity ) );
    return *this;
}

MeshNoisePreview::VertexLightShader& MeshNoisePreview::VertexLightShader::SetSunDirection( Vector3 direction )
{
    setUniform( mSunDirection, -direction );
    return *this;
}

MeshNoisePreview::VertexLightShader& MeshNoisePreview::VertexLightShader::SetRenderStyle( int spec )
{
#ifdef EQUAL_PREC
    setUniform( mCompressSpec, (float)spec );
#else
    setUniform( mCompressSpec, (uint32_t)spec );
#endif
    return *this;
}

MeshNoisePreview::VertexLightShader& MeshNoisePreview::VertexLightShader::SetHeightColorMap( ColorLayerValue const& colorMap )
{
    std::array<std::uint32_t, MaxHeightmapColorMapRes> data;

    if( !colorMap.size() )
    {
        data.fill( 0xfcfcfcff );
        ImageView1D heightMapImage( PixelFormat::RGBA8Srgb, { MaxHeightmapColorMapRes }, data );

        mHeightColors.bind( 0 );
        mHeightColors.setSubImage( 0, {}, heightMapImage );
    }
    else
    {
        auto copy = colorMap;
        std::sort( copy.begin(), copy.end(), []( auto const& first, auto const& second ) {
            return std::get<2>( first ) < std::get<2>( second );
        } );

        std::uint32_t index     = 0;
        std::uint32_t lastColor = {};
        for( auto& l: copy )
        {
            lastColor = Color4( std::get<0>( l ) ).toSrgbAlphaInt();
            auto lt   = ( std::uint32_t )( std::get<1>( l ) * ( MaxHeightmapColorMapRes ) );
            while( index < MaxHeightmapColorMapRes && index < lt )
                data[index++] = lastColor;
        }
        while( index < MaxHeightmapColorMapRes )
            data[index++] = lastColor;

        ImageView1D heightMapImage( PixelFormat::RGBA8Srgb, { MaxHeightmapColorMapRes }, data );
        mHeightColors.bind( 0 );
        mHeightColors.setSubImage( 0, {}, heightMapImage );
    }
    return *this;
}

void MeshNoisePreview::StartTimer()
{
    mTimerStart = std::chrono::high_resolution_clock::now();
}

float MeshNoisePreview::GetTimerDurationMs()
{
    return std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::high_resolution_clock::now() - mTimerStart ).count() / 1e3f;
}

void MeshNoisePreview::SetupSettingsHandlers()
{
    ImGuiSettingsHandler editorSettings;
    editorSettings.TypeName   = "NoiseToolMeshNoisePreview";
    editorSettings.TypeHash   = ImHashStr( editorSettings.TypeName );
    editorSettings.UserData   = this;
    editorSettings.WriteAllFn = []( ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* outBuf ) {
        auto* meshNoisePreview = (MeshNoisePreview*)handler->UserData;
        outBuf->appendf( "\n[%s][Settings]\n", handler->TypeName );

        outBuf->appendf( "tri_limit=%d\n", (int)meshNoisePreview->mTriLimit );
        outBuf->appendf( "frequency=%f\n", meshNoisePreview->mBuildData.frequency );
        outBuf->appendf( "iso_surface=%f\n", meshNoisePreview->mBuildData.isoSurface );
        outBuf->appendf( "heightmap_multiplier=%f\n", meshNoisePreview->mBuildData.heightmapMultiplier );
        outBuf->appendf( "seed=%d\n", meshNoisePreview->mBuildData.seed );
        outBuf->appendf( "color=%d\n", (int)meshNoisePreview->mBuildData.sunColor.toSrgbInt() );
        outBuf->appendf( "mesh_type=%d\n", (int)meshNoisePreview->mBuildData.meshType );
        outBuf->appendf( "enabled=%d\n", (int)meshNoisePreview->mEnabled );
        outBuf->appendf( "planes=%d:%d\n", meshNoisePreview->mBuildData.heightmapPlanes[0], meshNoisePreview->mBuildData.heightmapPlanes[1] );
        outBuf->appendf( "size=%d:%d\n", meshNoisePreview->mBuildData.heightmapSize[0], meshNoisePreview->mBuildData.heightmapSize[1] );
        outBuf->appendf( "sun_rotation=%f:%f\n", meshNoisePreview->mBuildData.sunRotation.theta, meshNoisePreview->mBuildData.sunRotation.phi );
        outBuf->appendf( "draw_style=%d\n", meshNoisePreview->mBuildData.compressPrec );
        outBuf->appendf( "sun_intensity=%f\n", meshNoisePreview->mBuildData.sunIntensity );
        for( auto& [color, level, active]: meshNoisePreview->mBuildData.strataColorPerHeight )
        {
            if( active )
            {
                outBuf->appendf( "strata_color=%d:%d\n", (int)color.toSrgbInt(), (int)( MaxHeightmapColorMapRes * level ) );
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
    editorSettings.ReadLineFn = []( ImGuiContext* ctx, ImGuiSettingsHandler* handler, void* entry, const char* line ) {
        auto* meshNoisePreview = (MeshNoisePreview*)handler->UserData;

        sscanf( line, "tri_limit=%d", &meshNoisePreview->mTriLimit );
        sscanf( line, "frequency=%f", &meshNoisePreview->mBuildData.frequency );
        sscanf( line, "iso_surface=%f", &meshNoisePreview->mBuildData.isoSurface );
        sscanf( line, "heightmap_multiplier=%f", &meshNoisePreview->mBuildData.heightmapMultiplier );
        sscanf( line, "seed=%d", &meshNoisePreview->mBuildData.seed );
        sscanf( line, "mesh_type=%d", (int*)&meshNoisePreview->mBuildData.meshType );
        sscanf( line, "planes=%d:%d", meshNoisePreview->mBuildData.heightmapPlanes, meshNoisePreview->mBuildData.heightmapPlanes + 1 );
        sscanf( line, "size=%d:%d", meshNoisePreview->mBuildData.heightmapSize, meshNoisePreview->mBuildData.heightmapSize + 1 );
        sscanf( line, "sun_rotation=%f:%f", &meshNoisePreview->mBuildData.sunRotation.theta, &meshNoisePreview->mBuildData.sunRotation.phi );
        sscanf( line, "draw_style=%d", &meshNoisePreview->mBuildData.compressPrec );
        sscanf( line, "sun_intensity=%f", &meshNoisePreview->mBuildData.sunIntensity );

        int i = 0;
        int l = 0;
        if( sscanf( line, "color=%d", &i ) == 1 )
        {
            meshNoisePreview->mBuildData.sunColor = Color3::fromSrgb( i );
        }
        else if( sscanf( line, "enabled=%d", &i ) == 1 )
        {
            meshNoisePreview->mEnabled = i;
        }
        else if( sscanf( line, "strata_color=%d:%d", &i, &l ) == 2 )
        {
            meshNoisePreview->mBuildData.strataColorPerHeight.emplace_back( Color3::fromSrgb( i ), (float)l / (float)MaxHeightmapColorMapRes, true );
        }
    };
    editorSettings.ApplyAllFn = []( ImGuiContext* ctx, ImGuiSettingsHandler* handler ) {
        auto* meshNoisePreview = (MeshNoisePreview*)handler->UserData;
        meshNoisePreview->mShader->SetHeightMultiplier( meshNoisePreview->mBuildData.heightmapMultiplier );
        meshNoisePreview->mShader->SetSunIntensity( meshNoisePreview->mBuildData.sunColor, meshNoisePreview->mBuildData.sunIntensity );
        meshNoisePreview->mShader->SetHeightColorMap( meshNoisePreview->mBuildData.strataColorPerHeight );
        meshNoisePreview->mShader->SetRenderStyle( meshNoisePreview->mBuildData.compressPrec );
        meshNoisePreview->mShader->SetSunDirection( meshNoisePreview->mBuildData.sunRotation.toDir() );
    };
    ImGuiExtra::AddOrReplaceSettingsHandler( editorSettings );
}

void MeshNoisePreview::UpdateHeightTexture()
{
    if( mBuildData.strataColorPerHeight.size() > 0 )
    {
        auto beg = mBuildData.strataColorPerHeight.begin();
        while( beg != mBuildData.strataColorPerHeight.end() )
        {
            if( !std::get<2>( *beg ) )
                beg = mBuildData.strataColorPerHeight.erase( beg );
            else
                beg++;
        }
    }
    mShader->SetHeightColorMap( mBuildData.strataColorPerHeight );
}
