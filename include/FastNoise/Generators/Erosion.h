#pragma once
#include "Generator.h"


namespace FastNoise
{
    class Erosion : public virtual Generator
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );
        const Metadata& GetMetadata() const override;


    protected:
        GeneratorSource mSource;
        int             mIteration        = 1;
        int             mSeedOffset       = 0;
        float           mRainRate         = 0.0008f;
        float           mEvaporation      = 0.0005f;
        float           mMinHeightDelta   = 0.05f;
        float           mReposeSlope      = 0.03f;
        float           mGravity          = 30.0f;
        float           mGradientSigma    = 0.5f;
        float           mSedimentCapacity = 50.0f;
        float           mDissolvingRate   = 0.25f;
        float           mDepositingRate   = 0.001f;
    };
} // namespace FastNoise
