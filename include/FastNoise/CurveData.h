#pragma once

#include "Serializer.h"
#include "spline.h"

namespace FastNoise
{
    struct CurveData
    {
        struct Edit
        {
            std::vector<float>        cx         = { 0.0f, 0.5f, 1.0f };
            std::vector<float>        cy         = { 0.0f, 0.5f, 1.0f };
            tk::spline<>::spline_type type       = tk::spline<>::spline_type::cspline_hermite;
            tk::spline<>::bd_type     left       = tk::spline<>::second_deriv;
            tk::spline<>::bd_type     right      = tk::spline<>::first_deriv;
            float                     leftValue  = 0;
            float                     rightValue = 1.0f;
            int                       dragged    = -1;
            bool                      monotonic  = true;
            bool                      edited     = false;
            bool                      dirty      = false;
            bool                      firstEdit  = true;
            bool                      liveUpdate = false;
            tk::spline<>              spline;
        };

        tk::spline<> spline;
        Edit         edits;

        CurveData()
        {
            tk::spline<>::spline_type type       = tk::spline<>::spline_type::cspline_hermite;
            tk::spline<>::bd_type     left       = tk::spline<>::second_deriv;
            tk::spline<>::bd_type     right      = tk::spline<>::first_deriv;
            float                     leftValue  = 0;
            float                     rightValue = 1.0f;
            bool                      monotonic  = true;
            std::vector<float>        cx         = { 0.0f, 0.5f, 1.0f };
            std::vector<float>        cy         = { 0.0f, 0.5f, 1.0f };

            spline = tk::spline<>::spline( cx, cy, type, monotonic, left, leftValue, right, rightValue );
        }

        void BeginEdit()
        {
            if( edits.edited )
                return;
            edits.cx         = spline.get_x();
            edits.cy         = spline.get_y();
            edits.type       = spline.get_type();
            edits.left       = spline.get_left_deriv();
            edits.right      = spline.get_right_deriv();
            edits.leftValue  = spline.get_left_value();
            edits.rightValue = spline.get_right_value();
            edits.monotonic  = edits.firstEdit ? spline.is_monotonic() : edits.monotonic;
            edits.dragged    = -1;
            edits.edited     = true;
            edits.dirty      = false;
            edits.firstEdit  = false;
            edits.spline     = spline;
        }

        bool EndEdits( bool apply )
        {
            apply |= edits.liveUpdate;
            if( edits.dirty )
            {
                edits.spline = tk::spline<>::spline( edits.cx, edits.cy, edits.type, edits.monotonic, edits.left,
                                                     edits.leftValue, edits.right, edits.rightValue );
            }
            if( apply && edits.edited && edits.dirty )
            {
                spline        = edits.spline;
                edits.edited  = false;
                edits.dirty   = false;
                edits.dragged = -1;
                return true;
            }
            return false;
        }

        inline bool operator==( const CurveData& other ) const
        {
            return spline == other.spline;
        }

        bool fromDataStream( const std::vector<uint8_t>& dataStream, size_t& serialIdx )
        {
            size_t s;

            if( !GetFromDataStream( dataStream, serialIdx, s ) )
                return false;

            std::vector<float> cx, cy;

            cx.resize( s );
            for( auto& vx: cx )
            {
                if( !GetFromDataStream( dataStream, serialIdx, vx ) )
                    return false;
            }

            if( !GetFromDataStream( dataStream, serialIdx, s ) )
                return false;
            cy.resize( s );
            for( auto& vy: cy )
            {
                if( !GetFromDataStream( dataStream, serialIdx, vy ) )
                    return false;
            }

            tk::spline<>::spline_type type       = tk::spline<>::spline_type::cspline_hermite;
            tk::spline<>::bd_type     left       = tk::spline<>::second_deriv;
            tk::spline<>::bd_type     right      = tk::spline<>::first_deriv;
            float                     leftValue  = 0;
            float                     rightValue = 0;
            bool                      monotonic  = false;
            if( !GetFromDataStream( dataStream, serialIdx, left ) )
                return false;
            if( !GetFromDataStream( dataStream, serialIdx, leftValue ) )
                return false;
            if( !GetFromDataStream( dataStream, serialIdx, right ) )
                return false;
            if( !GetFromDataStream( dataStream, serialIdx, rightValue ) )
                return false;
            if( !GetFromDataStream( dataStream, serialIdx, monotonic ) )
                return false;
            if( !GetFromDataStream( dataStream, serialIdx, type ) )
                return false;

            spline = tk::spline<>::spline( cx, cy, type, monotonic, left, leftValue, right, rightValue );
        }

        void toDataStream( std::vector<uint8_t>& dataStream ) const
        {
            auto const& cx = spline.get_x();
            auto const& cy = spline.get_y();
            AddToDataStream( dataStream, cx.size() );
            for( auto const& vx: cx )
                AddToDataStream( dataStream, vx );
            AddToDataStream( dataStream, cy.size() );
            for( auto const& vy: cy )
                AddToDataStream( dataStream, vy );
            auto left       = spline.get_left_deriv();
            auto leftValue  = spline.get_left_value();
            auto right      = spline.get_right_deriv();
            auto rightValue = spline.get_right_value();
            auto monotonic  = spline.is_monotonic();
            auto type       = spline.get_type();

            AddToDataStream( dataStream, left );
            AddToDataStream( dataStream, leftValue );
            AddToDataStream( dataStream, right );
            AddToDataStream( dataStream, rightValue );
            AddToDataStream( dataStream, monotonic );
            AddToDataStream( dataStream, type );
        }
    };
} // namespace FastNoise