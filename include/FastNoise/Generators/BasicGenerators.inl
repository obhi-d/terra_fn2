#include "FastSIMD/InlInclude.h"

#include "BasicGenerators.h"
#include "Utils.inl"

template<typename FS>
class FS_T<FastNoise::Constant, FS> : public virtual FastNoise::Constant, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename Input>
    FS_INLINE void GenBlockT( Params const& params, Uniform const& u, Input& i, Output& o ) const
    {
        constexpr auto N         = Input::N;
        auto           value     = float32v( mValue );
        auto           BlockSize = i.MaxVectorsInBlock();
        for( std::uint32_t b = 0; b < BlockSize; ++b )
            o.output[b] = value;
        // i.UV( b, u )[1];
        // i.LinearDistance( b, u );
        // i.UV( b, u )[1];
        // value;
    }
};

template<typename FS>
class FS_T<FastNoise::White, FS> : public virtual FastNoise::White, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename Input>
    FS_INLINE void GenBlockT( Params const& params, Uniform const& u, Input& i, Output& o ) const
    {
        constexpr auto N         = Input::N;
        auto           BlockSize = i.MaxVectorsInBlock();
        for( std::uint32_t b = 0; b < BlockSize; ++b )
        {
            std::array<int32v, N> tmp;
            for( uint n = 0; n < N; ++n )
                tmp[n] = ( FS_Castf32_i32( i.v[b][n] ) ^ ( FS_Castf32_i32( i.v[b][n] ) >> 16 ) ) *
                    int32v( FnPrimes::Lookup[n] );
            o.output[b] = FnUtils::GetValueCoord( params.seed, tmp );
        }
    }
};

template<typename FS>
class FS_T<FastNoise::Checkerboard, FS> : public virtual FastNoise::Checkerboard, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename Input>
    FS_INLINE void GenBlockT( Params const& params, Uniform const& u, Input& i, Output& o ) const
    {
        constexpr auto N          = Input::N;
        float32v       multiplier = FS_Reciprocal_f32( float32v( mSize ) );
        auto           BlockSize  = i.MaxVectorsInBlock();
        for( std::uint32_t b = 0; b < BlockSize; ++b )
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
    FS_INLINE void GenBlockT( Params const& params, Uniform const& u, Input& i, Output& o ) const
    {
        constexpr auto N          = Input::N;
        float32v       multiplier = FS_Reciprocal_f32( float32v( mScale ) );
        auto           BlockSize  = i.MaxVectorsInBlock();
        for( std::uint32_t b = 0; b < BlockSize; ++b )
        {
            float32v value = FS_Sin_f32( i.v[b][0] * multiplier );
            for( uint n = 1; n < N; ++n )
                value *= FS_Sin_f32( i.v[b][n] * multiplier );
            o.output[b] = value;
        }
    }
};

