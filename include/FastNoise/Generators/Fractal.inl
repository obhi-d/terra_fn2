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
    FS_INLINE void GenBlockT( Params const& params, Uniform const& u, Input& i, Output& Sum ) const
    {
        constexpr auto N = Input::N;

        typename Output::template HeapBlock<5> hb;
        Output                                 WeightedStrength( hb[0] );
        Output                                 Gain( hb[1] );
        Output                                 Noise( hb[2] );
        Output                                 LacInp( hb[3] );
        Output                                 Amp( hb[4] );
        Input                                  i2 = i;

        this->GetSourceValue( mGain, params, u, i, Gain );
        this->GetSourceValue( mWeightedStrength, params, u, i, WeightedStrength );
        this->GetSourceValue( mSource, params, u, i, Noise );

        float32v lacunarity( mLacunarity );
        Amp.Fill( float32v( mFractalBounding ) );

        auto newParams = params;

        for( uint b = 0; b < BlockSize; ++b )
            Sum[b] = Noise[b] * Amp[0];

        for( int t = 1; t < mOctaves; t++ )
        {
            newParams.seed -= int32v( -1 );

            i2.Multiply( lacunarity );
            this->GetSourceValue( mSource, newParams, u, i2, LacInp );

            for( uint b = 0; b < BlockSize; ++b )
            {
                Amp[b] *= FnUtils::Lerp( float32v( 1 ), ( Noise[b] + float32v( 1 ) ) * float32v( 0.5f ), WeightedStrength[b] );
                Amp[b] *= Gain[b];

                Noise[b] = LacInp[b];
                Sum[b] += Noise[b] * Amp[b];
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
    FS_INLINE void GenBlockT( Params const& params, Uniform const& u, Input& i, Output& Sum ) const
    {
        constexpr auto N = Input::N;

        typename Output::template HeapBlock<5> hb;
        Output                                 WeightedStrength( hb[0] );
        Output                                 Gain( hb[1] );
        Output                                 Noise( hb[2] );
        Output                                 LacInp( hb[3] );
        Output                                 Amp( hb[4] );
        Input                                  i2 = i;

        this->GetSourceValue( mGain, params, u, i, Gain );
        this->GetSourceValue( mWeightedStrength, params, u, i, WeightedStrength );
        this->GetSourceValue( mSource, params, u, i, Noise );
        Amp.Fill( float32v( mFractalBounding ) );

        float32v lacunarity( mLacunarity );
        auto     newParams = params;

        for( uint b = 0; b < BlockSize; ++b )
        {
            Noise[b] = FS_Abs_f32( Noise[b] );
            Sum[b]   = ( Noise[b] * float32v( -2 ) + float32v( 1 ) ) * Amp[0];
        }

        for( int t = 1; t < mOctaves; t++ )
        {
            newParams.seed -= int32v( -1 );

            i2.Multiply( lacunarity );
            this->GetSourceValue( mSource, newParams, u, i2, LacInp );

            for( uint b = 0; b < BlockSize; ++b )
            {
                Amp[b] *= FnUtils::Lerp( float32v( 1 ), float32v( 1 ) - Noise[b], WeightedStrength[b] );
                Amp[b] *= Gain[b];

                Noise[b] = FS_Abs_f32( LacInp[b] );
                Sum[b] += ( Noise[b] * float32v( -2 ) + float32v( 1 ) ) * Amp[b];
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
    FS_INLINE void GenBlockT( Params const& params, Uniform const& u, Input& i, Output& Sum ) const
    {
        constexpr auto N = Input::N;

        typename Output::template HeapBlock<6> hb;
        Output                                 WeightedStrength( hb[0] );
        Output                                 Gain( hb[1] );
        Output                                 Noise( hb[2] );
        Output                                 LacInp( hb[3] );
        Output                                 PingPongStrength( hb[4] );
        Output                                 Amp( hb[5] );
        Input                                  i2 = i;

        this->GetSourceValue( mGain, params, u, i, Gain );
        this->GetSourceValue( mWeightedStrength, params, u, i, WeightedStrength );
        this->GetSourceValue( mPingPongStrength, params, u, i, PingPongStrength );
        this->GetSourceValue( mSource, params, u, i, Sum );
        Amp.Fill( float32v( mFractalBounding ) );

        float32v lacunarity( mLacunarity );
        auto     newParams = params;

        for( uint b = 0; b < BlockSize; ++b )
        {
            Noise[b] = PingPong( ( Noise[b] + float32v( 1 ) ) * PingPongStrength[b] );
            Sum[b]   = Noise[b] * Amp[0];
        }

        for( int t = 1; t < mOctaves; t++ )
        {
            newParams.seed -= int32v( -1 );

            i2.Multiply( lacunarity );
            this->GetSourceValue( mSource, params, u, i2, LacInp );

            for( uint b = 0; b < BlockSize; ++b )
            {
                Amp[b] *= FnUtils::Lerp( float32v( 1 ), ( Noise[b] + float32v( 1 ) ) * float32v( 0.5f ), WeightedStrength[b] );
                Amp[b] *= Gain[b];

                Noise[b] = PingPong( ( LacInp[b] + float32v( 1 ) ) * PingPongStrength[b] );
                Sum[b] += Noise[b] * Amp[b];
            }
        }
    }
};
