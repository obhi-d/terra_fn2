#extension GL_ARB_explicit_attrib_location: enable
#extension GL_ARB_shading_language_420pack: enable
#extension GL_ARB_explicit_uniform_location: enable
#extension GL_ARB_shader_bit_encoding: enable
#extension GL_ARB_gpu_shader5 : enable
#extension GL_ARB_shading_language_packing : enable

#define UniformLocation(x) layout(location = x) 


// Uniform buffers */

UniformLocation(3) uniform vec2 heightMinMax = vec2(0.0f, 1.0f);
UniformLocation(4) uniform highp mat4 transformationProjectionMatrix  = mat4(1.0);
UniformLocation(6) uniform vec3 sunDirection;
#ifdef EQUAL_PREC
UniformLocation(7) uniform float compressSpec;
#else
UniformLocation(7) uniform int compressSpec;
#endif


#ifdef HAS_COMPRESSED_NORMALS

// https://www.shadertoy.com/view/Mtfyzl

vec2 msign( vec2 v )
{
    return vec2( (v.x>=0.0) ? 1.0 : -1.0, 
                (v.y>=0.0) ? 1.0 : -1.0 );
}

vec3 i_octahedral_32( uint data, uint sh )
{
    uint mu =(1u<<sh)-1u;
    
    uvec2 d = uvec2( data, data>>sh ) & mu;
    vec2 v = vec2(d)/float(mu);
    
    v = -1.0 + 2.0*v;
    vec3 nor = vec3(v, 1.0 - abs(v.x) - abs(v.y));
    float t = max(-nor.z,0.0);
    nor.x += (nor.x>0.0)?-t:t;
    nor.y += (nor.y>0.0)?-t:t;
    return normalize( nor );
}


uint octahedral_32( in vec3 nor )
{
    nor /= ( abs( nor.x ) + abs( nor.y ) + abs( nor.z ) );
    nor.xy = (nor.z >= 0.0) ? nor.xy : (1.0-abs(nor.yx))*msign(nor.xy);
    return packSnorm2x16(nor.xy);
}

vec3 i_octahedral_32( uint data )
{
    vec2 v = unpackSnorm2x16(data);
    vec3 nor = vec3(v, 1.0 - abs(v.x) - abs(v.y)); // Rune Stubbe's version,
    float t = max(-nor.z,0.0);                     // much faster than original
    nor.x += (nor.x>0.0)?-t:t;                     // implementation of this
    nor.y += (nor.y>0.0)?-t:t;                     // technique
    
    return normalize( nor );
}

out highp vec3 interpolatedNormal;

#endif

#if defined(HAS_COMPRESSED_NORMALS) || defined(HAS_TRUE_NORMAL)
out highp float height;
#else
out highp vec2 interpolatedLight;
#endif

/* Inputs */

layout(location = 0) in highp vec4 positionLight;

#ifdef HAS_TRUE_NORMAL
  layout(location = 1) in highp vec4 normal;
#endif

/* Outputs */


void main() 
{
    gl_Position = transformationProjectionMatrix * vec4(positionLight.x, positionLight.y, positionLight.z, 1.0);
#if defined(HAS_COMPRESSED_NORMALS) || defined(HAS_TRUE_NORMAL)
#ifdef HAS_TRUE_NORMAL
    interpolatedNormal = normal.xyz;
#else
#ifdef EQUAL_PREC
    interpolatedNormal = i_octahedral_32(floatBitsToUint(positionLight.w));
#else
    interpolatedNormal = i_octahedral_32(floatBitsToUint(positionLight.w), compressSpec);
#endif
#endif
    height = (positionLight.y - heightMinMax.x) / (heightMinMax.y - heightMinMax.x);
#else
    interpolatedLight.xy = positionLight.wy;
#endif   
}
