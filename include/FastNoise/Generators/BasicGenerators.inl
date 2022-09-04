#include "FastSIMD/InlInclude.h"

#include "BasicGenerators.h"
#include "Utils.inl"

template<typename FS>
class FS_T<FastNoise::Constant, FS> : public virtual FastNoise::Constant, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename Input>
    FS_INLINE void GenBlockT( Uniform const& u, Input& i, Output& o ) const
    {
        constexpr auto N     = Input::N;
        auto           value = float32v( mValue );
        for( std::uint32_t b = 0; b < BlockSize; ++b )
            o.output[b] = value;
    }
};

template<typename FS>
class FS_T<FastNoise::White, FS> : public virtual FastNoise::White, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename Input>
    FS_INLINE void GenBlockT( Uniform const& u, Input& i, Output& o ) const
    {
        constexpr auto N = Input::N;
        for( uint b = 0; b < BlockSize; ++b )
        {
            std::array<int32v, N> tmp;
            for( uint n = 0; n < N; ++n )
                tmp[n] = ( FS_Castf32_i32( i.v[b][n] ) ^ ( FS_Castf32_i32( i.v[b][n] ) >> 16 ) ) * int32v( FnPrimes::Lookup[n] );
            o.output[b] = FnUtils::GetValueCoord( u.seed, tmp );
        }
    }
};

template<typename FS>
class FS_T<FastNoise::Checkerboard, FS> : public virtual FastNoise::Checkerboard, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename Input>
    FS_INLINE void GenBlockT( Uniform const& u, Input& i, Output& o ) const
    {
        constexpr auto N          = Input::N;
        float32v       multiplier = FS_Reciprocal_f32( float32v( mSize ) );
        for( uint b = 0; b < BlockSize; ++b )
        {
            int32v value = FS_Convertf32_i32( i.v[b][0] * multiplier );
            for( uint n = 1; n < N; ++n )
                value ^= FS_Convertf32_i32( i.v[b][n] * multiplier );
            o.output[b] = float32v( 1.0f ) ^ FS_Casti32_f32( value << 31 );
        }
    }
};

template<typename FS>
class FS_T<FastNoise::SineWave, FS> : public virtual FastNoise::SineWave, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename Input>
    FS_INLINE void GenBlockT( Uniform const& u, Input& i, Output& o ) const
    {
        constexpr auto N          = Input::N;
        float32v       multiplier = FS_Reciprocal_f32( float32v( mScale ) );
        for( uint b = 0; b < BlockSize; ++b )
        {
            float32v value = FS_Sin_f32( i.v[b][0] * multiplier );
            for( uint n = 1; n < N; ++n )
                value *= FS_Sin_f32( i.v[b][n] * multiplier );
            o.output[b] = value;
        }
    }
};

template<typename FS>
class FS_T<FastNoise::PositionOutput, FS> : public virtual FastNoise::PositionOutput, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename Input>
    FS_INLINE void GenBlockT( Uniform const& u, Input& i, Output& o ) const
    {
        constexpr auto N = Input::N;
        for( uint b = 0; b < BlockSize; ++b )
        {
            o.output[b] = float32v( mOffset[0] ) * float32v( mMultiplier[0] );
            for( uint n = 1; n < N; ++n )
                o.output[b] += float32v( mOffset[n] ) * float32v( mMultiplier[n] );
        }

        //( ( ( pos += float32v( mOffset[offsetIdx++] ) ) *= float32v( mMultiplier[multiplierIdx++] ) ), ... );
        // return ( pos + ... );
    }
};

template<typename FS>
class FS_T<FastNoise::DistanceToPoint, FS> : public virtual FastNoise::DistanceToPoint, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename Input>
    FS_INLINE void GenBlockT( Uniform const& u, Input& i, Output& o ) const
    {
        constexpr auto N = Input::N;

        for( uint b = 0; b < BlockSize; ++b )
        {
            std::array<float32v, N> tmp;
            for( uint n = 0; n < N; ++n )
            {
                tmp[n] = i.v[b][n] - float32v( mPoint[n] );
            }

            o.output[b] = FnUtils::CalcDistance( mDistanceFunction, tmp, std::make_index_sequence<N> {} );
        }

        // size_t pointIdx = 0;

        //( ( pos -= float32v( mPoint[pointIdx++] ) ), ... );
        // return FnUtils::CalcDistance( mDistanceFunction, pos... );
    }
};
