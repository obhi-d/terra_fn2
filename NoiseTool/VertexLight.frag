#extension GL_ARB_explicit_attrib_location: enable
#extension GL_ARB_shading_language_420pack: enable
#extension GL_ARB_explicit_uniform_location: enable
#extension GL_ARB_shader_bit_encoding: enable
#extension GL_ARB_gpu_shader5 : enable
#extension GL_ARB_shading_language_packing : enable

#define UniformLocation(x) layout(location = x) 


// Uniform Buffers 
UniformLocation(3) uniform highp vec4 sunColor = vec4(1.0);
UniformLocation(1) uniform sampler1D heightColorMap;

#ifdef HAS_COMPRESSED_NORMALS
// https://www.shadertoy.com/view/Mtfyzl

UniformLocation(4) uniform vec3 sunDirection;
#ifdef EQUAL_PREC
UniformLocation(5) uniform float compressSpec;
#else
UniformLocation(5) uniform int compressSpec;
#endif

in highp vec3 interpolatedNormal;

#endif

#if defined(HAS_COMPRESSED_NORMALS) || defined(HAS_TRUE_NORMAL)
in highp float height;
#else
in highp vec2 interpolatedLight;
#endif


// Outputs */

layout(location = 0) out highp vec4 fragmentColor;

void main() 
{
    highp vec4 light;
#if defined(HAS_COMPRESSED_NORMALS) || defined(HAS_TRUE_NORMAL)
        light = texture(heightColorMap, height);
#else
        light = texture(heightColorMap, interpolatedLight.y) * interpolatedLight.x;
#endif
    if(!gl_FrontFacing) 
    { 
        light = (1.0 - light) * 0.08;
    }
    vec4 color = vec4(sunColor.xyz, 1.0);
    float intensity = sunColor.w;
#ifdef HAS_COMPRESSED_NORMALS
#ifdef EQUAL_PREC
    float factor = floor(dot(normalize(interpolatedNormal), sunDirection) * compressSpec) / compressSpec;
    fragmentColor = (color * 0.2 +  light * 0.8) * factor * intensity;
#else
    fragmentColor = (color * clamp(dot(normalize(interpolatedNormal), sunDirection), 0.0, 1.0 ) * light) * intensity;
#endif
#else
    fragmentColor = color * intensity * light;
#endif
}
