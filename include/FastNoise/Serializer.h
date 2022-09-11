
#pragma once
#include <cstdint>
#include <vector>

namespace FastNoise
{

    template<typename I>
    static std::string NumberToHex( I w, size_t hex_len = sizeof( I ) << 1 )
    {
        static const char* digits = "0123456789ABCDEF";
        std::string        rc( hex_len, '0' );
        for( size_t i = 0, j = ( hex_len - 1 ) * 4; i < hex_len; ++i, j -= 4 )
            rc[i] = digits[( w >> j ) & 0x0f];
        return rc;
    }

    template<typename T>
    bool GetFromDataStream( const std::vector<uint8_t>& dataStream, size_t& idx, T& value )
    {
        if( dataStream.size() < idx + sizeof( T ) )
        {
            return false;
        }

        value = *reinterpret_cast<const T*>( dataStream.data() + idx );

        idx += sizeof( T );
        return true;
    }

    template<typename T>
    void AddToDataStream( std::vector<uint8_t>& dataStream, T value )
    {
        for( size_t i = 0; i < sizeof( T ); i++ )
        {
            dataStream.push_back( (uint8_t)( value >> ( i * 8 ) ) );
        }
    }

    inline void AddToDataStream( std::vector<uint8_t>& dataStream, float value )
    {
        uint32_t cast = *(uint32_t*)( &value );
        for( size_t i = 0; i < sizeof( value ); i++ )
        {
            dataStream.push_back( (uint8_t)( cast >> ( i * 8 ) ) );
        }
    }
} // namespace FastNoise