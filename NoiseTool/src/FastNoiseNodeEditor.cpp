#include <cstdio>
#include <filesystem>
#include <random>
#include <sstream>
#include <string_view>

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>
#include <imnodes.h>
#include <misc/cpp/imgui_stdlib.h>

#include <Corrade/Containers/ArrayViewStl.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/ImGuiIntegration/Widgets.h>
#include <Magnum/PixelFormat.h>

#include "DemoNodeTrees.inl"
#include "FastNoiseNodeEditor.h"
#include "IconsFontAwesome6.h"
#include "ImGuiCurveEditor.h"
#include "ImGuiExtra.h"
#include "ImGuiFileDialog.h"
#include "ImGuiUtils.h"
#include "ImageImporter.h"

using namespace Magnum;

// clang-format off
std::array<NodeStyleDesc, 4> gStyles = 
{ 
  //              Title                              Title Hovered                       Title Selected                      Button Hovered                      Button Active                   Text Color
  NodeStyleDesc { ImVec4(0.10f, 0.30f, 0.20f, 1.0f), ImVec4(0.20f, 0.40f, 0.30f, 1.00f), ImVec4(0.30f, 0.50f, 0.40f, 1.00f), ImVec4(0.70f, 0.70f, 0.70f, 1.00f), ImVec4(0.1f, 0.1f, 0.2f, 1.0f), ImVec4(0.9f, 0.9f, 0.9f, 1.0f), }, 
  NodeStyleDesc { ImVec4(0.60f, 0.50f, 0.30f, 1.0f), ImVec4(0.60f, 0.56f, 0.36f, 1.00f), ImVec4(0.60f, 0.56f, 0.46f, 1.00f), ImVec4(0.70f, 0.70f, 0.70f, 1.00f), ImVec4(0.1f, 0.1f, 0.2f, 1.0f), ImVec4(0.9f, 0.9f, 0.9f, 1.0f), }, 
  NodeStyleDesc { ImVec4(0.50f, 0.34f, 0.38f, 1.0f), ImVec4(0.66f, 0.44f, 0.49f, 1.00f), ImVec4(0.71f, 0.47f, 0.53f, 1.00f), ImVec4(0.70f, 0.70f, 0.70f, 1.00f), ImVec4(0.1f, 0.1f, 0.2f, 1.0f), ImVec4(0.9f, 0.9f, 0.9f, 1.0f), }, 
  NodeStyleDesc { ImVec4(0.22f, 0.25f, 0.27f, 1.0f), ImVec4(0.40f, 0.44f, 0.47f, 1.00f), ImVec4(0.47f, 0.52f, 0.56f, 1.00f), ImVec4(0.70f, 0.70f, 0.70f, 1.00f), ImVec4(0.1f, 0.1f, 0.2f, 1.0f), ImVec4(0.9f, 0.9f, 0.9f, 1.0f), }, 
};
// clang-format on


void Import( FastNoise::ImageData& other )
{
    if( !other.sourceName.empty() )
        other = ImportImage( other.sourceName );
}

bool MatchingGroup( const std::vector<const char*>& a, const std::vector<const char*>& b )
{
    std::string aString;
    for( const char* c: a )
    {
        aString.append( c );
        aString.push_back( '\t' );
    }

    std::string bString;
    for( const char* c: b )
    {
        bString.append( c );
        bString.push_back( '\t' );
    }

    return aString == bString;
}

template<typename T>
bool MatchingMembers( const std::vector<T>& a, const std::vector<T>& b )
{
    if( a.size() != b.size() )
    {
        return false;
    }

    for( size_t i = 0; i < a.size(); i++ )
    {
        if( strcmp( a[i].name, b[i].name ) != 0 )
        {
            return false;
        }
    }
    return true;
}

FastNoiseNodeEditor::Node::Node( FastNoiseNodeEditor& e, FastNoise::NodeData* nodeData, bool generatePreview, int id ) :
    Node( e, std::unique_ptr<FastNoise::NodeData>( nodeData ), generatePreview, id )
{
}

FastNoiseNodeEditor::Node::Node( FastNoiseNodeEditor& e, std::unique_ptr<FastNoise::NodeData>&& nodeData,
                                 bool generatePreview, int id ) :
    editor( e ),
    data( std::move( nodeData ) ), nodeId( id ? id : e.GetFreeNodeId() )
{
    assert( !e.FindNodeFromId( id ) );

    GeneratePreview( generatePreview );
}

void FastNoiseNodeEditor::Node::GeneratePreview( bool nodeTreeChanged, bool benchmark )
{
    static FastNoise::Buffer noiseData( NoiseSize * NoiseSize );

    serialised     = FastNoise::Metadata::SerialiseNodeData( data.get(), true );
    auto generator = FastNoise::NewFromEncodedNodeTree( serialised.c_str(), editor.mMaxSIMDLevel );

    if( !benchmark && nodeTreeChanged )
    {
        generateAverages.clear();
    }

    if( editor.IsTexturePreviewEnabled() )
    {

        if( generator )
        {
            auto genRGB = FastNoise::New<FastNoise::ConvertRGBA8>( editor.mMaxSIMDLevel );
            genRGB->SetSource( generator );

            auto startTime = std::chrono::high_resolution_clock::now();

            editor.GenerateNodePreviewNoise( genRGB.get(), noiseData );

            generateAverages.push_back( std::chrono::duration_cast<std::chrono::nanoseconds>(
                                            std::chrono::high_resolution_clock::now() - startTime )
                                            .count() -
                                        editor.mOverheadNode.totalGenerateNs );

            std::sort( generateAverages.begin(), generateAverages.end() );

            totalGenerateNs = generateAverages[generateAverages.size() / 3];
        }
        else
        {
            std::fill( noiseData.begin(), noiseData.end(), 0.0f );
            serialised.clear();
            totalGenerateNs = 0;
        }

        if( benchmark )
        {
            return;
        }

        ImageView2D noiseImage(
            PixelFormat::RGBA8Unorm, { NoiseSize, NoiseSize },
            Corrade::Containers::ArrayView<const char>( (const char*)noiseData.begin(), noiseData.nbbytes() ) );

        noiseTexture.setStorage( 1, GL::TextureFormat::RGBA8, noiseImage.size() ).setSubImage( 0, {}, noiseImage );
        hasTexture = true;
    }
    else
    {
        hasTexture   = false;
        noiseTexture = GL::Texture2D();
    }

    for( auto& node: editor.mNodes )
    {
        for( FastNoise::NodeData* link: node.second.GetNodeIDLinks() )
        {
            if( link == data.get() )
            {
                node.second.GeneratePreview( nodeTreeChanged );
            }
        }
    }

    if( nodeTreeChanged )
    {
        if( editor.mSelectedNode == data.get() )
        {
            editor.ChangeSelectedNode( data.get() );
        }

        // Save nodes to ini
        ImGuiExtra::MarkSettingsDirty();
    }
}


std::vector<FastNoise::NodeData*> FastNoiseNodeEditor::Node::GetNodeIDLinks()
{
    std::vector<FastNoise::NodeData*> links;
    links.reserve( data->nodeLookups.size() + data->hybrids.size() );

    for( FastNoise::NodeData* link: data->nodeLookups )
    {
        links.emplace_back( link );
    }

    for( auto& link: data->hybrids )
    {
        links.emplace_back( link.first );
    }

    assert( links.size() < 16 );

    return links;
}

