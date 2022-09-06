#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
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
        bool                          fromFile( std::filesystem::path path )
        {
            std::basic_ifstream<std::uint8_t> ifs( path, std::ios::binary );
            // TODO serialize with endianness
            ifs.read( (std::uint8_t*)&width, sizeof( width ) );
            ifs.read( (std::uint8_t*)&height, sizeof( height ) );
            ifs.read( (std::uint8_t*)&depth, sizeof( depth ) );
            ifs.read( (std::uint8_t*)&pixelWidth, sizeof( pixelWidth ) );
            ifs.read( (std::uint8_t*)&format, sizeof( format ) );
            auto siz = width * height * depth * pixelWidth;
            if( !( *this )( nullptr ) )
                return false;
            ifs.read( data.get(), siz );
            return true;
        }

        inline bool fromFile()
        {
            std::filesystem::path imgPath = sourceName;
            imgPath.replace_extension( ".raw" );
            return fromFile( imgPath );
        }

        inline void toFile( std::filesystem::path path ) const
        {
            std::basic_ofstream<std::uint8_t> ifs( path, std::ios::binary );
            // TODO serialize with endianness
            ifs.write( (std::uint8_t*)&width, sizeof( width ) );
            ifs.write( (std::uint8_t*)&height, sizeof( height ) );
            ifs.write( (std::uint8_t*)&depth, sizeof( depth ) );
            ifs.write( (std::uint8_t*)&pixelWidth, sizeof( pixelWidth ) );
            ifs.write( (std::uint8_t*)&format, sizeof( format ) );
            ifs.write( data.get(), width * height * pixelWidth );
        }

        inline bool operator()( std::nullptr_t )
        {
            auto siz = width * height * depth * pixelWidth;
            if( !siz )
                return false;
            data = std::shared_ptr<std::uint8_t>( (std::uint8_t*)AlignedAllocate( 32, siz * 4 ), AlignedByteDeleter {} );
            return true;
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
            auto x = std::max<int>( 0, std::min<int>( (int)( u * ( (float)width - 0.5f ) ), width - 1 ) );
            auto y = std::max<int>( 0, std::min<int>( (int)( v * ( (float)height - 0.5f ) ), height - 1 ) );
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