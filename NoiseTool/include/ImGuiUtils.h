#pragma once
#include <Magnum/Magnum.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Math/Vector4.h>
#include "IconsFontAwesome6.h"
#include "Settings.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>
#include <imnodes.h>
#include <misc/cpp/imgui_stdlib.h>
#include "ImGuiFileDialog.h"

namespace Magnum
{

    inline bool IconButton( char const* name, float x, ImVec2 size, ImU32 hover )
    {
        bool clicked = false;
        ImGui::SetCursorPosX( x );
        std::string nameAlt = "##";
        nameAlt += name;
        if( ImGui::InvisibleButton( nameAlt.c_str(), ImVec2( 20, 20 ) ) )
            clicked = true;

        bool popStyle = false;
        if( ImGui::IsItemHovered() )
        {
            popStyle = true;
            ImGui::PushStyleColor( ImGuiCol_Text, hover );
        }

        ImGui::SameLine();
        ImGui::SetCursorPosX( x );
        ImGui::Text( name );

        if( popStyle )
        {
            ImGui::PopStyleColor();
            popStyle = false;
        }
        return clicked;
    }


    inline bool ToggleButton( char const* name, bool& toggle, ImVec2 size, const char* desc, int padding = 0 )
    {
        bool        clicked = false;
        std::string nameAlt = "##";
        nameAlt += name;
        auto x = ImGui::GetCursorPosX();
        auto y = ImGui::GetCursorPosY();
        if( ImGui::InvisibleButton( nameAlt.c_str(), size ) )
            clicked = true;

        if( ImGui::IsItemHovered() )
        {
            ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 4.f, 4.f ) );
            ImGui::BeginTooltip();
            ImGui::TextUnformatted( desc );
            ImGui::EndTooltip();
            ImGui::PopStyleVar();

            ImGui::PushStyleColor( ImGuiCol_Text, ImGui::GetStyleColorVec4( ImGuiCol_ButtonHovered ) );
        }
        else if( toggle )
        {
            ImGui::PushStyleColor( ImGuiCol_Text, ImGui::GetStyleColorVec4( ImGuiCol_ButtonActive ) );
        }
        else
            ImGui::PushStyleColor( ImGuiCol_Text, ImGui::GetStyleColorVec4( ImGuiCol_Text ) );

        ImGui::SameLine();
        if( padding )
            ImGui::SetCursorPosY( y + padding );
        ImGui::SetCursorPosX( x + padding );
        ImGui::Text( name );

        ImGui::PopStyleColor();

        if( clicked )
            toggle = !toggle;
        return clicked;
    }

    inline void Tooltip( const char* desc )
    {
        if( ImGui::IsItemHovered() )
        {
            ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 4.f, 4.f ) );
            ImGui::BeginTooltip();
            ImGui::TextUnformatted( desc );
            ImGui::EndTooltip();
            ImGui::PopStyleVar();
        }
    }

    inline bool Button( const char* name, const char* desc, ImVec2 size = { 0, 0 } )
    {
        bool result = ImGui::Button( name, size );
        Tooltip( desc );

        return result;
    }

    inline bool AutoElement( Vector2i& d, char const* name, int min, int max, char const* desc )
    {
        bool changed = ImGui::DragInt2( name, d.data(), 1, min, max );
        Tooltip( desc );

        return changed;
    }

    inline bool AutoElement( float& d, char const* name, float min, float max, char const* desc )
    {
        bool changed = ImGui::DragFloat( name, &d, 0.05f, min, max );
        Tooltip( desc );

        return changed;
    }

    inline bool AutoElement( int& d, char const* name, int min, int max, char const* desc )
    {
        bool changed = ImGui::DragInt( name, &d, 1, min, max );
        Tooltip( desc );

        return changed;
    }

    inline bool AutoElement( Rotation& d, char const* name, int min, int max, char const* desc )
    {
        bool changed = ImGui::DragFloat2( name, (float*)&d, 0.5f, 0, 360 );
        Tooltip( desc );

        return changed;
    }

    inline bool AutoElement( Color3& d, char const* name, int min, int max, char const* desc )
    {
        bool changed = ImGui::ColorEdit3( name, d.data(), ImGuiColorEditFlags_NoInputs );
        Tooltip( desc );

        return changed;
    }

    inline bool AutoElement( ColorLayerValue& d, char const* name, int min, int max, char const* desc )
    {
        bool changed = false;
        bool deleted = false;
        // color panel
        int   colorIndex   = 0;
        float lastAlpha[3] = { 0 };

        for( auto& [color, active]: d )
        {
            for( int cl = 0; cl < 3; cl++ )
            {
                ImGui::PushID( colorIndex++ );
                if( ImGui::ColorEdit4( "", color[cl].data(),
                                       ImGuiColorEditFlags_::ImGuiColorEditFlags_NoInputs |
                                           ImGuiColorEditFlags_AlphaBar ) )
                {
                    if( color[cl].a() < lastAlpha[cl] )
                        color[cl].a() = lastAlpha[cl];
                    lastAlpha[cl] = color[cl].a();
                    changed       = true;
                }
                ImGui::PopID();
                ImGui::SameLine();
            }
            ImGui::SetNextItemWidth( 20 );
            ImGui::PushID( colorIndex++ );
            if( ImGui::Button( ICON_FA_TRASH_CAN ) )
            {
                changed = true;
                active  = false;
                deleted = true;
            }
            ImGui::PopID();
        }

        if( Button( ICON_FA_LAYER_GROUP, desc ) )
        {
            if( d.empty() )
                d.emplace_back( Color4( 0.2f, 0.2f, 0.2f, 0.0f ), true );
            else
                d.emplace_back( d.back() );
            changed = true;
        }

        if( deleted )
        {
            auto beg = d.begin();
            while( beg != d.end() )
            {
                if( !std::get<1>( *beg ) )
                    beg = d.erase( beg );
                else
                    beg++;
            }
        }

        return changed;
    }

    inline bool AutoElement( std::string& d, char const* name, int min, int max, char const* desc )
    {
        bool changed = ImGui::InputText( "Name", &d, ImGuiInputTextFlags_CharsNoBlank );
        Tooltip( desc );

        return changed;
    }

    inline bool AutoElement( Directory& d, char const* name, int min, int max, char const* desc )
    {
        bool changed = false;
        if( Magnum::Button( "Browse", desc ) )
            ImGuiFileDialog::Instance()->OpenDialog( name, "PNG ", nullptr, d.path );

        if( ImGuiFileDialog::Instance()->Display( name, 32, ImVec2 { 600, 400 } ) )
        {
            if( changed = ImGuiFileDialog::Instance()->IsOk() )
                d.path = ImGuiFileDialog::Instance()->GetFilePathName();

            ImGuiFileDialog::Instance()->Close();
        }

        return changed;
    }


    inline bool AutoElement( GeneratorPtr& d, char const* name, float min, float max, char const* desc )
    {
        return false;
    }

    template<typename T, typename L>
    inline bool AutoElementP( T* d, char const* name, L min, L max, char const* desc )
    {
        return AutoElement( *d, name, min, max, desc );
    }
} // namespace Magnum