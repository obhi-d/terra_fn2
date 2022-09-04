#include "FastSIMD/InlInclude.h"

#include "Utils.inl"
#include "Value.h"

template<typename FS>
class FS_T<FastNoise::Value, FS> : public virtual FastNoise::Value, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename Input>
    FS_INLINE void GenBlockT( Uniform const& u, Input& i, Output& o ) const
    {
        constexpr auto N = Input::N;
        GenBlockT( u.seed, i, o, std::make_index_sequence<N> {} );
    }

    template<typename Input, size_t... I>
    FS_INLINE void GenBlockT( int32v seed, Input& i, Output& o, std::index_sequence<I...> ) const
    {
        for( uint b = 0; b < BlockSize; ++b )
        {
            o.output[b] = Gen( seed, i.v[b][I]... );
        }
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const
    {
        float32v xs = FS_Floor_f32( x );
        float32v ys = FS_Floor_f32( y );

        int32v x0 = FS_Convertf32_i32( xs ) * int32v( FnPrimes::X );
        int32v y0 = FS_Convertf32_i32( ys ) * int32v( FnPrimes::Y );
        int32v x1 = x0 + int32v( FnPrimes::X );
        int32v y1 = y0 + int32v( FnPrimes::Y );

        xs = FnUtils::InterpHermite( x - xs );
        ys = FnUtils::InterpHermite( y - ys );

        return FnUtils::Lerp(
            FnUtils::Lerp( FnUtils::GetValueCoord( seed, x0, y0 ), FnUtils::GetValueCoord( seed, x1, y0 ), xs ),
            FnUtils::Lerp( FnUtils::GetValueCoord( seed, x0, y1 ), FnUtils::GetValueCoord( seed, x1, y1 ), xs ), ys );
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const
    {
        float32v xs = FS_Floor_f32( x );
        float32v ys = FS_Floor_f32( y );
        float32v zs = FS_Floor_f32( z );

        int32v x0 = FS_Convertf32_i32( xs ) * int32v( FnPrimes::X );
        int32v y0 = FS_Convertf32_i32( ys ) * int32v( FnPrimes::Y );
        int32v z0 = FS_Convertf32_i32( zs ) * int32v( FnPrimes::Z );
        int32v x1 = x0 + int32v( FnPrimes::X );
        int32v y1 = y0 + int32v( FnPrimes::Y );
        int32v z1 = z0 + int32v( FnPrimes::Z );

        xs = FnUtils::InterpHermite( x - xs );
        ys = FnUtils::InterpHermite( y - ys );
        zs = FnUtils::InterpHermite( z - zs );

        return FnUtils::Lerp( FnUtils::Lerp(
                                  FnUtils::Lerp( FnUtils::GetValueCoord( seed, x0, y0, z0 ), FnUtils::GetValueCoord( seed, x1, y0, z0 ), xs ),
                                  FnUtils::Lerp( FnUtils::GetValueCoord( seed, x0, y1, z0 ), FnUtils::GetValueCoord( seed, x1, y1, z0 ), xs ), ys ),
                              FnUtils::Lerp(
                                  FnUtils::Lerp( FnUtils::GetValueCoord( seed, x0, y0, z1 ), FnUtils::GetValueCoord( seed, x1, y0, z1 ), xs ),
                                  FnUtils::Lerp( FnUtils::GetValueCoord( seed, x0, y1, z1 ), FnUtils::GetValueCoord( seed, x1, y1, z1 ), xs ), ys ),
                              zs );
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z, float32v w ) const
    {
        float32v xs = FS_Floor_f32( x );
        float32v ys = FS_Floor_f32( y );
        float32v zs = FS_Floor_f32( z );
        float32v ws = FS_Floor_f32( w );

        int32v x0 = FS_Convertf32_i32( xs ) * int32v( FnPrimes::X );
        int32v y0 = FS_Convertf32_i32( ys ) * int32v( FnPrimes::Y );
        int32v z0 = FS_Convertf32_i32( zs ) * int32v( FnPrimes::Z );
        int32v w0 = FS_Convertf32_i32( ws ) * int32v( FnPrimes::W );
        int32v x1 = x0 + int32v( FnPrimes::X );
        int32v y1 = y0 + int32v( FnPrimes::Y );
        int32v z1 = z0 + int32v( FnPrimes::Z );
        int32v w1 = w0 + int32v( FnPrimes::W );

        xs = FnUtils::InterpHermite( x - xs );
        ys = FnUtils::InterpHermite( y - ys );
        zs = FnUtils::InterpHermite( z - zs );
        ws = FnUtils::InterpHermite( w - ws );

        return FnUtils::Lerp( FnUtils::Lerp( FnUtils::Lerp(
                                                 FnUtils::Lerp( FnUtils::GetValueCoord( seed, x0, y0, z0, w0 ), FnUtils::GetValueCoord( seed, x1, y0, z0, w0 ), xs ),
                                                 FnUtils::Lerp( FnUtils::GetValueCoord( seed, x0, y1, z0, w0 ), FnUtils::GetValueCoord( seed, x1, y1, z0, w0 ), xs ), ys ),
                                             FnUtils::Lerp(
                                                 FnUtils::Lerp( FnUtils::GetValueCoord( seed, x0, y0, z1, w0 ), FnUtils::GetValueCoord( seed, x1, y0, z1, w0 ), xs ),
                                                 FnUtils::Lerp( FnUtils::GetValueCoord( seed, x0, y1, z1, w0 ), FnUtils::GetValueCoord( seed, x1, y1, z1, w0 ), xs ), ys ),
                                             zs ),
                              FnUtils::Lerp( FnUtils::Lerp(
                                                 FnUtils::Lerp( FnUtils::GetValueCoord( seed, x0, y0, z0, w1 ), FnUtils::GetValueCoord( seed, x1, y0, z0, w1 ), xs ),
                                                 FnUtils::Lerp( FnUtils::GetValueCoord( seed, x0, y1, z0, w1 ), FnUtils::GetValueCoord( seed, x1, y1, z0, w1 ), xs ), ys ),
                                             FnUtils::Lerp(
                                                 FnUtils::Lerp( FnUtils::GetValueCoord( seed, x0, y0, z1, w1 ), FnUtils::GetValueCoord( seed, x1, y0, z1, w1 ), xs ),
                                                 FnUtils::Lerp( FnUtils::GetValueCoord( seed, x0, y1, z1, w1 ), FnUtils::GetValueCoord( seed, x1, y1, z1, w1 ), xs ), ys ),
                                             zs ),
                              ws );
    }
};