uint64_t FastNoiseNodeEditor::Node::GetLocalGenerateNs()
{
    int64_t localTotal = totalGenerateNs;

    for( FastNoise::NodeData* link: GetNodeIDLinks() )
    {
        auto find = editor.mNodes.find( link );

        if( find != editor.mNodes.end() )
        {
            localTotal -= find->second.totalGenerateNs;
        }
    }

    return std::max<int64_t>( localTotal, 0 );
}

FastNoise::NodeData*& FastNoiseNodeEditor::Node::GetNodeLink( int attributeId )
{
    attributeId &= 15;

    if( attributeId < (int)data->nodeLookups.size() )
    {
        return data->nodeLookups[attributeId];
    }
    else
    {
        attributeId -= (int)data->nodeLookups.size();
        return data->hybrids[attributeId].first;
    }
}

void FastNoiseNodeEditor::Node::AutoPositionChildNodes( ImVec2 nodePos, float verticalSpacing )
{
    auto nodeLinks = GetNodeIDLinks();
    nodeLinks.erase( std::remove( nodeLinks.begin(), nodeLinks.end(), nullptr ), nodeLinks.end() );

    if( nodeLinks.empty() )
    {
        return;
    }

    ImVec2 nodeSpacing = { 280, verticalSpacing };

    nodePos.x -= nodeSpacing.x;
    nodePos.y -= nodeSpacing.y * 0.5f * ( nodeLinks.size() - 1 );

    for( FastNoise::NodeData* link: nodeLinks )
    {
        ImNodes::SetNodeScreenSpacePos( editor.mNodes.at( link ).nodeId, nodePos );

        editor.mNodes.at( link ).AutoPositionChildNodes(
            nodePos, nodeLinks.size() > 1 ? verticalSpacing * 0.6f : verticalSpacing );
        nodePos.y += nodeSpacing.y;
    }
}

bool FastNoiseNodeEditor::MetadataMenuItem::CanDraw( std::function<bool( const FastNoise::Metadata* )> isValid ) const
{
    return !isValid || isValid( metadata );
}

const FastNoise::Metadata* FastNoiseNodeEditor::MetadataMenuItem::DrawUI(
    std::function<bool( const FastNoise::Metadata* )> isValid, bool drawGroups ) const
{
    std::string format = FastNoise::Metadata::FormatMetadataNodeName( metadata, true );

    if( ImGui::MenuItem( format.c_str() ) )
    {
        return metadata;
    }
    return nullptr;
}

bool FastNoiseNodeEditor::MetadataMenuGroup::CanDraw( std::function<bool( const FastNoise::Metadata* )> isValid ) const
{
    for( const auto& item: items )
    {
        if( item->CanDraw( isValid ) )
        {
            return true;
        }
    }
    return false;
}

const FastNoise::Metadata* FastNoiseNodeEditor::MetadataMenuGroup::DrawUI(
    std::function<bool( const FastNoise::Metadata* )> isValid, bool drawGroups ) const
{
    const FastNoise::Metadata* returnPressed = nullptr;

    bool doGroup = drawGroups && name[0] != 0;

    if( !doGroup || ImGui::BeginMenu( name ) )
    {
        for( const auto& item: items )
        {
            if( item->CanDraw( isValid ) )
            {
                if( auto pressed = item->DrawUI( isValid, drawGroups ) )
                {
                    returnPressed = pressed;
                }
            }
        }
        if( doGroup )
        {
            ImGui::EndMenu();
        }
    }
    return returnPressed;
}

void FastNoiseNodeEditor::Node::SerialiseIncludingDependancies( ImGuiSettingsHandler* handler, ImGuiTextBuffer* buffer,
                                                                std::unordered_set<int>& serialisedNodeIds )
{
    if( serialisedNodeIds.find( nodeId ) != serialisedNodeIds.end() )
    {
        return;
    }

    for( FastNoise::NodeData* nodeData: GetNodeIDLinks() )
    {
        if( nodeData )
        {
            editor.mNodes.at( nodeData ).SerialiseIncludingDependancies( handler, buffer, serialisedNodeIds );
        }
    }

    buffer->appendf( "\n[%s][Node:%d]\n", handler->TypeName, data->metadata->id );


    for( const auto& var: data->variables )
    {
        buffer->appendf( "variable=%d\n", var.i );
    }
    for( const auto& node: data->nodeLookups )
    {
        buffer->appendf( "node=%d\n", node ? editor.mNodes.at( node ).nodeId : 0 );
    }
    for( const auto& hybrid: data->hybrids )
    {
        buffer->appendf( "hybrid=%i:%f\n", hybrid.first ? editor.mNodes.at( hybrid.first ).nodeId : 0, hybrid.second );
    }
    for( const auto& image: data->images )
    {
        buffer->appendf( "image=%d\n", image.index );
    }
    for( const auto& curve: data->curves )
    {
        auto const& cx = curve.spline.get_x();
        buffer->appendf( "curve=%d:[", (int)cx.size() );
        for( auto x: cx )
            buffer->appendf( "%f,", x );
        auto const& cy = curve.spline.get_y();
        buffer->appendf( "0]:%d:[", (int)cy.size() );
        for( auto y: cy )
            buffer->appendf( "%f,", y );
        auto left       = curve.spline.get_left_deriv();
        auto leftValue  = curve.spline.get_left_value();
        auto right      = curve.spline.get_right_deriv();
        auto rightValue = curve.spline.get_right_value();
        auto monotonic  = curve.spline.is_monotonic();
        auto type       = curve.spline.get_type();
        buffer->appendf( "0]:%d:%f:%d:%f:%d:%d\n", left, leftValue, right, rightValue, (int)monotonic, type );
    }
    // id must be after setting all members, it verifies and creates the node
    buffer->appendf( "id=%i\n", nodeId );

    // Must be after node creation
    ImVec2 gridPos = ImNodes::GetNodeGridSpacePos( nodeId );
    buffer->appendf( "grid_pos=%f:%f\n", gridPos.x, gridPos.y );

    serialisedNodeIds.emplace( nodeId );
}

