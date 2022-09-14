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
} // namespace Magnum