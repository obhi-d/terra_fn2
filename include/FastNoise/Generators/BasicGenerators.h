#pragma once
#include "CurveData.h"
#include "Generator.h"

namespace FastNoise
{
    class Constant : public virtual Generator
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );
        const Metadata& GetMetadata() const override;

        void SetValue( float value )
        {
            mValue = value;
        }

    protected:
        float mValue = 1.0f;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<Constant> : MetadataT<Generator>
    {
        SmartNode<> CreateNode( FastSIMD::eLevel ) const override;

        MetadataT()
        {
            groups.push_back( "Basic Generators" );
            this->AddVariable( "Value", 1.0f, &Constant::SetValue );
        }
    };
#endif

    class White : public virtual Generator
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );
        const Metadata& GetMetadata() const override;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<White> : MetadataT<Generator>
    {
        SmartNode<> CreateNode( FastSIMD::eLevel ) const override;

        MetadataT()
        {
            groups.push_back( "Basic Generators" );
        }
    };
#endif

    class Checkerboard : public virtual Generator
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );
        const Metadata& GetMetadata() const override;

        void SetSize( float value )
        {
            mSize = value;
        }

    protected:
        float mSize = 1.0f;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<Checkerboard> : MetadataT<Generator>
    {
        SmartNode<> CreateNode( FastSIMD::eLevel ) const override;

        MetadataT()
        {
            groups.push_back( "Basic Generators" );
            this->AddVariable( "Size", 1.0f, &Checkerboard::SetSize );
        }
    };
#endif

    class SineWave : public virtual Generator
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );
        const Metadata& GetMetadata() const override;

        void SetScale( float value )
        {
            mScale = value;
        }

    protected:
        float mScale = 1.0f;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<SineWave> : MetadataT<Generator>
    {
        SmartNode<> CreateNode( FastSIMD::eLevel ) const override;

        MetadataT()
        {
            groups.push_back( "Basic Generators" );
            this->AddVariable( "Scale", 1.0f, &SineWave::SetScale );
        }
    };
#endif

    class PositionOutput : public virtual Generator
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );
        const Metadata& GetMetadata() const override;

        template<Dim D>
        void Set( float multiplier, float offset = 0.0f )
        {
            mMultiplier[(int)D] = multiplier;
            mOffset[(int)D]     = offset;
        }

    protected:
        PerDimensionVariable<float> mMultiplier = 0.0f;
        PerDimensionVariable<float> mOffset     = 0.0f;

        template<typename T>
        friend struct MetadataT;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<PositionOutput> : MetadataT<Generator>
    {
        SmartNode<> CreateNode( FastSIMD::eLevel ) const override;

        MetadataT()
        {
            groups.push_back( "Basic Generators" );
            this->AddPerDimensionVariable( "Multiplier", 0.0f,
                                           []( PositionOutput* p ) { return std::ref( p->mMultiplier ); } );
            this->AddPerDimensionVariable( "Offset", 0.0f, []( PositionOutput* p ) { return std::ref( p->mOffset ); } );
        }
    };
#endif

    class DistanceToPoint : public virtual Generator
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );
        const Metadata& GetMetadata() const override;

        void SetSource( SmartNodeArg<> gen )
        {
            this->SetSourceMemberVariable( mSource, gen );
        }
        void SetDistanceFunction( DistanceFunction value )
        {
            mDistanceFunction = value;
        }

        template<Dim D>
        void SetScale( float value )
        {
            mPoint[(int)D] = value;
        }

    protected:
        GeneratorSource             mSource;
        DistanceFunction            mDistanceFunction = DistanceFunction::EuclideanSquared;
        PerDimensionVariable<float> mPoint            = 0.0f;

        template<typename T>
        friend struct MetadataT;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<DistanceToPoint> : MetadataT<Generator>
    {
        SmartNode<> CreateNode( FastSIMD::eLevel ) const override;

        MetadataT()
        {
            groups.push_back( "Basic Generators" );
            this->AddVariableEnum( "Distance Function", DistanceFunction::Euclidean,
                                   &DistanceToPoint::SetDistanceFunction, kDistanceFunction_Strings );
            this->AddPerDimensionVariable( "Point", 0.0f, []( DistanceToPoint* p ) { return std::ref( p->mPoint ); } );
        }
    };
#endif


    enum class Sampling
    {
        e1x,
        e2x,
        e3x,
        e4x
    };

    constexpr static const char* kSampling[] = {
        "x1",
        "x2",
        "x3",
        "x4",
    };


    class StrataMask : public virtual Generator
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );
        const Metadata& GetMetadata() const override;

        void SetSampling( Sampling sampling )
        {
            mSampling = sampling;
        }

        void SetMinScale( float iLevel )
        {
            mMinScale = iLevel;
        }

        void SetMaxScale( float iLevel )
        {
            mMaxScale = iLevel;
        }

        void SetImage( ImageData const& val )
        {
            mImage = &val;
        }

    protected:
        float mMinScale = 0.0f;
        float mMaxScale = 1.0f;

        ImageData const*               mImage    = nullptr;
        PerDimensionVariable<bool, 2>  mMirror   = false;
        PerDimensionVariable<float, 2> mOffset   = {};
        PerDimensionVariable<float, 2> mScale    = {};
        Sampling                       mSampling = Sampling::e1x;

        template<typename T>
        friend struct MetadataT;
    };


#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<StrataMask> : MetadataT<Generator>
    {
        MetadataT()
        {
            groups.push_back( "Basic Generators" );
            this->AddVariableEnum( "Sampling", Sampling::e1x, &StrataMask::SetSampling, kSampling );
            this->AddVariableImage( "StrataSource", &StrataMask::SetImage );
            this->AddVariable( "Min", -1.0f, &StrataMask::SetMinScale );
            this->AddVariable( "Max", 1.0f, &StrataMask::SetMaxScale );
            this->AddPerDimensionVariable( "Mirror", true, []( StrataMask* p ) { return std::ref( p->mMirror ); } );
            this->AddPerDimensionVariable(
                "Offset", 0.0f, []( StrataMask* p ) { return std::ref( p->mOffset ); }, 0.0f, 1.0f );
            this->AddPerDimensionVariable( "Scale", 1.0f, []( StrataMask* p ) { return std::ref( p->mScale ); } );
        }

        SmartNode<> CreateNode( FastSIMD::eLevel ) const override;
    };

#endif

    class CurveGen : public virtual Generator
    {

    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );
        const Metadata& GetMetadata() const override;

        void SetCurve( FastNoise::CurveData const& data )
        {
            mCurve = data.spline;
        }


    protected:
        PerDimensionVariable<bool, 2>  mApply    = false;
        PerDimensionVariable<float, 2> mStrength = false;

        tk::spline<> mCurve;
        template<typename T>
        friend struct MetadataT;
    };


#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<CurveGen> : MetadataT<Generator>
    {
        MetadataT()
        {
            groups.push_back( "Basic Generators" );
            this->AddVariableCurve( "Curve", &CurveGen::SetCurve );
            this->AddPerDimensionVariable( "Apply", true, []( CurveGen* p ) { return std::ref( p->mApply ); } );
            this->AddPerDimensionVariable( "Strength", 1.0f, []( CurveGen* p ) { return std::ref( p->mStrength ); } );
        }

        SmartNode<> CreateNode( FastSIMD::eLevel ) const override;
    };

#endif
} // namespace FastNoise
