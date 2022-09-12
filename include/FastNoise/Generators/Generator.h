#pragma once
#include <algorithm>
#include <any>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

#include <cstdint>

#include "FastNoise/AllocUtils.h"
#include "FastNoise/CurveData.h"
#include "FastNoise/FastNoise_Config.h"
#include "FastNoise/ImageData.h"

#if !defined( FASTNOISE_METADATA ) && defined( __INTELLISENSE__ )
//#define FASTNOISE_METADATA
#endif

namespace FastNoise
{
    enum class Dim
    {
        X,
        Y,
        Z,
        W,
        Count
    };

    constexpr static const char* kDim_Strings[] = {
        "X",
        "Y",
        "Z",
        "W",
    };

    enum class DistanceFunction
    {
        Euclidean,
        EuclideanSquared,
        Manhattan,
        Hybrid,
        MaxAxis,
    };

    constexpr static const char* kDistanceFunction_Strings[] = {
        "Euclidean", "Euclidean Squared", "Manhattan", "Hybrid", "Max Axis",
    };

    struct OutputMinMax
    {
        float min = INFINITY;
        float max = -INFINITY;

        OutputMinMax& operator<<( float v )
        {
            min = std::min( min, v );
            max = std::max( max, v );
            return *this;
        }

        OutputMinMax& operator<<( const OutputMinMax& v )
        {
            min = std::min( min, v.min );
            max = std::max( max, v.max );
            return *this;
        }
    };

    template<typename T>
    struct BaseSource
    {
        using Type = T;

        SmartNode<const T> base;
        const void*        simdGeneratorPtr = nullptr;

    protected:
        BaseSource() = default;
    };

    template<typename T>
    struct GeneratorSourceT : BaseSource<T>
    {
    };

    template<typename T>
    struct HybridSourceT : BaseSource<T>
    {
        float constant;

        HybridSourceT( float f = 0.0f )
        {
            constant = f;
        }
    };

    struct Buffer
    {
        std::unique_ptr<float, AlignedDeleter> data;
        uint                                   size      = 0;
        uint                                   capacity  = 0;
        uint                                   alignment = 32;
        Buffer()                                         = default;
        Buffer( Buffer&& )                               = default;

        Buffer& operator=( Buffer&& ) = default;


        Buffer( uint al, uint siz ) :
            data( (float*)AlignedAllocate( al, siz * 4 ) ), size( siz ), capacity( siz ), alignment( al )
        {
        }

        Buffer( uint siz ) : data( (float*)AlignedAllocate( 32, siz * 4 ) ), size( siz ), capacity( siz )
        {
        }

        Buffer( Buffer const& other ) : Buffer( other.alignment, other.size )
        {
            std::memcpy( data.get(), other.data.get(), other.size * 4 );
        }


        Buffer& operator=( Buffer const& other )
        {
            *this = Buffer( other.alignment, other.size );
            std::memcpy( data.get(), other.data.get(), other.size * 4 );
        }

        float* begin()
        {
            return data.get();
        }

        float const* begin() const
        {
            return data.get();
        }

        float* end()
        {
            return data.get() + size;
        }

        float const* end() const
        {
            return data.get() + size;
        }

        void resize( uint al, size_t size )
        {
            if( capacity < size )
            {
                data.reset( (float*)AlignedAllocate( al, size * 4 ) );
                capacity = (uint)size;
            }
            this->size = (uint)size;
        }

        float& operator[]( uint i )
        {
            return data.get()[i];
        }

        float operator[]( uint i ) const
        {
            return data.get()[i];
        }

        uint nbbytes() const
        {
            return size * 4;
        }
    };

    class FASTNOISE_API Generator
    {
    public:
        enum class GenType
        {
            e2D,
            e3D,
            e4D,
            eTiable2D,
            ePosArray2D,
            ePosArray3D,
            ePosArray4D,
            eSingle2D,
            eSingle3D,
            Single4D
        };

        struct Context
        {
            // External API
            std::array<int, 4> planeId           = { 0, 0, 0, 0 };
            std::array<int, 4> totalPlanes       = { 1, 1, 1, 1 };
            std::array<int, 4> startOffsetPlane0 = { 0, 0, 0, 0 };
            Context( Buffer& out, std::array<int, 4> start ) : output( out ), startOffsetPlane0( start )
            {
            }
            // Output
            Buffer&                 output;
            FastNoise::OutputMinMax minMax;
        };

