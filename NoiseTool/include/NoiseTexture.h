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
#include "Settings.h"


namespace Magnum
{
    class FastNoiseNodeEditor;
    class NoiseTexture
    {
    public:
        NoiseTexture();
        ~NoiseTexture();

        void Draw( FastNoiseNodeEditor& iParent );

    private:
        void ReGenerate();
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
        };

        struct TextureData
        {
            TextureData() = default;

            TextureData( Vector2i s, FastNoise::OutputMinMax mm, FastNoise::Buffer const& v ) : minMax( mm ), size( s )
            {
                copy = FastNoise::Buffer( v.size );
                std::memcpy( copy.begin(), v.begin(), v.nbbytes() );
                textureData = { (uint32_t*)copy.begin(), v.size };
            }

            TextureData( TextureData&& other ) :
                copy( std::move( other.copy ) ), minMax( other.minMax ), size( other.size )
            {
                textureData = { (uint32_t*)copy.begin(), copy.size };
            }

            TextureData& operator=( TextureData&& other )

            {
                copy        = std::move( other.copy );
                minMax      = other.minMax;
                size        = other.size;
                textureData = { (uint32_t*)copy.begin(), copy.size };
                return *this;
            }


            TextureData( TextureData const& other ) : copy( other.copy ), minMax( other.minMax ), size( other.size )
            {
                textureData = { (uint32_t*)copy.begin(), copy.size };
            }

            TextureData& operator=( TextureData const& other )

            {
                copy        = other.copy;
                minMax      = other.minMax;
                size        = other.size;
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
        };

        template<typename Wrapper>
        static TextureData BuildTexture( FastNoise::SmartNode<const FastNoise::Generator> generator, FastNoise::Buffer&,
                                         Magnum::Vector2i gridSize, Magnum::Vector2i size, Magnum::Vector2i offset,
                                         float freq, int seed );
        static void        BuildTerrainDataRAW( std::vector<std::uint16_t>& buffer, FastNoise::Buffer&,
                                                const ExportData& buildData, Magnum::Vector2i offset );

        void DoExport( int, int );
        void DoExportPNG();
        void SetPreviewTexture( ImageView2D& imageView );

        bool mDisableGrid = false;
        bool mRegenerate  = false;
        bool mHasTexture  = false;

        std::atomic_int mExportProgress = 0;
        GL::Texture2D   mNoiseTexture;

        FastNoise::OutputMinMax mMinMax;

        std::future<TextureData> mTexData;
        std::future<void>        mExportTask;
        std::string              mStatus;
        Vector2                  offset { 0, 0 };
        Vector2i                 size { 0, 0 };
        Settings::version        version;
        // std::vector<std::thread>   mThreads;
        // GenerateQueue<BuildData>   mGenerateQueue;
        // CompleteQueue<TextureData> mCompleteQueue;
    };
} // namespace Magnum