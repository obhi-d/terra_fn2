
float4 noise( float4 x, float4 y, int3 pixel, env_params params )
{
    float4 f  = float4( F2 ) * ( x + y );
    float4 x0 = floor( x + f );
    float4 y0 = floor( y + f );

    int4 i = int4( x0 ) * FnPrimes_X;
    int4 j = int4( y0 ) * FnPrimes_Y;

    float4 g = float4( G2 ) * ( x0 + y0 );
    x0       = x - ( x0 - g );
    y0       = y - ( y0 - g );
    int4 i1  = greater_than( x0, y0 );

    float4 x1 = mask_sub( x0, float4( 1.0 ), i1 ) + float4( G2 );
    float4 y1 = nmask_sub( y0, float4( 1.0 ), i1 ) + float4( G2 );
    float4 x2 = x0 + float4( ( G2 * float4( 2.0 ) ) - float4( 1.0 ) );
    float4 y2 = y0 + float4( ( G2 * float4( 2.0 ) ) - float4( 1.0 ) );

    float4 t0 = float4( 0.5f ) - ( x0 * x0 ) - ( y0 * y0 );
    float4 t1 = float4( 0.5f ) - ( x1 * x1 ) - ( y1 * y1 );
    float4 t2 = float4( 0.5f ) - ( x2 * x2 ) - ( y2 * y2 );

    t0 = max( t0, float4( 0 ) );
    t1 = max( t1, float4( 0 ) );
    t2 = max( t2, float4( 0 ) );

    t0 *= t0;
    t0 *= t0;
    t1 *= t1;
    t1 *= t1;
    t2 *= t2;
    t2 *= t2;

    int4   seed = int4( params.seed );
    float4 n0   = gradient_dot_fancy( hash_primes( seed, i, j ), x0, y0 );
    float4 n1   = gradient_dot_fancy(
          hash_primes( seed, mask_add( i, int4( FnPrimes_X ), i1 ), nmask_add( j, int4( FnPrimes_Y ), i1 ) ), x1, y1 );
    float4 n2 = gradient_dot_fancy( hash_primes( seed, i + int4( FnPrimes_X ), j + int4( FnPrimes_Y ) ), x2, y2 );

    return float4( 49.918426513671875f ) * ( n0 * t0 + ( n1 * t1 + n2 * t2 ) );
}
