
#pragma once

#ifdef _MSC_VER
#include <malloc.h>
#endif

#include <cstdlib>
#include <memory>


namespace FastNoise
{
    using uint = std::uint32_t;
    inline void* AlignedAllocate( size_t al, size_t size )
    {
#ifdef _MSC_VER
        return _aligned_malloc( size, al );
#else
        return std::aligned_alloc( al, size );
#endif
    }

    inline void AlignedFree( void* d )
    {
#ifdef _MSC_VER
        return _aligned_free( d );
#else
        return std::free( d );
#endif
    }

    struct AlignedDeleter
    {
        inline void operator()( void* d )
        {
            AlignedFree( d );
        }
    };

    struct AlignedByteDeleter
    {
        inline void operator()( std::uint8_t* d )
        {
            AlignedFree( d );
        }
    };

} // namespace FastNoise