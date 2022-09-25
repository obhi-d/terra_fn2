
layout( local_size_x = BLOCK_SIZE, local_size_y = BLOCK_SIZE, local_size_z = 1 ) in;

layout( std430, binding = 0 ) buffer b_heights
{
    float4 values[];
}
heights;

layout( std430, binding = 0 ) uniform u_env
{
    env_params data;
}
env;

void main()
{
    int pixelStart =
        ( gl_GlobalInvocationID.y * gl_NumWorkGroups.x * gl_WorkGroupSize.x + gl_GlobalInvocationID.x ) * 4;

    int sx = pixelStart % env.data.bufferSize.x;
    int sy = pixelStart / env.data.bufferSize.x;

    float4 x =
        float4( env.data.x + float( sx + 0 ) * env.data.frequency, env.data.x + float( sx + 1 ) * env.data.frequency,
                env.data.x + float( sx + 2 ) * env.data.frequency, env.data.x + float( sx + 3 ) * env.data.frequency );

    float4 y =
        float4( env.data.y + float( sy + 0 ) * env.data.frequency, env.data.y + float( sy + 1 ) * env.data.frequency,
                env.data.y + float( sy + 2 ) * env.data.frequency, env.data.y + float( sy + 3 ) * env.data.frequency );

    heights.values[pixelStart] = noise( x, y, int3( sx, sy, pixelStart ), env.data );
}