void FastNoiseNodeEditor::SetupSettingsHandlers()
{
    FastNoise::ImageData::Importer = Import;
    ImGuiSettingsHandler nodeSettings;
    nodeSettings.TypeName = "NoiseToolNodeData";
    nodeSettings.TypeHash = ImHashStr( nodeSettings.TypeName );
    nodeSettings.UserData = this;

    nodeSettings.WriteAllFn = []( ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* outBuf ) {
        auto* nodeEditor = (FastNoiseNodeEditor*)handler->UserData;

        std::unordered_set<int> serialisedNodeIds;

        // Save all root nodes
        for( auto& node: nodeEditor->mNodes )
        {
            bool hasReference = false;

            for( auto& linkNode: nodeEditor->mNodes )
            {
                auto links = linkNode.second.GetNodeIDLinks();

                if( std::find( links.begin(), links.end(), node.first ) != links.end() )
                {
                    hasReference = true;
                    break;
                }
            }

            if( !hasReference )
            {
                node.second.SerialiseIncludingDependancies( handler, outBuf, serialisedNodeIds );
            }
        }
    };
    nodeSettings.ReadOpenFn = []( ImGuiContext* ctx, ImGuiSettingsHandler* handler, const char* name ) -> void* {
        int metadataId;
        if( sscanf( name, "Node:%d", &metadataId ) == 1 )
        {
            if( const FastNoise::Metadata* metadata = FastNoise::Metadata::GetFromId( metadataId ) )
            {
                FastNoise::NodeData* nodeData = new FastNoise::NodeData( metadata );
                nodeData->nodeLookups.clear();
                nodeData->variables.clear();
                nodeData->hybrids.clear();
                nodeData->images.clear();
                nodeData->curves.clear();
                return nodeData;
            }
        }

        return nullptr;
    };
    nodeSettings.ReadLineFn = []( ImGuiContext* ctx, ImGuiSettingsHandler* handler, void* entry, const char* line ) {
        auto* nodeEditor = (FastNoiseNodeEditor*)handler->UserData;
        auto* nodeData   = (FastNoise::NodeData*)entry;

        ImVec2           imVec2;
        float            f;
        int              i;
        std::string_view vline( line );
        if( sscanf( line, "grid_pos=%f:%f", &imVec2.x, &imVec2.y ) == 2 )
        {
            auto find = nodeEditor->mNodes.find( nodeData );
            if( find != nodeEditor->mNodes.end() )
            {
                ImNodes::SetNodeGridSpacePos( find->second.nodeId, imVec2 );
            }

            if( nodeEditor->mNodes.size() <= 1 )
            {
                nodeEditor->ChangeSelectedNode( nodeData );
            }
        }
        else if( sscanf( line, "variable=%d", &i ) == 1 )
        {
            nodeData->variables.push_back( i );
        }
        else if( sscanf( line, "node=%d", &i ) == 1 )
        {
            Node* link = nodeEditor->FindNodeFromId( i );

            nodeData->nodeLookups.push_back( link ? link->data.get() : nullptr );
        }
        else if( sscanf( line, "image=%d", &i ) == 1 )
        {
            nodeData->images.push_back( { i } );
        }
        else if( vline.starts_with( "curve=" ) )
        {
            nodeData->curves.resize( nodeData->curves.size() + 1 );
            FastNoise::CurveData& curve = nodeData->curves.back();

            vline = vline.substr( sizeof( "curve=" ) - 1 );

            auto stop = vline.find_first_of( ':' );
            auto n    = vline.substr( 0, stop );
            std::from_chars( n.data(), n.data() + n.size(), i );
            vline = vline.substr( stop + 1 );

            std::vector<float> cx;
            cx.resize( (size_t)i );

            for( auto& x: cx )
            {
                stop = vline.find_first_of( ',' );
                n    = vline.substr( 0, stop );
                std::from_chars( n.data(), n.data() + n.size(), x );
                vline = vline.substr( stop + 1 );
            }

            stop  = vline.find_first_of( ':' );
            vline = vline.substr( stop + 1 );
            stop  = vline.find_first_of( ':' );
            n     = vline.substr( 0, stop );
            std::from_chars( n.data(), n.data() + n.size(), i );
            vline = vline.substr( stop + 1 );

            std::vector<float> cy;
            cy.resize( (size_t)i );

            for( auto& y: cy )
            {
                stop = vline.find_first_of( ',' );
                n    = vline.substr( 0, stop );
                std::from_chars( n.data(), n.data() + n.size(), y );
                vline = vline.substr( stop + 1 );
            }

            tk::spline<>::spline_type type       = tk::spline<>::spline_type::cspline_hermite;
            tk::spline<>::bd_type     left       = tk::spline<>::second_deriv;
            tk::spline<>::bd_type     right      = tk::spline<>::first_deriv;
            float                     leftValue  = 0;
            float                     rightValue = 0;
            bool                      monotonic  = false;

            stop  = vline.find_first_of( ':' );
            vline = vline.substr( stop + 1 );
            stop  = vline.find_first_of( ':' );
            n     = vline.substr( 0, stop );
            std::from_chars( n.data(), n.data() + n.size(), i );
            vline = vline.substr( stop + 1 );
            left  = (tk::spline<>::bd_type)i;
            stop  = vline.find_first_of( ':' );
            n     = vline.substr( 0, stop );
            std::from_chars( n.data(), n.data() + n.size(), leftValue );
            vline = vline.substr( stop + 1 );
            stop  = vline.find_first_of( ':' );
            n     = vline.substr( 0, stop );
            std::from_chars( n.data(), n.data() + n.size(), i );
            vline = vline.substr( stop + 1 );
            right = (tk::spline<>::bd_type)i;
            stop  = vline.find_first_of( ':' );
            n     = vline.substr( 0, stop );
            std::from_chars( n.data(), n.data() + n.size(), rightValue );
            vline     = vline.substr( stop + 1 );
            monotonic = ( vline[0] == '0' );
            vline     = vline.substr( 2 );
            stop      = vline.find_first_of( '}' );
            n         = vline.substr( 0, stop );
            std::from_chars( n.data(), n.data() + n.size(), i );
            type = (tk::spline<>::spline_type)i;

            curve.spline = tk::spline<>::spline( cx, cy, type, i == 1, left, leftValue, right, rightValue );
        }
        else if( sscanf( line, "hybrid=%d:%f", &i, &f ) == 2 )
        {
            Node* link = nodeEditor->FindNodeFromId( i );

            nodeData->hybrids.emplace_back( link ? link->data.get() : nullptr, f );
        }
        else if( sscanf( line, "id=%d", &i ) == 1 )
        {
            // Check the data is valid (node class may have changed)
            if( nodeData->variables.size() == nodeData->metadata->memberVariables.size() &&
                nodeData->nodeLookups.size() == nodeData->metadata->memberNodeLookups.size() &&
                nodeData->hybrids.size() == nodeData->metadata->memberHybrids.size() &&
                nodeData->images.size() == nodeData->metadata->memberImages.size() &&
                nodeData->curves.size() == nodeData->metadata->memberCurves.size() )
            {
                if( !nodeEditor->FindNodeFromId( i ) &&
                    nodeEditor->mNodes.try_emplace( nodeData, *nodeEditor, nodeData, true, i ).second )
                {
                    return;
                }
            }

            delete nodeData;
        }
    };


    ImGuiSettingsHandler textureSettings;
    textureSettings.TypeName   = "NoiseToolTextureMap";
    textureSettings.TypeHash   = ImHashStr( textureSettings.TypeName );
    textureSettings.UserData   = this;
    textureSettings.WriteAllFn = []( ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* outBuf ) {
        auto* nodeEditor = (FastNoiseNodeEditor*)handler->UserData;
        outBuf->appendf( "\n[%s][Settings]\n", handler->TypeName );

        FastNoise::ImageData::forEach(
            [&]( auto& e ) { outBuf->appendf( "image=%d!%s\n", e.second, e.first.sourceName.c_str() ); } );
    };
    textureSettings.ReadOpenFn = []( ImGuiContext* ctx, ImGuiSettingsHandler* handler, const char* name ) -> void* {
        if( strcmp( name, "Settings" ) == 0 )
        {
            return handler->UserData;
        }

        return nullptr;
    };
    textureSettings.ReadLineFn = []( ImGuiContext* ctx, ImGuiSettingsHandler* handler, void* entry, const char* line ) {
        std::string_view l( line );
        if( l.starts_with( "image=" ) )
        {
            l      = l.substr( sizeof( "image=" ) - 1 );
            auto n = l.find_first_of( '!' );
            if( l.npos != n )
            {
                int  idx    = -1;
                auto number = l.substr( 0, n );
                auto path   = l.substr( n + 1 );
                std::from_chars( number.data(), number.data() + number.size(), idx );
                if( idx >= 0 )
                {
                    FastNoise::ImageData::emplaceAt( std::move( path ), idx );
                }
            }
        }
    };
    textureSettings.ApplyAllFn = []( ImGuiContext* ctx, ImGuiSettingsHandler* handler ) {
        FastNoise::ImageData::fixIndexes();
    };

    ImGuiSettingsHandler editorSettings;
    editorSettings.TypeName   = "NoiseToolNodeGraph";
    editorSettings.TypeHash   = ImHashStr( editorSettings.TypeName );
    editorSettings.UserData   = this;
    editorSettings.WriteAllFn = []( ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* outBuf ) {
        auto* nodeEditor = (FastNoiseNodeEditor*)handler->UserData;
        outBuf->appendf( "\n[%s][Settings]\n", handler->TypeName );

        ImVec2 gridOffset = ImNodes::EditorContextGetPanning();
        outBuf->appendf( "grid_offset=%f:%f\n", gridOffset.x, gridOffset.y );
        outBuf->appendf( "image_path=%s\n", nodeEditor->mLastImportImagePath.c_str() );
        outBuf->appendf( "texure_preview=%c\n", (char)nodeEditor->mEnableTexPreview );
        outBuf->appendf( "selected_image=%d\n", nodeEditor->mSelectedImage.index );
    };
    editorSettings.ReadOpenFn = []( ImGuiContext* ctx, ImGuiSettingsHandler* handler, const char* name ) -> void* {
        if( strcmp( name, "Settings" ) == 0 )
        {
            return handler->UserData;
        }

        return nullptr;
    };
    editorSettings.ReadLineFn = []( ImGuiContext* ctx, ImGuiSettingsHandler* handler, void* entry, const char* line ) {
        auto* nodeEditor = (FastNoiseNodeEditor*)handler->UserData;

        ImVec2 imVec2;
        if( sscanf( line, "grid_offset=%f:%f", &imVec2.x, &imVec2.y ) == 2 )
        {
            ImNodes::EditorContextResetPanning( imVec2 );
        }

        sscanf( line, "texure_preview=%c", (char*)&nodeEditor->mEnableTexPreview );
        sscanf( line, "selected_image=%d\n", &nodeEditor->mSelectedImage.index );
        std::string_view l( line );
        if( l.starts_with( "image_path=" ) )
        {
            nodeEditor->mLastImportImagePath = l.substr( sizeof( "image_path" ) );
        }
    };

    ImGuiSettingsHandler histroySettings;
    histroySettings.TypeName   = "NoiseToolHistory";
    histroySettings.TypeHash   = ImHashStr( histroySettings.TypeName );
    histroySettings.UserData   = this;
    histroySettings.WriteAllFn = []( ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* outBuf ) {
        auto* nodeEditor = (FastNoiseNodeEditor*)handler->UserData;
        outBuf->appendf( "\n[%s][Settings]\n", handler->TypeName );
        for( auto const& e: nodeEditor->mHistory )
        {
            if( e.second.empty() )
                continue;
            outBuf->appendf( "-%s=%s\n", e.first.c_str(), e.second.c_str() );
        }
    };
    histroySettings.ReadOpenFn = []( ImGuiContext* ctx, ImGuiSettingsHandler* handler, const char* name ) -> void* {
        if( strcmp( name, "Settings" ) == 0 )
        {
            return handler->UserData;
        }

        return nullptr;
    };
    histroySettings.ReadLineFn = []( ImGuiContext* ctx, ImGuiSettingsHandler* handler, void* entry, const char* line ) {
        auto*                               nodeEditor = (FastNoiseNodeEditor*)handler->UserData;
        std::pair<std::string, std::string> nameVal;
        if( line[0] == '-' )
        {
            line++;
            int i = 0;
            while( line[i] && line[i] != '=' )
                i++;
            nameVal.first.append( line, i );
            if( !line[i] )
                return;
            line += ( i + 1 );
            i = 0;
            while( line[i] && line[i] != '\n' && line[i] != '\r' )
                i++;
            nameVal.second.append( line, i++ );
        }
        nodeEditor->mHistory.emplace_back( std ::move( nameVal ) );
    };

    ImGuiExtra::AddOrReplaceSettingsHandler( editorSettings );
    ImGuiExtra::AddOrReplaceSettingsHandler( textureSettings );
    ImGuiExtra::AddOrReplaceSettingsHandler( nodeSettings );
    ImGuiExtra::AddOrReplaceSettingsHandler( histroySettings );
}

