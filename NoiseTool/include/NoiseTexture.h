#pragma once
#include <array>
#include <atomic>
#include <cstring>
#include <future>
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
#include "Worker.h"


namespace Magnum
{
    class FastNoiseNodeEditor;
    class NoiseTexture
    {
    public:
        NoiseTexture();
        ~NoiseTexture();

        void Draw( FastNoiseNodeEditor& iParent );
        void SetGenerator( FastNoise::SmartNodeArg<> generator )
        {
            mBuildData.generator = generator;
            mRegenerate          = true;
        }
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
        struct BaseBuildData
        {
            FastNoise::SmartNode<const FastNoise::Generator> generator;
            Vector2i                                         gridSize;
            float                                            frequency;
            int32_t                                          seed;
        };

        struct ExportData : BaseBuildData
        {
            Vector2i    gridStart = Vector2i( 0, 0 );
            Vector2i    gridCount = Vector2i( 1, 1 );
            std::string path;
            std::string name;

            inline ExportData& operator=( BaseBuildData& data )
            {
                *(BaseBuildData*)this = data;
                return *this;
            }
        };

        struct BuildData : BaseBuildData
        {
            mutable FastNoise::Buffer texBuffer;
            Vector2i                  size;
            Vector2                   offset;
            uint64_t                  iteration;
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
        static TextureData BuildTexture( FastNoise::SmartNode<const FastNoise::Generator> generator, uint64_t iter,
                                         FastNoise::Buffer&, Magnum::Vector2i gridSize, Magnum::Vector2i size,
                                         Magnum::Vector2i offset, float freq, int seed );
        static void        BuildTerrainDataRAW( std::vector<std::uint16_t>& buffer, FastNoise::Buffer&,
                                                const ExportData& buildData, Magnum::Vector2i offset );

        void DoExport( Vector2i gridSize );
        void DoExportPNG();
        void SetupSettingsHandlers();
        void SetPreviewTexture( ImageView2D& imageView );

        bool mDisableGrid = false;
        bool mRegenerate  = false;
        bool mHasTexture  = false;

        std::string     mName           = "unnamed";
        std::atomic_int mExportProgress = 0;
        std::string     mStatus;
        GL::Texture2D   mNoiseTexture;
        uint64_t        mCurrentIteration = 0;

        BuildData  mBuildData;
        ExportData mExportBuildData;

        FastNoise::OutputMinMax mMinMax;

        std::future<TextureData> mTexData;
        std::future<void>        mExportTask;
        // std::vector<std::thread>   mThreads;
        // GenerateQueue<BuildData>   mGenerateQueue;
        // CompleteQueue<TextureData> mCompleteQueue;
    };
} // namespace Magnum