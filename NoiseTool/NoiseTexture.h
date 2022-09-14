#pragma once
#include <array>
#include <atomic>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <Magnum/GL/GL.h>
#include <Magnum/GL/Texture.h>
#include <Magnum/ImageView.h>
#include <Magnum/Magnum.h>
#include <Magnum/Math/Vector4.h>

#include "FastNoise/FastNoise.h"
#include "MultiThreadQueues.h"

namespace Magnum
{
    class FastNoiseNodeEditor;
    class NoiseTexture
    {
    public:
        enum GenType
        {
            GenType_2D,
            GenType_Count
        };

        inline static const char* GenTypeStrings = "2D\0"
                                                   "2D Tiled\0"
                                                   "3D Slice\0"
                                                   "4D Slice\0";

        NoiseTexture();
        ~NoiseTexture();

        void Draw( FastNoiseNodeEditor* iParent = nullptr );
        void ReGenerate( FastNoise::SmartNodeArg<> generator );
        void SetName( std::string name )
        {
            mName = name;
        }

        std::string& GetName()
        {
            return mName;
        }

        Vector2i GetOffset2D() const
        {
            return Vector2i( (int)mBuildData.offset.x(), (int)mBuildData.offset.y() );
        }

    private:
        struct BuildData
        {
            FastNoise::SmartNode<const FastNoise::Generator> generator;
            Vector2i                                         size;
            Vector2i                                         numberOfPlanes = Vector2i( 1, 1 );
            Vector2i                                         plane;
            Vector4                                          offset;
            float                                            frequency;
            int32_t                                          seed;
            uint64_t                                         iteration;
            GenType                                          generationType;
            std::string                                      path;
            std::string                                      name;
        };

        struct TextureData
        {
            TextureData() = default;

            TextureData( uint64_t iter, Vector2i s, FastNoise::OutputMinMax mm, FastNoise::Buffer const& v ) :
                minMax( mm ), size( s ), iteration( iter )
            {
                copy = FastNoise::Buffer( v.size );
                std::memcpy( copy.begin(), v.begin(), v.nbbytes() );
                textureData = { (uint32_t*)copy.begin(), v.size };
            }

            TextureData( TextureData&& other ) :
                copy( std::move( other.copy ) ), minMax( other.minMax ), size( other.size ),
                iteration( other.iteration )
            {
                textureData = { (uint32_t*)copy.begin(), copy.size };
            }

            TextureData& operator=( TextureData&& other )

            {
                copy        = std::move( other.copy );
                minMax      = other.minMax;
                size        = other.size;
                iteration   = other.iteration;
                textureData = { (uint32_t*)copy.begin(), copy.size };
                return *this;
            }


            TextureData( TextureData const& other ) :
                copy( other.copy ), minMax( other.minMax ), size( other.size ), iteration( other.iteration )
            {
                textureData = { (uint32_t*)copy.begin(), copy.size };
            }

            TextureData& operator=( TextureData const& other )

            {
                copy        = other.copy;
                minMax      = other.minMax;
                size        = other.size;
                iteration   = other.iteration;
                textureData = { (uint32_t*)copy.begin(), copy.size };
                return *this;
            }

            void Free()
            {
            }

            FastNoise::Buffer               copy;
            Containers::ArrayView<uint32_t> textureData;
            FastNoise::OutputMinMax         minMax;
            Vector2i                        size;
            uint64_t                        iteration;
        };

        template<typename Wrapper>
        static TextureData BuildTexture( const BuildData& buildData, Magnum::Vector4 offset );
        static void        BuildTerrainDataRAW( std::vector<std::uint16_t>& buffer, const BuildData& buildData,
                                                Magnum::Vector4 offset );
        static void        BuildTerrainDataRAW( std::vector<std::uint8_t>& buffer, const BuildData& buildData,
                                                Magnum::Vector4 offset );
        static void        GenerateLoopThread( GenerateQueue<BuildData>&   generateQueue,
                                               CompleteQueue<TextureData>& completeQueue );

        void DoExport( Vector2i gridSize );
        void DoExportRAW();
        void DoExportBMP();
        void DoExportPNG();
        void SetupSettingsHandlers();
        void SetPreviewTexture( ImageView2D& imageView );

        bool            mDisableGrid = false;
        std::string     mName;
        std::atomic_int mExportProgress = 0;
        std::string     mStatus;
        GL::Texture2D   mNoiseTexture;
        uint64_t        mCurrentIteration = 0;

        BuildData               mBuildData;
        BuildData               mExportBuildData;
        FastNoise::OutputMinMax mMinMax;

        std::thread                mExportThread;
        std::vector<std::thread>   mThreads;
        GenerateQueue<BuildData>   mGenerateQueue;
        CompleteQueue<TextureData> mCompleteQueue;
    };
} // namespace Magnum