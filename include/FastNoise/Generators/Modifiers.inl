#include "FastSIMD/InlInclude.h"
#include "Modifiers.h"

template<typename FS>
class FS_T<FastNoise::DomainScale, FS> : public virtual FastNoise::DomainScale, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename Input>
    FS_INLINE void GenBlockT( Params const& params, Uniform const& u, Input& i, Output& o ) const
    {
        constexpr auto N = Input::N;
        Input          i2( i.size() );

        auto scale     = float32v( mScale );
        auto BlockSize = i.MaxVectorsInBlock();
        for( std::uint32_t b = 0; b < BlockSize; ++b )
            for( uint n = 0; n < N; ++n )
                i2.v[b][n] = i.v[b][n] * scale;
        GetSourceValue( mSource, params, u, i2, o );
    }
};

template<typename FS>
class FS_T<FastNoise::DomainOffset, FS> : public virtual FastNoise::DomainOffset, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename Input>
    FS_INLINE void GenBlockT( Params const& params, Uniform const& u, Input& i, Output& o ) const
    {
        constexpr auto              N = Input::N;
        typename Output::LocalBlock lb;
        Output                      offset( lb );
        Input                       i2 = i;

        for( uint n = 0; n < N; ++n )
        {
            this->GetSourceValue( mOffset[n], params, u, i, offset );
            auto BlockSize = i.MaxVectorsInBlock();
            for( std::uint32_t b = 0; b < BlockSize; ++b )
                i2.v[b][n] += offset.output[b];
        }

        GetSourceValue( mSource, params, u, i2, o );
    }
};

template<typename FS>
class FS_T<FastNoise::DomainRotate, FS> : public virtual FastNoise::DomainRotate, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename Input>
    FS_INLINE void GenBlockT( Params const& params, Uniform const& u, Input& i, Output& o ) const
    {
        constexpr auto N = Input::N;

        if constexpr( N == 2 )
        {
            Input i2( i.size() );
            if( mPitchSin == 0.0f && mRollSin == 0.0f )
            {
                auto BlockSize = i.MaxVectorsInBlock();
                for( std::uint32_t b = 0; b < BlockSize; ++b )
                {
                    auto x     = i.v[b][0];
                    auto y     = i.v[b][1];
                    i2.v[b][0] = FS_FNMulAdd_f32( y, float32v( mYawSin ), x * float32v( mYawCos ) );
                    i2.v[b][1] = FS_FMulAdd_f32( x, float32v( mYawSin ), y * float32v( mYawCos ) );
                }
            }
            else
            {
                auto BlockSize = i.MaxVectorsInBlock();
                for( std::uint32_t b = 0; b < BlockSize; ++b )
                {
                    auto x     = i.v[b][0];
                    auto y     = i.v[b][1];
                    i2.v[b][0] = FS_FMulAdd_f32( x, float32v( mXa ), FS_FMulAdd_f32( y, float32v( mXb ), float32v( 0 ) ) );
                    i2.v[b][1] = FS_FMulAdd_f32( x, float32v( mYa ), FS_FMulAdd_f32( y, float32v( mYb ), float32v( 0 ) ) );
                }
            }
            this->GetSourceValue( mSource, params, u, i2, o );
        }
        else if constexpr( N == 3 )
        {
            Input i2( i.size() );
            auto  BlockSize = i.MaxVectorsInBlock();
            for( std::uint32_t b = 0; b < BlockSize; ++b )
            {
                auto x     = i.v[b][0];
                auto y     = i.v[b][1];
                auto z     = i.v[b][2];
                i2.v[b][0] = FS_FMulAdd_f32( x, float32v( mXa ), FS_FMulAdd_f32( y, float32v( mXb ), z * float32v( mXc ) ) );
                i2.v[b][1] = FS_FMulAdd_f32( x, float32v( mYa ), FS_FMulAdd_f32( y, float32v( mYb ), z * float32v( mYc ) ) );
                i2.v[b][2] = FS_FMulAdd_f32( x, float32v( mZa ), FS_FMulAdd_f32( y, float32v( mZb ), z * float32v( mZc ) ) );
            }
            this->GetSourceValue( mSource, params, u, i2, o );
        }
        else
            this->GetSourceValue( mSource, params, u, i, o );
    }
};

template<typename FS>
class FS_T<FastNoise::SeedOffset, FS> : public virtual FastNoise::SeedOffset, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename Input>
    FS_INLINE void GenBlockT( Params const& params, Uniform const& u, Input& i, Output& o ) const
    {
        auto newParams = params;
        newParams.seed += int32v( mOffset );
        this->GetSourceValue( mSource, newParams, u, i, o );
    }
};

template<typename FS>
class FS_T<FastNoise::Remap, FS> : public virtual FastNoise::Remap, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename Input>
    FS_INLINE void GenBlockT( Params const& params, Uniform const& u, Input& i, Output& o ) const
    {
        this->GetSourceValue( mSource, params, u, i, o );
        auto BlockSize = i.MaxVectorsInBlock();
        for( std::uint32_t b = 0; b < BlockSize; ++b )
            o.output[b] = float32v( mToMin ) + ( ( o.output[b] - float32v( mFromMin ) ) / float32v( mFromMax - mFromMin ) * float32v( mToMax - mToMin ) );
    }
};

