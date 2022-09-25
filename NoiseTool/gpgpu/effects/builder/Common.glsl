
#define int4   ivec4
#define float4 vec4
#define float3 vec3
#define uint4  uvec4
#define float2 vec2
#define int2   ivec2


struct env_params
{
    int   seed;
    float frequency;
    float wavelength;

    float2 start;
    float2 size;

    float2 center;
    float2 gridSize;
    float2 recipSize;
    float2 recipGridSize;
    float2 halfRecipGridSize;

    int2 bufferSize;
    int2 startCoord;
};

const int4   FnPrimes_X = int4( 501125321 );
const int4   FnPrimes_Y = int4( 1136930381 );
const float4 ROOT2      = float4( 1.4142135623730950488 );
const float4 ROOT3      = float4( 1.7320508075688772935 );
const float4 SQRT3      = float4( 1.7320508075688772935274463415059 );
const float4 F2         = float4( float4( 0.5 ) * ( SQRT3 - float4( 1.0 ) ) );
const float4 G2         = float4( ( float4( 3.0 ) - SQRT3 ) / float4( 6.0 ) );

float4 ltsel( float4 a, float4 b, float4 v1, float4 v2 )
{
    return float4( a.x < b.x ? v1.x : v2.x, a.y < b.y ? v1.y : v2.y, a.z < b.z ? v1.z : v2.z, a.w < b.w ? v1.w : v2.w );
}

float4 gtsel( float4 a, float4 b, float4 v1, float4 v2 )
{
    return float4( a.x > b.x ? v1.x : v2.x, a.y > b.y ? v1.y : v2.y, a.z > b.z ? v1.z : v2.z, a.w > b.w ? v1.w : v2.w );
}

int4 less_than( float4 a, float4 b )
{
    return int4( a.x < b.x, a.y < b.y, a.z < b.z, a.w < b.w );
}

int4 greater_than( float4 a, float4 b )
{
    return int4( a.x > b.x, a.y > b.y, a.z > b.z, a.w > b.w );
}

float4 select( int4 mask, float4 a, float4 b )
{
    return float4( mask.x != 0 ? a.x : b.x, mask.y != 0 ? a.y : b.y, mask.z != 0 ? a.z : b.z, mask.w != 0 ? a.w : b.w );
}

float4 mask_add( float4 a, float4 b, int4 mask )
{
    return float4( mask.x != 0 ? a.x + b.x : a.x, mask.y != 0 ? a.y + b.y : a.y, mask.z != 0 ? a.z + b.z : a.z,
                   mask.w != 0 ? a.w + b.w : a.w );
}

float4 mask_sub( float4 a, float4 b, int4 mask )
{
    return float4( mask.x != 0 ? a.x - b.x : a.x, mask.y != 0 ? a.y - b.y : a.y, mask.z != 0 ? a.z - b.z : a.z,
                   mask.w != 0 ? a.w - b.w : a.w );
}

float4 mask_mul( float4 a, float4 b, int4 mask )
{
    return float4( mask.x != 0 ? a.x * b.x : a.x, mask.y != 0 ? a.y * b.y : a.y, mask.z != 0 ? a.z * b.z : a.z,
                   mask.w != 0 ? a.w * b.w : a.w );
}

int4 mask_add( int4 a, int4 b, int4 mask )
{
    return int4( mask.x != 0 ? a.x + b.x : a.x, mask.y != 0 ? a.y + b.y : a.y, mask.z != 0 ? a.z + b.z : a.z,
                 mask.w != 0 ? a.w + b.w : a.w );
}

int4 nmask_add( int4 a, int4 b, int4 mask )
{
    return int4( mask.x != 0 ? a.x : a.x + b.x, mask.y != 0 ? a.y : a.y + b.y, mask.z != 0 ? a.z : a.z + b.z,
                 mask.w != 0 ? a.w : a.w + b.w );
}

int4 mask_sub( int4 a, int4 b, int4 mask )
{
    return int4( mask.x != 0 ? a.x - b.x : a.x, mask.y != 0 ? a.y - b.y : a.y, mask.z != 0 ? a.z - b.z : a.z,
                 mask.w != 0 ? a.w - b.w : a.w );
}

int4 nmask_sub( int4 a, int4 b, int4 mask )
{
    return int4( mask.x != 0 ? a.x : a.x - b.x, mask.y != 0 ? a.y : a.y - b.y, mask.z != 0 ? a.z : a.z - b.z,
                 mask.w != 0 ? a.w : a.w - b.w );
}

int4 mask_mul( int4 a, int4 b, int4 mask )
{
    return int4( mask.x != 0 ? a.x * b.x : a.x, mask.y != 0 ? a.y * b.y : a.y, mask.z != 0 ? a.z * b.z : a.z,
                 mask.w != 0 ? a.w * b.w : a.w );
}

int4 nequal( int4 a, int4 b )
{
    return int4( a.x != b.x, a.y != b.y, a.z != b.z, a.w != b.w );
}

int4 hash_primes( int4 seed, int4 x, int4 y )
{
    int4 hash = seed;
    hash ^= x ^ y;
    hash *= int4( 0x27d4eb2d );
    return ( hash >> int4( 15 ) ) ^ hash;
}


float4 gradient_dot( int4 hash, float4 x, float4 y )
{
    int4 bit1 = hash << int4( 31 );
    int4 bit2 = ( hash >> int4( 1 ) ) << 31;
    int4 mask = nequal( int4( hash & int4( 1 << 2 ) ), int4( 0 ) );
    x         = intBitsToFloat( floatBitsToInt( x ) ^ floatBitsToInt( bit1 ) );
    y         = intBitsToFloat( floatBitsToInt( y ) ^ floatBitsToInt( bit2 ) );
    float4 a  = select( mask, y, x );
    float4 b  = select( mask, x, y );
    return ( float4( 1.0f ) + ROOT2 ) * a + b;
}

float4 nmask( float4 a, int4 m )
{
    return select( m, float4( 0.0 ), a );
}

float4 gradient_dot_fancy( int4 hash, float4 x, float4 y )
{
    int4 index = int4( float4( hash & int4( 0x3FFFFF ) ) * float4( 1.3333333333333333f ) );

    int4   xy  = index << 29;
    float4 a   = select( xy, y, x );
    float4 b   = select( xy, x, y );
    b          = intBitsToFloat( floatBitsToInt( b ) ^ ( index << 31 ) );
    int4 amul2 = ( index << 30 ) >> 31;
    a *= select( amul2, float4( 2.0 ), ROOT3 );
    b = nmask( b, amul2 );

    // Bit-8 = Flip sign of a + b
    return intBitsToFloat( floatBitsToInt( a + b ) ^ ( ( index >> 3 ) << 31 ) );
}
