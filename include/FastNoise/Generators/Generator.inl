#pragma once
#include <algorithm>
#include <cmath>
#include <execution>
#include <ranges>
#include <thread>

#include <cassert>
#include <cstdint>
#include <cstring>
#include "FastSIMD/InlInclude.h"

#include "Generator.h"

#ifdef FS_SIMD_CLASS
#pragma warning( disable : 4250 )
#endif


template<typename FS>
class FS_T<FastNoise::Generator, FS> : public virtual FastNoise::Generator
{
    FASTSIMD_DECLARE_FS_TYPES;


public:
    using uint                                     = std::uint32_t;
    using DimInt                                   = std::array<int, FastNoise::DimCount>;
    using DimFloat                                 = std::array<float, FastNoise::DimCount>;
    static constexpr std::uint32_t VectorsPerBlock = 64;
    using GeneratorInput                           = FastNoise::GeneratorInput;

    struct Uniform
    {
        float32v wavelength;
        float32v vrecipSize[FastNoise::DimCount];
        float32v vhalfRecipGridSize[FastNoise::DimCount];
        float32v vrecipGridSize[FastNoise::DimCount];

        DimFloat offset            = {};
        DimFloat size              = {};
        DimFloat center            = {};
        DimFloat gridSize          = {};
        DimFloat recipSize         = {};
        DimFloat recipGridSize     = {};
        DimFloat halfRecipGridSize = {};
        DimInt   startCoord        = {};

        float                 freq = {};
        int                   seed = 0;
        GeneratorInput const& ctx;

        Uniform( Uniform const& ) = default;
        Uniform( GeneratorInput const& vctx ) : ctx( vctx )
        {
        }

        void ComputeDerived( uint N )
        {
            for( uint i = 0; i < N; ++i )
            {
                recipSize[i]          = 1.0f / size[i];
                halfRecipGridSize[i]  = 0.5f * recipSize[i];
                recipGridSize[i]      = 1.0f / gridSize[i];
                center[i]             = offset[i] + ( size[i] * 0.5f );
                vrecipSize[i]         = float32v( recipSize[i] );
                vhalfRecipGridSize[i] = float32v( halfRecipGridSize[i] );
                vrecipGridSize[i]     = float32v( recipGridSize[i] );
            }


            wavelength = float32v( 1 / freq );
        }
    };

    struct Params
    {
        int32v seed;
    };

    template<unsigned int DimSize>
    struct BlockInput
    {
        // MinMax
        using DimVec   = std::array<float32v, DimSize>;
        using DimIVec  = std::array<int32v, DimSize>;
        using BlockVec = std::array<DimVec, VectorsPerBlock>;

        static constexpr auto N = DimSize;

        BlockVec v;
        uint     nbValues = 0;

        BlockInput() = default;
        BlockInput( uint v ) : nbValues( v )
        {
        }

        DimVec GridId( uint b, Uniform const& u )
        {
            DimVec uv;
            for( uint i = 0; i < N; ++i )
                uv[i] = FS_Ceil_f32( ( v[b][i] ) * u.vrecipGridSize[i] + float32v( 0.5f ) );
            return uv;
        }

        // Un modded
        DimVec UV( uint b, Uniform const& u ) const
        {
            DimVec uv;
            uv[0] = ( v[b][0] ) * u.vrecipGridSize[0] + float32v( 0.5f );
            uv[0] = FS_Abs_f32( FS_Select_f32( uv[0] > float32v( 0.0f ), FS_Floor_f32( uv[0] ) - uv[0],
                                               uv[0] - FS_Ceil_f32( uv[0] ) ) );

            uv[1] = ( v[b][1] ) * u.vrecipGridSize[1] + float32v( 0.5f );
            uv[1] = FS_Abs_f32( FS_Select_f32( uv[1] > float32v( 0.0f ), FS_Floor_f32( uv[1] ) - uv[1],
                                               uv[1] - FS_Ceil_f32( uv[1] ) ) );

            return uv;
        }


        float32v SquareDistance( uint b, Uniform const& u ) const
        {
            auto uv = UV( b, u );

            float32v sum = float32v( 0.0f );
            DimVec   d;
            for( uint i = 0; i < N; ++i )
            {
                d[i] = uv[i] - float32v( 0.5f );
                d[i] = float32v( 2.0f ) * d[i] * d[i];
                sum += d[i];
            }
            return sum;
        }

        float32v LinearDistance( uint b, Uniform const& u ) const
        {
            auto uv = UV( b, u );

            float32v sum = float32v( 0.0f );
            DimVec   d;
            for( uint i = 0; i < N; ++i )
            {
                d[i] = uv[i] - float32v( 0.5f );
                sum += FS_Abs_f32( d[i] );
            }
            return ( sum );
        }

        void Multiply( float32v scale )
        {
            for( uint bs = 0; bs < VectorsPerBlock; bs++ )
                for( uint i = 0; i < N; ++i )
                    v[bs][i] *= scale;
        }

