#extension GL_ARB_explicit_attrib_location: enable
#extension GL_ARB_shading_language_420pack: enable
#extension GL_ARB_explicit_uniform_location: enable


// Uniform buffers */
layout(location = 0) uniform highp mat4 transformationProjectionMatrix  = mat4(1.0);
layout(location = 1) uniform highp float heightMultiplier  = 1.0f;

layout(location = 0) out highp vec3 worldPos;

layout(location = 0) in highp vec2 posBuffer;
layout(location = 1) in highp float heightmap;


void main() 
{
    worldPos = vec3(posBuffer.x, heightmap * heightMultiplier, posBuffer.y);
    gl_Position = transformationProjectionMatrix * vec4(worldPos, 1.0);
}