FastNoiseNodeEditor::FastNoiseNodeEditor() :
    mOverheadNode( *this, new FastNoise::NodeData( &FastNoise::Metadata::Get<FastNoise::Constant>() ), false )
{
#ifdef IMGUI_HAS_DOCK
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
#endif
    ImGui::GetIO().ConfigWindowsResizeFromEdges = true;

    ImNodes::CreateContext();
    ImNodes::GetIO().AltMouseButton = ImGuiMouseButton_Right;

    ImGui::StyleColorsDark();
    ImNodes::StyleColorsDark();

    ImNodes::GetStyle().MiniMapPadding = ImVec2( 8, 8 );

#ifndef NDEBUG
    mNodeBenchmarkMax = 0;
#endif

    SetupSettingsHandlers();

    // Create Metadata context menu tree
    std::unordered_map<std::string, std::pair<MetadataMenuGroup*, int>> groupMap;
    auto*                                                               root = new MetadataMenuGroup( "" );
    mContextMetadata.emplace_back( root );

    auto menuSort = []( const MetadataMenu* a, const MetadataMenu* b ) {
        return std::strcmp( a->GetName(), b->GetName() ) < 0;
    };


    auto const& metadataArray = FastNoise::Metadata::GetAll();
    mNodeStyles.resize( metadataArray.size() );
    for( const FastNoise::Metadata* metadata: metadataArray )
    {
        auto* metaDataGroup = root;

        std::string groupTree;
        int         lastStyle = 0;
        for( const char* group: metadata->groups )
        {
            groupTree += group;
            auto find = groupMap.find( groupTree );
            if( find == groupMap.end() )
            {
                auto* newGroup = new MetadataMenuGroup( group );
                mContextMetadata.emplace_back( newGroup );
                metaDataGroup->items.emplace_back( newGroup );
                find = groupMap.emplace( groupTree, std::make_pair( newGroup, (int)groupMap.size() ) ).first;

                std::sort( metaDataGroup->items.begin(), metaDataGroup->items.end(), menuSort );
            }

            metaDataGroup = find->second.first;
            groupTree += '\t';
            lastStyle = find->second.second;
        }

        mNodeStyles[metadata->id] = (unsigned)lastStyle % gStyles.size();
        metaDataGroup->items.emplace_back( mContextMetadata.emplace_back( new MetadataMenuItem( metadata ) ).get() );
        std::sort( metaDataGroup->items.begin(), metaDataGroup->items.end(), menuSort );
    }
}

