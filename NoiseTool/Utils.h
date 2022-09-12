#pragma once
#include <algorithm>
#include <cmath>
#include <execution>
#include <ranges>
#include <thread>

#include <Corrade/Containers/ArrayViewStl.h>
#include <Corrade/Utility/Resource.h>
#include <Magnum/GL/GL.h>
#include <Magnum/GL/Texture.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/ImGuiIntegration/Widgets.h>
#include <Magnum/ImageView.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Frustum.h>
#include <Magnum/Math/Functions.h>
#include <Magnum/Math/Intersection.h>
#include <Magnum/Math/Matrix4.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Shaders/Implementation/CreateCompatibilityShader.h>


namespace Magnum
{

    inline uint32_t packSnorm2x16( Vector2 const& v )
    {
        union
        {
            signed short in[2];
            uint32_t     out;
        } u;

        Vector2 result( round( clamp( v, -1.0f, 1.0f ) * 32767.0f ) );

        u.in[0] = result[0];
        u.in[1] = result[1];

        return u.out;
    }

    inline auto msign( Vector2 v )
    {
        return Vector2( ( v.x() >= 0.0f ) ? 1.0f : -1.0f, ( v.y() >= 0.0 ) ? 1.0f : -1.0f );
    }

    inline auto CompressNormal( Vector3 nor, std::uint32_t CompressionPrec ) -> float
    {
        nor /= ( std::abs( nor.x() ) + std::abs( nor.y() ) + std::abs( nor.z() ) );
        nor.xy()  = ( nor.z() >= 0.0 )
             ? nor.xy()
             : ( ( Vector2( 1.0f ) - abs( Vector2( nor.y(), nor.x() ) ) ) * msign( nor.xy() ) );
        Vector2 v = Vector2( 0.5 ) + Vector2( 0.5 ) * nor.xy();

        std::uint32_t mu = ( 1u << CompressionPrec ) - 1u;
        Vector2ui     d  = Vector2ui( floor( clamp( v, -1.0f, 1.0f ) * float( mu ) + Vector2( 0.5 ) ) );
        mu               = ( d.y() << CompressionPrec ) | d.x();
        return *(float*)( &mu );
    }

    inline auto CompressNormal( Vector3 nor ) -> float
    {
        nor /= ( std::abs( nor.x() ) + std::abs( nor.y() ) + std::abs( nor.z() ) );
        nor.xy()   = ( nor.z() >= 0.0 )
              ? nor.xy()
              : ( ( Vector2( 1.0f ) - abs( Vector2( nor.y(), nor.x() ) ) ) * msign( nor.xy() ) );
        auto value = packSnorm2x16( nor.xy() );
        return *(float*)( &value );
    }
} // namespace Magnum