#extension GL_ARB_explicit_attrib_location: enable
#extension GL_ARB_shading_language_420pack: enable
#extension GL_ARB_explicit_uniform_location: enable
#extension GL_ARB_shader_bit_encoding: enable
#extension GL_ARB_gpu_shader5 : enable
#extension GL_ARB_shading_language_packing : enable

#define UniformLocation(x) layout(location = x) 


// Uniform Buffers 
UniformLocation(0) uniform sampler1D heightColorMapX;
UniformLocation(1) uniform sampler1D heightColorMapY;
UniformLocation(2) uniform sampler1D heightColorMapZ;
UniformLocation(5) uniform highp vec4 sunColor = vec4(1.0);

#ifdef HAS_COMPRESSED_NORMALS
// https://www.shadertoy.com/view/Mtfyzl

UniformLocation(6) uniform vec3 sunDirection;
#ifdef EQUAL_PREC
UniformLocation(7) uniform float compressSpec;
#else
UniformLocation(7) uniform int compressSpec;
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
    highp vec4 lightX = vec4(1.0);
    highp vec4 lightY = vec4(1.0);
    highp vec4 lightZ = vec4(1.0);
#if defined(HAS_COMPRESSED_NORMALS) || defined(HAS_TRUE_NORMAL)
        vec3 normal = normalize(interpolatedNormal);
        lightX = texture(heightColorMapX, clamp(normal.x * 0.5 + 0.5, 0.0, 1.0));
        lightY = texture(heightColorMapY, clamp(height * normal.y, 0.0, 1.0));
        lightZ = texture(heightColorMapZ, clamp(normal.z * 0.5 + 0.5, 0.0, 1.0));
#else
        lightY = texture(heightColorMapY, clamp(interpolatedLight.y, 0.0, 1.0)) * interpolatedLight.x;
#endif
    if(gl_FrontFacing) 
    { 
        lightY = (1.0 - lightY) * 0.08;
    }
    vec4 color = vec4(sunColor.xyz, 1.0);
    float intensity = sunColor.w;
#ifdef HAS_COMPRESSED_NORMALS
#ifdef EQUAL_PREC
    float factor = floor(clamp(dot(normal, sunDirection), 0.0, 1.0) * compressSpec) / compressSpec;
    fragmentColor = mix(color, lightX *.4 + lightY * .6 + lightZ * .4, factor) * factor * intensity;
#else
    fragmentColor = (color * clamp(dot(normal, sunDirection), 0.0, 1.0 ) * lightX * lightY * lightZ) * intensity;
#endif
#else
    fragmentColor = color * intensity * lightY;
#endif
}