void FastNoiseNodeEditor::DrawEditor( bool locked )
{
    auto mainWndFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;
    if( locked )
        mainWndFlags |= ImGuiWindowFlags_NoResize;
    if( ImGui::Begin( "Node Editor", nullptr, mainWndFlags ) )
    {
        auto newSize = ImGui::GetWindowSize();


        ImNodes::BeginNodeEditor();

        DoHelp();
        ImGui::SameLine();

        Magnum::ToggleButton( ICON_FA_EYE, mEnableTexPreview, ImVec2( 40, 40 ), "Show texture preview", 2 );

        DoContextMenu();

        DoNodes();

        ImNodes::MiniMap( 0.2f, ImNodesMiniMapLocation_BottomLeft );

        ImNodes::EndNodeEditor();

        CheckLinks();
    }
    ImGui::End();
}

void Magnum::FastNoiseNodeEditor::DrawTexture()
{
    mNoiseTexture.Draw( *this );
}

void FastNoiseNodeEditor::Draw( const Matrix4& transformation, const Matrix4& projection,
                                const Vector3& cameraPosition )
{
    //  const ImGuiViewport* viewport = ImGui::GetMainViewport();
    //  ImGui::DockSpaceOverViewport( viewport, ImGuiDockNodeFlags_PassthruCentralNode );

    mMeshNoisePreview.Draw( *this, transformation, projection, cameraPosition );

    DoImages();
    DoHistory();
}

void FastNoiseNodeEditor::BeginDraw()
{
}

void FastNoiseNodeEditor::EndDraw()
{
}

void FastNoiseNodeEditor::CheckLinks()
{
    // Check for new links
    int  startNodeId, endNodeId;
    int  startAttr, endAttr;
    bool createdFromSnap;
    if( ImNodes::IsLinkCreated( &startNodeId, &startAttr, &endNodeId, &endAttr, &createdFromSnap ) )
    {
        Node* startNode = FindNodeFromId( startNodeId );
        Node* endNode   = FindNodeFromId( endNodeId );

        if( startNode && endNode )
        {
            auto& link = endNode->GetNodeLink( endAttr );

            if( !createdFromSnap || !link )
            {
                link = startNode->data.get();
                endNode->GeneratePreview( true );
            }
        }
    }

    int attrStart;
    if( ImNodes::IsLinkDropped( &attrStart, false ) )
    {
        Node* node = FindNodeFromId( Node::GetNodeIdFromAttribute( attrStart ) );

        if( node && ( attrStart & Node::AttributeBitMask ) == Node::AttributeBitMask )
        {
            mDroppedLinkNode = node->data.get();
            mDroppedLink     = true;
        }
    }
}

void FastNoiseNodeEditor::DeleteNode( FastNoise::NodeData* nodeData )
{
    mNodes.erase( nodeData );

    for( auto& node: mNodes )
    {
        bool changed = false;
        int  attrId  = node.second.GetStartingAttributeId();

        for( FastNoise::NodeData* link: node.second.GetNodeIDLinks() )
        {
            if( link == nodeData )
            {
                node.second.GetNodeLink( attrId ) = nullptr;
                changed                           = true;
            }
            attrId++;
        }

        if( changed )
        {
            node.second.GeneratePreview( true );
        }
    }
}

void FastNoiseNodeEditor::UpdateSelected()
{
    std::vector<int> linksToDelete;
    int              selectedLinkCount = ImNodes::NumSelectedLinks();

    bool delKeyPressed = ImGui::IsKeyPressed( ImGui::GetKeyIndex( ImGuiKey_Delete ), false ) ||
        ImGui::IsKeyPressed( ImGui::GetKeyIndex( ImGuiKey_Backspace ), false );

    if( selectedLinkCount && delKeyPressed )
    {
        linksToDelete.resize( selectedLinkCount );
        ImNodes::GetSelectedLinks( linksToDelete.data() );
    }

    int destroyedLinkId;
    if( ImNodes::IsLinkDestroyed( &destroyedLinkId ) )
    {
        linksToDelete.push_back( destroyedLinkId );
    }

    for( int deleteID: linksToDelete )
    {
        for( auto& node: mNodes )
        {
            bool changed     = false;
            int  attributeId = node.second.GetStartingAttributeId();

            for( FastNoise::NodeData* link: node.second.GetNodeIDLinks() )
            {
                (void)link;
                if( attributeId == deleteID )
                {
                    node.second.GetNodeLink( attributeId ) = nullptr;
                    changed                                = true;
                }
                attributeId++;
            }

            if( changed )
            {
                node.second.GeneratePreview( true );
            }
        }
    }

    int selectedNodeCount = ImNodes::NumSelectedNodes();

    if( selectedNodeCount && ImGui::IsKeyPressed( ImGui::GetKeyIndex( ImGuiKey_Delete ), false ) )
    {
        std::vector<int> selected( selectedNodeCount );

        ImNodes::GetSelectedNodes( selected.data() );

        for( int deleteID: selected )
        {
            if( Node* node = FindNodeFromId( deleteID ) )
            {
                DeleteNode( node->data.get() );
            }
        }
    }
}

void FastNoiseNodeEditor::AddHistoryRecord()
{
    auto find = mNodes.find( mSelectedNode );
    if( find != mNodes.end() )
    {
        auto name = Settings::get().name();
        if( name.empty() )
            name = "?";
        mHistory.emplace_back( std::move( name ), find->second.serialised );
    }
}

void FastNoiseNodeEditor::DoHistory()
{
    // if( ImGui::Begin( "History" ) )
    {

        ImGui::Text( "Saved Node Groups" );
        ImGui::Separator();
        if( ImGui::BeginTable( "HistoryTable", 2, ImGuiTableFlags_SizingFixedFit ) )
        {
            ImGui::TableSetupColumn( "", 0, 320 );
            ImGui::TableSetupColumn( "", 0, 80 );

            int id = 0;
            for( auto& e: mHistory )
            {

                if( e.second.empty() )
                    continue;
                ImGui::TableNextColumn();
                ImGui::Text( e.first.c_str() );
                ImGui::TableNextColumn();
                ImGui::PushID( id++ );
                if( ImGui::Button( ICON_FA_COPY ) )
                {
                    ImGui::SetClipboardText( e.second.c_str() );
                }
                ImGui::PopID();
                ImGui::SameLine();
                ImGui::PushID( id++ );
                if( ImGui::Button( ICON_FA_TRASH_CAN ) )
                {
                    e.first  = "";
                    e.second = "";
                    ImGuiExtra::MarkSettingsDirty();
                }
                ImGui::PopID();
                ImGui::TableNextRow();
            }
            ImGui::EndTable();
        }
    }
    // ImGui::End();
}