        uint size() const
        {
            return nbValues;
        }

        uint MaxVectorsInBlock() const
        {
            return ( ( nbValues + FS_Size_32() - 1 ) / FS_Size_32() );
        }
    };

    struct Output
    {
        template<uint N>
        struct HeapBlock
        {
            float32v output[VectorsPerBlock * N];

            float32v* operator[]( uint i )
            {
                return output + i * VectorsPerBlock;
            }
        };

        struct LocalBlock
        {
            float32v output[VectorsPerBlock];
        };


        float32v*               output /*[VectorsPerBlock]*/ = nullptr;
        FastNoise::OutputMinMax minMax;
        Output() = default;

        Output( LocalBlock& local ) : output( local.output )
        {
        }
        Output( float32v* local ) : output( local )
        {
        }

        float32v& operator[]( uint i )
        {
            return output[i];
        }

        float32v operator[]( uint i ) const
        {
            return output[i];
        }

        void Fill( float32v value )
        {
            for( uint i = 0; i < VectorsPerBlock; ++i )
                output[i] = value;
        }


        void DoMinMax( uint iTotal )
        {
            float32v min( minMax.min );
            float32v max( minMax.max );

            constexpr auto limit    = VectorsPerBlock * FS_Size_32();
            auto           minMaxLt = ( iTotal ) / FS_Size_32();

            for( std::uint32_t i = 0; i < minMaxLt; ++i )
            {
                min = FS_Min_f32( min, output[i] );
                max = FS_Max_f32( max, output[i] );
            }

            float* minP = reinterpret_cast<float*>( &min );
            float* maxP = reinterpret_cast<float*>( &max );
            for( size_t i = 0; i < FS_Size_32(); i++ )
            {
                minMax << FastNoise::OutputMinMax { minP[i], maxP[i] };
            }

            auto   remaining = iTotal % FS_Size_32();
            float* foutp     = (float*)( output + minMaxLt );
            for( size_t i = 0; i < remaining; i++ )
            {
                minMax << foutp[i];
            }
        }
    };

    void ApplyChanges() override
    {
    }


#define FASTNOISE_DECL_GEN_T( N )                                                                                      \
    virtual void FS_VECTORCALL GenBlock( Params const&, Uniform const&, BlockInput<N>&, Output& ) const = 0

    FASTNOISE_DECL_GEN_T( 2 );

#define FASTNOISE_IMPL_GEN_T_N( N )                                                                                    \
    void FS_VECTORCALL GenBlock( Params const& p, Uniform const& u, BlockInput<N>& i, Output& o ) const override       \
    {                                                                                                                  \
        GenBlockT( p, u, i, o );                                                                                       \
    }

#define FASTNOISE_IMPL_GEN_T                                                                                           \
    using Uniform                                  = typename FS_T<FastNoise::Generator, FS>::Uniform;                 \
    using Output                                   = typename FS_T<FastNoise::Generator, FS>::Output;                  \
    using Params                                   = typename FS_T<FastNoise::Generator, FS>::Params;                  \
    using uint                                     = typename FS_T<FastNoise::Generator, FS>::uint;                    \
    static constexpr std::uint32_t VectorsPerBlock = FS_T<FastNoise::Generator, FS>::VectorsPerBlock;                  \
    template<unsigned int D>                                                                                           \
    using BlockInput = typename FS_T<FastNoise::Generator, FS>::template BlockInput<D>;                                \
    using FS_T<FastNoise::Generator, FS>::GetSourceValue;                                                              \
    using FS_T<FastNoise::Generator, FS>::GetSourceSIMD;                                                               \
                                                                                                                       \
    FASTNOISE_IMPL_GEN_T_N( 2 )


    FastSIMD::eLevel GetSIMDLevel() const final
    {
        return FS::SIMD_Level;
    }

    using VoidPtrStorageType = const FS_T<Generator, FS>*;

    void SetSourceSIMDPtr( const Generator* base, const void** simdPtr ) final
    {
        if( !base )
        {
            *simdPtr = nullptr;
            return;
        }
        auto simd = dynamic_cast<VoidPtrStorageType>( base );

        assert( simd );
        *simdPtr = reinterpret_cast<const void*>( simd );
    }

    template<typename T, const std::uint32_t N>
    FS_INLINE void FS_VECTORCALL GetSourceValue( const FastNoise::HybridSourceT<T>& memberVariable, Params const& p,
                                                 Uniform const& u, BlockInput<N>& i, Output& o ) const
    {
        if( memberVariable.simdGeneratorPtr )
        {
            auto simdGen = reinterpret_cast<VoidPtrStorageType>( memberVariable.simdGeneratorPtr );
            simdGen->GenBlock( p, u, i, o );
        }
        else
            o.Fill( float32v( memberVariable.constant ) );
    }