template<typename FS>
class FS_T<FastNoise::Normalize, FS> : public virtual FastNoise::Normalize, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;


    template<typename Input>
    FS_INLINE void GenBlockT( Params const& params, Uniform const& u, Input& i, Output& o ) const
    {
        this->GetSourceValue( mSource, params, u, i, o );
    }

    void Finalize( Context& c ) const override
    {
        auto data  = (float32v*)c.output.data.get();
        auto count = ( c.output.size + FS_Size_32() - 1 ) / FS_Size_32();
        auto ratio = float32v( 1.0f / ( c.minMax.max - c.minMax.min ) );
        for( uint i = 0; i < count; ++i )
        {
            auto less = data[i] < float32v( c.minMax.min );
            auto more = data[i] > float32v( c.minMax.max );
            data[i]   = ( data[i] - float32v( c.minMax.min ) ) * ratio;
        }
    }
};

template<typename FS>
class FS_T<FastNoise::Octaves, FS> : public virtual FastNoise::Octaves, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;


    template<typename Input>
    FS_INLINE void GenBlockT( Params const& params, Uniform const& u, Input& i, Output& Noise ) const
    {
        typename Output::LocalBlock lb;
        Output                      next( lb );

        this->GetSourceValue( mSource, params, u, i, Noise );
        float32v ratio     = float32v( 1 / mFactor );
        auto     factor    = float32v( mFactor );
        Input    inpScaled = i;
        for( int oct = 0; oct < mCount; ++oct )
        {
            inpScaled.Multiply( ratio );
            this->GetSourceValue( mSource, params, u, inpScaled, next );
            auto BlockSize = i.MaxVectorsInBlock();
            for( std::uint32_t b = 0; b < BlockSize; ++b )
                Noise[b] += factor * next[b];
            factor *= factor;
        }
    }
};

template<typename FS>
class FS_T<FastNoise::Clamp, FS> : public virtual FastNoise::Clamp, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;


    template<typename Input>
    FS_INLINE void GenBlockT( Params const& params, Uniform const& u, Input& i, Output& Noise ) const
    {

        this->GetSourceValue( mSource, params, u, i, Noise );
        auto max       = float32v( mMax );
        auto min       = float32v( mMin );
        auto BlockSize = i.MaxVectorsInBlock();
        for( std::uint32_t i = 0; i < BlockSize; ++i )
        {
            Noise[i] = FS_Select_f32( Noise[i] < min, min, Noise[i] );
            Noise[i] = FS_Select_f32( Noise[i] > max, max, Noise[i] );
        }
    }
};

template<typename FS>
class FS_T<FastNoise::ConvertRGBA8, FS> : public virtual FastNoise::ConvertRGBA8, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename Input>
    FS_INLINE void GenBlockT( Params const& params, Uniform const& u, Input& i, Output& o ) const
    {
        this->GetSourceValue( mSource, params, u, i, o );
        auto BlockSize = i.MaxVectorsInBlock();
        for( std::uint32_t b = 0; b < BlockSize; ++b )
        {
            float32v source = o.output[b];

            source = FS_Min_f32( source, float32v( mMax ) );
            source = FS_Max_f32( source, float32v( mMin ) );
            source -= float32v( mMin );

            source *= float32v( 255.0f / ( mMax - mMin ) );

            int32v byteVal = FS_Convertf32_i32( source );

            int32v output = int32v( 255 << 24 );
            output |= byteVal;
            output |= byteVal << 8;
            output |= byteVal << 16;

            o.output[b] = FS_Casti32_f32( output );
        }
    }
};

template<typename FS>
class FS_T<FastNoise::ConvertRAW16, FS> : public virtual FastNoise::ConvertRAW16, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename Input>
    FS_INLINE void GenBlockT( Params const& params, Uniform const& u, Input& i, Output& o ) const
    {
        this->GetSourceValue( mSource, params, u, i, o );
        auto BlockSize = i.MaxVectorsInBlock();
        for( std::uint32_t b = 0; b < BlockSize; ++b )
        {
            float32v source = o.output[b];

            source = FS_Min_f32( source, float32v( mMax ) );
            source = FS_Max_f32( source, float32v( mMin ) );
            source -= float32v( mMin );

            source *= float32v( 65535.0f / ( mMax - mMin ) );

            o.output[b] = source;
        }
    }
};

