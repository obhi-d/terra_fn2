
#pragma once

#include <cstdint>
#include <string>

#include <Magnum/Magnum.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Vector4.h>
#include "FastNoise/FastNoise.h"

#define TOKEN_PASTE2( X, Y ) X##Y
#define TOKEN_PASTE( X, Y )  TOKEN_PASTE2( X, Y )

namespace Magnum
{
    using Color4x3        = std::array<Color4, 3>;
    using ColorLayerValue = std::vector<std::tuple<Color4x3, bool>>;
    using GeneratorPtr    = FastNoise::SmartNode<const FastNoise::Generator>;

    struct Directory
    {
        std::string path;
    };

    struct Rotation
    {
        float theta = 0.0f;
        float phi   = 0.0f;

        Vector3 toDir() const
        {
            constexpr float pi       = 3.14159265358979323846f;
            constexpr float radf     = pi / 180.0f;
            auto            theta    = this->theta * radf;
            auto            phi      = this->phi * radf;
            auto            sinTheta = std::sin( theta );
            auto            cosTheta = std::cos( theta );
            auto            sinPhi   = std::sin( phi );
            return -Vector3( sinPhi * cosTheta, std::cos( phi ), sinPhi * sinTheta ).normalized();
        }
    };


    class Settings
    {

        enum FlagBit : uint32_t
        {
#define PROPERTY( Type, Name, Loc, Init, Min, Max, Desc ) TOKEN_PASTE( eProp_, Name ),

#include "Settings.inl"

#undef PROPERTY

            e_ticketCount
        };

    public:
        struct version
        {
            version()
            {
                value.fill( -1 );
            }

            version( int v )
            {
                value.fill( v );
            }

            inline int& operator[]( int i )
            {
                return value[i];
            }
            inline int operator[]( int i ) const
            {
                return value[i];
            }

            std::array<int, FlagBit::e_ticketCount> value;
        };

        Settings();

        static Settings& get();

        void beginFrame();
        void draw();
        void drawExport();
        void endFrame();
        void quit();

        void resetOffsets();

        Vector2i mapSize() const
        {
            return gridSize() * gridCount();
        }


        inline bool check( version& t, FlagBit p ) const
        {
            if( t[p] != _version[p] )
            {
                t[p] = _version[p];
                return true;
            }
            return false;
        }

        bool versionCheck_grid( version& t ) const
        {
            return check( t, eProp_gridSize ) || check( t, eProp_gridCount ) || check( t, eProp_gridCount );
        }

        bool versionCheck_edit( version& t ) const
        {
            return versionCheck_grid( t ) || check( t, eProp_frequency ) || check( t, eProp_seed ) ||
                check( t, eProp_generator );
        }


#define PROPERTY( Type, Name, Loc, Init, Min, Max, Desc )                                                              \
    Type const& Name() const                                                                                           \
    {                                                                                                                  \
        return TOKEN_PASTE( _, Name );                                                                                 \
    }                                                                                                                  \
    void Name( Type value )                                                                                            \
    {                                                                                                                  \
        TOKEN_PASTE( _, Name ) = std::move( value );                                                                   \
        _version[TOKEN_PASTE( eProp_, Name )]++;                                                                       \
    }                                                                                                                  \
    bool TOKEN_PASTE( versionCheck_, Name )( version & t )                                                             \
    {                                                                                                                  \
        return check( t, TOKEN_PASTE( eProp_, Name ) );                                                                \
    }
#include "Settings.inl"

#undef PROPERTY

    private:
#define PROPERTY( Type, Name, Loc, Init, Min, Max, Desc ) Type TOKEN_PASTE( _, Name ) = Init;

#include "Settings.inl"

#undef PROPERTY

        version _version { 0 };
    };
} // namespace Magnum
