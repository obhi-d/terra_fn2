#include "FastSIMD/InlInclude.h"

#include "Blends.h"

template<typename FS>
class FS_T<FastNoise::Add, FS> : public virtual FastNoise::Add, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename Input>
    FS_INLINE void GenBlockT( Params const& params, Uniform const& u, Input& i, Output& o ) const
    {
        constexpr auto              N = Input::N;
        typename Output::LocalBlock o2b;
        Output                      o2( o2b );
        this->GetSourceValue( mLHS, params, u, i, o );
        this->GetSourceValue( mRHS, params, u, i, o2 );
        auto BlockSize = i.MaxVectorsInBlock();
        for( std::uint32_t b = 0; b < BlockSize; ++b )
            o.output[b] = o.output[b] + o2.output[b];
    }
};

template<typename FS>
class FS_T<FastNoise::Subtract, FS> : public virtual FastNoise::Subtract, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename Input>
    FS_INLINE void GenBlockT( Params const& params, Uniform const& u, Input& i, Output& o ) const
    {
        constexpr auto              N = Input::N;
        typename Output::LocalBlock o2b;
        Output                      o2( o2b );
        this->GetSourceValue( mLHS, params, u, i, o );
        this->GetSourceValue( mRHS, params, u, i, o2 );
        auto BlockSize = i.MaxVectorsInBlock();
        for( std::uint32_t b = 0; b < BlockSize; ++b )
            o.output[b] = o.output[b] - o2.output[b];
    }
};

template<typename FS>
class FS_T<FastNoise::Multiply, FS> : public virtual FastNoise::Multiply, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename Input>
    FS_INLINE void GenBlockT( Params const& params, Uniform const& u, Input& i, Output& o ) const
    {
        constexpr auto              N = Input::N;
        typename Output::LocalBlock o2b;
        Output                      o2( o2b );
        this->GetSourceValue( mLHS, params, u, i, o );
        this->GetSourceValue( mRHS, params, u, i, o2 );
        auto BlockSize = i.MaxVectorsInBlock();
        for( std::uint32_t b = 0; b < BlockSize; ++b )
            o.output[b] = o.output[b] * o2.output[b];
    }
};

template<typename FS>
class FS_T<FastNoise::Divide, FS> : public virtual FastNoise::Divide, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename Input>
    FS_INLINE void GenBlockT( Params const& params, Uniform const& u, Input& i, Output& o ) const
    {
        constexpr auto              N = Input::N;
        typename Output::LocalBlock o2b;
        Output                      o2( o2b );
        this->GetSourceValue( mLHS, params, u, i, o );
        this->GetSourceValue( mRHS, params, u, i, o2 );
        auto BlockSize = i.MaxVectorsInBlock();
        for( std::uint32_t b = 0; b < BlockSize; ++b )
            o.output[b] = o.output[b] / o2.output[b];
    }
};

template<typename FS>
class FS_T<FastNoise::PowFloat, FS> : public virtual FastNoise::PowFloat, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename Input>
    FS_INLINE void GenBlockT( Params const& params, Uniform const& u, Input& i, Output& o ) const
    {
        constexpr auto              N = Input::N;
        typename Output::LocalBlock o2b;
        Output                      o2( o2b );
        this->GetSourceValue( mValue, params, u, i, o );
        this->GetSourceValue( mPow, params, u, i, o2 );
        auto BlockSize = i.MaxVectorsInBlock();
        for( std::uint32_t b = 0; b < BlockSize; ++b )
            o.output[b] = FS_Pow_f32( o.output[b], o2.output[b] );
    }
};

template<typename FS>
class FS_T<FastNoise::PowInt, FS> : public virtual FastNoise::PowInt, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename Input>
    FS_INLINE void GenBlockT( Params const& params, Uniform const& u, Input& i, Output& o ) const
    {
        constexpr auto N = Input::N;
        this->GetSourceValue( mValue, params, u, i, o );
        auto BlockSize = i.MaxVectorsInBlock();
        for( std::uint32_t b = 0; b < BlockSize; ++b )
        {
            auto value = o.output[b];
            for( int i = 1; i < mPow; ++i )
                o.output[b] *= value;
        }
    }
};

template<typename FS>
class FS_T<FastNoise::Min, FS> : public virtual FastNoise::Min, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename Input>
    FS_INLINE void GenBlockT( Params const& params, Uniform const& u, Input& i, Output& o ) const
    {
        constexpr auto              N = Input::N;
        typename Output::LocalBlock o2b;
        Output                      o2( o2b );
        this->GetSourceValue( mLHS, params, u, i, o );
        this->GetSourceValue( mRHS, params, u, i, o2 );
        auto BlockSize = i.MaxVectorsInBlock();
        for( std::uint32_t b = 0; b < BlockSize; ++b )
            o.output[b] = FS_Min_f32( o.output[b], o2.output[b] );
    }
};

