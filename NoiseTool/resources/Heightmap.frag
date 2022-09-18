#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_explicit_attrib_location: enable
#extension GL_ARB_shading_language_420pack: enable
#extension GL_ARB_explicit_uniform_location: enable


// Uniform Buffers 
layout(location = 2) uniform sampler1D heightColorMapX;
layout(location = 3) uniform sampler1D heightColorMapY;
layout(location = 4) uniform sampler1D heightColorMapZ;
layout(location = 5) uniform highp vec4 sunColor = vec4(1.0);
layout(location = 6) uniform highp vec3 sunDirection = vec3(1.0);
layout(location = 7) uniform highp float style = 10;

layout(location = 0) in highp vec3 worldPos;

layout(location = 0) out highp vec4 fragmentColor;

void main() 
{
    highp vec4 lightX = vec4(1.0);
    highp vec4 lightY = vec4(1.0);
    highp vec4 lightZ = vec4(1.0);

    vec3 x = dFdx(worldPos);
    vec3 y = dFdy(worldPos);
    
      
    vec3 normal = normalize(cross(x,y));
    
    lightX = texture(heightColorMapX, clamp(normal.x * 0.5 + 0.5, 0.0, 1.0));
    lightY = texture(heightColorMapY, clamp(worldPos.y * normal.y, 0.0, 1.0));
    lightZ = texture(heightColorMapZ, clamp(normal.z * 0.5 + 0.5, 0.0, 1.0));

    if(!gl_FrontFacing) 
    { 
        lightY = (1.0 - lightY) * 0.08;
    }

    vec4 color = vec4(sunColor.xyz, 1.0);
    float intensity = sunColor.w;

    float factor = floor(clamp(dot(normal, sunDirection), 0.0, 1.0) * style) / style;
    fragmentColor = mix(color, lightX * .4 + lightY * .6 + lightZ * .4, 0.8) * factor * intensity;
}
