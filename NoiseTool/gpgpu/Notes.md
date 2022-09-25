## Sources

If a data source is defined in shader parameters
for example -
source mySource;
When source is a GpuBuffer, code is generated as follows
```glsl
layout( std430, binding = x ) buffer b_source_x
{
    vec4 values[];
}source_x;
```
And in `noise_params`:
```glsl
float source_x;
bool  has_source_x;
```

Functions generated.
```glsl
float4 sample_x(int pixel)
{
  if (params.has_source_x)
  {
    return source_x.values[pixel];
  }
  else
    return params.source_x;
}
```

## Shaders

Declaration of parameters
```
<type> <name>[<min>, <max>] = <init>;
```

Allowed types:
- `float`
- `int`
- `float2`
- `int2`
- `source`
- `bool`
- `texture`
- `curve`


## CurveData
```
layout(std430, binding = x) buffer readonly curve_x
{
  int npoints;
  int ncoeff;
  float data[];
};
```
Sampler
```
float sample_curve_x(float x)
{
...
}
```