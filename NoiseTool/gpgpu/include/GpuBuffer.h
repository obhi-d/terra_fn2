
#include <memory>

#pragma once
namespace terra
{
    class GpuBuffer
    {
    };

    using GpuBufferRef = std::shared_ptr<GpuBuffer>;
} // namespace terra