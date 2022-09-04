#include "FastSIMD/InlInclude.h"

#include "Fractal.h"

template<typename FS, typename T>
class FS_T<FastNoise::Fractal<T>, FS> : public virtual FastNoise::Fractal<T>, public FS_T<FastNoise::Generator, FS>
{
};

template<typename FS>
class FS_T<FastNoise::FractalFBm, FS> : public virtual FastNoise::FractalFBm, public FS_T<FastNoise::Fractal<>, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename Input>
    FS_INLINE void GenBlockT( Uniform const& u, Input& i, Output& o ) const
    {
        constexpr auto N = Input::N;

        typename Output::template HeapBlock<4> hb;
        Output                                 WeightedStrength( hb[0] );
        Output                                 Gain( hb[1] );
        Output                                 Noise( hb[2] );
        Output                                 LastNoise( hb[3] );
        Input                                  i2 = i;

        this->GetSourceValue( mGain, u, i, Gain );
        this->GetSourceValue( mWeightedStrength, u, i, WeightedStrength );
        this->GetSourceValue( mSource, u, i, o );

        float32v lacunarity( mLacunarity );
        float32v amp( mFractalBounding );
        int32v   seed = u.seed;

        for( uint b = 0; b < BlockSize; ++b )
        {
            LastNoise.output[b] = o.output[b];
            o.output[b] *= amp;
        }


        for( int t = 1; t < mOctaves; t++ )
        {
            seed -= int32v( -1 );
            for( uint b = 0; b < BlockSize; ++b )
                for( uint n = 0; n < N; ++n )
                    i2.v[b][n] *= lacunarity;

            this->GetSourceValue( mSource, u, i2, Noise );
            for( uint b = 0; b < BlockSize; ++b )
            {
                amp *= FnUtils::Lerp( float32v( 1 ), ( LastNoise.output[b] + float32v( 1 ) ) * float32v( 0.5f ), WeightedStrength.output[b] );
                amp *= Gain.output[b];

                LastNoise.output[b] = Noise.output[b];
                o.output[b] += LastNoise.output[b] * amp;
            }
        }
    }
};

template<typename FS>
class FS_T<FastNoise::FractalRidged, FS> : public virtual FastNoise::FractalRidged, public FS_T<FastNoise::Fractal<>, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename Input>
    FS_INLINE void GenBlockT( Uniform const& u, Input& i, Output& o ) const
    {
        constexpr auto N = Input::N;

        typename Output::template HeapBlock<4> hb;
        Output                                 WeightedStrength( hb[0] );
        Output                                 Gain( hb[1] );
        Output                                 Noise( hb[2] );
        Output                                 LastNoise( hb[3] );
        Input                                  i2 = i;

        this->GetSourceValue( mGain, u, i, Gain );
        this->GetSourceValue( mWeightedStrength, u, i, WeightedStrength );
        this->GetSourceValue( mSource, u, i, o );

        float32v lacunarity( mLacunarity );
        float32v amp( mFractalBounding );
        int32v   seed = u.seed;

        for( uint b = 0; b < BlockSize; ++b )
        {
            LastNoise.output[b] = FS_Abs_f32( o.output[b] );
            o.output[b]         = ( LastNoise.output[b] * float32v( -2 ) + float32v( 1 ) ) * amp;
        }

        for( int t = 1; t < mOctaves; t++ )
        {
            seed -= int32v( -1 );
            for( uint b = 0; b < BlockSize; ++b )
                for( uint n = 0; n < N; ++n )
                    i2.v[b][n] *= lacunarity;
            this->GetSourceValue( mSource, u, i2, Noise );
            for( uint b = 0; b < BlockSize; ++b )
            {
                amp *= FnUtils::Lerp( float32v( 1 ), float32v( 1 ) - LastNoise.output[b], WeightedStrength.output[b] );
                amp *= Gain.output[b];

                LastNoise.output[b] = FS_Abs_f32( Noise.output[b] );
                o.output[b] += ( LastNoise.output[b] * float32v( -2 ) + float32v( 1 ) ) * amp;
            }
        }
    }
};

template<typename FS>
class FS_T<FastNoise::FractalPingPong, FS> : public virtual FastNoise::FractalPingPong, public FS_T<FastNoise::Fractal<>, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    static float32v PingPong( float32v t )
    {
        t -= FS_Round_f32( t * float32v( 0.5f ) ) * float32v( 2 );
        return FS_Select_f32( t < float32v( 1 ), t, float32v( 2 ) - t );
    }

    template<typename Input>
    FS_INLINE void GenBlockT( Uniform const& u, Input& i, Output& o ) const
    {
        constexpr auto N = Input::N;

        typename Output::template HeapBlock<5> hb;
        Output                                 WeightedStrength( hb[0] );
        Output                                 Gain( hb[1] );
        Output                                 Noise( hb[2] );
        Output                                 PingPongStrength( hb[3] );
        Output                                 LastNoise( hb[4] );
        Input                                  i2 = i;

        this->GetSourceValue( mGain, u, i, Gain );
        this->GetSourceValue( mWeightedStrength, u, i, WeightedStrength );
        this->GetSourceValue( mPingPongStrength, u, i, PingPongStrength );
        this->GetSourceValue( mSource, u, i, o );

        float32v lacunarity( mLacunarity );
        float32v amp( mFractalBounding );

        for( uint b = 0; b < BlockSize; ++b )
        {
            LastNoise.output[b] = PingPong( ( o.output[b] + float32v( 1 ) ) * PingPongStrength.output[b] );
            o.output[b]         = LastNoise.output[b] * amp;
        }

        int32v seed = u.seed;

        for( int t = 1; t < mOctaves; t++ )
        {
            seed -= int32v( -1 );
            for( uint b = 0; b < BlockSize; ++b )
                for( uint n = 0; n < N; ++n )
                    i2.v[b][n] *= lacunarity;
            this->GetSourceValue( mSource, u, i2, Noise );

            for( uint b = 0; b < BlockSize; ++b )
            {
                amp *= FnUtils::Lerp( float32v( 1 ), ( LastNoise.output[b] + float32v( 1 ) ) * float32v( 0.5f ), WeightedStrength.output[b] );
                amp *= Gain.output[b];

                LastNoise.output[b] = PingPong( ( Noise.output[b] + float32v( 1 ) ) * PingPongStrength.output[b] );
                o.output[b] += LastNoise.output[b] * amp;
            }
        }
    }
};
