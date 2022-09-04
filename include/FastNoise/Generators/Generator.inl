#include <cassert>
#include <cstdint>
#include <cstring>
#include "FastSIMD/InlInclude.h"

#include "BS_thread_pool.hpp"
#include "Generator.h"

#ifdef FS_SIMD_CLASS
#pragma warning( disable : 4250 )
#endif

template<typename FS>
class FS_T<FastNoise::Generator, FS> : public virtual FastNoise::Generator
{
    FASTSIMD_DECLARE_FS_TYPES;
    inline static BS::thread_pool mJobPool;


public:
    using uint                               = std::uint32_t;
    using Int4                               = std::array<int, 4>;
    static constexpr std::uint32_t BlockSize = 64;


    struct Uniform
    {
        int32v         seed;
        Int4           size;
        Int4           offset;
        Context const& input;


        Uniform( Uniform const& ) = default;
        Uniform( Context const& ctx ) :
            input( ctx )
        {
        }
    };

    template<unsigned int DimSize>
    struct BlockInput
    {
        // MinMax
        using DimVec   = std::array<float32v, DimSize>;
        using BlockVec = std::array<DimVec, BlockSize>;

        static constexpr auto N = DimSize;

        BlockVec v;
        uint     nbValues = 0;

        BlockInput() = default;
        BlockInput( uint v ) :
            nbValues( v )
        {
        }

        uint size()
        {
            return nbValues;
        }
    };

    struct Output
    {
        struct HeapBlock
        {
            std::unique_ptr<float32v[]> output;
            HeapBlock() :
                output { new float32v[BlockSize] }
            {
            }
            HeapBlock( uint Times ) :
                output { new float32v[BlockSize * Times] }
            {
            }
            float32v* operator[]( uint i )
            {
                return output.get() + i * BlockSize;
            }
        };

        struct LocalBlock
        {
            float32v output[BlockSize];
        };


        float32v*               output /*[BlockSize]*/ = nullptr;
        FastNoise::OutputMinMax minMax;
        Output() = default;

        Output( LocalBlock& local ) :
            output( local.output )
        {
        }
        Output( float32v* local ) :
            output( local )
        {
        }

        void Fill( float32v value )
        {
            for( uint i = 0; i < BlockSize; ++i )
                output[i] = value;
        }

        template<typename Input>
        void DoMinMax( Input const& iInp )
        {
            float32v min( INFINITY );
            float32v max( -INFINITY );

            constexpr auto limit    = BlockSize * FS_Size_32();
            auto           minMaxLt = ( iInp.nbValues ) / FS_Size_32();

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

            auto   remaining = iInp.nbValues % FS_Size_32();
            float* foutp     = (float*)( output + minMaxLt );
            for( size_t i = 0; i < remaining; i++ )
            {
                minMax << foutp[i];
            }
        }
    };


    static auto& GetJobPool()
    {
        return mJobPool;
    }

    // Called with min/max computed
    virtual void Finalize( Context& ) const
    {
    }

#define FASTNOISE_DECL_GEN_T( N ) \
    virtual void FS_VECTORCALL GenBlock( Uniform const&, BlockInput<N>&, Output& ) const = 0

    FASTNOISE_DECL_GEN_T( 2 );
    FASTNOISE_DECL_GEN_T( 3 );
    FASTNOISE_DECL_GEN_T( 4 );

#define FASTNOISE_IMPL_GEN_T_N( N )                                                             \
    void FS_VECTORCALL GenBlock( Uniform const& u, BlockInput<N>& i, Output& o ) const override \
    {                                                                                           \
        GenBlockT( u, i, o );                                                                   \
        o.DoMinMax( i );                                                                        \
    }

#define FASTNOISE_IMPL_GEN_T                                                   \
    using Uniform = typename FS_T<FastNoise::Generator, FS>::Uniform;          \
    using Output  = typename FS_T<FastNoise::Generator, FS>::Output;           \
    template<unsigned int D>                                                   \
    using BlockInput = typename FS_T<FastNoise::Generator, FS>::BlockInput<D>; \
                                                                               \
    FASTNOISE_IMPL_GEN_T_N( 2 )                                                \
    FASTNOISE_IMPL_GEN_T_N( 3 )                                                \
    FASTNOISE_IMPL_GEN_T_N( 4 )


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
    FS_INLINE void FS_VECTORCALL GetSourceValue( const FastNoise::HybridSourceT<T>& memberVariable, Uniform const& u, BlockInput<N>& i, Output& o ) const
    {
        if( memberVariable.simdGeneratorPtr )
        {
            auto simdGen = reinterpret_cast<VoidPtrStorageType>( memberVariable.simdGeneratorPtr );
            simdGen->GenBlock( u, i, o );
        }
        else
            o.Fill( float32v( memberVariable.constant ) );
    }

