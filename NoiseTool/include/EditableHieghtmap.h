#pragma once
#include <chrono>
#include <cstring>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <Magnum/GL/AbstractShaderProgram.h>
#include <Magnum/GL/Buffer.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/Shader.h>
#include <Magnum/GL/Texture.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Vector3.h>

#include <robin_hood.h>

#include "FastNoise/FastNoise.h"
#include "MultiThreadQueues.h"
#include "Settings.h"

namespace Magnum
{
    class FastNoiseNodeEditor;
    class EditableHeightmap
    {
    public:
        EditableHeightmap();

        void Draw( FastNoiseNodeEditor& editor, const Matrix4& transformation, const Matrix4& projection,
                   const Vector3& cameraPosition );


        FastNoise::OutputMinMax GetMinMax() const
        {
            return minMax;
        }

    private:
        void RegenreateGrid();
        void UpdateHeights();


        class Shader : public GL::AbstractShaderProgram
        {
        public:
            using PositionXY = GL::Attribute<0, Vector2>;
            using Height     = GL::Attribute<1, float>;

            explicit Shader();
            explicit Shader( NoCreateT ) noexcept : AbstractShaderProgram { NoCreate }
            {
            }

            Shader( const Shader& )                = delete;
            Shader( Shader&& ) noexcept            = default;
            Shader& operator=( const Shader& )     = delete;
            Shader& operator=( Shader&& ) noexcept = default;

            Shader& SetTransformationProjectionMatrix( const Matrix4& matrix );
            Shader& SetSunIntensity( Color3 color, float intensity );
            Shader& SetSunDirection( Vector3 sunDir );
            Shader& SetHeightColorMap( ColorLayerValue const& colorMap );
            Shader& SetHeightMultiplier( float iHeight );
            Shader& SetRenderStyle( int spec );


        private:
            GL::Shader CreateShader( GL::Version version, GL::Shader::Type type );

            GL::Texture1D mHeightColors[3];

            int mTransformationProjectionMatrixUniform = 0;
            int mHeightmapMultiplier                   = 1;
            int mHeightColorMapUniform[3]              = { 2, 3, 4 };
            int mSunColor                              = 5;
            int mSunDirection                          = 6;
            int mCompressSpec                          = 7;
        };


        FastNoise::OutputMinMax minMax;
        // Bound settings

        GL::Buffer xyBuffer;
        GL::Buffer heightBuffer;
        GL::Buffer indexBuffer;
        // buffer, offset, x,y
        FastNoise::Buffer heights;

        std::unique_ptr<GL::Mesh> mesh;
        Shader                    shader;
        Settings::version         version;
    };
} // namespace Magnum