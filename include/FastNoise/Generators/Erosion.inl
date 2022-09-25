#include "FastSIMD/InlInclude.h"

#include "Erosion.h"
#include "Utils.inl"

template<typename FS>
class FS_T<FastNoise::Erosion, FS> : public virtual FastNoise::Erosion, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename Input>
    FS_INLINE void GenBlockT( Params const& params, Uniform const& u, Input& i, Output& o ) const
    {
        constexpr auto              N = Input::N;
        typename Output::LocalBlock lb[3];
        Output                      sediment( lb[0] );
        Output                      water( lb[1] );
        Output                      velocity( lb[2] );
        int32v                      seed     = int32v( u.seed + mSeedOffset );
        auto                        rainRate = float32v( mRainRate );
        for( int i = 0; i < 3; ++i )
            lb[i].setZero();

        auto BlockSize = i.MaxVectorsInBlock();
        for( std::uint32_t b = 0; b < BlockSize; ++b )
        {
            for( int i = 0; i < mIteration; ++i )
            {

                float32v x = FS_Floor_f32( i.v[b][0] * u.wavelength );
                float32v y = FS_Floor_f32( i.v[b][1] * u.wavelength );

                int32v   x0   = FS_Convertf32_i32( x ) * int32v( FnPrimes::X );
                int32v   y0   = FS_Convertf32_i32( y ) * int32v( FnPrimes::Y );
                float32v rain = Utils::GetValueCoord( seed, x0, y0 );
                water[b] += rain * rainRate;
            }
        }
        for( uint n = 0; n < N; ++n )
            i2.v[b][n] = i.v[b][n] * scale;
        GetSourceValue( mSource, params, u, i2, o );
    }
};