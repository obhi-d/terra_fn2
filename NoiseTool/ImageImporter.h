
#pragma once
#include <functional>
#include <string_view>

#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/StridedArrayView.h>

#include <Magnum/Trade/ImageData.h>

#include <Magnum/PixelFormat.h>
#include <Magnum/Trade/AbstractImageConverter.h>
#include <Magnum/Trade/AbstractImporter.h>


#include "FastNoise/FastNoise.h"
#include "FastNoise/ImageData.h"

namespace Magnum
{

    inline FastNoise::ImageData ImportImage( std::string_view file )
    {
        PluginManager::Manager<Trade::AbstractImporter> manager;
        Containers::Pointer<Trade::AbstractImporter>    importer =
            manager.loadAndInstantiate( file.ends_with( ".dds" ) ? "DdsImporter" : "StbImageImporter" );
        if( !importer || !importer->openFile( std::string { file } ) )
        {
            return FastNoise::ImageData {};
        }

        auto image2D = importer->image2D( 0 );
        if( image2D && !image2D->isCompressed() )
        {
            FastNoise::ImageData image;
            image.width      = image2D->size().x();
            image.height     = image2D->size().y();
            image.pixelWidth = image2D->pixelSize();
            image.sourceName = file;

            switch( image2D->format() )
            {
            case PixelFormat::RGB8Unorm:
                image.format = FastNoise::ImageData::Format::ERGB;
                break;
            case PixelFormat::RGBA8Unorm:
                image.format = FastNoise::ImageData::Format::ERGBA;
                break;
            case PixelFormat::R32F:
                image.format = FastNoise::ImageData::Format::EFloat;
                break;
            case PixelFormat::R8UI:
            case PixelFormat::R8Unorm:
                image.format = FastNoise::ImageData::Format::EByte;
                break;
            case PixelFormat::R32I:
            case PixelFormat::R32UI:
                image.format = FastNoise::ImageData::Format::EUInt;
                break;
            case PixelFormat::R16Snorm:
            case PixelFormat::R16I:
                image.format = FastNoise::ImageData::Format::EUInt16;
                break;
            default:
                return FastNoise::ImageData {};
            }
            image( std::nullptr_t {} );
            std::memcpy( image.data.get(), image2D->data(), image.size() );
            std::string_view rep( (const char*)image.data.get(), image.size() );
            return image;
        }

        return FastNoise::ImageData {};
    }

} // namespace Magnum