    template<typename T, const std::uint32_t N>
    FS_INLINE void FS_VECTORCALL GetSourceValue( const FastNoise::GeneratorSourceT<T>& memberVariable, Uniform const& u, BlockInput<N>& i, Output& o ) const
    {
        assert( memberVariable.simdGeneratorPtr );
        auto simdGen = reinterpret_cast<VoidPtrStorageType>( memberVariable.simdGeneratorPtr );
        simdGen->GenBlock( u, i, o );
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
    void GenUniformGrid( Context& context, Int4 start, Int4 size, float frequency, int seed ) const
    {
        /// ==============================
        using BlockTy = BlockInput<DimSize>;

        float32v freqV( frequency );

        int32v Idx[DimSize];
        int32v Max[DimSize];
        int32v Size[DimSize];

        size_t totalValues = 1;

        auto uniform = Uniform( context );
        uniform.seed = int32v( seed );

        for( std::uint32_t i = 0; i < DimSize; ++i )
        {
            totalValues *= size[i];
            Idx[i]            = int32v( start[i] );
            Size[i]           = int32v( size[i] );
            Max[i]            = Size[i] + Idx[i] + int32v( -1 );
            uniform.offset[i] = start[i];
            uniform.size[i]   = size[i];
        }

        Idx[0] += int32v::FS_Incremented();

        {
            size_t mulSize = 1;
            for( std::uint32_t i = 0; i < DimSize - 1; ++i )
            {
                mulSize *= size[i];
                AxisReset<true>( Idx[i], Idx[i + 1], Max[i], Size[i], mulSize );
            }
        }

        auto constexpr ModulatedBlockSize = FS_Size_32() * BlockSize;
        size_t blockCount                 = ( totalValues + ModulatedBlockSize - 1 ) / ( ModulatedBlockSize * FS_Size_32() );
        size_t index                      = 0;

        auto                           blocks = std::vector<std::pair<BlockTy, Output>>( blockCount );
        std::vector<std::future<void>> results;
        results.reserve( blockCount - 1 );
        context.output.resize( alignof( float32v ), blockCount * ModulatedBlockSize );
        std::fill( context.output.begin(), context.output.end(), 1.0f );
        for( size_t b = 0; b < blockCount; ++b )
        {
            auto& input    = blocks[b].first;
            auto& output   = blocks[b].second;
            output.output  = (float32v*)( context.output.data.get() + b * ModulatedBlockSize );
            input.nbValues = 0;
            uint block     = 0;
            while( index < totalValues && input.nbValues < ModulatedBlockSize )
            {
                for( int i = 0; i < DimSize; ++i )
                    input.v[block][i] = FS_Converti32_f32( Idx[i] ) * freqV;

                block++;
                index += FS_Size_32();
                input.nbValues += FS_Size_32();
                Idx[0] += int32v( FS_Size_32() );
                {
                    size_t mulSize = 1;
                    for( std::uint32_t i = 0; i < DimSize - 1; ++i )
                    {
                        mulSize *= size[i];
                        AxisReset<false>( Idx[i], Idx[i + 1], Max[i], Size[i], mulSize );
                    }
                }
            }

            if( b == blockCount - 1 )
            {
                GenBlock( uniform, input, output );
            }
            else
            {
                results.emplace_back( mJobPool.submit( [&input, &output, &uniform, this]() { GenBlock( uniform, input, output ); } ) );
            }
        }

        for( auto& r: results )
            r.wait();
        for( auto& b: blocks )
            context.minMax << b.second.minMax;
        Finalize( context );
        context.output.resize( alignof( float32v ), totalValues );
    }

    void GenTileable2D( Context& context, int xSize, int ySize, float frequency, int seed ) const final
    {
        using BlockTy = BlockInput<2>;
        auto uniform  = Uniform( context );
        uniform.seed  = int32v( seed );

        int32v xIdx( 0 );
        int32v yIdx( 0 );

        int32v xSizeV( xSize );
        int32v ySizeV( ySize );
        int32v xMax = xSizeV + xIdx + int32v( -1 );

        size_t totalValues = xSize * ySize;
        size_t index       = 0;

        float    pi2Recip( 0.15915493667f );
        float    xSizePi = (float)xSize * pi2Recip;
        float    ySizePi = (float)ySize * pi2Recip;
        float32v xFreq   = float32v( frequency * xSizePi );
        float32v yFreq   = float32v( frequency * ySizePi );
        float32v xMul    = float32v( 1 / xSizePi );
        float32v yMul    = float32v( 1 / ySizePi );

        xIdx += int32v::FS_Incremented();
        AxisReset<true>( xIdx, yIdx, xMax, xSizeV, xSize );

        auto constexpr ModulatedBlockSize         = FS_Size_32() * BlockSize;
        size_t                         blockCount = ( totalValues + ModulatedBlockSize - 1 ) / ( ModulatedBlockSize );
        auto                           blocks     = std::vector<std::pair<BlockTy, Output>>( blockCount );
        std::vector<std::future<void>> results;
        results.reserve( blockCount - 1 );
        context.output.resize( alignof( float32v ), blockCount * ModulatedBlockSize );

        for( size_t b = 0; b < blockCount; ++b )
        {
            auto& input    = blocks[b].first;
            auto& output   = blocks[b].second;
            output.output  = (float32v*)( context.output.data.get() + b * ModulatedBlockSize );
            input.nbValues = 0;
            uint block     = 0;
            while( index < totalValues && input.nbValues < ModulatedBlockSize )
            {
                float32v xF = FS_Converti32_f32( xIdx ) * xMul;
                float32v yF = FS_Converti32_f32( yIdx ) * yMul;

                input.v[block][0] = FS_Cos_f32( xF ) * xFreq;
                input.v[block][1] = FS_Cos_f32( yF ) * yFreq;

                block++;
                index += FS_Size_32();
                input.nbValues += FS_Size_32();
                xIdx += int32v( FS_Size_32() );
                AxisReset<false>( xIdx, yIdx, xMax, xSizeV, xSize );
            }
            if( b == blockCount - 1 )
            {
                GenBlock( uniform, input, output );
            }
            else
            {
                results.emplace_back(
                    mJobPool.submit( [&input, &output, &uniform, this]() {
                        GenBlock( uniform, input, output );
                    } ) );
            }
        }
        for( auto& r: results )
            r.wait();
        for( auto& b: blocks )
            context.minMax << b.second.minMax;
        Finalize( context );
        context.output.resize( alignof( float32v ), totalValues );
    }

    void GenUniformGrid2D( Context& out,
                           int xStart, int yStart,
                           int xSize, int ySize,
                           float frequency, int seed ) const final
    {
        GenUniformGrid<2>( out, { xStart, yStart }, { xSize, ySize }, frequency, seed );
    }

    void GenUniformGrid3D( Context& out,
                           int xStart, int yStart, int zStart,
                           int xSize, int ySize, int zSize,
                           float frequency, int seed ) const final
    {
        GenUniformGrid<3>( out, { xStart, yStart, zStart }, { xSize, ySize, zSize }, frequency, seed );
    }

    virtual void GenUniformGrid4D( Context& out,
                                   int xStart, int yStart, int zStart, int wStart,
                                   int xSize, int ySize, int zSize, int wSize,
                                   float frequency, int seed ) const final
    {
        GenUniformGrid<4>( out, { xStart, yStart, zStart, wStart }, { xSize, ySize, zSize, wSize }, frequency, seed );
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