        template<typename T>
        friend struct MetadataT;

        virtual ~Generator() = default;

        virtual FastSIMD::eLevel GetSIMDLevel() const = 0;
        virtual const Metadata&  GetMetadata() const  = 0;

        virtual void GenUniformGrid2D( Context& out, int xStart, int yStart, int xSize, int ySize, float frequency,
                                       int seed ) const = 0;

        virtual void GenUniformGrid3D( Context& out, int xStart, int yStart, int zStart, int xSize, int ySize,
                                       int zSize, float frequency, int seed ) const = 0;

        virtual void GenUniformGrid4D( Context& out, int xStart, int yStart, int zStart, int wStart, int xSize,
                                       int ySize, int zSize, int wSize, float frequency, int seed ) const = 0;

        virtual void GenTileable2D( Context& out, int xSize, int ySize, float frequency, int seed ) const = 0;

        // called when all member variables have been set
        virtual void ApplyChanges() = 0;

    protected:
        template<typename T>
        void SetSourceMemberVariable( BaseSource<T>& memberVariable, SmartNodeArg<T> gen )
        {
            static_assert( std::is_base_of<Generator, T>::value, "T must be child of FastNoise::Generator class" );

            assert( !gen.get() || GetSIMDLevel() == gen->GetSIMDLevel() ); // Ensure that all SIMD levels match

            SetSourceSIMDPtr( dynamic_cast<const Generator*>( gen.get() ), &memberVariable.simdGeneratorPtr );
            memberVariable.base = gen;
        }

    private:
        virtual void SetSourceSIMDPtr( const Generator* base, const void** simdPtr ) = 0;
    };

    using GeneratorSource = GeneratorSourceT<Generator>;
    using HybridSource    = HybridSourceT<Generator>;

    template<typename T, int D = (int)Dim::Count>
    struct PerDimensionVariable
    {
        static constexpr int N = D;
        using Type             = T;

        std::array<T, D> varArray = {};

        PerDimensionVariable() = default;
        PerDimensionVariable( std::initializer_list<T> val ) : varArray( std::move( val ) )
        {
        }

        template<typename U = T>
        PerDimensionVariable( U value = 0 )
        {
            for( T& element: varArray )
            {
                element = value;
            }
        }

        T& operator[]( size_t i )
        {
            return varArray[i];
        }

