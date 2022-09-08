#include "FastSIMD/InlInclude.h"

#include "DomainWarpFractal.h"

template<typename FS>
class FS_T<FastNoise::DomainWarpFractalProgressive, FS> : public virtual FastNoise::DomainWarpFractalProgressive, public FS_T<FastNoise::Fractal<FastNoise::DomainWarp>, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;


    template<typename Input>
    FS_INLINE void GenBlockT( Params const& params, Uniform const& u, Input& i, Output& o ) const
    {
        constexpr auto N = Input::N;

        typename Output::LocalBlock o2b;
        typename Output::LocalBlock o3b;
        Output                      WeightedStrength( o2b );
        Output                      Gain( o3b );
        Input                       i2 = i;

        auto* warp = this->GetSourceSIMD( mSource );
        this->GetSourceValue( warp->GetWarpAmplitude(), params, u, i, o );
        this->GetSourceValue( mWeightedStrength, params, u, i, WeightedStrength );
        this->GetSourceValue( mGain, params, u, i, Gain );

        auto BlockSize = i.MaxVectorsInBlock();
        for( std::uint32_t b = 0; b < BlockSize; ++b )
        {
            auto& pos = i2.v[b];


            float32v amp              = float32v( mFractalBounding ) * o.output[b];
            float32v weightedStrength = WeightedStrength.output[b];
            float32v freq             = float32v( warp->GetWarpFrequency() );
            int32v   seedInc          = params.seed;

            float32v gain = Gain.output[b];
            float32v lacunarity( mLacunarity );
            float32v strength;

            if constexpr( N == 2 )
                strength = warp->Warp( seedInc, amp, ( pos[0] * freq ), ( pos[1] * freq ), pos[0], pos[1] );
            else if constexpr( N == 3 )
                strength = warp->Warp( seedInc, amp, ( pos[0] * freq ), ( pos[1] * freq ), ( pos[2] * freq ), pos[0], pos[1], pos[2] );
            else if constexpr( N == 4 )
                strength = warp->Warp( seedInc, amp, ( pos[0] * freq ), ( pos[1] * freq ), ( pos[2] * freq ), ( pos[3] * freq ), pos[0], pos[1], pos[2], pos[3] );

            for( int i = 1; i < mOctaves; i++ )
            {
                seedInc -= int32v( -1 );
                freq *= lacunarity;
                amp *= FnUtils::Lerp( float32v( 1 ), float32v( 1 ) - strength, weightedStrength );
                amp *= gain;

                if constexpr( N == 2 )
                    strength = warp->Warp( seedInc, amp, ( pos[0] * freq ), ( pos[1] * freq ), pos[0], pos[1] );
                else if constexpr( N == 3 )
                    strength = warp->Warp( seedInc, amp, ( pos[0] * freq ), ( pos[1] * freq ), ( pos[2] * freq ), pos[0], pos[1], pos[2] );
                else if constexpr( N == 4 )
                    strength = warp->Warp( seedInc, amp, ( pos[0] * freq ), ( pos[1] * freq ), ( pos[2] * freq ), ( pos[3] * freq ), pos[0], pos[1], pos[2], pos[3] );
            }
        }

        this->GetSourceValue( warp->GetWarpSource(), params, u, i2, o );
    }
};

template<typename FS>
class FS_T<FastNoise::DomainWarpFractalIndependant, FS> : public virtual FastNoise::DomainWarpFractalIndependant, public FS_T<FastNoise::Fractal<FastNoise::DomainWarp>, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename Input>
    FS_INLINE void GenBlockT( Params const& params, Uniform const& u, Input& i, Output& o ) const
    {
        constexpr auto N = Input::N;

        typename Output::LocalBlock o2b;
        typename Output::LocalBlock o3b;
        Output                      WeightedStrength( o2b );
        Output                      Gain( o3b );
        Input                       i2 = i;

        auto* warp = this->GetSourceSIMD( mSource );
        this->GetSourceValue( warp->GetWarpAmplitude(), params, u, i, o );
        this->GetSourceValue( mWeightedStrength, params, u, i, WeightedStrength );
        this->GetSourceValue( mGain, params, u, i, Gain );
        auto BlockSize = i.MaxVectorsInBlock();
        for( std::uint32_t b = 0; b < BlockSize; ++b )
        {
            auto  spos = i.v[b];
            auto& pos  = i2.v[b];


            float32v amp              = float32v( mFractalBounding ) * o.output[b];
            float32v weightedStrength = WeightedStrength.output[b];
            float32v freq             = float32v( warp->GetWarpFrequency() );
            int32v   seedInc          = params.seed;
            float32v gain             = Gain.output[b];
            float32v lacunarity( mLacunarity );
            float32v strength;

            if constexpr( N == 2 )
                strength = warp->Warp( seedInc, amp, ( spos[0] * freq ), ( spos[1] * freq ), pos[0], pos[1] );
            else if constexpr( N == 3 )
                strength = warp->Warp( seedInc, amp, ( spos[0] * freq ), ( spos[1] * freq ), ( spos[2] * freq ), pos[0], pos[1], pos[2] );
            else if constexpr( N == 4 )
                strength = warp->Warp( seedInc, amp, ( spos[0] * freq ), ( spos[1] * freq ), ( spos[2] * freq ), ( spos[3] * freq ), pos[0], pos[1], pos[2], pos[3] );

            for( int i = 1; i < mOctaves; i++ )
            {
                seedInc -= int32v( -1 );
                freq *= lacunarity;
                amp *= FnUtils::Lerp( float32v( 1 ), float32v( 1 ) - strength, weightedStrength );
                amp *= gain;

                if constexpr( N == 2 )
                    strength = warp->Warp( seedInc, amp, ( spos[0] * freq ), ( spos[1] * freq ), pos[0], pos[1] );
                else if constexpr( N == 3 )
                    strength = warp->Warp( seedInc, amp, ( spos[0] * freq ), ( spos[1] * freq ), ( spos[2] * freq ), pos[0], pos[1], pos[2] );
                else if constexpr( N == 4 )
                    strength = warp->Warp( seedInc, amp, ( spos[0] * freq ), ( spos[1] * freq ), ( spos[2] * freq ), ( spos[3] * freq ), pos[0], pos[1], pos[2], pos[3] );
            }
        }

        this->GetSourceValue( warp->GetWarpSource(), params, u, i2, o );
    }
};
