
#include <Magnum/Magnum.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Math/Vector4.h>
#include <string_view>
#include "ImGuiExtra.h"
#include "ImGuiUtils.h"


namespace Magnum
{

    void Write( ImGuiTextBuffer* outBuf, std::string_view name, float value )
    {
        outBuf->appendf( "%s=%f\n", name.data(), value );
    }
    bool Read( std::string_view line, std::string_view name, float& value )
    {
        if( line.starts_with( name ) )
        {
            line = line.substr( name.length() + 1 );
            return ( sscanf( line.data(), "%f", &value ) == 1 );
        }
        return false;
    }

    void Write( ImGuiTextBuffer* outBuf, std::string_view name, int value )
    {
        outBuf->appendf( "%s=%d\n", name.data(), value );
    }
    bool Read( std::string_view line, std::string_view name, int& value )
    {
        if( line.starts_with( name ) )
        {

            line = line.substr( name.length() + 1 );
            return ( sscanf( line.data(), "%d", &value ) == 1 );
        }
        return false;
    }

    void Write( ImGuiTextBuffer* outBuf, const char* name, Vector2 value )
    {
        outBuf->appendf( "%s=%f:%f\n", name, value.x(), value.y() );
    }
    bool Read( std::string_view line, std::string_view name, Vector2& value )
    {
        if( line.starts_with( name ) )
        {

            line = line.substr( name.length() + 1 );
            return ( sscanf( line.data(), "%f:%f", &value.x(), &value.y() ) == 1 );
        }
        return false;
    }

    void Write( ImGuiTextBuffer* outBuf, const char* name, Vector2i value )
    {
        outBuf->appendf( "%s=%d:%d\n", name, value.x(), value.y() );
    }
    bool Read( std::string_view line, std::string_view name, Vector2i& value )
    {
        if( line.starts_with( name ) )
        {

            line = line.substr( name.length() + 1 );
            return ( sscanf( line.data(), "%d:%d", &value.x(), &value.y() ) == 1 );
        }
        return false;
    }

    void Write( ImGuiTextBuffer* outBuf, const char* name, Vector3i value )
    {
        outBuf->appendf( "%s=%d:%d:%d\n", name, value.x(), value.y(), value.z() );
    }
    bool Read( std::string_view line, std::string_view name, Vector3i& value )
    {
        if( line.starts_with( name ) )
        {

            line = line.substr( name.length() + 1 );
            return ( sscanf( line.data(), "%d:%d:%d", &value.x(), &value.y(), &value.z() ) == 1 );
        }
        return false;
    }

    void Write( ImGuiTextBuffer* outBuf, const char* name, Color3 value )
    {
        outBuf->appendf( "%s=%df\n", name, value.toSrgbInt() );
    }
    bool Read( std::string_view line, std::string_view name, Color3& value )
    {
        if( line.starts_with( name ) )
        {
            int val  = 0;
            line     = line.substr( name.length() + 1 );
            bool ret = ( sscanf( line.data(), "%d", &val ) == 1 );
            value    = Color3::fromSrgb( val );
        }
        return false;
    }

    void Write( ImGuiTextBuffer* outBuf, const char* name, Color4 value )
    {
        outBuf->appendf( "%s=%df\n", name, value.toSrgbAlphaInt() );
    }
    bool Read( std::string_view line, std::string_view name, Color4& value )
    {
        if( line.starts_with( name ) )
        {
            int val  = 0;
            line     = line.substr( name.length() + 1 );
            bool ret = ( sscanf( line.data(), "%d", &val ) == 1 );
            value    = Color4::fromSrgbAlpha( val );
        }
        return false;
    }

    void Write( ImGuiTextBuffer* outBuf, const char* name, std::string const& value )
    {
        outBuf->appendf( "%s=%s\n", name, value.c_str() );
    }
    bool Read( std::string_view line, std::string_view name, std::string& value )
    {
        if( line.starts_with( name ) )
        {
            line = line.substr( name.length() + 1 );

            value = line;
            if( !value.empty() )
            {
                if( value.back() == '\n' )
                    value.pop_back();
                if( value.back() == '\r' )
                    value.pop_back();
            }
            return true;
        }
        return false;
    }


    void Write( ImGuiTextBuffer* outBuf, const char* name, Directory const& value )
    {
        Write( outBuf, name, value.path );
    }
    bool Read( std::string_view line, std::string_view name, Directory& value )
    {
        return Read( line, name, value.path );
    }

    void Write( ImGuiTextBuffer* outBuf, const char* name, ColorLayerValue const& value )
    {
        for( auto& [color, active]: value )
            Write( outBuf, name,
                   Vector3i( color[0].toSrgbAlphaInt(), color[1].toSrgbAlphaInt(), color[2].toSrgbAlphaInt() ) );
    }
    bool Read( std::string_view line, std::string_view name, ColorLayerValue& value )
    {
        if( line.starts_with( name ) )
        {
            Vector3i cl;
            Read( line, name, cl );
            value.emplace_back( Color4x3 { Color4::fromSrgbAlpha( cl[0] ), Color4::fromSrgbAlpha( cl[1] ),
                                           Color4::fromSrgbAlpha( cl[2] ) },
                                true );
        }
        return false;
    }

    void Write( ImGuiTextBuffer* outBuf, const char* name, Rotation value )
    {
        outBuf->appendf( "%s=%f:%f\n", name, value.phi, value.theta );
    }
    bool Read( std::string_view line, std::string_view name, Rotation& value )
    {
        if( line.starts_with( name ) )
        {

            line = line.substr( name.length() + 1 );
            return ( sscanf( line.data(), "%f:%f", &value.phi, &value.theta ) == 1 );
        }
        return false;
    }


    void Write( ImGuiTextBuffer* outBuf, const char* name, GeneratorPtr value )
    {
    }
    bool Read( std::string_view line, std::string_view name, GeneratorPtr& value )
    {
        return true;
    }
} // namespace Magnum