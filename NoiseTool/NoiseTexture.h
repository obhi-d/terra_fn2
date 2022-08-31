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
    class NoiseTexture
    {
    public:
        enum GenType
        {
            GenType_2D,
            GenType_2DTiled,
            GenType_3D,
            GenType_4D,
            GenType_Count
        };

        inline static const char* GenTypeStrings =
            "2D\0"
            "2D Tiled\0"
            "3D Slice\0"
            "4D Slice\0";

        NoiseTexture();
        ~NoiseTexture();

        void Draw();
        void ReGenerate( FastNoise::SmartNodeArg<> generator );

    private:
        struct BuildData
        {
            FastNoise::SmartNode<const FastNoise::Generator> generator;
            Vector2i size;
            Vector2i numberOfPlanes = Vector2i( 1, 1 );
            Vector4 offset;
            float frequency;
            int32_t seed;
            uint64_t iteration;
            GenType generationType;
            std::array<char, 256> path = {};
        };

        struct TextureData
        {
            TextureData() = default;

            TextureData( uint64_t iter, Vector2i s, FastNoise::OutputMinMax mm, const std::vector<float>& v ) :
                minMax( mm ), size( s ), iteration( iter )
            {
                if( v.empty() )
                {
                    return;
                }

                uint32_t* texDataPtr = new uint32_t[v.size()];

                std::memcpy( texDataPtr, v.data(), v.size() * sizeof( float ) );

                textureData = { texDataPtr, v.size() };
            }

            void Free()
            {
                delete[] textureData.data();

                textureData = nullptr;
            }

            Containers::ArrayView<uint32_t> textureData;
            FastNoise::OutputMinMax minMax;
            Vector2i size;
            uint64_t iteration;
        };


        template<typename Wrapper>
        static TextureData BuildTexture( const BuildData& buildData );
        static void BuildTerrainDataRAW( std::vector<std::uint16_t>& buffer, const BuildData& buildData, Magnum::Vector4 offset );
        static void GenerateLoopThread( GenerateQueue<BuildData>& generateQueue, CompleteQueue<TextureData>& completeQueue );

        void DoExport();
        void DoExportRAW();
        void DoExportBMP();
        void SetupSettingsHandlers();
        void SetPreviewTexture( ImageView2D& imageView );

        std::atomic_int exportProgress = 0;
        std::string status;
        GL::Texture2D mNoiseTexture;
        uint64_t mCurrentIteration = 0;

        BuildData mBuildData;
        BuildData mExportBuildData;
        FastNoise::OutputMinMax mMinMax;

        std::thread mExportThread;
        std::vector<std::thread> mThreads;
        GenerateQueue<BuildData> mGenerateQueue;
        CompleteQueue<TextureData> mCompleteQueue;
    };
} // namespace Magnum