    template<typename T, const std::uint32_t N>
    FS_INLINE void FS_VECTORCALL GetSourceValue( const FastNoise::GeneratorSourceT<T>& memberVariable, Params const& p,
                                                 Uniform const& u, BlockInput<N>& i, Output& o ) const
    {
        assert( memberVariable.simdGeneratorPtr );
        auto simdGen = reinterpret_cast<VoidPtrStorageType>( memberVariable.simdGeneratorPtr );
        simdGen->GenBlock( p, u, i, o );
    }

    template<typename T>
    FS_INLINE const FS_T<T, FS>* GetSourceSIMD( const FastNoise::GeneratorSourceT<T>& memberVariable ) const
    {
        assert( memberVariable.simdGeneratorPtr );
        auto simdGen = reinterpret_cast<VoidPtrStorageType>( memberVariable.simdGeneratorPtr );

        auto simdT = static_cast<const FS_T<T, FS>*>( simdGen );
        return simdT;
    }

    template<std::uint32_t DimSize>
    void GenUniformGrid( GeneratorInput& gi ) const
    {
        /// ==============================
        using BlockTy = BlockInput<DimSize>;

        float32v freqV( gi.frequency );

        int32v Idx[DimSize];
        int32v Max[DimSize];
        int32v Size[DimSize];

        size_t totalValues = 1;

        auto uniform = Uniform( gi );

        uniform.freq = (float)gi.frequency;
        uniform.seed = gi.seed;

        Params params;
        params.seed = int32v( gi.seed );

        for( std::uint32_t i = 0; i < DimSize; ++i )
        {
            totalValues *= gi.size[i];
            Idx[i]  = int32v( gi.start[i] );
            Size[i] = int32v( gi.size[i] );
            Max[i]  = Size[i] + Idx[i] + int32v( -1 );

            uniform.startCoord[i] = gi.start[i];
            uniform.offset[i]     = (float)( gi.start[i] ) * gi.frequency;
            uniform.size[i]       = (float)( gi.size[i] - 1 ) * gi.frequency;
            uniform.gridSize[i]   = (float)( gi.gridSize[i] ) * gi.frequency;
        }

        uniform.ComputeDerived( DimSize );

        Idx[0] += int32v::FS_Incremented();

        {
            size_t mulSize = 1;
            for( std::uint32_t i = 0; i < DimSize - 1; ++i )
            {
                mulSize *= gi.size[i];
                AxisReset<true>( Idx[i], Idx[i + 1], Max[i], Size[i], mulSize );
            }
        }

        auto constexpr ModulatedVectorsPerBlock = FS_Size_32() * VectorsPerBlock;
        size_t blockCount = ( totalValues + ModulatedVectorsPerBlock - 1 ) / ( ModulatedVectorsPerBlock );
        size_t index      = 0;

        auto blocks = std::vector<std::pair<BlockTy, Output>>( blockCount );
        gi.output.resize( alignof( float32v ), blockCount * ModulatedVectorsPerBlock );

        for( size_t b = 0; b < blockCount; ++b )
        {
            auto& block    = blocks[b];
            auto& input    = block.first;
            auto& output   = block.second;
            output.output  = (float32v*)( gi.output.data.get() + b * ModulatedVectorsPerBlock );
            input.nbValues = 0;
            uint vertBlock = 0;
            while( index < totalValues && input.nbValues < ModulatedVectorsPerBlock )
            {
                for( int i = 0; i < DimSize; ++i )
                    input.v[vertBlock][i] = FS_Converti32_f32( Idx[i] ) * freqV;

                vertBlock++;
                index += FS_Size_32();
                input.nbValues += FS_Size_32();
                Idx[0] += int32v( FS_Size_32() );
                {
                    size_t mulSize = 1;
                    for( std::uint32_t i = 0; i < DimSize - 1; ++i )
                    {
                        mulSize *= gi.size[i];
                        AxisReset<false>( Idx[i], Idx[i + 1], Max[i], Size[i], mulSize );
                    }
                }
            }
        }

        std::for_each( Exec, blocks.begin(), blocks.end(), [&]( auto& block ) {
            GenBlock( params, uniform, block.first, block.second );
            block.second.DoMinMax( block.first.nbValues );
        } );

        for( auto& b: blocks )
            gi.minMax << b.second.minMax;
        gi.output.resize( alignof( float32v ), totalValues );
    }

    void GenUniformGrid2D( GeneratorInput& gi ) const final
    {
        GenUniformGrid<2>( gi );
    }

private:
    template<bool INITIAL>
    static FS_INLINE void AxisReset( int32v& aIdx, int32v& bIdx, int32v aMax, int32v aSize, size_t aStep )
    {
        for( size_t resetLoop = INITIAL ? aStep : 0; resetLoop < FS_Size_32(); resetLoop += aStep )
        {
            mask32v aReset = aIdx > aMax;
            bIdx           = FS_MaskedIncrement_i32( bIdx, aReset );
            aIdx           = FS_MaskedSub_i32( aIdx, aSize, aReset );
        }
    }
};
