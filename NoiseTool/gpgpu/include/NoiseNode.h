#include <string>
#include <variant>

#include "Common.h"
#include "TextureData.h"

namespace terra
{
    enum class ParameterType
    {
        eInt,
        eFloat,
        eInt2,
        eFloat2,
        eBool,
        eTexture,
        eDataSource,
        eCurveData,
        eInvalid
    };

    struct ParameterInfo
    {
        std::string   name;
        uint32        uboOffset   = 0;
        uint32        availOffset = 0;
        uint32        binding     = 0;
        ParameterType type        = ParameterType::eInvalid;
        union
        {
            float fmax;
            int   imax = 0;
        };
        union
        {
            float fmin;
            int   imin = 0;
        };
        union
        {
            float fdefault;
            int   idefault = 0;
        };
    };

    class NoiseNodeDesc
    {
        std::vector<ParameterInfo> parameterDef;
    };

    using Parameter = std::variant<int, float, int2, float2, bool, TextureData, DataSource, CurveDataPtr>;
    class NoiseNode
    {
        NoiseNodeDesc&         desc;
        std::vector<Parameter> parameters;
    };
} // namespace terra