template<typename FS>
class FS_T<FastNoise::PositionOutput, FS> : public virtual FastNoise::PositionOutput,
                                            public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename Input>
    FS_INLINE void GenBlockT( Params const& params, Uniform const& u, Input& i, Output& o ) const
    {
        constexpr auto N         = Input::N;
        auto           BlockSize = i.MaxVectorsInBlock();
        for( std::uint32_t b = 0; b < BlockSize; ++b )
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
class FS_T<FastNoise::DistanceToPoint, FS> : public virtual FastNoise::DistanceToPoint,
                                             public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename Input>
    FS_INLINE void GenBlockT( Params const& params, Uniform const& u, Input& i, Output& o ) const
    {
        constexpr auto N = Input::N;

        auto BlockSize = i.MaxVectorsInBlock();
        for( std::uint32_t b = 0; b < BlockSize; ++b )
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


template<typename FS>
class FS_T<FastNoise::StrataMask, FS> : public virtual FastNoise::StrataMask, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename Input, FastNoise::Sampling Sampling>
    FS_INLINE void GenBlockTS( Params const& params, Uniform const& u, Input& i, Output& o ) const
    {
        constexpr auto N = Input::N;

        if( N == 2 && mImage->data.get() )
        {
            typename Output::LocalBlock lb[4];
            Output                      scaleX( lb[0] );
            Output                      scaleY( lb[1] );
            Output                      offsetX( lb[2] );
            Output                      offsetY( lb[3] );

            this->GetSourceValue( mOffset[0], params, u, i, offsetX );
            this->GetSourceValue( mOffset[1], params, u, i, offsetY );
            this->GetSourceValue( mScale[0], params, u, i, scaleX );
            this->GetSourceValue( mScale[1], params, u, i, scaleY );

            auto minS   = float32v( mMinScale );
            auto rangeS = float32v( mMaxScale - mMinScale );

            auto BlockSize = i.MaxVectorsInBlock();
            for( std::uint32_t b = 0; b < BlockSize; ++b )
            {
                auto uv   = i.UV( b, u );
                auto vecU = FS_FMulAdd_f32( uv[0], scaleX[b], float32v( offsetX[b] ) );
                auto vecV = FS_FMulAdd_f32( uv[1], scaleY[b], float32v( offsetY[b] ) );


                float32v sample;
                // we cannot vector sample the image so fill up a vector
                {
                    float* fu = (float*)&vecU;
                    float* fv = (float*)&vecV;
                    float* fd = (float*)&sample;
                    for( uint p = 0; p < FS_Size_32(); ++p )
                    {
                        fu[p] = std::clamp( std::fmod( fu[p], 1.0f ), 0.0f, 1.0f );
                        fv[p] = std::clamp( std::fmod( fv[p], 1.0f ), 0.0f, 1.0f );
                        if constexpr( Sampling == FastNoise::Sampling::e1x )
                            fd[p] = mImage->sample( fu[p], fv[p] );
                        else
                            fd[p] = mImage->sampleNx<(int)Sampling>( fu[p], fv[p] );
                    }
                }
                o.output[b] = ( ( sample * rangeS ) + minS );
            }
        }
    }


    template<typename Input>
    FS_INLINE void GenBlockT( Params const& params, Uniform const& u, Input& i, Output& o ) const
    {
        switch( mSampling )
        {

#define FASTNOISE_SamplingCase( x )                                                                                    \
    case FastNoise::Sampling::x:                                                                                       \
        return GenBlockTS<Input, FastNoise::Sampling::x>( params, u, i, o )
        default:
            FASTNOISE_SamplingCase( e1x );
            FASTNOISE_SamplingCase( e2x );
            FASTNOISE_SamplingCase( e3x );
            FASTNOISE_SamplingCase( e4x );
        }
    }
};


template<typename FS>
class FS_T<FastNoise::CurveGen, FS> : public virtual FastNoise::CurveGen, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename Input>
    FS_INLINE void GenBlockT( Params const& params, Uniform const& u, Input& i, Output& o ) const
    {
        constexpr auto N = Input::N;

        if( N == 2 )
        {
            auto BlockSize = i.MaxVectorsInBlock();
            auto strX      = float32v( mStrength[0] );
            auto strY      = float32v( mStrength[1] );
            for( std::uint32_t b = 0; b < BlockSize; ++b )
            {
                auto     uv     = i.UV( b, u );
                float32v sample = float32v( 0.0f );
                // we cannot vector sample the image so fill up a vector
                {
                    float32v vfd;
                    float*   fd = (float*)&vfd;
                    if( mApply[0] )
                    {
                        float* fu = (float*)&uv[0];
                        for( uint p = 0; p < FS_Size_32(); ++p )
                            fd[p] = mCurve( std::clamp( fu[p], 0.0f, 0.1f ) ) * 2 - 1.0f;
                        sample = vfd * strX;
                    }

                    if( mApply[1] )
                    {
                        float32v vfd;
                        float*   fd = (float*)&vfd;
                        float*   fv = (float*)&uv[1];
                        for( uint p = 0; p < FS_Size_32(); ++p )
                            fd[p] = mCurve( std::clamp( fv[p], 0.0f, 0.1f ) ) * 2 - 1.0f;
                        sample = FS_FMulAdd_f32( vfd, strY, sample );
                    }
                }
                o.output[b] = sample;
            }
        }
    }
};

template<typename FS>
class FS_T<FastNoise::GridRandomConstant, FS> : public virtual FastNoise::GridRandomConstant,
                                                public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename Input>
    FS_INLINE void GenBlockT( Params const& params, Uniform const& u, Input& i, Output& o ) const
    {
        constexpr auto N = Input::N;

        auto BlockSize = i.MaxVectorsInBlock();
        auto gridYSize = float32v( (float)u.recipSize[1] );
        auto min       = float32v( mMin );
        auto range     = float32v( mMax ) - min;

        for( std::uint32_t b = 0; b < BlockSize; ++b )
        {
            auto      grid  = i.GridId( b, u );
            auto      ids   = FS_Castf32_i32( FS_FMulAdd_f32( grid[0], gridYSize, grid[1] ) );
            uint32_t* id    = (uint32_t*)&ids;
            float32v& value = o.output[b];
            float*    fv    = (float*)&value;
            for( uint p = 0; p < FS_Size_32(); ++p )
            {
                id[p] = ( id[p] - mSeedOffset );
                id[p] ^= ( id[p] << 13 );
                id[p] ^= ( id[p] << 17 );
                id[p] ^= ( id[p] << 5 );
                fv[p] = ( ( *( id + p ) % 1000 ) / 1000.0f );
            }
            o.output[b] = FS_FMulAdd_f32( value, range, min );
        }
    }
};