template<typename FS>
class FS_T<FastNoise::Terrace, FS> : public virtual FastNoise::Terrace, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename Input>
    FS_INLINE void GenBlockT( Params const& params, Uniform const& u, Input& i, Output& o ) const
    {
        constexpr auto N = Input::N;

        this->GetSourceValue( mSource, params, u, i, o );
        auto Multiplier      = float32v( mMultiplier );
        auto SmoothnessRecip = float32v( mSmoothnessRecip );
        auto MultiplierRecip = float32v( mMultiplierRecip );

        auto BlockSize = i.MaxVectorsInBlock();
        for( std::uint32_t b = 0; b < BlockSize; ++b )
            o.output[b] *= Multiplier;

        if( mSmoothness != 0.0f )
        {
            auto BlockSize = i.MaxVectorsInBlock();
            for( std::uint32_t b = 0; b < BlockSize; ++b )
            {
                float32v rounded = FS_Round_f32( o.output[b] );

                float32v diff     = rounded - o.output[b];
                mask32v  diffSign = diff < float32v( 0 );

                diff = FS_Abs_f32( diff );
                diff = float32v( 0.5f ) - diff;

                diff *= SmoothnessRecip;
                diff = FS_Min_f32( diff, float32v( 0.5f ) );
                diff = FS_Select_f32( diffSign, float32v( 0.5f ) - diff, diff - float32v( 0.5f ) );

                rounded += diff;
                o.output[b] = rounded * MultiplierRecip;
            }
        }
        else
        {
            auto BlockSize = i.MaxVectorsInBlock();
            for( std::uint32_t b = 0; b < BlockSize; ++b )
            {
                float32v rounded = FS_Round_f32( o.output[b] );
                o.output[b]      = rounded * MultiplierRecip;
            }
        }
    }
};

template<typename FS>
class FS_T<FastNoise::DomainAxisScale, FS> : public virtual FastNoise::DomainAxisScale, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename Input>
    FS_INLINE void GenBlockT( Params const& params, Uniform const& u, Input& i, Output& o ) const
    {
        constexpr auto N = Input::N;

        Input i2 = i;

        for( uint n = 0; n < N; ++n )
        {
            auto BlockSize = i.MaxVectorsInBlock();
            for( std::uint32_t b = 0; b < BlockSize; ++b )
                i2.v[b][n] *= float32v( mScale[n] );
        }

        this->GetSourceValue( mSource, params, u, i2, o );
    }
};

template<typename FS>
class FS_T<FastNoise::AddDimension, FS> : public virtual FastNoise::AddDimension, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename Input>
    FS_INLINE void GenBlockT( Params const& params, Uniform const& u, Input& i, Output& o ) const
    {
        constexpr auto N = Input::N;

        if constexpr( N == (size_t)FastNoise::Dim::Count )
        {
            this->GetSourceValue( mSource, params, u, i, o );
        }
        else
        {
            this->GetSourceValue( mNewDimensionPosition, params, u, i, o );
            BlockInput<N + 1> i2( i.size() );

            auto BlockSize = i.MaxVectorsInBlock();
            for( std::uint32_t b = 0; b < BlockSize; ++b )
            {
                for( uint n = 0; n < N; ++n )
                    i2.v[b][n] = i.v[b][n];

                i2.v[b][N] = o.output[b];
            }

            this->GetSourceValue( mSource, params, u, i2, o );
        }
    }
};

template<typename FS>
class FS_T<FastNoise::RemoveDimension, FS> : public virtual FastNoise::RemoveDimension, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename Input>
    FS_INLINE void GenBlockT( Params const& params, Uniform const& u, Input& i, Output& o ) const
    {
        constexpr auto N = Input::N;
        if constexpr( N <= 2 )
            return;
        else
        {
            BlockInput<N - 1> i2( i.size() );

            auto BlockSize = i.MaxVectorsInBlock();
            for( std::uint32_t b = 0; b < BlockSize; ++b )
            {
                auto& dst = i2.v[b];
                auto& src = i.v[b];
                for( uint n = 0, k = 0; n < N; ++n )
                {
                    if( n == (uint)mRemoveDimension )
                        continue;
                    dst[k++] = src[n];
                }
            }

            this->GetSourceValue( mSource, params, u, i2, o );
        }
    }
};

template<typename FS>
class FS_T<FastNoise::GeneratorCache, FS> : public virtual FastNoise::GeneratorCache, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename Input>
    FS_INLINE void GenBlockT( Params const& params, Uniform const& u, Input& i, Output& o ) const
    {
        this->GetSourceValue( mSource, params, u, i, o );
    }
};

template<typename FS>
class FS_T<FastNoise::EdgeFalloff, FS> : public virtual FastNoise::EdgeFalloff, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename Input>
    FS_INLINE void GenBlockT( Params const& params, Uniform const& u, Input& i, Output& o ) const
    {
        constexpr auto N = Input::N;
        if( mType == FastNoise::FalloffType::ePlaneEdge )
        {
            //...
        }
        else
        {
            auto edgeLvl = float32v( mEdgeLevel );
            this->GetSourceValue( mSource, params, u, i, o );
            auto BlockSize = i.MaxVectorsInBlock();
            for( std::uint32_t b = 0; b < BlockSize; ++b )
            {
                auto dist  = FS_Pow_f32( i.RadialDistance( b, u ), float32v( mFalloff ) );
                auto value = o.output[b];
                value -= edgeLvl;
                auto mask   = dist < float32v( 1.0f );
                dist        = dist * dist * ( float32v( 3.0f ) - ( float32v( 2.0f ) * dist ) );
                value       = ( value - value * dist ) + edgeLvl;
                o.output[b] = value;
            }
        }
        // u.ctx.
    }
};