template<typename FS>
class FS_T<FastNoise::Max, FS> : public virtual FastNoise::Max, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename Input>
    FS_INLINE void GenBlockT( Params const& params, Uniform const& u, Input& i, Output& o ) const
    {
        constexpr auto              N = Input::N;
        typename Output::LocalBlock o2b;
        Output                      o2( o2b );
        this->GetSourceValue( mLHS, params, u, i, o );
        this->GetSourceValue( mRHS, params, u, i, o2 );
        auto BlockSize = i.MaxVectorsInBlock();
        for( std::uint32_t b = 0; b < BlockSize; ++b )
            o.output[b] = FS_Max_f32( o.output[b], o2.output[b] );
    }
};

template<typename FS>
class FS_T<FastNoise::MinSmooth, FS> : public virtual FastNoise::MinSmooth, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename Input>
    FS_INLINE void GenBlockT( Params const& params, Uniform const& u, Input& i, Output& o ) const
    {
        constexpr auto              N = Input::N;
        typename Output::LocalBlock o2b, o3b;
        Output                      o2( o2b );
        Output                      o3( o3b );
        this->GetSourceValue( mLHS, params, u, i, o );
        this->GetSourceValue( mRHS, params, u, i, o2 );
        this->GetSourceValue( mSmoothness, params, u, i, o3 );
        auto BlockSize = i.MaxVectorsInBlock();
        for( std::uint32_t bs = 0; bs < BlockSize; ++bs )
        {
            float32v a          = o.output[bs];
            float32v b          = o2.output[bs];
            float32v smoothness = FS_Max_f32( float32v( 1.175494351e-38f ), FS_Abs_f32( o3.output[bs] ) );
            float32v h          = FS_Max_f32( smoothness - FS_Abs_f32( a - b ), float32v( 0.0f ) );
            h *= FS_Reciprocal_f32( smoothness );
            o.output[bs] = FS_FNMulAdd_f32( float32v( 1.0f / 6.0f ), h * h * h * smoothness, FS_Min_f32( a, b ) );
        }
    }
};

template<typename FS>
class FS_T<FastNoise::MaxSmooth, FS> : public virtual FastNoise::MaxSmooth, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename Input>
    FS_INLINE void GenBlockT( Params const& params, Uniform const& u, Input& i, Output& o ) const
    {
        constexpr auto              N = Input::N;
        typename Output::LocalBlock o2b, o3b;
        Output                      o2( o2b );
        Output                      o3( o3b );
        this->GetSourceValue( mLHS, params, u, i, o );
        this->GetSourceValue( mRHS, params, u, i, o2 );
        this->GetSourceValue( mSmoothness, params, u, i, o3 );
        auto BlockSize = i.MaxVectorsInBlock();
        for( std::uint32_t bs = 0; bs < BlockSize; ++bs )
        {
            float32v a          = o.output[bs];
            float32v b          = o2.output[bs];
            float32v smoothness = FS_Max_f32( float32v( 1.175494351e-38f ), FS_Abs_f32( o3.output[bs] ) );
            float32v h          = FS_Max_f32( smoothness - FS_Abs_f32( a - b ), float32v( 0.0f ) );
            h *= FS_Reciprocal_f32( smoothness );
            o.output[bs] = -FS_FNMulAdd_f32( float32v( 1.0f / 6.0f ), h * h * h * smoothness, FS_Min_f32( a, b ) );
        }
    }
};

template<typename FS>
class FS_T<FastNoise::Fade, FS> : public virtual FastNoise::Fade, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename Input>
    FS_INLINE void GenBlockT( Params const& params, Uniform const& u, Input& i, Output& o ) const
    {
        constexpr auto              N = Input::N;
        typename Output::LocalBlock o2b, o3b;
        Output                      o2( o2b );
        Output                      o3( o3b );
        this->GetSourceValue( mFade, params, u, i, o );
        this->GetSourceValue( mA, params, u, i, o2 );
        this->GetSourceValue( mB, params, u, i, o3 );
        auto BlockSize = i.MaxVectorsInBlock();
        for( std::uint32_t b = 0; b < BlockSize; ++b )
        {
            float32v fade = FS_Abs_f32( o.output[b] );
            o.output[b]   = FS_FMulAdd_f32( o2.output[b], float32v( 1 ) - fade, o3.output[b] * fade );
        }
    }
};