        const T& operator[]( size_t i ) const
        {
            return varArray[i];
        }
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<Generator> : Metadata
    {
    protected:
        template<typename T, typename U, typename = std::enable_if_t<!std::is_enum_v<T>>>
        void AddVariable( const char* name, T defaultV, U&& func, T minV = 0, T maxV = 0, bool sameL = false )
        {
            MemberVariable member;
            member.name         = name;
            member.valueDefault = defaultV;
            member.valueMin     = minV;
            member.valueMax     = maxV;
            member.sameLine     = sameL;
            member.type         = std::is_same_v<T, float>
                        ? MemberVariable::EFloat
                        : ( std::is_same_v<T, bool> ? MemberVariable::EBool : MemberVariable::EInt );

            member.setFunc = [func]( Generator* g, MemberVariable::ValueUnion v ) {
                if( auto* gRealType = dynamic_cast<GetArg<U, 0>>( g ) )
                {
                    func( gRealType, v );
                    return true;
                }
                return false;
            };

            memberVariables.push_back( member );
        }

        template<typename T, typename U, typename = std::enable_if_t<!std::is_enum_v<T>>>
        void AddVariable( const char* name, T defaultV, void ( U::*func )( T ), T minV = 0, T maxV = 0,
                          bool sameL = false )
        {
            MemberVariable member;
            member.name         = name;
            member.valueDefault = defaultV;
            member.valueMin     = minV;
            member.valueMax     = maxV;
            member.sameLine     = sameL;
            member.type         = std::is_same_v<T, float>
                        ? MemberVariable::EFloat
                        : ( std::is_same_v<T, bool> ? MemberVariable::EBool : MemberVariable::EInt );

            member.setFunc = [func]( Generator* g, MemberVariable::ValueUnion v ) {
                if( U* gRealType = dynamic_cast<U*>( g ) )
                {
                    ( gRealType->*func )( v );
                    return true;
                }
                return false;
            };

            memberVariables.push_back( member );
        }

        template<typename T, typename U, typename = std::enable_if_t<std::is_enum_v<T>>, typename... ENUM_NAMES>
        void AddVariableEnum( const char* name, T defaultV, void ( U::*func )( T ), ENUM_NAMES... enumNames )
        {
            MemberVariable member;
            member.name         = name;
            member.type         = MemberVariable::EEnum;
            member.valueDefault = (int)defaultV;
            member.enumNames    = { enumNames... };

            member.setFunc = [func]( Generator* g, MemberVariable::ValueUnion v ) {
                if( U* gRealType = dynamic_cast<U*>( g ) )
                {
                    ( gRealType->*func )( (T)v.i );
                    return true;
                }
                return false;
            };

            memberVariables.push_back( member );
        }

        template<typename T, typename U, size_t ENUM_NAMES, typename = std::enable_if_t<std::is_enum_v<T>>>
        void AddVariableEnum( const char* name, T defaultV, void ( U::*func )( T ),
                              const char* const ( &enumNames )[ENUM_NAMES] )
        {
            MemberVariable member;
            member.name         = name;
            member.type         = MemberVariable::EEnum;
            member.valueDefault = (int)defaultV;
            member.enumNames    = { enumNames, enumNames + ENUM_NAMES };

            member.setFunc = [func]( Generator* g, MemberVariable::ValueUnion v ) {
                if( U* gRealType = dynamic_cast<U*>( g ) )
                {
                    ( gRealType->*func )( (T)v.i );
                    return true;
                }
                return false;
            };

            memberVariables.push_back( member );
        }

        template<typename T, typename U, typename = std::enable_if_t<!std::is_enum_v<T>>>
        void AddPerDimensionVariable( const char* name, T defaultV, U&& func, T minV = {}, T maxV = {} )
        {
            using PDT        = typename std::invoke_result_t<U, GetArg<U, 0>>::type;
            auto constexpr N = PDT::N;
            for( int idx = 0; idx < N; idx++ )
            {
                MemberVariable member;
                member.name         = name;
                member.valueDefault = defaultV;
                member.valueMin     = minV;
                member.valueMax     = maxV;
                member.sameLine     = idx != ( N - 1 );
                member.type         = std::is_same_v<T, float>
                            ? MemberVariable::EFloat
                            : ( std::is_same_v<T, bool> ? MemberVariable::EBool : MemberVariable::EInt );
                member.dimensionIdx = idx;

                member.setFunc = [func, idx]( Generator* g, MemberVariable::ValueUnion v ) {
                    if( auto* gRealType = dynamic_cast<GetArg<U, 0>>( g ) )
                    {
                        func( gRealType ).get()[idx] = v;
                        return true;
                    }
                    return false;
                };

                memberVariables.push_back( member );
            }
        }

        template<typename T, typename U>
        void AddGeneratorSource( const char* name, void ( U::*func )( SmartNodeArg<T> ) )
        {
            MemberNodeLookup member;
            member.name = name;

            member.setFunc = [func]( Generator* g, SmartNodeArg<> s ) {
                if( const T* sUpCast = dynamic_cast<const T*>( s.get() ) )
                {
                    if( U* gRealType = dynamic_cast<U*>( g ) )
                    {
                        SmartNode<const T> source( s, sUpCast );
                        ( gRealType->*func )( source );
                        return true;
                    }
                }
                return false;
            };

            memberNodeLookups.push_back( member );
        }

        template<typename U>
        void AddPerDimensionGeneratorSource( const char* name, U&& func )
        {
            using PDT              = typename std::invoke_result_t<U, GetArg<U, 0>>::type;
            using GeneratorSourceT = typename PDT::Type;
            using T                = typename GeneratorSourceT::Type;
            auto constexpr N       = PDT::N;
            for( int idx = 0; (size_t)idx < N; idx++ )
            {
                MemberNodeLookup member;
                member.name         = name;
                member.dimensionIdx = idx;
                member.setFunc      = [func, idx]( Generator* g, SmartNodeArg<> s ) {
                    if( const T* sUpCast = dynamic_cast<const T*>( s.get() ) )
                    {
                        if( auto* gRealType = dynamic_cast<GetArg<U, 0>>( g ) )
                        {
                            SmartNode<const T> source( s, sUpCast );
                            g->SetSourceMemberVariable( func( gRealType ).get()[idx], source );
                            return true;
                        }
                    }
                    return false;
                };

                memberNodeLookups.push_back( member );
            }
        }


        template<typename T, typename U>
        void AddHybridSource( const char* name, float defaultValue, void ( U::*funcNode )( SmartNodeArg<T> ),
                              void ( U::*funcValue )( float ) )
        {
            MemberHybrid member;
            member.name         = name;
            member.valueDefault = defaultValue;

            member.setNodeFunc = [funcNode]( Generator* g, SmartNodeArg<> s ) {
                if( const T* sUpCast = dynamic_cast<const T*>( s.get() ) )
                {
                    if( U* gRealType = dynamic_cast<U*>( g ) )
                    {
                        SmartNode<const T> source( s, sUpCast );
                        ( gRealType->*funcNode )( source );
                        return true;
                    }
                }
                return false;
            };

            member.setValueFunc = [funcValue]( Generator* g, float v ) {
                if( U* gRealType = dynamic_cast<U*>( g ) )
                {
                    ( gRealType->*funcValue )( v );
                    return true;
                }
                return false;
            };

            memberHybrids.push_back( member );
        }

        template<typename U>
        void AddPerDimensionHybridSource( const char* name, float defaultV, U&& func )
        {
            using PDT           = typename std::invoke_result_t<U, GetArg<U, 0>>::type;
            using HybridSourceT = typename PDT::Type;
            using T             = typename HybridSourceT::Type;
            auto constexpr N    = PDT::N;
            for( int idx = 0; idx < N; idx++ )
            {
                MemberHybrid member;
                member.name         = name;
                member.valueDefault = defaultV;
                member.dimensionIdx = idx;

                member.setNodeFunc = [func, idx]( Generator* g, SmartNodeArg<> s ) {
                    if( const T* sUpCast = dynamic_cast<const T*>( s.get() ) )
                    {
                        if( auto* gRealType = dynamic_cast<GetArg<U, 0>>( g ) )
                        {
                            SmartNode<const T> source( s, sUpCast );
                            g->SetSourceMemberVariable( func( gRealType ).get()[idx], source );
                            return true;
                        }
                    }
                    return false;
                };

                member.setValueFunc = [func, idx]( Generator* g, float v ) {
                    if( auto* gRealType = dynamic_cast<GetArg<U, 0>>( g ) )
                    {
                        func( gRealType ).get()[idx] = v;
                        return true;
                    }
                    return false;
                };

                memberHybrids.push_back( member );
            }
        }

        template<typename U>
        void AddVariableImage( const char* name, void ( U::*func )( ImageData const& ) )
        {
            MemberImage member;
            member.name    = name;
            member.setFunc = [func]( Generator* g, ImageData const& s ) {
                if( U* gRealType = dynamic_cast<U*>( g ) )
                {
                    ( gRealType->*func )( s );
                    return true;
                }
                return false;
            };
            memberImages.push_back( member );
        }

        template<typename U>
        void AddVariableCurve( const char* name, void ( U::*func )( FastNoise::CurveData const& data ) )
        {
            MemberCurve member;
            member.name    = name;
            member.setFunc = [func]( Generator* g, CurveData const& s ) {
                if( U* gRealType = dynamic_cast<U*>( g ) )
                {
                    ( gRealType->*func )( s );
                    return true;
                }
                return false;
            };
            memberCurves.push_back( member );
        }

    private:
        template<typename F, typename Ret, typename... Args>
        static std::tuple<Args...> GetArg_Helper( Ret ( F::* )( Args... ) const );

        template<typename F, size_t I>
        using GetArg = std::tuple_element_t<I, decltype( GetArg_Helper( &F::operator() ) )>;
    };
#endif
} // namespace FastNoise
