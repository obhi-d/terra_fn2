
#pragma once

#include "GpuBuffer.h"
namespace terra
{
    class NoiseNode;
    using NoiseNodePtr = std::shared_ptr<NoiseNode>;
    class DataSource
    {
        NoiseNodePtr source       = nullptr;
        float        defaultValue = 0.0f;
    };
} // namespace terra