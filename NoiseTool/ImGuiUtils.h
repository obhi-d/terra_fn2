#pragma once
#include "IconsFontAwesome6.h"

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


    inline bool ToggleButton( char const* name, bool& toggle, ImVec2 size, const char* desc )
    {
        bool        clicked = false;
        std::string nameAlt = "##";
        nameAlt += name;
        auto x = ImGui::GetCursorPosX();
        if( ImGui::InvisibleButton( nameAlt.c_str(), ImVec2( 20, 20 ) ) )
            clicked = true;

        if( ImGui::IsItemHovered() )
        {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted( desc );
            ImGui::EndTooltip();

            ImGui::PushStyleColor( ImGuiCol_Text, ImGui::GetStyleColorVec4( ImGuiCol_ButtonHovered ) );
        }
        else if( toggle )
        {
            ImGui::PushStyleColor( ImGuiCol_Text, ImGui::GetStyleColorVec4( ImGuiCol_ButtonActive ) );
        }
        else
            ImGui::PushStyleColor( ImGuiCol_Text, ImGui::GetStyleColorVec4( ImGuiCol_Text ) );

        ImGui::SameLine();
        ImGui::SetCursorPosX( x );
        ImGui::Text( name );

        ImGui::PopStyleColor();

        if( clicked )
            toggle = !toggle;
        return toggle;
    }

    inline bool Button( const char* name, const char* desc )
    {
        bool result = ImGui::Button( name );

        if( ImGui::IsItemHovered() )
        {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted( desc );
            ImGui::EndTooltip();
        }

        return result;
    }
} // namespace Magnum