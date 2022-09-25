
#include <Magnum/GL/GL.h>
#include "Common.h"

using namespace Magnum;
namespace terra
{

    class NoiseParams
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

    class NoiseShader
    {
    public:
    };

} // namespace terra
