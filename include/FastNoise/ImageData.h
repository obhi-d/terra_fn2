#pragma once
#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>

#include "FastNoise/AllocUtils.h"
#include "FastNoise/FastNoise_Config.h"

namespace FastNoise
{

    struct FASTNOISE_API ImageData
    {
        enum class Format
        {
            EByte,
            EUInt,
            EFloat,
            ERGBA,
            ERGB
        };

        struct RGBA
        {
            std::array<std::uint8_t, 4> value;
        };

        struct RGB
        {
            std::array<std::uint8_t, 4> value;
        };


        std::string                   sourceName;
        std::shared_ptr<std::uint8_t> data;
        std::uint32_t                 width      = 0;
        std::uint32_t                 height     = 1;
        std::uint32_t                 depth      = 1;
        std::uint32_t                 pixelWidth = 4;
        Format                        format     = Format::ERGBA;

        void operator()( std::nullptr_t )
        {
            auto siz = width * height * depth * pixelWidth;
            data     = std::shared_ptr<std::uint8_t>( (std::uint8_t*)AlignedAllocate( 32, siz * 4 ), AlignedByteDeleter {} );
        }

        inline bool operator==( ImageData const& other ) const
        {
            return sourceName == other.sourceName;
        }

        inline bool operator!=( ImageData const& other ) const
        {
            return sourceName != other.sourceName;
        }

        std::size_t size() const
        {
            return width * height * depth * pixelWidth;
        }

        template<typename T>
        T& get( std::uint32_t i, std::uint32_t j )
        {
            return *(T*)( data.get() + ( ( j * width + i ) * pixelWidth ) );
        }

        template<typename T>
        T get( std::uint32_t i, std::uint32_t j ) const
        {
            return *(T const*)( data.get() + ( ( j * width + i ) * pixelWidth ) );
        }

        float sample( float u, float v ) const
        {
            auto x = ( std::uint32_t )( u * ( (float)width + 0.5f ) );
            auto y = ( std::uint32_t )( v * ( (float)height + 0.5f ) );
            switch( format )
            {
            case Format::EByte:
                return (float)get<std::uint8_t>( x, y ) / 255.f;
            case Format::ERGB:
            {
                auto rgb = get<RGB>( x, y );
                return (float)( (std::uint32_t)rgb.value[0] + ( ( (std::uint32_t)rgb.value[1] ) << 8 ) +
                                ( ( (std::uint32_t)rgb.value[2] ) << 16 ) ) /
                    (float)( 1 << 24 );
            }
            case Format::ERGBA:
            case Format::EUInt:
                return (float)( (double)get<std::uint32_t>( x, y ) / (double)std::numeric_limits<std::uint32_t>::max() );
            case Format::EFloat:
                return get<float>( x, y );
            }
        }
    };
} // namespace FastNoise