void FastNoiseNodeEditor::DoImages()
{
    // if( ImGui::Begin( "Images", 0, ImGuiWindowFlags_AlwaysUseWindowPadding ) )
    {
        ImGui::Text( "Saved Image Paths" );
        ImGui::Separator();
        if( ImGui::BeginTable( "ImageTable", 2, ImGuiTableFlags_SizingFixedFit ) )
        {
            ImGui::TableSetupColumn( "", 0, 300 );
            ImGui::TableSetupColumn( "", 0, 100 );

            FastNoise::ImageData::forEach( [&]( auto& e ) {
                if( e.first.sourceName.empty() )
                    return;
                ImGui::TableNextColumn();
                bool selected = e.second == mSelectedImage.index;
                if( ImGui::Selectable( e.first.sourceName.c_str(), &selected,
                                       ImGuiSelectableFlags_SpanAvailWidth | ImGuiSelectableFlags_SelectOnClick,
                                       ImVec2( 0, 20 ) ) )
                {
                    if( selected )
                        mSelectedImage.index = e.second;
                    else if( mSelectedImage.index == e.second )
                        mSelectedImage.index = -1;
                }
                if( selected )
                    mSelectedImage.index = e.second;
                ImGui::TableNextColumn();
                ImGui::PushID( e.second );
                bool loaded = e.first.isLoaded();
                if( ToggleButton( ICON_FA_DATABASE, loaded, ImVec2( 20, 20 ),
                                  loaded ? "Click to unload this image." : "Click to load this image from file." ) )
                {
                    if( loaded )
                        e.first.ensure( false );
                    else
                        e.first.unload();
                }
                ImGui::SameLine();
                if( Button( ICON_FA_ROTATE_RIGHT, "Reload this image", ImVec2( 20, 20 ) ) )
                {
                    e.first.ensure( true );
                }
                ImGui::SameLine();
                if( Button( ICON_FA_TRASH_CAN, "Delete this image", ImVec2( 20, 20 ) ) )
                {
                    if( mSelectedImage.index == e.second )
                        mSelectedImage.index = -1;
                    FastNoise::ImageData::remove( e.second );
                    ImGuiExtra::MarkSettingsDirty();
                }
                ImGui::PopID();
                ImGui::TableNextRow();
            } );
            ImGui::EndTable();
        }


        if( Magnum::Button( ICON_FA_FILE_IMPORT, "Browse for images/masks to add" ) )
            ImGuiFileDialog::Instance()->OpenDialog( "ImageFileDlgKey", "Images", ".png,.hdr,.bmp,.tga,.jpeg,.jpg",
                                                     mLastImportImagePath );
        ImGui::SameLine();
        if( Magnum::Button( ICON_FA_ARROW_UP_FROM_BRACKET, "Click to unload all images." ) )
        {
            FastNoise::ImageData::forEach( [&]( auto& e ) { e.first.unload(); } );
        }

        if( ImGuiFileDialog::Instance()->Display( "ImageFileDlgKey", 32, ImVec2 { 600, 400 } ) )
        {
            if( ImGuiFileDialog::Instance()->IsOk() )
            {
                mLastImportImagePath = ImGuiFileDialog::Instance()->GetCurrentPath();
                mSelectedImage.index =
                    FastNoise::ImageData::add( Magnum::ImportImage( ImGuiFileDialog::Instance()->GetFilePathName() ) );
            }

            ImGuiFileDialog::Instance()->Close();
        }
    }
    // ImGui::End();
}

void FastNoiseNodeEditor::SetSIMDLevel( FastSIMD::eLevel lvl )
{
    mMaxSIMDLevel = lvl;

    mOverheadNode.generateAverages.clear();
    // DoNodeBenchmarks();

    for( auto& node: mNodes )
    {
        node.second.generateAverages.clear();
        node.second.GeneratePreview( false );
    }

    ChangeSelectedNode( mSelectedNode );
}

