
#pragma once
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>
#include <numeric>
#include "IconsFontAwesome6.h"

#include <FastNoise/CurveData.h>

namespace ImGui
{
    bool DrawEditorPopup( const char* label, FastNoise::CurveData& data )
    {
        ImGuiContext& g = *GImGui;

        constexpr float ItemWidth  = 80;
        constexpr float Spacing    = 4;
        constexpr float Canvas     = 240;
        constexpr float Smoothing  = ( Canvas * 2 ) / 3;
        constexpr float CurveWidth = 2;
        constexpr float GrabRadius = 8 / 2;
        constexpr float ControlSpc = 40;
        const ImColor   CircleColor( 0.9f, 0.1f, 0.1f, 1.0f );
        const ImColor   CircleBorderColor( 0.9f, 0.7f, 0.8f, 1.0f );
        const ImColor   HoveredCircleColor( 0.1f, 0.1f, 0.8f, 1.0f );
        const ImColor   HoveredCircleBorderColor( 0.8f, 0.8f, 0.5f, 1.0f );
        ImGuiStyle&     style = g.Style;

        data.BeginEdit();

        auto& edits      = data.edits;
        auto& curve      = edits.spline;
        int   type       = edits.type;
        int   leftBound  = edits.left - 1;
        int   rightBound = edits.right - 1;

        switch( curve.get_type() )
        {
        case tk::spline<>::cspline:
            type = 1;
            break;
        case tk::spline<>::cspline_hermite:
            type = 2;
            break;
        }
        float fixedX = ImGui::GetCursorPosX();
        float firstY = ImGui::GetCursorPosY() + 2;
        ImGui::SetNextItemWidth( ItemWidth );
        ImGui::SetCursorPosY( firstY );
        ImGui::PushID( "#type" );
        if( ImGui::Combo( "", &type, "Linear\0Cubic\0Hermite\0" ) )
        {
            edits.dirty = true;
        }
        ImGui::PopID();
        ImGui::SameLine();
        ImGui::Dummy( ImVec2( Spacing, 0 ) );
        ImGui::SetNextItemWidth( ItemWidth );
        ImGui::SetCursorPosX( fixedX + ItemWidth + ControlSpc );
        ImGui::SetCursorPosY( firstY );
        if( ImGui::Checkbox( "Monotonic", &edits.monotonic ) )
        {
            edits.dirty = true;
        }
        ImGui::SetCursorPosY( ImGui::GetCursorPosY() + 2 );
        ImGui::SetNextItemWidth( ItemWidth );
        ImGui::PushID( "#left" );
        if( ImGui::Combo( ICON_FA_LEFT_LONG, &leftBound, "First\0Second\0NotAKnot\0" ) )
        {
            edits.dirty = true;
        }
        ImGui::SameLine();

        ImGui::SetCursorPosX( fixedX + ItemWidth + ControlSpc );
        ImGui::SetNextItemWidth( ItemWidth );
        ImGui::PushID( "#value" );
        if( ImGui::DragFloat( "", &edits.leftValue, 0.01, 0, 1 ) )
            edits.dirty = true;
        ImGui::PopID();
        ImGui::PopID();
        ImGui::SetCursorPosY( ImGui::GetCursorPosY() + 2 );
        ImGui::SetNextItemWidth( ItemWidth );
        ImGui::PushID( "#right" );
        if( ImGui::Combo( ICON_FA_RIGHT_LONG, &rightBound, "First\0Second\0NotAKnot\0" ) )
        {
            edits.dirty = true;
        }
        ImGui::SameLine();
        ImGui::SetCursorPosX( fixedX + ItemWidth + ControlSpc );
        ImGui::SetNextItemWidth( ItemWidth );
        ImGui::PushID( "#value" );
        if( ImGui::DragFloat( "", &edits.rightValue, 0.01, 0, 1 ) )
            edits.dirty = true;
        ImGui::PopID();
        ImGui::PopID();
        ImGui::SetCursorPosY( ImGui::GetCursorPosY() + 2 );
        ImGui::SetNextItemWidth( ItemWidth );
        if( ImGui::Checkbox( "Live Update", &edits.liveUpdate ) )
        {
            edits.dirty = true;
        }

        ImDrawList*  DrawList = GetWindowDrawList();
        ImGuiWindow* Window   = GetCurrentWindow();
        ImVec2       canvas( Canvas, Canvas );

        Dummy( ImVec2( 0, 4 ) );

        ImRect bb( Window->DC.CursorPos, Window->DC.CursorPos + canvas );
        ItemSize( bb );
        if( !ItemAdd( bb, NULL ) )
            return false;

        auto          avail = GetContentRegionAvail();
        const ImGuiID id    = Window->GetID( label );

        PushItemFlag( ImGuiItemFlags_NoNav, true );
        RenderFrame( bb.Min, bb.Max, GetColorU32( ImGuiCol_FrameBg, 1 ), true, style.FrameRounding );
        SetCursorScreenPos( bb.Min );
        InvisibleButton( "#f", canvas );
        // background grid
        for( int i = 0; i <= canvas.x; i += ( canvas.x / 4 ) )
        {
            DrawList->AddLine( ImVec2( bb.Min.x + i, bb.Min.y ), ImVec2( bb.Min.x + i, bb.Max.y ),
                               GetColorU32( ImGuiCol_TextDisabled ) );
        }

        for( int i = 0; i <= canvas.y; i += ( canvas.y / 4 ) )
        {
            DrawList->AddLine( ImVec2( bb.Min.x, bb.Min.y + i ), ImVec2( bb.Max.x, bb.Min.y + i ),
                               GetColorU32( ImGuiCol_TextDisabled ) );
        }

        DrawList->PushClipRect( bb.Min, bb.Max, true );
        // Curve
        ImColor         color( style.Colors[ImGuiCol_PlotLines] );
        float           lastY        = curve( 0 );
        float           lastX        = 0;
        constexpr float CurveSpacing = 1 / ( Smoothing - 1 );
        for( int i = 1; i < Smoothing; ++i )
        {
            float  currentX = i * CurveSpacing;
            float  currentY = curve( currentX );
            ImVec2 r( lastX * ( bb.Max.x - bb.Min.x ) + bb.Min.x, ( 1 - lastY ) * ( bb.Max.y - bb.Min.y ) + bb.Min.y );
            ImVec2 s( currentX * ( bb.Max.x - bb.Min.x ) + bb.Min.x,
                      ( 1 - currentY ) * ( bb.Max.y - bb.Min.y ) + bb.Min.y );
            DrawList->AddLine( r, s, color, CurveWidth );
            lastX = currentX;
            lastY = currentY;
        }

        auto const& x = edits.cx;
        auto const& y = edits.cy;

        auto&  io             = GetIO();
        ImVec2 mouse          = io.MousePos;
        char   control[]      = "#0p";
        bool   itemControlled = false;


        for( uint32_t i = 0; i < (uint32_t)x.size(); ++i )
        {
            ImVec2 p( x[i] * ( bb.Max.x - bb.Min.x ) + bb.Min.x, ( 1 - y[i] ) * ( bb.Max.y - bb.Min.y ) + bb.Min.y );
            control[1] += i;
            auto distance = p - mouse;
            if( std::fabs( distance.x ) < GrabRadius && std::fabs( distance.y ) < GrabRadius ||
                edits.dragged == (int)i )
            {
                DrawList->AddCircleFilled( p, GrabRadius, HoveredCircleColor );
                DrawList->AddCircle( p, GrabRadius + 1, HoveredCircleBorderColor );
                SetTooltip( "(%4.3f, %4.3f)", x[i], y[i] );
                if( IsMouseDown( ImGuiMouseButton_Left ) || IsMouseDragging( ImGuiMouseButton_Left ) )
                {
                    itemControlled = true;
                    edits.cx[i] += ( io.MouseDelta.x / canvas.x );
                    edits.cy[i] -= ( io.MouseDelta.y / canvas.y );
                    edits.dirty   = true;
                    edits.dragged = (int)i;
                    edits.cx[i]   = std::clamp( edits.cx[i], 0.0f, 1.0f );
                    edits.cy[i]   = std::clamp( edits.cy[i], 0.0f, 1.0f );
                }
                else if( IsMouseClicked( ImGuiMouseButton_Right ) )
                {
                    if( x.size() > 3 )
                    {
                        edits.cx.erase( edits.cx.begin() + i );
                        edits.cy.erase( edits.cy.begin() + i );
                        edits.dirty = true;
                    }
                    else if( i == 1 )
                    {
                        edits.cx[1] = 0.5f;
                        edits.cy[1] = 0.5f;
                        edits.dirty = true;
                    }
                }
            }
            else
            {
                DrawList->AddCircleFilled( p, GrabRadius, CircleColor );
                DrawList->AddCircle( p, GrabRadius, CircleBorderColor );
            }
        }
        DrawList->PopClipRect();

        if( !itemControlled && IsMouseClicked( ImGuiMouseButton_Left ) && bb.Contains( mouse ) )
        {
            float nx = ( mouse.x - bb.Min.x ) / ( bb.Max.x - bb.Min.x );
            float ny = 1 - ( mouse.y - bb.Min.y ) / ( bb.Max.y - bb.Min.y );

            auto bound = std::lower_bound( x.begin(), x.end(), nx );
            if( bound == x.end() || *bound != nx )
            {
                auto where = std::distance( x.begin(), bound );
                edits.cx.insert( bound, nx );
                edits.cy.insert( edits.cy.begin() + where, ny );
                edits.dirty = true;
            }
        }
        else if( itemControlled && edits.dragged >= 0 && edits.dragged < x.size() )
        {
            // sort
            float xx = x[edits.dragged];
            float yy = y[edits.dragged];
            edits.cx.erase( edits.cx.begin() + edits.dragged );
            edits.cy.erase( edits.cy.begin() + edits.dragged );
            auto bound = std::lower_bound( x.begin(), x.end(), xx );
            if( bound != x.end() && *bound == xx )
            {
                auto where    = std::distance( x.begin(), bound );
                edits.dragged = (int)where;
            }
            else if( bound == x.end() || *bound != xx )
            {
                auto where = std::distance( x.begin(), bound );
                edits.cx.insert( bound, xx );
                edits.cy.insert( edits.cy.begin() + where, yy );
                edits.dragged = (int)where;
            }
        }

        if( edits.dirty )
        {
            switch( type )
            {
            case 0:
                edits.type = tk::spline<>::linear;
                break;
            case 1:
                edits.type = tk::spline<>::cspline;
                break;
            case 2:
                edits.type = tk::spline<>::cspline_hermite;
                break;
            }

            rightBound += 1;
            leftBound += 1;

            edits.right = (tk::spline<>::bd_type)rightBound;
            edits.left  = (tk::spline<>::bd_type)leftBound;
        }

        PopItemFlag();
        if( IsMouseReleased( ImGuiMouseButton_Left ) || IsMouseReleased( ImGuiMouseButton_Right ) )
            return data.EndEdits( true );
        return data.EndEdits( false );
    }

    bool DrawCurveEditor( const char* label, FastNoise::CurveData& data )
    {
        ImGuiContext& g            = *GImGui;
        bool          valueChanged = false;
        const float   square_sz    = GetFrameHeight();
        ImGuiStyle&   style        = g.Style;
        // const float   w_full       = CalcItemWidth();

        PushID( label );
        if( Button( ICON_FA_BEZIER_CURVE, ImVec2( 20, 20 ) ) )
        {
            OpenPopup( "#curve" );
        }

        style.PopupRounding = 16;
        style.WindowPadding = ImVec2( 16, 16 );

        if( BeginPopup( "#curve", ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize ) )
        {
            auto picker_active_window = g.CurrentWindow;
            valueChanged |= DrawEditorPopup( "##curve", data );
            EndPopup();
        }

        style.WindowPadding = ImVec2( 0, 0 );
        style.PopupRounding = 0;

        PopID();
        return valueChanged;
    }

}; // namespace ImGui