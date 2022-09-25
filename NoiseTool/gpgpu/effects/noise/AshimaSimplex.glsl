vec3 permute( vec3 x )
{
    return mod( ( ( x * 34.0 ) + 1.0 ) * x, 289.0 );
}

float salt( float seed )
{
    float a = mod( seed, 5901. );
    float b = mod( a, 2. ) == 0. ? -0.01 : 0.11;
    return a + 4179. / sqrt( a * 5. ) * b + 1001. * a / seed;
}

float noisexy( vec2 v )
{
    const float4 C = float4( 0.211324865405187, 0.366025403784439, -0.577350269189626, 0.024390243902439 );

    vec2 i  = floor( v + dot( v, C.yy ) );
    vec2 x0 = v - i + dot( i, C.xx );
    vec2 i1;
    i1         = ( x0.x > x0.y ) ? vec2( 1.0, 0.0 ) : vec2( 0.0, 1.0 );
    float4 x12 = x0.xyxy + C.xxzz;
    x12.xy -= i1;
    i       = mod( i, 289.0 );
    vec3 p  = permute( permute( i.y + vec3( 0.0, i1.y, 1.0 ) ) + i.x + vec3( 0.0, i1.x, 1.0 ) );
    vec3 m  = max( 0.5 - vec3( dot( x0, x0 ), dot( x12.xy, x12.xy ), dot( x12.zw, x12.zw ) ), 0.0 );
    m       = m * m;
    m       = m * m;
    vec3 x  = 2.0 * fract( p * C.www ) - 1.0;
    vec3 h  = abs( x ) - 0.5;
    vec3 ox = floor( x + 0.5 );
    vec3 a0 = x - ox;
    m *= 1.79284291400159 - 0.85373472095314 * ( a0 * a0 + h * h );
    vec3 g;
    g.x  = a0.x * x0.x + h.x * x0.y;
    g.yz = a0.yz * x12.xz + h.yz * x12.yw;
    return 130.0 * dot( m, g );
}

float4 noise( float4 x, float4 y, int3 pixel, env_params params )
{
    float seed = salt( float( params.seed ) );
    return float4( noisexy( vec2( x.x + seed, y.x + seed ) ), noisexy( vec2( x.y + seed, y.y + seed ) ),
                   noisexy( vec2( x.z + seed, y.z + seed ) ), noisexy( vec2( x.w + seed, y.w + seed ) ) );
}