void FastNoiseNodeEditor::DoNodes()
{

    for( auto& node: mNodes )
    {
        bool        edited = Settings::get().versionCheck_edit( mVersion );
        auto const& style  = gStyles[mNodeStyles[node.first->metadata->id]];

        ImNodes::PushColorStyle( ImNodesCol_TitleBar, ImGui::GetColorU32( style.title ) );
        ImNodes::PushColorStyle( ImNodesCol_TitleBarHovered, ImGui::GetColorU32( style.titleHovered ) );
        // ImNodes::PushColorStyle( ImNodesCol_TitleBarSelected, ImGui::GetColorU32( style.titleSelected ) );
        ImGui::PushStyleColor( ImGuiCol_Text, style.textColor );

        ImNodes::BeginNode( node.second.nodeId );
        ImNodes::BeginNodeTitleBar();

        bool toggled = mSelectedNode == node.first;

        ImGui::PushStyleColor( ImGuiCol_ButtonHovered, style.buttonHover );
        ImGui::PushStyleColor( ImGuiCol_ButtonActive, style.textColor );

        if( Magnum::ToggleButton( toggled ? ICON_FA_LOCATION_DOT : ICON_FA_LOCATION_CROSSHAIRS, toggled,
                                  ImVec2( 20, 20 ), "Preview this node" ) &&
            toggled )
            ChangeSelectedNode( node.first );


        ImGui::SameLine();

        std::string formatName = FastNoise::Metadata::FormatMetadataNodeName( node.second.data->metadata );
        ImGui::TextUnformatted( formatName.c_str() );

        ImGui::PopStyleColor();
        ImGui::PopStyleColor();

        if( ImGui::IsItemHovered() )
        {
            ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 4.f, 4.f ) );
            ImGui::BeginTooltip();
            ImGui::Text( "Total: %.3gus", node.second.totalGenerateNs / 1e+3f );
            ImGui::EndTooltip();
            ImGui::PopStyleVar();
        }

        ImNodes::EndNodeTitleBar();


        // Right click node title to change node type
        ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 4, 4 ) );
        if( ImGui::BeginPopupContextItem() )
        {
            if( ImGui::MenuItem( ICON_FA_COPY " Node Tree" ) )
            {
                AddHistoryRecord();
                ImGui::SetClipboardText( node.second.serialised.c_str() );
                Debug {} << node.second.serialised.c_str();
            }

            ImGui::Separator();
            ImGui::MenuItem( "Convert To:", nullptr, nullptr, false );

            auto& nodeMetadata = node.second.data->metadata;
            auto  newMetadata  = mContextMetadata.front()->DrawUI(
                [nodeMetadata]( const FastNoise::Metadata* metadata ) {
                    return metadata != nodeMetadata && MatchingGroup( metadata->groups, nodeMetadata->groups );
                },
                false );

            if( newMetadata )
            {
                if( MatchingMembers( newMetadata->memberVariables, nodeMetadata->memberVariables ) &&
                    MatchingMembers( newMetadata->memberNodeLookups, nodeMetadata->memberNodeLookups ) &&
                    MatchingMembers( newMetadata->memberHybrids, nodeMetadata->memberHybrids ) )
                {
                    nodeMetadata = newMetadata;
                }
                else
                {
                    FastNoise::NodeData newData( newMetadata );

                    std::queue<FastNoise::NodeData*> links;

                    for( FastNoise::NodeData* link: node.second.data->nodeLookups )
                    {
                        links.emplace( link );
                    }
                    for( auto& link: node.second.data->hybrids )
                    {
                        links.emplace( link.first );
                    }

                    for( auto& link: newData.nodeLookups )
                    {
                        if( links.empty() )
                            break;
                        link = links.front();
                        links.pop();
                    }
                    for( auto& link: newData.hybrids )
                    {
                        if( links.empty() )
                            break;
                        link.first = links.front();
                        links.pop();
                    }

                    *node.second.data = std::move( newData );
                }

                edited = true;
            }

            ImGui::EndPopup();
        }
        ImGui::PopStyleVar();

        ImGui::PushItemWidth( 60.0f );

        ImNodes::PushAttributeFlag( ImNodesAttributeFlags_EnableLinkCreationOnSnap );
        ImNodes::PushAttributeFlag( ImNodesAttributeFlags_EnableLinkDetachWithDragClick );
        int   attributeId  = node.second.GetStartingAttributeId();
        auto& nodeMetadata = node.second.data->metadata;
        auto& nodeData     = node.second.data;

        for( auto& memberNode: nodeMetadata->memberNodeLookups )
        {
            ImNodes::BeginInputAttribute( attributeId++ );
            formatName = FastNoise::Metadata::FormatMetadataMemberName( memberNode );
            ImGui::TextUnformatted( formatName.c_str() );
            ImNodes::EndInputAttribute();
        }

        for( size_t i = 0; i < node.second.data->metadata->memberHybrids.size(); i++ )
        {
            ImNodes::BeginInputAttribute( attributeId++ );

            bool        isLinked    = node.second.data->hybrids[i].first;
            const char* floatFormat = "%.3f";

            if( isLinked )
            {
                ImGui::PushItemFlag( ImGuiItemFlags_Disabled, true );
                floatFormat = "";
            }

            formatName = FastNoise::Metadata::FormatMetadataMemberName( nodeMetadata->memberHybrids[i] );

            if( ImGui::DragFloat( formatName.c_str(), &nodeData->hybrids[i].second, 0.02f, 0, 0, floatFormat ) )
            {
                edited = true;
            }

            if( isLinked )
            {
                ImGui::PopItemFlag();
            }
            ImNodes::EndInputAttribute();
        }

        bool lastSameLine = false;

        for( size_t i = 0; i < nodeMetadata->memberVariables.size(); i++ )
        {
            ImNodes::BeginStaticAttribute( 0 );

            auto& nodeVar = nodeMetadata->memberVariables[i];

            formatName = FastNoise::Metadata::FormatMetadataMemberName( nodeVar );

            if( lastSameLine )
                ImGui::SameLine();

            switch( nodeVar.type )
            {
            case FastNoise::Metadata::MemberVariable::EFloat:
            {
                if( ImGui::DragFloat( formatName.c_str(), &nodeData->variables[i].f, 0.02f, nodeVar.valueMin.f,
                                      nodeVar.valueMax.f ) )
                {
                    edited = true;
                }
            }
            break;
            case FastNoise::Metadata::MemberVariable::EBool:
            {
                if( ImGui::Checkbox( formatName.c_str(), &nodeData->variables[i].b ) )
                {
                    edited = true;
                }
            }
            break;
            case FastNoise::Metadata::MemberVariable::EInt:
            {
                if( ImGui::DragInt( formatName.c_str(), &nodeData->variables[i].i, 0.2f, nodeVar.valueMin.i,
                                    nodeVar.valueMax.i ) )
                {
                    edited = true;
                }
            }
            break;
            case FastNoise::Metadata::MemberVariable::EEnum:
            {
                if( ImGui::Combo( formatName.c_str(), &nodeData->variables[i].i, nodeVar.enumNames.data(),
                                  (int)nodeVar.enumNames.size() ) ||
                    ImGuiExtra::ScrollCombo( &nodeData->variables[i].i, (int)nodeVar.enumNames.size() ) )
                {
                    edited = true;
                }
            }
            break;
            }

            ImNodes::EndStaticAttribute();
            lastSameLine = nodeVar.sameLine;
        }

        for( size_t i = 0; i < nodeMetadata->memberImages.size(); ++i )
        {
            ImNodes::BeginStaticAttribute( 0 );

            auto& nodeVar = nodeMetadata->memberImages[i];
            if( ImGui::Button( ICON_FA_IMAGE ) )
            {
                nodeData->images[i].index = mSelectedImage.index;
                edited                    = true;
            }
            auto const& image = nodeData->images[i].toImage();
            ImGui::SameLine();
            if( !image.sourceName.empty() )
                ImGui::Text( image.sourceName.c_str() );
            else
                ImGui::Text( ICON_FA_ARROW_LEFT " use selected image" );
            ImNodes::EndStaticAttribute();
        }

        for( size_t i = 0; i < nodeMetadata->memberCurves.size(); ++i )
        {
            auto const& nodeVar = nodeMetadata->memberCurves[i];
            auto&       curve   = nodeData->curves[i];
            if( ImGui::DrawCurveEditor( nodeVar.name, curve ) )
            {
                edited = true;
            }
        }

        ImGui::PopItemWidth();
        ImNodes::PopAttributeFlag();
        ImNodes::BeginOutputAttribute( node.second.GetOutputAttributeId(), ImNodesPinShape_QuadFilled );

        Vector2 noiseSize = { (float)Node::NoiseSize, (float)Node::NoiseSize };

        if( !node.second.hasTexture || edited )
            node.second.GeneratePreview( true );

        if( mEnableTexPreview )
        {
            ImGuiIntegration::image( node.second.noiseTexture, noiseSize );
        }

        ImNodes::EndOutputAttribute();

        ImNodes::EndNode();


        ImGui::PopStyleColor();
        ImNodes::PopColorStyle();
        // ImNodes::PopColorStyle();
        ImNodes::PopColorStyle();
    }

    // Do current node links
    for( auto& node: mNodes )
    {
        int attributeId = node.second.GetStartingAttributeId();

        for( FastNoise::NodeData* link: node.second.GetNodeIDLinks() )
        {
            if( link )
            {
                ImNodes::Link( attributeId, mNodes.at( link ).GetOutputAttributeId(), attributeId );
            }
            attributeId++;
        }
    }
}

FastNoiseNodeEditor::Node& FastNoiseNodeEditor::AddNode( ImVec2 startPos, const FastNoise::Metadata* metadata,
                                                         bool generatePreview )
{
    FastNoise::NodeData* nodeData = new FastNoise::NodeData( metadata );

    auto newNode = mNodes.try_emplace( nodeData, *this, nodeData, generatePreview );

    ImNodes::SetNodeScreenSpacePos( newNode.first->second.nodeId, startPos );

    if( mNodes.size() <= 1 )
    {
        ChangeSelectedNode( nodeData );
    }

    return newNode.first->second;
}

bool FastNoiseNodeEditor::AddNodeFromEncodedString( const char* string, ImVec2 nodePos )
{
    std::vector<std::unique_ptr<FastNoise::NodeData>> nodeData;

    if( FastNoise::NodeData* firstNodeData = FastNoise::Metadata::DeserialiseNodeData( string, nodeData ) )
    {
        for( auto& data: nodeData )
        {
            FastNoise::NodeData* newNodeData = data.get();
            mNodes.emplace( std::piecewise_construct, std::forward_as_tuple( newNodeData ),
                            std::forward_as_tuple( *this, std::move( data ) ) );
        }

        if( mNodes.size() == nodeData.size() )
        {
            ChangeSelectedNode( firstNodeData );
        }

        Node& firstNode = mNodes.at( firstNodeData );

        ImNodes::SetNodeScreenSpacePos( firstNode.nodeId, nodePos );
        firstNode.AutoPositionChildNodes( nodePos );
        return true;
    }

    return false;
}

