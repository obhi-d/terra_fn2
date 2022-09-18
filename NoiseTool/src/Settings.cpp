
#include "Settings.h"
#include "StreamHelper.h"


using namespace Magnum;

std::string const& format( std::string_view iname )
{
    static std::string name = {};
    name                    = "";
    for( auto i: iname )
    {
        if( std::isupper( i ) )
            name.push_back( ' ' );
        name.push_back( i );
    }
    name[0] = std::toupper( name[0] );
    return name;
}

#define FORMAT_NAME( name ) format( name ).c_str()


Settings::Settings()
{
    ImGuiSettingsHandler editorSettings;
    editorSettings.TypeName   = "TerraSettings";
    editorSettings.TypeHash   = ImHashStr( editorSettings.TypeName );
    editorSettings.UserData   = this;
    editorSettings.WriteAllFn = []( ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* outBuf ) {
        auto* _ = (Settings*)handler->UserData;
        outBuf->appendf( "\n[%s][Settings]\n", handler->TypeName );
#define PROPERTY( Type, Name, Loc, Init, Min, Max, Desc ) Write( outBuf, #Name, _->TOKEN_PASTE( _, Name ) );
#include "Settings.inl"
#undef PROPERTY
    };
    editorSettings.ReadOpenFn = []( ImGuiContext* ctx, ImGuiSettingsHandler* handler, const char* name ) -> void* {
        if( strcmp( name, "Settings" ) == 0 )
        {
            return handler->UserData;
        }

        return nullptr;
    };
    editorSettings.ReadLineFn = []( ImGuiContext* ctx, ImGuiSettingsHandler* handler, void* entry, const char* line ) {
        auto* _ = (Settings*)handler->UserData;
#define PROPERTY( Type, Name, Loc, Init, Min, Max, Desc ) Read( line, #Name, _->TOKEN_PASTE( _, Name ) );
#include "Settings.inl"
#undef PROPERTY
    };
    ImGuiExtra::AddOrReplaceSettingsHandler( editorSettings );
}

Settings& Settings::get()
{
    static Settings settings;
    return settings;
}

void Settings::beginFrame()
{
}

enum Placement
{
    SettingsUI_NewLine,
    SettingsUI_SameLine,
    ExportUI_NewLine,
    ExportUI_SameLine,
    Hidden
};

void Settings::draw()
{

#define PROPERTY( Type, Name, Loc, Init, Min, Max, Desc )                                                              \
    if constexpr( Loc != Hidden )                                                                                      \
    {                                                                                                                  \
        if constexpr( Loc == SettingsUI_SameLine )                                                                     \
            ImGui::SameLine();                                                                                         \
        if constexpr( Loc == SettingsUI_SameLine || Loc == SettingsUI_NewLine )                                        \
        {                                                                                                              \
            if( AutoElementP( &TOKEN_PASTE( _, Name ), (const char*)FORMAT_NAME( #Name ), Min, Max,                    \
                              (const char*)Desc ) )                                                                    \
                _version[TOKEN_PASTE( eProp_, Name )]++;                                                               \
        }                                                                                                              \
    }

#include "Settings.inl"

#undef PROPERTY
}

void Settings::drawExport()
{
#define PROPERTY( Type, Name, Loc, Init, Min, Max, Desc )                                                              \
    if constexpr( Loc != Hidden )                                                                                      \
    {                                                                                                                  \
        if constexpr( Loc == ExportUI_SameLine )                                                                       \
            ImGui::SameLine();                                                                                         \
        if constexpr( Loc == ExportUI_SameLine || Loc == ExportUI_NewLine )                                            \
        {                                                                                                              \
            if( AutoElementP( &TOKEN_PASTE( _, Name ), (const char*)FORMAT_NAME( #Name ), Min, Max,                    \
                              (const char*)Desc ) )                                                                    \
                _version[TOKEN_PASTE( eProp_, Name )]++;                                                               \
        }                                                                                                              \
    }

#include "Settings.inl"

#undef PROPERTY
}

void Settings::endFrame()
{
}

void Settings::resetOffsets()
{
    auto ms = Vector2i( 0, 0 );
    if( ms != offset() )
        offset( ms );
}

void Settings::quit()
{
    for( auto t: _version.value )
    {
        if( t != 0 )
        {
            ImGuiExtra::MarkSettingsDirty();
            return;
        }
    }
}