void FastNoiseNodeEditor::DoHelp()
{
    ImGui::Text( " Help" );
    if( ImGui::IsItemHovered() )
    {
        ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 4.f, 4.f ) );
        ImGui::BeginTooltip();
        constexpr float alignPx = 110;

        ImGui::Text( "Add nodes" );
        ImGui::SameLine( alignPx );
        ImGui::Text( "Right mouse click" );

        ImGui::Text( "Pan graph" );
        ImGui::SameLine( alignPx );
        ImGui::Text( "Right mouse drag" );

        ImGui::Text( "Delete node/link" );
        ImGui::SameLine( alignPx );
        ImGui::Text( "Backspace or Delete" );

        ImGui::Text( "Node options" );
        ImGui::SameLine( alignPx );
        ImGui::Text( "Right click node title" );

        ImGui::EndTooltip();
        ImGui::PopStyleVar();
    }
}

void FastNoiseNodeEditor::DoContextMenu()
{
    std::string className;
    ImVec2      drag            = ImGui::GetMouseDragDelta( ImGuiMouseButton_Right );
    float       distance        = sqrtf( ImDot( drag, drag ) );
    bool        openImportModal = false;

    ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 4, 4 ) );
    if( distance < 5.0f && ImGui::BeginPopupContextWindow( "new_node", 1 ) )
    {
        mContextStartPos = ImGui::GetMousePosOnOpeningCurrentPopup();

        if( auto newMetadata = mContextMetadata.front()->DrawUI() )
        {
            AddNode( mContextStartPos, newMetadata );
        }

        if( ImGui::MenuItem( ICON_FA_FILE_IMPORT " Import Tree" ) )
        {
            openImportModal = true;
        }

        if( ImGui::MenuItem( ICON_FA_PASTE " Paste Tree" ) )
        {
            AddNodeFromEncodedString( ImGui::GetClipboardText(), mContextStartPos );
        }
        ImGui::EndPopup();
    }

    if( openImportModal )
    {
        mImportNodeModal = true;
        mImportNodeString.clear();
        ImGui::OpenPopup( "New From Encoded Node Tree" );
    }

    ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, { 5, 5 } );
    ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, { 5, 5 } );
    ImGui::SetNextWindowSize( { 400, 92 }, ImGuiCond_Always );
    ImGui::SetNextWindowPos( ImGui::GetIO().DisplaySize / 2, ImGuiCond_Always, { 0.5f, 0.5f } );

    if( ImGui::BeginPopupModal( "New From Encoded Node Tree", &mImportNodeModal,
                                ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings ) )
    {
        if( openImportModal )
        {
            ImGui::SetKeyboardFocusHere();
        }

        bool txtEnter = ImGui::InputText( "Base64 String", &mImportNodeString,
                                          ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll |
                                              ImGuiInputTextFlags_CharsNoBlank );

        if( txtEnter | ImGui::Button( "Create", { 100, 30 } ) )
        {
            if( AddNodeFromEncodedString( mImportNodeString.c_str(), mContextStartPos ) )
            {
                mImportNodeModal = false;
            }
            else
            {
                mImportNodeString = "DESERIALISATION FAILED";
            }
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar( 2 );

    if( mDroppedLink )
    {
        ImGui::OpenPopup( "new_node_drop" );
        mDroppedLink = false;
    }
    if( ImGui::BeginPopup( "new_node_drop",
                           ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar |
                               ImGuiWindowFlags_NoSavedSettings ) )
    {
        ImVec2 startPos = ImGui::GetMousePosOnOpeningCurrentPopup();

        auto newMetadata = mContextMetadata.front()->DrawUI( []( const FastNoise::Metadata* metadata ) {
            return !metadata->memberNodeLookups.empty() || !metadata->memberHybrids.empty();
        } );

        if( newMetadata )
        {
            auto& newNode = AddNode( startPos, newMetadata );

            newNode.GetNodeLink( 0 ) = mDroppedLinkNode;
            newNode.GeneratePreview( true );
        }

        ImGui::EndPopup();
    }

    ImGui::PopStyleVar();
}

FastNoise::SmartNode<> FastNoiseNodeEditor::GenerateSelectedPreview()
{
    auto find = mNodes.find( mSelectedNode );

    FastNoise::SmartNode<> generator;

    if( find != mNodes.end() )
    {
        generator = FastNoise::NewFromEncodedNodeTree( find->second.serialised.c_str(), mMaxSIMDLevel );

        if( generator )
        {
            mActualSIMDLevel = generator->GetSIMDLevel();
        }
    }

    return generator;
}

FastNoise::OutputMinMax FastNoiseNodeEditor::GenerateNodePreviewNoise( FastNoise::Generator* gen,
                                                                       FastNoise::Buffer&    noise )
{

    FastNoise::GeneratorInput context( noise );
    context.frequency   = Settings::get().frequency();
    context.seed        = Settings::get().seed();
    context.size[0]     = Node::NoiseSize;
    context.size[1]     = Node::NoiseSize;
    context.gridSize[0] = Node::NoiseSize - 1;
    context.gridSize[1] = Node::NoiseSize - 1;
    context.start[0]    = Node::NoiseSize / -2;
    context.start[1]    = Node::NoiseSize / -2;
    gen->GenUniformGrid2D( context );

    return context.minMax;
}

FastNoiseNodeEditor::Node* FastNoiseNodeEditor::FindNodeFromId( int id )
{
    auto find =
        std::find_if( mNodes.begin(), mNodes.end(), [id]( const auto& node ) { return node.second.nodeId == id; } );

    if( find != mNodes.end() )
    {
        return &find->second;
    }

    return nullptr;
}

int FastNoiseNodeEditor::GetFreeNodeId()
{
    static int newNodeId = 0;

    do
    {
        newNodeId = std::max( 1, ( newNodeId + 1 ) & ( INT_MAX >> Node::AttributeBitCount ) );

    } while( FindNodeFromId( newNodeId ) );

    return newNodeId;
}

void FastNoiseNodeEditor::ChangeSelectedNode( FastNoise::NodeData* newId )
{
    mSelectedNode = newId;

    FastNoise::SmartNode<> generator = GenerateSelectedPreview();

    if( generator )
    {
        Settings::get().generator( generator );
    }
}

const char* FastNoiseNodeEditor::GetSIMDLevelName( FastSIMD::eLevel lvl )
{
    switch( lvl )
    {
    default:
    case FastSIMD::Level_Null:
        return "NULL";
    case FastSIMD::Level_Scalar:
        return "Scalar";
    case FastSIMD::Level_SSE:
        return "SSE";
    case FastSIMD::Level_SSE2:
        return "SSE2";
    case FastSIMD::Level_SSE3:
        return "SSE3";
    case FastSIMD::Level_SSSE3:
        return "SSSE3";
    case FastSIMD::Level_SSE41:
        return "SSE4.1";
    case FastSIMD::Level_SSE42:
        return "SSE4.2";
    case FastSIMD::Level_AVX:
        return "AVX";
    case FastSIMD::Level_AVX2:
        return "AVX2";
    case FastSIMD::Level_AVX512:
        return "AVX512";
    case FastSIMD::Level_NEON:
        return "NEON";
    }
}
