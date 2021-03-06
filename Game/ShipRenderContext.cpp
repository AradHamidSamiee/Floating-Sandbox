/***************************************************************************************
* Original Author:      Gabriele Giuseppini
* Created:              2018-03-22
* Copyright:            Gabriele Giuseppini  (https://github.com/GabrieleGiuseppini)
***************************************************************************************/
#include "ShipRenderContext.h"

#include "GameParameters.h"

#include <GameCore/GameException.h>
#include <GameCore/GameMath.h>
#include <GameCore/GameWallClock.h>
#include <GameCore/Log.h>

#include <algorithm>
#include <cstring>

namespace Render {

ShipRenderContext::ShipRenderContext(
    ShipId shipId,
    size_t pointCount,
    size_t shipCount,
    RgbaImageData shipTexture,
    ShaderManager<ShaderManagerTraits> & shaderManager,
    GlobalRenderContext const & globalRenderContext,
    RenderParameters const & renderParameters,
    float shipFlameSizeAdjustment)
    : mShipId(shipId)
    , mPointCount(pointCount)
    , mShipCount(shipCount)
    , mMaxMaxPlaneId(0)
    , mIsViewModelDirty(false)
    // Buffers
    , mPointAttributeGroup1Buffer()
    , mPointAttributeGroup1VBO()
    , mPointAttributeGroup2Buffer()
    , mPointAttributeGroup2VBO()
    , mPointColorVBO()
    , mPointTemperatureVBO()
    //
    , mStressedSpringElementBuffer()
    , mStressedSpringElementVBO()
    , mStressedSpringElementVBOAllocatedElementSize(0u)
    //
    , mFlameVertexBuffer()    
    , mFlameBackgroundCount(0u)
    , mFlameForegroundCount(0u)
    , mFlameVBO()
    , mFlameVBOAllocatedVertexSize(0u)
    , mFlameWindSpeedMagnitudeRunningAverage(0.0f)
    , mFlameWindSpeedMagnitudeAverage(0.0f)
    , mIsFlameWindSpeedMagnitudeAverageDirty(true)
    //
    , mExplosionPlaneVertexBuffers()
    , mExplosionTotalVertexCount(0u)
    , mExplosionVBO()
    , mExplosionVBOAllocatedVertexSize(0u)
    //
    , mSparkleVertexBuffer()
    , mSparkleVBO()
    , mSparkleVBOAllocatedVertexSize(0u)
    //
    , mGenericMipMappedTextureAirBubbleVertexBuffer()
    , mGenericMipMappedTexturePlaneVertexBuffers()
    , mGenericMipMappedTextureTotalVertexCount(0u)
    , mGenericMipMappedTextureVBO()
    , mGenericMipMappedTextureVBOAllocatedVertexSize(0u)
    //
    , mHighlightVertexBuffers()
    , mHighlightVBO()
    , mHighlightVBOAllocatedVertexSize(0u)
    //
    , mVectorArrowVertexBuffer()
    , mVectorArrowVBO()
    , mVectorArrowVBOAllocatedVertexSize(0u)
    , mVectorArrowColor(0.0f, 0.0f, 0.0f, 1.0f)
    , mIsVectorArrowColorDirty(true)
    // Element (index) buffers
    , mPointElementBuffer()
    , mEphemeralPointElementBuffer()
    , mSpringElementBuffer()
    , mRopeElementBuffer()
    , mTriangleElementBuffer()
    , mAreElementBuffersDirty(true)
    , mElementVBO()
    , mElementVBOAllocatedIndexSize(0u)
    , mPointElementVBOStartIndex(0)
    , mEphemeralPointElementVBOStartIndex(0)
    , mSpringElementVBOStartIndex(0)
    , mRopeElementVBOStartIndex(0)
    , mTriangleElementVBOStartIndex(0)
    // VAOs
    , mShipVAO()
    , mFlameVAO()
    , mExplosionVAO()
    , mSparkleVAO()
    , mGenericMipMappedTextureVAO()
    , mVectorArrowVAO()
    // Textures
    , mShipTextureOpenGLHandle()
    , mStressedSpringTextureOpenGLHandle()
    , mExplosionTextureAtlasMetadata(globalRenderContext.GetExplosionTextureAtlasMetadata())
    , mGenericLinearTextureAtlasMetadata(globalRenderContext.GetGenericLinearTextureAtlasMetadata())
    , mGenericMipMappedTextureAtlasMetadata(globalRenderContext.GetGenericMipMappedTextureAtlasMetadata())
    // Managers
    , mShaderManager(shaderManager)
    // Non-render parameters
    , mHalfFlameQuadWidth(0.0f) // Will be calculated
    , mFlameQuadHeight(0.0f) // Will be calculated
{
    GLuint tmpGLuint;

    // Clear errors
    glGetError();


    //
    // Initialize buffers
    //

    GLuint vbos[11];
    glGenBuffers(11, vbos);
    CheckOpenGLError();

    mPointAttributeGroup1VBO = vbos[0];
    glBindBuffer(GL_ARRAY_BUFFER, *mPointAttributeGroup1VBO);
    glBufferData(GL_ARRAY_BUFFER, pointCount * sizeof(vec4f), nullptr, GL_STREAM_DRAW);
    mPointAttributeGroup1Buffer.reset(new vec4f[pointCount]);
    std::memset(mPointAttributeGroup1Buffer.get(), 0, pointCount * sizeof(vec4f));

    mPointAttributeGroup2VBO = vbos[1];
    glBindBuffer(GL_ARRAY_BUFFER, *mPointAttributeGroup2VBO);
    glBufferData(GL_ARRAY_BUFFER, pointCount * sizeof(vec4f), nullptr, GL_STREAM_DRAW);
    mPointAttributeGroup2Buffer.reset(new vec4f[pointCount]);
    std::memset(mPointAttributeGroup2Buffer.get(), 0, pointCount * sizeof(vec4f));

    mPointColorVBO = vbos[2];
    glBindBuffer(GL_ARRAY_BUFFER, *mPointColorVBO);
    glBufferData(GL_ARRAY_BUFFER, pointCount * sizeof(vec4f), nullptr, GL_STATIC_DRAW);

    mPointTemperatureVBO = vbos[3];
    glBindBuffer(GL_ARRAY_BUFFER, *mPointTemperatureVBO);
    glBufferData(GL_ARRAY_BUFFER, pointCount * sizeof(float), nullptr, GL_STREAM_DRAW);

    mStressedSpringElementVBO = vbos[4];
    mStressedSpringElementBuffer.reserve(1024); // Arbitrary

    mFlameVBO = vbos[5];

    mExplosionVBO = vbos[6];

    mSparkleVBO = vbos[7];
    mSparkleVertexBuffer.reserve(256); // Arbitrary

    mGenericMipMappedTextureVBO = vbos[8];

    mHighlightVBO = vbos[9];

    mVectorArrowVBO = vbos[10];

    glBindBuffer(GL_ARRAY_BUFFER, 0);


    //
    // Initialize element (index) buffers
    //

    glGenBuffers(1, &tmpGLuint);
    mElementVBO = tmpGLuint;

    mPointElementBuffer.reserve(pointCount);
    mEphemeralPointElementBuffer.reserve(GameParameters::MaxEphemeralParticles);
    mSpringElementBuffer.reserve(pointCount * GameParameters::MaxSpringsPerPoint);
    mRopeElementBuffer.reserve(pointCount); // Arbitrary
    mTriangleElementBuffer.reserve(pointCount * GameParameters::MaxTrianglesPerPoint);


    //
    // Initialize Ship VAO
    //

    {
        glGenVertexArrays(1, &tmpGLuint);
        mShipVAO = tmpGLuint;

        glBindVertexArray(*mShipVAO);
        CheckOpenGLError();

        //
        // Describe vertex attributes
        //

        glBindBuffer(GL_ARRAY_BUFFER, *mPointAttributeGroup1VBO);
        glEnableVertexAttribArray(static_cast<GLuint>(VertexAttributeType::ShipPointAttributeGroup1));
        glVertexAttribPointer(static_cast<GLuint>(VertexAttributeType::ShipPointAttributeGroup1), 4, GL_FLOAT, GL_FALSE, sizeof(vec4f), (void*)(0));
        CheckOpenGLError();

        glBindBuffer(GL_ARRAY_BUFFER, *mPointAttributeGroup2VBO);
        glEnableVertexAttribArray(static_cast<GLuint>(VertexAttributeType::ShipPointAttributeGroup2));
        glVertexAttribPointer(static_cast<GLuint>(VertexAttributeType::ShipPointAttributeGroup2), 4, GL_FLOAT, GL_FALSE, sizeof(vec4f), (void*)(0));
        CheckOpenGLError();

        glBindBuffer(GL_ARRAY_BUFFER, *mPointColorVBO);
        glEnableVertexAttribArray(static_cast<GLuint>(VertexAttributeType::ShipPointColor));
        glVertexAttribPointer(static_cast<GLuint>(VertexAttributeType::ShipPointColor), 4, GL_FLOAT, GL_FALSE, sizeof(vec4f), (void*)(0));
        CheckOpenGLError();

        glBindBuffer(GL_ARRAY_BUFFER, *mPointTemperatureVBO);
        glEnableVertexAttribArray(static_cast<GLuint>(VertexAttributeType::ShipPointTemperature));
        glVertexAttribPointer(static_cast<GLuint>(VertexAttributeType::ShipPointTemperature), 1, GL_FLOAT, GL_FALSE, sizeof(float), (void*)(0));
        CheckOpenGLError();

        glBindBuffer(GL_ARRAY_BUFFER, 0);

        //
        // Associate element VBO
        //

        // NOTE: Intel drivers have a bug in the VAO ARB: they do not store the ELEMENT_ARRAY_BUFFER binding
        // in the VAO. So we won't associate the element VBO here, but rather before the drawing call.
        ////glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *mPointElementVBO);
        ////CheckOpenGLError();

        glBindVertexArray(0);
    }


    //
    // Initialize Flame VAO's
    //

    {
        glGenVertexArrays(1, &tmpGLuint);
        mFlameVAO = tmpGLuint;

        glBindVertexArray(*mFlameVAO);

        // Describe vertex attributes
        glBindBuffer(GL_ARRAY_BUFFER, *mFlameVBO);
        static_assert(sizeof(FlameVertex) == (4 + 2) * sizeof(float));
        glEnableVertexAttribArray(static_cast<GLuint>(VertexAttributeType::Flame1));
        glVertexAttribPointer(static_cast<GLuint>(VertexAttributeType::Flame1), 4, GL_FLOAT, GL_FALSE, sizeof(FlameVertex), (void*)0);
        glEnableVertexAttribArray(static_cast<GLuint>(VertexAttributeType::Flame2));
        glVertexAttribPointer(static_cast<GLuint>(VertexAttributeType::Flame2), 2, GL_FLOAT, GL_FALSE, sizeof(FlameVertex), (void*)((4) * sizeof(float)));
        CheckOpenGLError();

        glBindVertexArray(0);
    }


    //
    // Initialize Explosion VAO
    //

    {
        glGenVertexArrays(1, &tmpGLuint);
        mExplosionVAO = tmpGLuint;

        glBindVertexArray(*mExplosionVAO);

        // Describe vertex attributes
        glBindBuffer(GL_ARRAY_BUFFER, *mExplosionVBO);
        static_assert(sizeof(ExplosionVertex) == (4 + 4 + 2) * sizeof(float));
        glEnableVertexAttribArray(static_cast<GLuint>(VertexAttributeType::Explosion1));
        glVertexAttribPointer(static_cast<GLuint>(VertexAttributeType::Explosion1), 4, GL_FLOAT, GL_FALSE, sizeof(ExplosionVertex), (void*)0);
        glEnableVertexAttribArray(static_cast<GLuint>(VertexAttributeType::Explosion2));
        glVertexAttribPointer(static_cast<GLuint>(VertexAttributeType::Explosion2), 4, GL_FLOAT, GL_FALSE, sizeof(ExplosionVertex), (void*)((4) * sizeof(float)));
        glEnableVertexAttribArray(static_cast<GLuint>(VertexAttributeType::Explosion3));
        glVertexAttribPointer(static_cast<GLuint>(VertexAttributeType::Explosion3), 2, GL_FLOAT, GL_FALSE, sizeof(ExplosionVertex), (void*)((4 + 4) * sizeof(float)));
        CheckOpenGLError();

        glBindVertexArray(0);
    }


    //
    // Initialize Sparkle VAO
    //

    {
        glGenVertexArrays(1, &tmpGLuint);
        mSparkleVAO = tmpGLuint;

        glBindVertexArray(*mSparkleVAO);

        // Describe vertex attributes
        glBindBuffer(GL_ARRAY_BUFFER, *mSparkleVBO);
        static_assert(sizeof(SparkleVertex) == (4 + 4) * sizeof(float));
        glEnableVertexAttribArray(static_cast<GLuint>(VertexAttributeType::Sparkle1));
        glVertexAttribPointer(static_cast<GLuint>(VertexAttributeType::Sparkle1), 4, GL_FLOAT, GL_FALSE, sizeof(SparkleVertex), (void *)0);
        glEnableVertexAttribArray(static_cast<GLuint>(VertexAttributeType::Sparkle2));
        glVertexAttribPointer(static_cast<GLuint>(VertexAttributeType::Sparkle2), 4, GL_FLOAT, GL_FALSE, sizeof(SparkleVertex), (void *)((4) * sizeof(float)));
        CheckOpenGLError();

        glBindVertexArray(0);
    }


    //
    // Initialize GenericMipMappedTexture VAO
    //

    {
        glGenVertexArrays(1, &tmpGLuint);
        mGenericMipMappedTextureVAO = tmpGLuint;

        glBindVertexArray(*mGenericMipMappedTextureVAO);

        // Describe vertex attributes
        glBindBuffer(GL_ARRAY_BUFFER, *mGenericMipMappedTextureVBO);
        static_assert(sizeof(GenericTextureVertex) == (4 + 4 + 3) * sizeof(float));
        glEnableVertexAttribArray(static_cast<GLuint>(VertexAttributeType::GenericMipMappedTexture1));
        glVertexAttribPointer(static_cast<GLuint>(VertexAttributeType::GenericMipMappedTexture1), 4, GL_FLOAT, GL_FALSE, sizeof(GenericTextureVertex), (void*)0);
        glEnableVertexAttribArray(static_cast<GLuint>(VertexAttributeType::GenericMipMappedTexture2));
        glVertexAttribPointer(static_cast<GLuint>(VertexAttributeType::GenericMipMappedTexture2), 4, GL_FLOAT, GL_FALSE, sizeof(GenericTextureVertex), (void*)((4) * sizeof(float)));
        glEnableVertexAttribArray(static_cast<GLuint>(VertexAttributeType::GenericMipMappedTexture3));
        glVertexAttribPointer(static_cast<GLuint>(VertexAttributeType::GenericMipMappedTexture3), 3, GL_FLOAT, GL_FALSE, sizeof(GenericTextureVertex), (void*)((4 + 4) * sizeof(float)));
        CheckOpenGLError();

        glBindVertexArray(0);
    }


    //
    // Initialize Highlight VAO
    //

    {
        glGenVertexArrays(1, &tmpGLuint);
        mHighlightVAO = tmpGLuint;

        glBindVertexArray(*mHighlightVAO);

        // Describe vertex attributes
        glBindBuffer(GL_ARRAY_BUFFER, *mHighlightVBO);
        static_assert(sizeof(HighlightVertex) == (2 + 2 + 3 + 1 + 1) * sizeof(float));
        glEnableVertexAttribArray(static_cast<GLuint>(VertexAttributeType::Highlight1));
        glVertexAttribPointer(static_cast<GLuint>(VertexAttributeType::Highlight1), 4, GL_FLOAT, GL_FALSE, sizeof(HighlightVertex), (void *)((0) * sizeof(float)));
        glEnableVertexAttribArray(static_cast<GLuint>(VertexAttributeType::Highlight2));
        glVertexAttribPointer(static_cast<GLuint>(VertexAttributeType::Highlight2), 4, GL_FLOAT, GL_FALSE, sizeof(HighlightVertex), (void *)((4) * sizeof(float)));
        glEnableVertexAttribArray(static_cast<GLuint>(VertexAttributeType::Highlight3));
        glVertexAttribPointer(static_cast<GLuint>(VertexAttributeType::Highlight3), 1, GL_FLOAT, GL_FALSE, sizeof(HighlightVertex), (void *)((4 + 4) * sizeof(float)));
        CheckOpenGLError();

        glBindVertexArray(0);
    }


    //
    // Initialize VectorArrow VAO
    //

    {
        glGenVertexArrays(1, &tmpGLuint);
        mVectorArrowVAO = tmpGLuint;

        glBindVertexArray(*mVectorArrowVAO);
        CheckOpenGLError();

        // Describe vertex attributes
        glBindBuffer(GL_ARRAY_BUFFER, *mVectorArrowVBO);
        glEnableVertexAttribArray(static_cast<GLuint>(VertexAttributeType::VectorArrow));
        glVertexAttribPointer(static_cast<GLuint>(VertexAttributeType::VectorArrow), 3, GL_FLOAT, GL_FALSE, sizeof(vec3f), (void*)(0));
        CheckOpenGLError();

        glBindVertexArray(0);
    }


    //
    // Initialize Ship texture
    //

    glGenTextures(1, &tmpGLuint);
    mShipTextureOpenGLHandle = tmpGLuint;

    // Bind texture
    mShaderManager.ActivateTexture<ProgramParameterType::SharedTexture>();
    glBindTexture(GL_TEXTURE_2D, *mShipTextureOpenGLHandle);
    CheckOpenGLError();

    // Upload texture
    GameOpenGL::UploadMipmappedTexture(std::move(shipTexture));

    // Set repeat mode
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    CheckOpenGLError();

    // Set filtering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    CheckOpenGLError();

    // Set texture parameter
    mShaderManager.ActivateProgram<ProgramType::ShipSpringsTexture>();
    mShaderManager.SetTextureParameters<ProgramType::ShipSpringsTexture>();
    mShaderManager.ActivateProgram<ProgramType::ShipSpringsTextureWithTemperature>();
    mShaderManager.SetTextureParameters<ProgramType::ShipSpringsTextureWithTemperature>();
    mShaderManager.ActivateProgram<ProgramType::ShipTrianglesTexture>();
    mShaderManager.SetTextureParameters<ProgramType::ShipTrianglesTexture>();
    mShaderManager.ActivateProgram<ProgramType::ShipTrianglesTextureWithTemperature>();
    mShaderManager.SetTextureParameters<ProgramType::ShipTrianglesTextureWithTemperature>();

    // Unbind texture
    glBindTexture(GL_TEXTURE_2D, 0);


    //
    // Initialize StressedSpring texture
    //

    glGenTextures(1, &tmpGLuint);
    mStressedSpringTextureOpenGLHandle = tmpGLuint;

    // Bind texture
    mShaderManager.ActivateTexture<ProgramParameterType::SharedTexture>();
    glBindTexture(GL_TEXTURE_2D, *mStressedSpringTextureOpenGLHandle);
    CheckOpenGLError();

    // Set repeat mode
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    CheckOpenGLError();

    // Set filtering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    CheckOpenGLError();

    // Make texture data
    unsigned char buf[] = {
        239, 16, 39, 255,       255, 253, 181,  255,    239, 16, 39, 255,
        255, 253, 181, 255,     239, 16, 39, 255,       255, 253, 181,  255,
        239, 16, 39, 255,       255, 253, 181,  255,    239, 16, 39, 255
    };

    // Upload texture data
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 3, 3, 0, GL_RGBA, GL_UNSIGNED_BYTE, buf);
    CheckOpenGLError();

    // Unbind texture
    glBindTexture(GL_TEXTURE_2D, 0);


    //
    // Set initial values of non-render parameters from which
    // other parameters are calculated
    //

    SetShipFlameSizeAdjustment(shipFlameSizeAdjustment);


    //
    // Update parameters for initial values
    //
    
    ApplyViewModelChanges(renderParameters);
    ApplyEffectiveAmbientLightIntensityChanges(renderParameters);
    ApplyFlatLampLightColorChanges(renderParameters);
    ApplyWaterColorChanges(renderParameters);
    ApplyWaterContrastChanges(renderParameters);
    ApplyWaterLevelOfDetailChanges(renderParameters);
    ApplyHeatOverlayTransparencyChanges(renderParameters);
}

ShipRenderContext::~ShipRenderContext()
{
}

//////////////////////////////////////////////////////////////////////////////////

void ShipRenderContext::UploadStart(PlaneId maxMaxPlaneId)
{
    //
    // Reset explosions, sparkles, air bubbles, generic textures, highlights,
    // vector arrows;
    // they are all uploaded as needed
    //

    {
        size_t const newSize = static_cast<size_t>(maxMaxPlaneId) + 1u;
        assert(mExplosionPlaneVertexBuffers.size() <= newSize);

        size_t const clearCount = mExplosionPlaneVertexBuffers.size();
        for (size_t i = 0; i < clearCount; ++i)
        {
            mExplosionPlaneVertexBuffers[i].vertexBuffer.clear();
        }

        if (newSize != mExplosionPlaneVertexBuffers.size())
            mExplosionPlaneVertexBuffers.resize(newSize);
    }


    mSparkleVertexBuffer.clear();

    {
        mGenericMipMappedTextureAirBubbleVertexBuffer.clear();

        size_t const newSize = static_cast<size_t>(maxMaxPlaneId) + 1u;
        assert(mGenericMipMappedTexturePlaneVertexBuffers.size() <= newSize);

        size_t const clearCount = mGenericMipMappedTexturePlaneVertexBuffers.size();
        for (size_t i = 0; i < clearCount; ++i)
        {
            mGenericMipMappedTexturePlaneVertexBuffers[i].vertexBuffer.clear();
        }

        if (newSize != mGenericMipMappedTexturePlaneVertexBuffers.size())
            mGenericMipMappedTexturePlaneVertexBuffers.resize(newSize);
    }

    for (size_t i = 0; i <= static_cast<size_t>(HighlightModeType::_Last); ++i)
    {
        mHighlightVertexBuffers[i].clear();
    }

    mVectorArrowVertexBuffer.clear();

    //
    // Check if the max max plane ID has changed
    //

    if (maxMaxPlaneId != mMaxMaxPlaneId)
    {
        // Update value
        mMaxMaxPlaneId = maxMaxPlaneId;
        mIsViewModelDirty = true;
    }
}

void ShipRenderContext::UploadPointImmutableAttributes(vec2f const * textureCoordinates)
{
    // Uploaded only once, but we treat them as if they could
    // be uploaded any time

    // Interleave texture coordinates into AttributeGroup1 buffer
    vec4f * restrict pDst = mPointAttributeGroup1Buffer.get();
    vec2f const * restrict pSrc = textureCoordinates;
    for (size_t i = 0; i < mPointCount; ++i)
    {
        pDst[i].z = pSrc[i].x;
        pDst[i].w = pSrc[i].y;
    }
}

void ShipRenderContext::UploadPointMutableAttributesStart()
{
    // Nop
}

void ShipRenderContext::UploadPointMutableAttributes(
    vec2f const * position,
    float const * light,
    float const * water,
    size_t lightAndWaterCount)
{
    // Uploaded at each cycle

    // Interleave positions into AttributeGroup1 buffer
    vec4f * restrict pDst1 = mPointAttributeGroup1Buffer.get();
    vec2f const * restrict pSrc = position;
    for (size_t i = 0; i < mPointCount; ++i)
    {
        pDst1[i].x = pSrc[i].x;
        pDst1[i].y = pSrc[i].y;
    }

    // Interleave light and water into AttributeGroup2 buffer
    vec4f * restrict pDst2 = mPointAttributeGroup2Buffer.get();
    float const * restrict pSrc1 = light;
    float const * restrict pSrc2 = water;
    for (size_t i = 0; i < lightAndWaterCount; ++i)
    {
        pDst2[i].x = pSrc1[i];
        pDst2[i].y = pSrc2[i];
    }
}

void ShipRenderContext::UploadPointMutableAttributesPlaneId(
    float const * planeId,
    size_t startDst,
    size_t count)
{
    // Uploaded sparingly, but we treat them as if they could
    // be uploaded at any time

    // Interleave plane ID into AttributeGroup2 buffer
    assert(startDst + count <= mPointCount);
    vec4f * restrict pDst = &(mPointAttributeGroup2Buffer.get()[startDst]);
    float const * restrict pSrc = planeId;
    for (size_t i = 0; i < count; ++i)
        pDst[i].z = pSrc[i];
}

void ShipRenderContext::UploadPointMutableAttributesDecay(
    float const * decay,
    size_t startDst,
    size_t count)
{
    // Uploaded sparingly, but we treat them as if they could
    // be uploaded at any time

    // Interleave decay into AttributeGroup2 buffer
    assert(startDst + count <= mPointCount);
    vec4f * restrict pDst = &(mPointAttributeGroup2Buffer.get()[startDst]);
    float const * restrict pSrc = decay;
    for (size_t i = 0; i < count; ++i)
        pDst[i].w = pSrc[i];
}

void ShipRenderContext::UploadPointMutableAttributesEnd()
{
    // Nop
}

void ShipRenderContext::UploadPointColors(
    vec4f const * color,
    size_t startDst,
    size_t count)
{
    // Uploaded sparingly

    // We've been invoked on the render thread

    //
    // Upload color range
    //

    assert(startDst + count <= mPointCount);

    glBindBuffer(GL_ARRAY_BUFFER, *mPointColorVBO);

    glBufferSubData(GL_ARRAY_BUFFER, startDst * sizeof(vec4f), count * sizeof(vec4f), color);
    CheckOpenGLError();

    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void ShipRenderContext::UploadPointTemperature(
    float const * temperature,
    size_t startDst,
    size_t count)
{
    // Uploaded sparingly

    // We've been invoked on the render thread

    //
    // Upload temperature range
    //

    assert(startDst + count <= mPointCount);

    glBindBuffer(GL_ARRAY_BUFFER, *mPointTemperatureVBO);

    glBufferSubData(GL_ARRAY_BUFFER, startDst * sizeof(float), count * sizeof(float), temperature);
    CheckOpenGLError();

    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void ShipRenderContext::UploadElementsStart()
{
    // Elements are uploaded sparingly

    // Empty all buffers - except triangles - as elements will be completely re-populated soon
    // (with a yet-unknown quantity of elements);
    //
    // If the client does not upload new triangles, it means we have to reuse the last known set

    mPointElementBuffer.clear();
    mSpringElementBuffer.clear();
    mRopeElementBuffer.clear();
    mAreElementBuffersDirty = true;
}

void ShipRenderContext::UploadElementTrianglesStart(size_t trianglesCount)
{
    // Client wants to upload a new set of triangles
    //
    // No need to clear, we'll repopulate everything

    mTriangleElementBuffer.resize(trianglesCount);
}

void ShipRenderContext::UploadElementTrianglesEnd()
{
    // Nop
}

void ShipRenderContext::UploadElementsEnd()
{
    // Nop
}

void ShipRenderContext::UploadElementStressedSpringsStart()
{
    //
    // Stressed springs are not sticky: we upload them at each frame,
    // though they will be empty most of the time
    //

    mStressedSpringElementBuffer.clear();
}

void ShipRenderContext::UploadElementStressedSpringsEnd()
{
    // Nop
}

void ShipRenderContext::UploadFlamesStart(
    size_t count,
    float windSpeedMagnitude)
{
    //
    // Flames are not sticky: we upload them at each frame,
    // though they will be empty most of the time
    //

    mFlameVertexBuffer.reset_fill(6 * count);

    mFlameBackgroundCount = 0;
    mFlameForegroundCount = 0;

    //
    // Update wind speed
    //

    float newWind = mFlameWindSpeedMagnitudeRunningAverage.Update(windSpeedMagnitude);

    // Pickup wind speed magnitude, if it has changed
    if (newWind != mFlameWindSpeedMagnitudeAverage)
    {
        mFlameWindSpeedMagnitudeAverage = newWind;
        mIsFlameWindSpeedMagnitudeAverageDirty = true;
    }
}

void ShipRenderContext::UploadFlamesEnd()
{
    assert((mFlameBackgroundCount + mFlameForegroundCount) * 6u == mFlameVertexBuffer.size());

    // Nop    
}

void ShipRenderContext::UploadElementEphemeralPointsStart()
{
    // Client wants to upload a new set of ephemeral point elements

    // Empty buffer
    mEphemeralPointElementBuffer.clear();
}

void ShipRenderContext::UploadElementEphemeralPointsEnd()
{
    // Nop
}

void ShipRenderContext::UploadVectors(
    size_t count,
    vec2f const * position,
    float const * planeId,
    vec2f const * vector,
    float lengthAdjustment,
    vec4f const & color)
{
    static float const CosAlphaLeftRight = cos(-2.f * Pi<float> / 8.f);
    static float const SinAlphaLeft = sin(-2.f * Pi<float> / 8.f);
    static float const SinAlphaRight = -SinAlphaLeft;

    static vec2f const XMatrixLeft = vec2f(CosAlphaLeftRight, SinAlphaLeft);
    static vec2f const YMatrixLeft = vec2f(-SinAlphaLeft, CosAlphaLeftRight);
    static vec2f const XMatrixRight = vec2f(CosAlphaLeftRight, SinAlphaRight);
    static vec2f const YMatrixRight = vec2f(-SinAlphaRight, CosAlphaLeftRight);

    //
    // Create buffer with endpoint positions of each segment of each arrow
    //

    mVectorArrowVertexBuffer.reserve(count * 3 * 2);

    for (size_t i = 0; i < count; ++i)
    {
        // Stem
        vec2f stemEndpoint = position[i] + vector[i] * lengthAdjustment;
        mVectorArrowVertexBuffer.emplace_back(position[i], planeId[i]);
        mVectorArrowVertexBuffer.emplace_back(stemEndpoint, planeId[i]);

        // Left
        vec2f leftDir = vec2f(-vector[i].dot(XMatrixLeft), -vector[i].dot(YMatrixLeft)).normalise();
        mVectorArrowVertexBuffer.emplace_back(stemEndpoint, planeId[i]);
        mVectorArrowVertexBuffer.emplace_back(stemEndpoint + leftDir * 0.2f, planeId[i]);

        // Right
        vec2f rightDir = vec2f(-vector[i].dot(XMatrixRight), -vector[i].dot(YMatrixRight)).normalise();
        mVectorArrowVertexBuffer.emplace_back(stemEndpoint, planeId[i]);
        mVectorArrowVertexBuffer.emplace_back(stemEndpoint + rightDir * 0.2f, planeId[i]);
    }

    if (color != mVectorArrowColor)
    {
        mVectorArrowColor = color;

        mIsVectorArrowColorDirty = true;
    }
}

void ShipRenderContext::UploadEnd()
{
    // Nop
}

void ShipRenderContext::ProcessParameterChanges(RenderParameters const & renderParameters)
{
    if (renderParameters.IsViewDirty || mIsViewModelDirty)
    {
        ApplyViewModelChanges(renderParameters);
        mIsViewModelDirty = false;
    }

    if (renderParameters.IsEffectiveAmbientLightIntensityDirty)
    {
        ApplyEffectiveAmbientLightIntensityChanges(renderParameters);
    }

    if (renderParameters.IsFlatLampLightColorDirty)
    {
        ApplyFlatLampLightColorChanges(renderParameters);
    }

    if (renderParameters.IsShipWaterColorDirty)
    {
        ApplyWaterColorChanges(renderParameters);
    }

    if (renderParameters.IsShipWaterContrastDirty)
    {
        ApplyWaterContrastChanges(renderParameters);
    }

    if (renderParameters.IsShipWaterLevelOfDetailDirty)
    {
        ApplyWaterLevelOfDetailChanges(renderParameters);
    }

    if (renderParameters.IsHeatOverlayTransparencyDirty)
    {
        ApplyHeatOverlayTransparencyChanges(renderParameters);
    }
}

void ShipRenderContext::RenderPrepare(RenderParameters const & renderParameters)
{
    // We've been invoked on the render thread

    //
    // Upload Point AttributeGroup1 buffer
    //

    glBindBuffer(GL_ARRAY_BUFFER, *mPointAttributeGroup1VBO);

    glBufferSubData(GL_ARRAY_BUFFER, 0, mPointCount * sizeof(vec4f), mPointAttributeGroup1Buffer.get());
    CheckOpenGLError();

    //
    // Upload Point AttributeGroup2 buffer
    //

    glBindBuffer(GL_ARRAY_BUFFER, *mPointAttributeGroup2VBO);

    glBufferSubData(GL_ARRAY_BUFFER, 0, mPointCount * sizeof(vec4f), mPointAttributeGroup2Buffer.get());
    CheckOpenGLError();

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    //
    // Upload element buffers, if needed
    //

    if (mAreElementBuffersDirty)
    {
        //
        // Upload all elements to the VBO, remembering the starting VBO index
        // of each element type which we'll need at primitives' render time
        //

        // Note: byte-granularity indices
        mTriangleElementVBOStartIndex = 0;
        mRopeElementVBOStartIndex = mTriangleElementVBOStartIndex + mTriangleElementBuffer.size() * sizeof(TriangleElement);
        mSpringElementVBOStartIndex = mRopeElementVBOStartIndex + mRopeElementBuffer.size() * sizeof(LineElement);
        mPointElementVBOStartIndex = mSpringElementVBOStartIndex + mSpringElementBuffer.size() * sizeof(LineElement);
        mEphemeralPointElementVBOStartIndex = mPointElementVBOStartIndex + mPointElementBuffer.size() * sizeof(PointElement);
        size_t requiredIndexSize = mEphemeralPointElementVBOStartIndex + mEphemeralPointElementBuffer.size() * sizeof(PointElement);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *mElementVBO);

        if (mElementVBOAllocatedIndexSize != requiredIndexSize)
        {
            // Re-allocate VBO buffer
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, requiredIndexSize, nullptr, GL_STATIC_DRAW);
            CheckOpenGLError();

            mElementVBOAllocatedIndexSize = requiredIndexSize;
        }

        // Upload triangles
        glBufferSubData(
            GL_ELEMENT_ARRAY_BUFFER,
            mTriangleElementVBOStartIndex,
            mTriangleElementBuffer.size() * sizeof(TriangleElement),
            mTriangleElementBuffer.data());

        // Upload ropes
        glBufferSubData(
            GL_ELEMENT_ARRAY_BUFFER,
            mRopeElementVBOStartIndex,
            mRopeElementBuffer.size() * sizeof(LineElement),
            mRopeElementBuffer.data());

        // Upload springs
        glBufferSubData(
            GL_ELEMENT_ARRAY_BUFFER,
            mSpringElementVBOStartIndex,
            mSpringElementBuffer.size() * sizeof(LineElement),
            mSpringElementBuffer.data());

        // Upload points
        glBufferSubData(
            GL_ELEMENT_ARRAY_BUFFER,
            mPointElementVBOStartIndex,
            mPointElementBuffer.size() * sizeof(PointElement),
            mPointElementBuffer.data());

        // Upload ephemeral points
        glBufferSubData(
            GL_ELEMENT_ARRAY_BUFFER,
            mEphemeralPointElementVBOStartIndex,
            mEphemeralPointElementBuffer.size() * sizeof(PointElement),
            mEphemeralPointElementBuffer.data());

        CheckOpenGLError();

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        mAreElementBuffersDirty = false;
    }

    //
    // Prepare flames
    //

    RenderPrepareFlames(renderParameters);

    //
    // Prepare stressed springs
    //

    if (renderParameters.ShowStressedSprings
        && !mStressedSpringElementBuffer.empty())
    {
        //
        // Upload buffer
        //

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *mStressedSpringElementVBO);

        if (mStressedSpringElementBuffer.size() > mStressedSpringElementVBOAllocatedElementSize)
        {
            // Re-allocate VBO buffer and upload
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, mStressedSpringElementBuffer.size() * sizeof(LineElement), mStressedSpringElementBuffer.data(), GL_STREAM_DRAW);
            CheckOpenGLError();

            mStressedSpringElementVBOAllocatedElementSize = mStressedSpringElementBuffer.size();
        }
        else
        {
            // No size change, just upload VBO buffer
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, mStressedSpringElementBuffer.size() * sizeof(LineElement), mStressedSpringElementBuffer.data());
            CheckOpenGLError();
        }

        // Bind again element VBO
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *mElementVBO);
    }

    //
    // Prepare sparkles
    //

    RenderPrepareSparkles(renderParameters);

    //
    // Prepare generic textures
    //

    RenderPrepareGenericMipMappedTextures(renderParameters);

    //
    // Prepare explosions
    //

    RenderPrepareExplosions(renderParameters);

    //
    // Prepare highlights
    //

    RenderPrepareHighlights(renderParameters);

    //
    // Prepare vectors
    //

    RenderPrepareVectorArrows(renderParameters);
}

void ShipRenderContext::RenderDraw(
    RenderParameters const & renderParameters,
    RenderStatistics & renderStats)
{
    // We've been invoked on the render thread

    //
    // Render background flames
    //

    if (renderParameters.ShipFlameRenderMode == ShipFlameRenderModeType::Mode1)
    {
        RenderDrawFlames<ProgramType::ShipFlamesBackground1>(
            0,
            mFlameBackgroundCount,
            renderParameters,
            renderStats);
    }
    else if (renderParameters.ShipFlameRenderMode == ShipFlameRenderModeType::Mode2)
    {
        RenderDrawFlames<ProgramType::ShipFlamesBackground2>(
            0,
            mFlameBackgroundCount,
            renderParameters,
            renderStats);
    }
    else if (renderParameters.ShipFlameRenderMode == ShipFlameRenderModeType::Mode3)
    {
        RenderDrawFlames<ProgramType::ShipFlamesBackground3>(
            0,
            mFlameBackgroundCount,
            renderParameters,
            renderStats);
    }

    //
    // Draw ship elements
    //

    glBindVertexArray(*mShipVAO);

    {
        //
        // Bind element VBO
        //
        // NOTE: Intel drivers have a bug in the VAO ARB: they do not store the ELEMENT_ARRAY_BUFFER binding
        // in the VAO
        //

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *mElementVBO);

        //
        // Bind ship texture
        //

        assert(!!mShipTextureOpenGLHandle);

        mShaderManager.ActivateTexture<ProgramParameterType::SharedTexture>();
        glBindTexture(GL_TEXTURE_2D, *mShipTextureOpenGLHandle);

        //
        // Draw triangles
        //
        // Best to draw triangles (temporally) before springs and ropes, otherwise
        // the latter, which use anti-aliasing, would end up being contoured with background
        // when drawn Z-ally over triangles
        //
        // Also, edge springs might just contain transparent pixels (when textured), which
        // would result in the same artifact
        //

        if (renderParameters.DebugShipRenderMode == DebugShipRenderModeType::Wireframe
            || renderParameters.DebugShipRenderMode == DebugShipRenderModeType::Decay
            || renderParameters.DebugShipRenderMode == DebugShipRenderModeType::Structure
            || renderParameters.DebugShipRenderMode == DebugShipRenderModeType::None)
        {
            if (renderParameters.DebugShipRenderMode == DebugShipRenderModeType::Decay)
            {
                // Use decay program
                mShaderManager.ActivateProgram<ProgramType::ShipTrianglesDecay>();
            }
            else
            {
                if (renderParameters.DebugShipRenderMode == DebugShipRenderModeType::None)
                {
                    // Use texture program
                    if (renderParameters.DrawHeatOverlay)
                        mShaderManager.ActivateProgram<ProgramType::ShipTrianglesTextureWithTemperature>();
                    else
                        mShaderManager.ActivateProgram<ProgramType::ShipTrianglesTexture>();
                }
                else
                {
                    // Use color program
                    if (renderParameters.DrawHeatOverlay)
                        mShaderManager.ActivateProgram<ProgramType::ShipTrianglesColorWithTemperature>();
                    else
                        mShaderManager.ActivateProgram<ProgramType::ShipTrianglesColor>();
                }
            }

            if (renderParameters.DebugShipRenderMode == DebugShipRenderModeType::Wireframe)
                glLineWidth(0.1f);

            // Draw!
            glDrawElements(
                GL_TRIANGLES,
                static_cast<GLsizei>(3 * mTriangleElementBuffer.size()),
                GL_UNSIGNED_INT,
                (GLvoid *)mTriangleElementVBOStartIndex);

            // Update stats
            renderStats.LastRenderedShipTriangles += mTriangleElementBuffer.size();
        }

        //
        // Set line width, for ropes and springs
        //

        glLineWidth(0.1f * 2.0f * renderParameters.View.GetCanvasToVisibleWorldHeightRatio());

        //
        // Draw ropes, unless it's a debug mode that doesn't want them
        //
        // Note: when DebugRenderMode is springs|edgeSprings, ropes would all be uploaded
        // as springs.
        //

        if (renderParameters.DebugShipRenderMode == DebugShipRenderModeType::Structure
            || renderParameters.DebugShipRenderMode == DebugShipRenderModeType::None)
        {
            if (renderParameters.DrawHeatOverlay)
                mShaderManager.ActivateProgram<ProgramType::ShipRopesWithTemperature>();
            else
                mShaderManager.ActivateProgram<ProgramType::ShipRopes>();

            glDrawElements(
                GL_LINES,
                static_cast<GLsizei>(2 * mRopeElementBuffer.size()),
                GL_UNSIGNED_INT,
                (GLvoid *)mRopeElementVBOStartIndex);

            // Update stats
            renderStats.LastRenderedShipRopes += mRopeElementBuffer.size();
        }

        //
        // Draw springs
        //
        // We draw springs when:
        // - DebugRenderMode is springs|edgeSprings, in which case we use colors - so to show
        //   structural springs -, or
        // - DebugRenderMode is structure, in which case we use colors - so to draw 1D chains -, or
        // - DebugRenderMode is none, in which case we use texture - so to draw 1D chains
        //
        // Note: when DebugRenderMode is springs|edgeSprings, ropes would all be here.
        //

        if (renderParameters.DebugShipRenderMode == DebugShipRenderModeType::Springs
            || renderParameters.DebugShipRenderMode == DebugShipRenderModeType::EdgeSprings
            || renderParameters.DebugShipRenderMode == DebugShipRenderModeType::Structure
            || renderParameters.DebugShipRenderMode == DebugShipRenderModeType::None)
        {
            if (renderParameters.DebugShipRenderMode == DebugShipRenderModeType::None)
            {
                // Use texture program
                if (renderParameters.DrawHeatOverlay)
                    mShaderManager.ActivateProgram<ProgramType::ShipSpringsTextureWithTemperature>();
                else
                    mShaderManager.ActivateProgram<ProgramType::ShipSpringsTexture>();
            }
            else
            {
                // Use color program
                if (renderParameters.DrawHeatOverlay)
                    mShaderManager.ActivateProgram<ProgramType::ShipSpringsColorWithTemperature>();
                else
                    mShaderManager.ActivateProgram<ProgramType::ShipSpringsColor>();
            }

            glDrawElements(
                GL_LINES,
                static_cast<GLsizei>(2 * mSpringElementBuffer.size()),
                GL_UNSIGNED_INT,
                (GLvoid *)mSpringElementVBOStartIndex);

            // Update stats
            renderStats.LastRenderedShipSprings += mSpringElementBuffer.size();
        }

        //
        // Draw stressed springs
        //

        if (renderParameters.ShowStressedSprings
            && !mStressedSpringElementBuffer.empty())
        {
            mShaderManager.ActivateProgram<ProgramType::ShipStressedSprings>();

            // Bind stressed spring element VBO
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *mStressedSpringElementVBO);

            // Bind stressed spring texture
            mShaderManager.ActivateTexture<ProgramParameterType::SharedTexture>();
            glBindTexture(GL_TEXTURE_2D, *mStressedSpringTextureOpenGLHandle);
            CheckOpenGLError();

            // Draw
            glDrawElements(
                GL_LINES,
                static_cast<GLsizei>(2 * mStressedSpringElementBuffer.size()),
                GL_UNSIGNED_INT,
                (GLvoid *)0);

            // Bind again ship element VBO
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *mElementVBO);
        }

        //
        // Draw points (orphaned/all non-ephemerals, and ephemerals)
        //

        if (renderParameters.DebugShipRenderMode == DebugShipRenderModeType::Points
            || renderParameters.DebugShipRenderMode == DebugShipRenderModeType::Structure
            || renderParameters.DebugShipRenderMode == DebugShipRenderModeType::None)
        {
            auto const totalPoints = mPointElementBuffer.size() + mEphemeralPointElementBuffer.size();

            if (renderParameters.DrawHeatOverlay)
                mShaderManager.ActivateProgram<ProgramType::ShipPointsColorWithTemperature>();
            else
                mShaderManager.ActivateProgram<ProgramType::ShipPointsColor>();

            glPointSize(0.3f * renderParameters.View.GetCanvasToVisibleWorldHeightRatio());

            glDrawElements(
                GL_POINTS,
                static_cast<GLsizei>(1 * totalPoints),
                GL_UNSIGNED_INT,
                (GLvoid *)mPointElementVBOStartIndex);

            // Update stats
            renderStats.LastRenderedShipPoints += totalPoints;
        }

        // We are done with the ship VAO
        glBindVertexArray(0);
    }

    //
    // Render foreground flames
    //

    if (renderParameters.ShipFlameRenderMode == ShipFlameRenderModeType::Mode1)
    {
        RenderDrawFlames<ProgramType::ShipFlamesForeground1>(
            mFlameBackgroundCount,
            mFlameForegroundCount,
            renderParameters,
            renderStats);
    }
    else if (renderParameters.ShipFlameRenderMode == ShipFlameRenderModeType::Mode2)
    {
        RenderDrawFlames<ProgramType::ShipFlamesForeground2>(
            mFlameBackgroundCount,
            mFlameForegroundCount,
            renderParameters,
            renderStats);
    }
    else if (renderParameters.ShipFlameRenderMode == ShipFlameRenderModeType::Mode3)
    {
        RenderDrawFlames<ProgramType::ShipFlamesForeground3>(
            mFlameBackgroundCount,
            mFlameForegroundCount,
            renderParameters,
            renderStats);
    }

    //
    // Render sparkles
    //

    RenderDrawSparkles(renderParameters);

    //
    // Render generic textures
    //

    RenderDrawGenericMipMappedTextures(renderParameters, renderStats);

    //
    // Render explosions
    //

    RenderDrawExplosions(renderParameters);

    //
    // Render highlights
    //

    RenderDrawHighlights(renderParameters);

    //
    // Render vectors
    //

    RenderDrawVectorArrows(renderParameters);

    //
    // Update stats
    //

    renderStats.LastRenderedShipPlanes += mMaxMaxPlaneId + 1;
}

/////////////////////////////////////////////////////////////////////////////////////////////

void ShipRenderContext::RenderPrepareFlames(RenderParameters const & renderParameters)
{
    //
    // Pickup wind speed magnitude, if it has changed
    //

    if (mIsFlameWindSpeedMagnitudeAverageDirty)
    {
        // Calculate wind angle: we do this here once instead of doing it for each and every pixel
        float const windRotationAngle = std::copysign(
            0.6f * SmoothStep(0.0f, 100.0f, std::abs(mFlameWindSpeedMagnitudeAverage)),
            -mFlameWindSpeedMagnitudeAverage);

        switch (renderParameters.ShipFlameRenderMode)
        {
            case ShipFlameRenderModeType::Mode1:
            {
                mShaderManager.ActivateProgram<ProgramType::ShipFlamesBackground1>();
                mShaderManager.SetProgramParameter<ProgramType::ShipFlamesBackground1, ProgramParameterType::FlameWindRotationAngle>(
                    windRotationAngle);

                mShaderManager.ActivateProgram<ProgramType::ShipFlamesForeground1>();
                mShaderManager.SetProgramParameter<ProgramType::ShipFlamesForeground1, ProgramParameterType::FlameWindRotationAngle>(
                    windRotationAngle);

                break;
            }

            case ShipFlameRenderModeType::Mode2:
            {
                mShaderManager.ActivateProgram<ProgramType::ShipFlamesBackground2>();
                mShaderManager.SetProgramParameter<ProgramType::ShipFlamesBackground2, ProgramParameterType::FlameWindRotationAngle>(
                    windRotationAngle);

                mShaderManager.ActivateProgram<ProgramType::ShipFlamesForeground2>();
                mShaderManager.SetProgramParameter<ProgramType::ShipFlamesForeground2, ProgramParameterType::FlameWindRotationAngle>(
                    windRotationAngle);

                break;
            }

            case ShipFlameRenderModeType::Mode3:
            {
                mShaderManager.ActivateProgram<ProgramType::ShipFlamesBackground3>();
                mShaderManager.SetProgramParameter<ProgramType::ShipFlamesBackground3, ProgramParameterType::FlameWindRotationAngle>(
                    windRotationAngle);

                mShaderManager.ActivateProgram<ProgramType::ShipFlamesForeground3>();
                mShaderManager.SetProgramParameter<ProgramType::ShipFlamesForeground3, ProgramParameterType::FlameWindRotationAngle>(
                    windRotationAngle);

                break;
            }

            case ShipFlameRenderModeType::NoDraw:
            {
                break;
            }
        }

        mIsFlameWindSpeedMagnitudeAverageDirty = true;
    }

    //
    // Upload buffers, if needed
    //
    
    if (!mFlameVertexBuffer.empty())
    {
        glBindBuffer(GL_ARRAY_BUFFER, *mFlameVBO);

        if (mFlameVertexBuffer.size() > mFlameVBOAllocatedVertexSize)
        {
            // Re-allocate VBO buffer and upload
            glBufferData(GL_ARRAY_BUFFER, mFlameVertexBuffer.size() * sizeof(FlameVertex), mFlameVertexBuffer.data(), GL_STREAM_DRAW);
            CheckOpenGLError();

            mFlameVBOAllocatedVertexSize = mFlameVertexBuffer.size();
        }
        else
        {
            // No size change, just upload VBO buffer
            glBufferSubData(GL_ARRAY_BUFFER, 0, mFlameVertexBuffer.size() * sizeof(FlameVertex), mFlameVertexBuffer.data());
            CheckOpenGLError();
        }

        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
}

template<ProgramType ShaderProgram>
void ShipRenderContext::RenderDrawFlames(
    size_t startFlameIndex,
    size_t flameCount,
    RenderParameters const & renderParameters,
    RenderStatistics & renderStats)
{
    assert(renderParameters.ShipFlameRenderMode != ShipFlameRenderModeType::NoDraw);

    if (flameCount > 0)
    {
        glBindVertexArray(*mFlameVAO);

        mShaderManager.ActivateProgram<ShaderProgram>();

        // Set flame speed parameter
        mShaderManager.SetProgramParameter<ShaderProgram, ProgramParameterType::FlameSpeed>(
            GameWallClock::GetInstance().NowAsFloat() * 0.345f);

        // Render
        if (renderParameters.ShipFlameRenderMode == ShipFlameRenderModeType::Mode1)
        {
            glDrawArrays(
                GL_TRIANGLES,
                static_cast<GLint>(startFlameIndex * 6u),
                static_cast<GLint>(flameCount * 6u));
        }
        else
        {
            glDrawArraysInstanced(
                GL_TRIANGLES,
                static_cast<GLint>(startFlameIndex * 6u),
                static_cast<GLint>(flameCount * 6u),
                2); // Without border, with border
        }

        glBindVertexArray(0);

        // Update stats
        renderStats.LastRenderedShipFlames += flameCount; // # of quads
    }
}

void ShipRenderContext::RenderPrepareSparkles(RenderParameters const & /*renderParameters*/)
{
    if (!mSparkleVertexBuffer.empty())
    {
        glBindBuffer(GL_ARRAY_BUFFER, *mSparkleVBO);

        if (mSparkleVertexBuffer.size() > mSparkleVBOAllocatedVertexSize)
        {
            // Re-allocate VBO buffer and upload
            glBufferData(GL_ARRAY_BUFFER, mSparkleVertexBuffer.size() * sizeof(SparkleVertex), mSparkleVertexBuffer.data(), GL_DYNAMIC_DRAW);
            CheckOpenGLError();

            mSparkleVBOAllocatedVertexSize = mSparkleVertexBuffer.size();
        }
        else
        {
            // No size change, just upload VBO buffer
            glBufferSubData(GL_ARRAY_BUFFER, 0, mSparkleVertexBuffer.size() * sizeof(SparkleVertex), mSparkleVertexBuffer.data());
            CheckOpenGLError();
        }

        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
}

void ShipRenderContext::RenderDrawSparkles(RenderParameters const & renderParameters)
{
    if (!mSparkleVertexBuffer.empty())
    {
        glBindVertexArray(*mSparkleVAO);

        mShaderManager.ActivateProgram<ProgramType::ShipSparkles>();

        if (renderParameters.DebugShipRenderMode == DebugShipRenderModeType::Wireframe)
            glLineWidth(0.1f);

        assert(0 == (mSparkleVertexBuffer.size() % 6));
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(mSparkleVertexBuffer.size()));

        glBindVertexArray(0);
    }
}

void ShipRenderContext::RenderPrepareGenericMipMappedTextures(RenderParameters const & /*renderParameters*/)
{
    size_t const nonAirBubblesTotalVertexCount = std::accumulate(
        mGenericMipMappedTexturePlaneVertexBuffers.cbegin(),
        mGenericMipMappedTexturePlaneVertexBuffers.cend(),
        size_t(0),
        [](size_t const total, auto const & vec)
        {
            return total + vec.vertexBuffer.size();
        });

    mGenericMipMappedTextureTotalVertexCount = mGenericMipMappedTextureAirBubbleVertexBuffer.size() + nonAirBubblesTotalVertexCount;

    if (mGenericMipMappedTextureTotalVertexCount > 0)
    {
        glBindBuffer(GL_ARRAY_BUFFER, *mGenericMipMappedTextureVBO);

        if (mGenericMipMappedTextureTotalVertexCount > mGenericMipMappedTextureVBOAllocatedVertexSize)
        {
            // Re-allocate VBO buffer
            glBufferData(GL_ARRAY_BUFFER, mGenericMipMappedTextureTotalVertexCount * sizeof(GenericTextureVertex), nullptr, GL_DYNAMIC_DRAW);
            CheckOpenGLError();

            mGenericMipMappedTextureVBOAllocatedVertexSize = mGenericMipMappedTextureTotalVertexCount;
        }

        // Map vertex buffer
        auto mappedBuffer = reinterpret_cast<uint8_t *>(glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY));
        CheckOpenGLError();

        // Upload air bubbles
        if (!mGenericMipMappedTextureAirBubbleVertexBuffer.empty())
        {
            size_t const byteCopySize = mGenericMipMappedTextureAirBubbleVertexBuffer.size() * sizeof(GenericTextureVertex);
            std::memcpy(mappedBuffer, mGenericMipMappedTextureAirBubbleVertexBuffer.data(), byteCopySize);
            mappedBuffer += byteCopySize;
        }

        // Upload all planes of other textures
        for (auto const & plane : mGenericMipMappedTexturePlaneVertexBuffers)
        {
            if (!plane.vertexBuffer.empty())
            {
                size_t const byteCopySize = plane.vertexBuffer.size() * sizeof(GenericTextureVertex);
                std::memcpy(mappedBuffer, plane.vertexBuffer.data(), byteCopySize);
                mappedBuffer += byteCopySize;
            }
        }

        // Unmap vertex buffer
        glUnmapBuffer(GL_ARRAY_BUFFER);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
}

void ShipRenderContext::RenderDrawGenericMipMappedTextures(
    RenderParameters const & renderParameters,
    RenderStatistics & renderStats)
{
    if (mGenericMipMappedTextureTotalVertexCount > 0) // Calculated at prepare() time
    {
        glBindVertexArray(*mGenericMipMappedTextureVAO);

        mShaderManager.ActivateProgram<ProgramType::ShipGenericMipMappedTextures>();

        if (renderParameters.DebugShipRenderMode == DebugShipRenderModeType::Wireframe)
            glLineWidth(0.1f);

        assert(0 == (mGenericMipMappedTextureTotalVertexCount % 6));
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(mGenericMipMappedTextureTotalVertexCount));

        glBindVertexArray(0);

        // Update stats
        renderStats.LastRenderedShipGenericMipMappedTextures += mGenericMipMappedTextureTotalVertexCount / 6; // # of quads
    }
}

void ShipRenderContext::RenderPrepareExplosions(RenderParameters const & /*renderParameters*/)
{
    mExplosionTotalVertexCount = std::accumulate(
        mExplosionPlaneVertexBuffers.cbegin(),
        mExplosionPlaneVertexBuffers.cend(),
        size_t(0),
        [](size_t const total, auto const & vec)
        {
            return total + vec.vertexBuffer.size();
        });

    if (mExplosionTotalVertexCount > 0)
    {
        glBindBuffer(GL_ARRAY_BUFFER, *mExplosionVBO);

        if (mExplosionTotalVertexCount > mExplosionVBOAllocatedVertexSize)
        {
            // Re-allocate VBO buffer
            glBufferData(GL_ARRAY_BUFFER, mExplosionTotalVertexCount * sizeof(ExplosionVertex), nullptr, GL_STREAM_DRAW);
            CheckOpenGLError();

            mExplosionVBOAllocatedVertexSize = mExplosionTotalVertexCount;
        }

        // Map vertex buffer
        auto mappedBuffer = reinterpret_cast<uint8_t *>(glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY));
        CheckOpenGLError();

        // Upload all planes
        for (auto const & plane : mExplosionPlaneVertexBuffers)
        {
            if (!plane.vertexBuffer.empty())
            {
                size_t const byteCopySize = plane.vertexBuffer.size() * sizeof(ExplosionVertex);
                std::memcpy(mappedBuffer, plane.vertexBuffer.data(), byteCopySize);
                mappedBuffer += byteCopySize;
            }
        }

        // Unmap vertex buffer
        glUnmapBuffer(GL_ARRAY_BUFFER);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
}

void ShipRenderContext::RenderDrawExplosions(RenderParameters const & renderParameters)
{
    if (mExplosionTotalVertexCount > 0) // Calculated at prepare() time
    {
        glBindVertexArray(*mExplosionVAO);

        mShaderManager.ActivateProgram<ProgramType::ShipExplosions>();

        if (renderParameters.DebugShipRenderMode == DebugShipRenderModeType::Wireframe)
            glLineWidth(0.1f);

        assert(0 == (mExplosionTotalVertexCount % 6));
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(mExplosionTotalVertexCount));

        glBindVertexArray(0);
    }
}

void ShipRenderContext::RenderPrepareHighlights(RenderParameters const & /*renderParameters*/)
{
    for (size_t i = 0; i <= static_cast<size_t>(HighlightModeType::_Last); ++i)
    {
        if (!mHighlightVertexBuffers[i].empty())
        {
            glBindBuffer(GL_ARRAY_BUFFER, *mHighlightVBO);

            if (mHighlightVertexBuffers[i].size() > mHighlightVBOAllocatedVertexSize)
            {
                // Re-allocate VBO buffer and upload
                glBufferData(GL_ARRAY_BUFFER, mHighlightVertexBuffers[i].size() * sizeof(HighlightVertex), mHighlightVertexBuffers[i].data(), GL_DYNAMIC_DRAW);
                CheckOpenGLError();

                mHighlightVBOAllocatedVertexSize = mVectorArrowVertexBuffer.size();
            }
            else
            {
                // No size change, just upload VBO buffer
                glBufferSubData(GL_ARRAY_BUFFER, 0, mHighlightVertexBuffers[i].size() * sizeof(HighlightVertex), mHighlightVertexBuffers[i].data());
                CheckOpenGLError();
            }

            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }
    }
}

void ShipRenderContext::RenderDrawHighlights(RenderParameters const & renderParameters)
{
    for (size_t i = 0; i <= static_cast<size_t>(HighlightModeType::_Last); ++i)
    {
        if (!mHighlightVertexBuffers[i].empty())
        {
            glBindVertexArray(*mHighlightVAO);

            switch (static_cast<HighlightModeType>(i))
            {
                case HighlightModeType::Circle:
                {
                    mShaderManager.ActivateProgram<ProgramType::ShipCircleHighlights>();
                    break;
                }

                case HighlightModeType::ElectricalElement:
                {
                    mShaderManager.ActivateProgram<ProgramType::ShipElectricalElementHighlights>();
                    break;
                }

                default:
                {
                    assert(false);
                    break;
                }
            }

            if (renderParameters.DebugShipRenderMode == DebugShipRenderModeType::Wireframe)
                glLineWidth(0.1f);

            assert(0 == (mHighlightVertexBuffers[i].size() % 6));
            glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(mHighlightVertexBuffers[i].size()));

            glBindVertexArray(0);
        }
    }
}

void ShipRenderContext::RenderPrepareVectorArrows(RenderParameters const & /*renderParameters*/)
{
    if (!mVectorArrowVertexBuffer.empty())
    {
        //
        // Color
        //

        if (mIsVectorArrowColorDirty)
        {
            mShaderManager.ActivateProgram<ProgramType::ShipVectors>();

            mShaderManager.SetProgramParameter<ProgramType::ShipVectors, ProgramParameterType::MatteColor>(
                mVectorArrowColor.x,
                mVectorArrowColor.y,
                mVectorArrowColor.z,
                mVectorArrowColor.w);

            mIsVectorArrowColorDirty = false;
        }

        glBindBuffer(GL_ARRAY_BUFFER, *mVectorArrowVBO);

        if (mVectorArrowVertexBuffer.size() > mVectorArrowVBOAllocatedVertexSize)
        {
            // Re-allocate VBO buffer and upload
            glBufferData(GL_ARRAY_BUFFER, mVectorArrowVertexBuffer.size() * sizeof(vec3f), mVectorArrowVertexBuffer.data(), GL_DYNAMIC_DRAW);
            CheckOpenGLError();

            mVectorArrowVBOAllocatedVertexSize = mVectorArrowVertexBuffer.size();
        }
        else
        {
            // No size change, just upload VBO buffer
            glBufferSubData(GL_ARRAY_BUFFER, 0, mVectorArrowVertexBuffer.size() * sizeof(vec3f), mVectorArrowVertexBuffer.data());
            CheckOpenGLError();
        }

        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
}

void ShipRenderContext::RenderDrawVectorArrows(RenderParameters const & /*renderParameters*/)
{
    if (!mVectorArrowVertexBuffer.empty())
    {
        glBindVertexArray(*mVectorArrowVAO);

        mShaderManager.ActivateProgram<ProgramType::ShipVectors>();

        glLineWidth(0.5f);

        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(mVectorArrowVertexBuffer.size()));

        glBindVertexArray(0);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////

void ShipRenderContext::ApplyViewModelChanges(RenderParameters const & renderParameters)
{
    //
    // Each plane Z segment is divided into a number of layers, one for each type of rendering we do for a ship:
    //      - 0: Ropes (always behind)
    //      - 1: Flames - background
    //      - 2: Springs
    //      - 3: Triangles
    //          - Triangles are always drawn temporally before ropes and springs though, to avoid anti-aliasing issues
    //      - 4: Stressed springs
    //      - 5: Points
    //      - 6: Flames - foreground
    //      - 7: Sparkles
    //      - 8: Generic textures
    //      - 9: Explosions
    //      - 10: Highlights
    //      - 11: Vectors
    //

    constexpr float ShipRegionZStart = 1.0f; // Far
    constexpr float ShipRegionZWidth = -2.0f; // Near (-1)

    constexpr int NLayers = 12;

    auto const & view = renderParameters.View;

    ViewModel::ProjectionMatrix shipOrthoMatrix;

    //
    // Layer 0: Ropes
    //

    view.CalculateShipOrthoMatrix(
        ShipRegionZStart,
        ShipRegionZWidth,
        static_cast<int>(mShipId),
        static_cast<int>(mShipCount),
        static_cast<int>(mMaxMaxPlaneId),
        0,
        NLayers,
        shipOrthoMatrix);

    mShaderManager.ActivateProgram<ProgramType::ShipRopes>();
    mShaderManager.SetProgramParameter<ProgramType::ShipRopes, ProgramParameterType::OrthoMatrix>(
        shipOrthoMatrix);

    mShaderManager.ActivateProgram<ProgramType::ShipRopesWithTemperature>();
    mShaderManager.SetProgramParameter<ProgramType::ShipRopesWithTemperature, ProgramParameterType::OrthoMatrix>(
        shipOrthoMatrix);

    //
    // Layer 1: Flames - background
    //

    view.CalculateShipOrthoMatrix(
        ShipRegionZStart,
        ShipRegionZWidth,
        static_cast<int>(mShipId),
        static_cast<int>(mShipCount),
        static_cast<int>(mMaxMaxPlaneId),
        1,
        NLayers,
        shipOrthoMatrix);

    mShaderManager.ActivateProgram<ProgramType::ShipFlamesBackground1>();
    mShaderManager.SetProgramParameter<ProgramType::ShipFlamesBackground1, ProgramParameterType::OrthoMatrix>(
        shipOrthoMatrix);

    mShaderManager.ActivateProgram<ProgramType::ShipFlamesBackground2>();
    mShaderManager.SetProgramParameter<ProgramType::ShipFlamesBackground2, ProgramParameterType::OrthoMatrix>(
        shipOrthoMatrix);

    mShaderManager.ActivateProgram<ProgramType::ShipFlamesBackground3>();
    mShaderManager.SetProgramParameter<ProgramType::ShipFlamesBackground3, ProgramParameterType::OrthoMatrix>(
        shipOrthoMatrix);


    //
    // Layer 2: Springs
    //

    view.CalculateShipOrthoMatrix(
        ShipRegionZStart,
        ShipRegionZWidth,
        static_cast<int>(mShipId),
        static_cast<int>(mShipCount),
        static_cast<int>(mMaxMaxPlaneId),
        2,
        NLayers,
        shipOrthoMatrix);

    mShaderManager.ActivateProgram<ProgramType::ShipSpringsColor>();
    mShaderManager.SetProgramParameter<ProgramType::ShipSpringsColor, ProgramParameterType::OrthoMatrix>(
        shipOrthoMatrix);

    mShaderManager.ActivateProgram<ProgramType::ShipSpringsColorWithTemperature>();
    mShaderManager.SetProgramParameter<ProgramType::ShipSpringsColorWithTemperature, ProgramParameterType::OrthoMatrix>(
        shipOrthoMatrix);

    mShaderManager.ActivateProgram<ProgramType::ShipSpringsTexture>();
    mShaderManager.SetProgramParameter<ProgramType::ShipSpringsTexture, ProgramParameterType::OrthoMatrix>(
        shipOrthoMatrix);

    mShaderManager.ActivateProgram<ProgramType::ShipSpringsTextureWithTemperature>();
    mShaderManager.SetProgramParameter<ProgramType::ShipSpringsTextureWithTemperature, ProgramParameterType::OrthoMatrix>(
        shipOrthoMatrix);

    //
    // Layer 3: Triangles
    //

    view.CalculateShipOrthoMatrix(
        ShipRegionZStart,
        ShipRegionZWidth,
        static_cast<int>(mShipId),
        static_cast<int>(mShipCount),
        static_cast<int>(mMaxMaxPlaneId),
        3,
        NLayers,
        shipOrthoMatrix);

    mShaderManager.ActivateProgram<ProgramType::ShipTrianglesColor>();
    mShaderManager.SetProgramParameter<ProgramType::ShipTrianglesColor, ProgramParameterType::OrthoMatrix>(
        shipOrthoMatrix);

    mShaderManager.ActivateProgram<ProgramType::ShipTrianglesColorWithTemperature>();
    mShaderManager.SetProgramParameter<ProgramType::ShipTrianglesColorWithTemperature, ProgramParameterType::OrthoMatrix>(
        shipOrthoMatrix);

    mShaderManager.ActivateProgram<ProgramType::ShipTrianglesDecay>();
    mShaderManager.SetProgramParameter<ProgramType::ShipTrianglesDecay, ProgramParameterType::OrthoMatrix>(
        shipOrthoMatrix);

    mShaderManager.ActivateProgram<ProgramType::ShipTrianglesTexture>();
    mShaderManager.SetProgramParameter<ProgramType::ShipTrianglesTexture, ProgramParameterType::OrthoMatrix>(
        shipOrthoMatrix);

    mShaderManager.ActivateProgram<ProgramType::ShipTrianglesTextureWithTemperature>();
    mShaderManager.SetProgramParameter<ProgramType::ShipTrianglesTextureWithTemperature, ProgramParameterType::OrthoMatrix>(
        shipOrthoMatrix);

    //
    // Layer 4: Stressed Springs
    //

    view.CalculateShipOrthoMatrix(
        ShipRegionZStart,
        ShipRegionZWidth,
        static_cast<int>(mShipId),
        static_cast<int>(mShipCount),
        static_cast<int>(mMaxMaxPlaneId),
        4,
        NLayers,
        shipOrthoMatrix);

    mShaderManager.ActivateProgram<ProgramType::ShipStressedSprings>();
    mShaderManager.SetProgramParameter<ProgramType::ShipStressedSprings, ProgramParameterType::OrthoMatrix>(
        shipOrthoMatrix);

    //
    // Layer 5: Points
    //

    view.CalculateShipOrthoMatrix(
        ShipRegionZStart,
        ShipRegionZWidth,
        static_cast<int>(mShipId),
        static_cast<int>(mShipCount),
        static_cast<int>(mMaxMaxPlaneId),
        5,
        NLayers,
        shipOrthoMatrix);

    mShaderManager.ActivateProgram<ProgramType::ShipPointsColor>();
    mShaderManager.SetProgramParameter<ProgramType::ShipPointsColor, ProgramParameterType::OrthoMatrix>(
        shipOrthoMatrix);

    mShaderManager.ActivateProgram<ProgramType::ShipPointsColorWithTemperature>();
    mShaderManager.SetProgramParameter<ProgramType::ShipPointsColorWithTemperature, ProgramParameterType::OrthoMatrix>(
        shipOrthoMatrix);

    //
    // Layer 6: Flames - foreground
    //

    view.CalculateShipOrthoMatrix(
        ShipRegionZStart,
        ShipRegionZWidth,
        static_cast<int>(mShipId),
        static_cast<int>(mShipCount),
        static_cast<int>(mMaxMaxPlaneId),
        6,
        NLayers,
        shipOrthoMatrix);

    mShaderManager.ActivateProgram<ProgramType::ShipFlamesForeground1>();
    mShaderManager.SetProgramParameter<ProgramType::ShipFlamesForeground1, ProgramParameterType::OrthoMatrix>(
        shipOrthoMatrix);

    mShaderManager.ActivateProgram<ProgramType::ShipFlamesForeground2>();
    mShaderManager.SetProgramParameter<ProgramType::ShipFlamesForeground2, ProgramParameterType::OrthoMatrix>(
        shipOrthoMatrix);

    mShaderManager.ActivateProgram<ProgramType::ShipFlamesForeground3>();
    mShaderManager.SetProgramParameter<ProgramType::ShipFlamesForeground3, ProgramParameterType::OrthoMatrix>(
        shipOrthoMatrix);

    //
    // Layer 7: Sparkles
    //

    view.CalculateShipOrthoMatrix(
        ShipRegionZStart,
        ShipRegionZWidth,
        static_cast<int>(mShipId),
        static_cast<int>(mShipCount),
        static_cast<int>(mMaxMaxPlaneId),
        7,
        NLayers,
        shipOrthoMatrix);

    mShaderManager.ActivateProgram<ProgramType::ShipSparkles>();
    mShaderManager.SetProgramParameter<ProgramType::ShipSparkles, ProgramParameterType::OrthoMatrix>(
        shipOrthoMatrix);

    //
    // Layer 8: Generic Textures
    //

    view.CalculateShipOrthoMatrix(
        ShipRegionZStart,
        ShipRegionZWidth,
        static_cast<int>(mShipId),
        static_cast<int>(mShipCount),
        static_cast<int>(mMaxMaxPlaneId),
        8,
        NLayers,
        shipOrthoMatrix);

    mShaderManager.ActivateProgram<ProgramType::ShipGenericMipMappedTextures>();
    mShaderManager.SetProgramParameter<ProgramType::ShipGenericMipMappedTextures, ProgramParameterType::OrthoMatrix>(
        shipOrthoMatrix);

    //
    // Layer 9: Explosions
    //

    view.CalculateShipOrthoMatrix(
        ShipRegionZStart,
        ShipRegionZWidth,
        static_cast<int>(mShipId),
        static_cast<int>(mShipCount),
        static_cast<int>(mMaxMaxPlaneId),
        9,
        NLayers,
        shipOrthoMatrix);

    mShaderManager.ActivateProgram<ProgramType::ShipExplosions>();
    mShaderManager.SetProgramParameter<ProgramType::ShipExplosions, ProgramParameterType::OrthoMatrix>(
        shipOrthoMatrix);

    //
    // Layer 10: Highlights
    //

    view.CalculateShipOrthoMatrix(
        ShipRegionZStart,
        ShipRegionZWidth,
        static_cast<int>(mShipId),
        static_cast<int>(mShipCount),
        static_cast<int>(mMaxMaxPlaneId),
        10,
        NLayers,
        shipOrthoMatrix);

    mShaderManager.ActivateProgram<ProgramType::ShipElectricalElementHighlights>();
    mShaderManager.SetProgramParameter<ProgramType::ShipElectricalElementHighlights, ProgramParameterType::OrthoMatrix>(
        shipOrthoMatrix);

    mShaderManager.ActivateProgram<ProgramType::ShipCircleHighlights>();
    mShaderManager.SetProgramParameter<ProgramType::ShipCircleHighlights, ProgramParameterType::OrthoMatrix>(
        shipOrthoMatrix);

    //
    // Layer 11: Vectors
    //

    view.CalculateShipOrthoMatrix(
        ShipRegionZStart,
        ShipRegionZWidth,
        static_cast<int>(mShipId),
        static_cast<int>(mShipCount),
        static_cast<int>(mMaxMaxPlaneId),
        11,
        NLayers,
        shipOrthoMatrix);

    mShaderManager.ActivateProgram<ProgramType::ShipVectors>();
    mShaderManager.SetProgramParameter<ProgramType::ShipVectors, ProgramParameterType::OrthoMatrix>(
        shipOrthoMatrix);
}

void ShipRenderContext::ApplyEffectiveAmbientLightIntensityChanges(RenderParameters const & renderParameters)
{
    //
    // Set parameter in all programs
    //

    mShaderManager.ActivateProgram<ProgramType::ShipRopes>();
    mShaderManager.SetProgramParameter<ProgramType::ShipRopes, ProgramParameterType::EffectiveAmbientLightIntensity>(
        renderParameters.EffectiveAmbientLightIntensity);

    mShaderManager.ActivateProgram<ProgramType::ShipRopesWithTemperature>();
    mShaderManager.SetProgramParameter<ProgramType::ShipRopesWithTemperature, ProgramParameterType::EffectiveAmbientLightIntensity>(
        renderParameters.EffectiveAmbientLightIntensity);

    mShaderManager.ActivateProgram<ProgramType::ShipSpringsColor>();
    mShaderManager.SetProgramParameter<ProgramType::ShipSpringsColor, ProgramParameterType::EffectiveAmbientLightIntensity>(
        renderParameters.EffectiveAmbientLightIntensity);

    mShaderManager.ActivateProgram<ProgramType::ShipSpringsColorWithTemperature>();
    mShaderManager.SetProgramParameter<ProgramType::ShipSpringsColorWithTemperature, ProgramParameterType::EffectiveAmbientLightIntensity>(
        renderParameters.EffectiveAmbientLightIntensity);

    mShaderManager.ActivateProgram<ProgramType::ShipSpringsTexture>();
    mShaderManager.SetProgramParameter<ProgramType::ShipSpringsTexture, ProgramParameterType::EffectiveAmbientLightIntensity>(
        renderParameters.EffectiveAmbientLightIntensity);

    mShaderManager.ActivateProgram<ProgramType::ShipSpringsTextureWithTemperature>();
    mShaderManager.SetProgramParameter<ProgramType::ShipSpringsTextureWithTemperature, ProgramParameterType::EffectiveAmbientLightIntensity>(
        renderParameters.EffectiveAmbientLightIntensity);

    mShaderManager.ActivateProgram<ProgramType::ShipTrianglesColor>();
    mShaderManager.SetProgramParameter<ProgramType::ShipTrianglesColor, ProgramParameterType::EffectiveAmbientLightIntensity>(
        renderParameters.EffectiveAmbientLightIntensity);

    mShaderManager.ActivateProgram<ProgramType::ShipTrianglesColorWithTemperature>();
    mShaderManager.SetProgramParameter<ProgramType::ShipTrianglesColorWithTemperature, ProgramParameterType::EffectiveAmbientLightIntensity>(
        renderParameters.EffectiveAmbientLightIntensity);

    mShaderManager.ActivateProgram<ProgramType::ShipTrianglesDecay>();
    mShaderManager.SetProgramParameter<ProgramType::ShipTrianglesDecay, ProgramParameterType::EffectiveAmbientLightIntensity>(
        renderParameters.EffectiveAmbientLightIntensity);

    mShaderManager.ActivateProgram<ProgramType::ShipTrianglesTexture>();
    mShaderManager.SetProgramParameter<ProgramType::ShipTrianglesTexture, ProgramParameterType::EffectiveAmbientLightIntensity>(
        renderParameters.EffectiveAmbientLightIntensity);

    mShaderManager.ActivateProgram<ProgramType::ShipTrianglesTextureWithTemperature>();
    mShaderManager.SetProgramParameter<ProgramType::ShipTrianglesTextureWithTemperature, ProgramParameterType::EffectiveAmbientLightIntensity>(
        renderParameters.EffectiveAmbientLightIntensity);

    mShaderManager.ActivateProgram<ProgramType::ShipPointsColor>();
    mShaderManager.SetProgramParameter<ProgramType::ShipPointsColor, ProgramParameterType::EffectiveAmbientLightIntensity>(
        renderParameters.EffectiveAmbientLightIntensity);

    mShaderManager.ActivateProgram<ProgramType::ShipPointsColorWithTemperature>();
    mShaderManager.SetProgramParameter<ProgramType::ShipPointsColorWithTemperature, ProgramParameterType::EffectiveAmbientLightIntensity>(
        renderParameters.EffectiveAmbientLightIntensity);

    mShaderManager.ActivateProgram<ProgramType::ShipGenericMipMappedTextures>();
    mShaderManager.SetProgramParameter<ProgramType::ShipGenericMipMappedTextures, ProgramParameterType::EffectiveAmbientLightIntensity>(
        renderParameters.EffectiveAmbientLightIntensity);
}

void ShipRenderContext::ApplyFlatLampLightColorChanges(RenderParameters const & renderParameters)
{
    //
    // Set parameter in all programs
    //

    vec4f const lampLightColor = renderParameters.FlatLampLightColor.toVec4f(1.0f);

    mShaderManager.ActivateProgram<ProgramType::ShipRopes>();
    mShaderManager.SetProgramParameter<ProgramType::ShipRopes, ProgramParameterType::LampLightColor>(
        lampLightColor.x, lampLightColor.y, lampLightColor.z, lampLightColor.w);

    mShaderManager.ActivateProgram<ProgramType::ShipRopesWithTemperature>();
    mShaderManager.SetProgramParameter<ProgramType::ShipRopesWithTemperature, ProgramParameterType::LampLightColor>(
        lampLightColor.x, lampLightColor.y, lampLightColor.z, lampLightColor.w);

    mShaderManager.ActivateProgram<ProgramType::ShipSpringsColor>();
    mShaderManager.SetProgramParameter<ProgramType::ShipSpringsColor, ProgramParameterType::LampLightColor>(
        lampLightColor.x, lampLightColor.y, lampLightColor.z, lampLightColor.w);

    mShaderManager.ActivateProgram<ProgramType::ShipSpringsColorWithTemperature>();
    mShaderManager.SetProgramParameter<ProgramType::ShipSpringsColorWithTemperature, ProgramParameterType::LampLightColor>(
        lampLightColor.x, lampLightColor.y, lampLightColor.z, lampLightColor.w);

    mShaderManager.ActivateProgram<ProgramType::ShipSpringsTexture>();
    mShaderManager.SetProgramParameter<ProgramType::ShipSpringsTexture, ProgramParameterType::LampLightColor>(
        lampLightColor.x, lampLightColor.y, lampLightColor.z, lampLightColor.w);

    mShaderManager.ActivateProgram<ProgramType::ShipSpringsTextureWithTemperature>();
    mShaderManager.SetProgramParameter<ProgramType::ShipSpringsTextureWithTemperature, ProgramParameterType::LampLightColor>(
        lampLightColor.x, lampLightColor.y, lampLightColor.z, lampLightColor.w);

    mShaderManager.ActivateProgram<ProgramType::ShipTrianglesColor>();
    mShaderManager.SetProgramParameter<ProgramType::ShipTrianglesColor, ProgramParameterType::LampLightColor>(
        lampLightColor.x, lampLightColor.y, lampLightColor.z, lampLightColor.w);

    mShaderManager.ActivateProgram<ProgramType::ShipTrianglesColorWithTemperature>();
    mShaderManager.SetProgramParameter<ProgramType::ShipTrianglesColorWithTemperature, ProgramParameterType::LampLightColor>(
        lampLightColor.x, lampLightColor.y, lampLightColor.z, lampLightColor.w);

    mShaderManager.ActivateProgram<ProgramType::ShipTrianglesTexture>();
    mShaderManager.SetProgramParameter<ProgramType::ShipTrianglesTexture, ProgramParameterType::LampLightColor>(
        lampLightColor.x, lampLightColor.y, lampLightColor.z, lampLightColor.w);

    mShaderManager.ActivateProgram<ProgramType::ShipTrianglesTextureWithTemperature>();
    mShaderManager.SetProgramParameter<ProgramType::ShipTrianglesTextureWithTemperature, ProgramParameterType::LampLightColor>(
        lampLightColor.x, lampLightColor.y, lampLightColor.z, lampLightColor.w);

    mShaderManager.ActivateProgram<ProgramType::ShipPointsColor>();
    mShaderManager.SetProgramParameter<ProgramType::ShipPointsColor, ProgramParameterType::LampLightColor>(
        lampLightColor.x, lampLightColor.y, lampLightColor.z, lampLightColor.w);

    mShaderManager.ActivateProgram<ProgramType::ShipPointsColorWithTemperature>();
    mShaderManager.SetProgramParameter<ProgramType::ShipPointsColorWithTemperature, ProgramParameterType::LampLightColor>(
        lampLightColor.x, lampLightColor.y, lampLightColor.z, lampLightColor.w);
}

void ShipRenderContext::ApplyWaterColorChanges(RenderParameters const & renderParameters)
{
    //
    // Set parameter in all programs
    //

    auto const waterColor = renderParameters.ShipWaterColor;

    mShaderManager.ActivateProgram<ProgramType::ShipRopes>();
    mShaderManager.SetProgramParameter<ProgramType::ShipRopes, ProgramParameterType::WaterColor>(
        waterColor.x, waterColor.y, waterColor.z, waterColor.w);

    mShaderManager.ActivateProgram<ProgramType::ShipRopesWithTemperature>();
    mShaderManager.SetProgramParameter<ProgramType::ShipRopesWithTemperature, ProgramParameterType::WaterColor>(
        waterColor.x, waterColor.y, waterColor.z, waterColor.w);

    mShaderManager.ActivateProgram<ProgramType::ShipSpringsColor>();
    mShaderManager.SetProgramParameter<ProgramType::ShipSpringsColor, ProgramParameterType::WaterColor>(
        waterColor.x, waterColor.y, waterColor.z, waterColor.w);

    mShaderManager.ActivateProgram<ProgramType::ShipSpringsColorWithTemperature>();
    mShaderManager.SetProgramParameter<ProgramType::ShipSpringsColorWithTemperature, ProgramParameterType::WaterColor>(
        waterColor.x, waterColor.y, waterColor.z, waterColor.w);

    mShaderManager.ActivateProgram<ProgramType::ShipSpringsTexture>();
    mShaderManager.SetProgramParameter<ProgramType::ShipSpringsTexture, ProgramParameterType::WaterColor>(
        waterColor.x, waterColor.y, waterColor.z, waterColor.w);

    mShaderManager.ActivateProgram<ProgramType::ShipSpringsTextureWithTemperature>();
    mShaderManager.SetProgramParameter<ProgramType::ShipSpringsTextureWithTemperature, ProgramParameterType::WaterColor>(
        waterColor.x, waterColor.y, waterColor.z, waterColor.w);

    mShaderManager.ActivateProgram<ProgramType::ShipTrianglesColor>();
    mShaderManager.SetProgramParameter<ProgramType::ShipTrianglesColor, ProgramParameterType::WaterColor>(
        waterColor.x, waterColor.y, waterColor.z, waterColor.w);

    mShaderManager.ActivateProgram<ProgramType::ShipTrianglesColorWithTemperature>();
    mShaderManager.SetProgramParameter<ProgramType::ShipTrianglesColorWithTemperature, ProgramParameterType::WaterColor>(
        waterColor.x, waterColor.y, waterColor.z, waterColor.w);

    mShaderManager.ActivateProgram<ProgramType::ShipTrianglesTexture>();
    mShaderManager.SetProgramParameter<ProgramType::ShipTrianglesTexture, ProgramParameterType::WaterColor>(
        waterColor.x, waterColor.y, waterColor.z, waterColor.w);

    mShaderManager.ActivateProgram<ProgramType::ShipTrianglesTextureWithTemperature>();
    mShaderManager.SetProgramParameter<ProgramType::ShipTrianglesTextureWithTemperature, ProgramParameterType::WaterColor>(
        waterColor.x, waterColor.y, waterColor.z, waterColor.w);

    mShaderManager.ActivateProgram<ProgramType::ShipPointsColor>();
    mShaderManager.SetProgramParameter<ProgramType::ShipPointsColor, ProgramParameterType::WaterColor>(
        waterColor.x, waterColor.y, waterColor.z, waterColor.w);

    mShaderManager.ActivateProgram<ProgramType::ShipPointsColorWithTemperature>();
    mShaderManager.SetProgramParameter<ProgramType::ShipPointsColorWithTemperature, ProgramParameterType::WaterColor>(
        waterColor.x, waterColor.y, waterColor.z, waterColor.w);
}

void ShipRenderContext::ApplyWaterContrastChanges(RenderParameters const & renderParameters)
{
    //
    // Set parameter in all programs
    //

    mShaderManager.ActivateProgram<ProgramType::ShipRopes>();
    mShaderManager.SetProgramParameter<ProgramType::ShipRopes, ProgramParameterType::WaterContrast>(
        renderParameters.ShipWaterContrast);

    mShaderManager.ActivateProgram<ProgramType::ShipRopesWithTemperature>();
    mShaderManager.SetProgramParameter<ProgramType::ShipRopesWithTemperature, ProgramParameterType::WaterContrast>(
        renderParameters.ShipWaterContrast);

    mShaderManager.ActivateProgram<ProgramType::ShipSpringsColor>();
    mShaderManager.SetProgramParameter<ProgramType::ShipSpringsColor, ProgramParameterType::WaterContrast>(
        renderParameters.ShipWaterContrast);

    mShaderManager.ActivateProgram<ProgramType::ShipSpringsColorWithTemperature>();
    mShaderManager.SetProgramParameter<ProgramType::ShipSpringsColorWithTemperature, ProgramParameterType::WaterContrast>(
        renderParameters.ShipWaterContrast);

    mShaderManager.ActivateProgram<ProgramType::ShipSpringsTexture>();
    mShaderManager.SetProgramParameter<ProgramType::ShipSpringsTexture, ProgramParameterType::WaterContrast>(
        renderParameters.ShipWaterContrast);

    mShaderManager.ActivateProgram<ProgramType::ShipSpringsTextureWithTemperature>();
    mShaderManager.SetProgramParameter<ProgramType::ShipSpringsTextureWithTemperature, ProgramParameterType::WaterContrast>(
        renderParameters.ShipWaterContrast);

    mShaderManager.ActivateProgram<ProgramType::ShipTrianglesColor>();
    mShaderManager.SetProgramParameter<ProgramType::ShipTrianglesColor, ProgramParameterType::WaterContrast>(
        renderParameters.ShipWaterContrast);

    mShaderManager.ActivateProgram<ProgramType::ShipTrianglesColorWithTemperature>();
    mShaderManager.SetProgramParameter<ProgramType::ShipTrianglesColorWithTemperature, ProgramParameterType::WaterContrast>(
        renderParameters.ShipWaterContrast);

    mShaderManager.ActivateProgram<ProgramType::ShipTrianglesTexture>();
    mShaderManager.SetProgramParameter<ProgramType::ShipTrianglesTexture, ProgramParameterType::WaterContrast>(
        renderParameters.ShipWaterContrast);

    mShaderManager.ActivateProgram<ProgramType::ShipTrianglesTextureWithTemperature>();
    mShaderManager.SetProgramParameter<ProgramType::ShipTrianglesTextureWithTemperature, ProgramParameterType::WaterContrast>(
        renderParameters.ShipWaterContrast);

    mShaderManager.ActivateProgram<ProgramType::ShipPointsColor>();
    mShaderManager.SetProgramParameter<ProgramType::ShipPointsColor, ProgramParameterType::WaterContrast>(
        renderParameters.ShipWaterContrast);

    mShaderManager.ActivateProgram<ProgramType::ShipPointsColorWithTemperature>();
    mShaderManager.SetProgramParameter<ProgramType::ShipPointsColorWithTemperature, ProgramParameterType::WaterContrast>(
        renderParameters.ShipWaterContrast);
}

void ShipRenderContext::ApplyWaterLevelOfDetailChanges(RenderParameters const & renderParameters)
{
    // Transform: 0->1 == 2.0->0.01
    float const waterLevelThreshold = 2.0f + renderParameters.ShipWaterLevelOfDetail * (-2.0f + 0.01f);

    //
    // Set parameter in all programs
    //

    mShaderManager.ActivateProgram<ProgramType::ShipRopes>();
    mShaderManager.SetProgramParameter<ProgramType::ShipRopes, ProgramParameterType::WaterLevelThreshold>(
        waterLevelThreshold);

    mShaderManager.ActivateProgram<ProgramType::ShipRopesWithTemperature>();
    mShaderManager.SetProgramParameter<ProgramType::ShipRopesWithTemperature, ProgramParameterType::WaterLevelThreshold>(
        waterLevelThreshold);

    mShaderManager.ActivateProgram<ProgramType::ShipSpringsColor>();
    mShaderManager.SetProgramParameter<ProgramType::ShipSpringsColor, ProgramParameterType::WaterLevelThreshold>(
        waterLevelThreshold);

    mShaderManager.ActivateProgram<ProgramType::ShipSpringsColorWithTemperature>();
    mShaderManager.SetProgramParameter<ProgramType::ShipSpringsColorWithTemperature, ProgramParameterType::WaterLevelThreshold>(
        waterLevelThreshold);

    mShaderManager.ActivateProgram<ProgramType::ShipSpringsTexture>();
    mShaderManager.SetProgramParameter<ProgramType::ShipSpringsTexture, ProgramParameterType::WaterLevelThreshold>(
        waterLevelThreshold);

    mShaderManager.ActivateProgram<ProgramType::ShipSpringsTextureWithTemperature>();
    mShaderManager.SetProgramParameter<ProgramType::ShipSpringsTextureWithTemperature, ProgramParameterType::WaterLevelThreshold>(
        waterLevelThreshold);

    mShaderManager.ActivateProgram<ProgramType::ShipTrianglesColor>();
    mShaderManager.SetProgramParameter<ProgramType::ShipTrianglesColor, ProgramParameterType::WaterLevelThreshold>(
        waterLevelThreshold);

    mShaderManager.ActivateProgram<ProgramType::ShipTrianglesColorWithTemperature>();
    mShaderManager.SetProgramParameter<ProgramType::ShipTrianglesColorWithTemperature, ProgramParameterType::WaterLevelThreshold>(
        waterLevelThreshold);

    mShaderManager.ActivateProgram<ProgramType::ShipTrianglesTexture>();
    mShaderManager.SetProgramParameter<ProgramType::ShipTrianglesTexture, ProgramParameterType::WaterLevelThreshold>(
        waterLevelThreshold);

    mShaderManager.ActivateProgram<ProgramType::ShipTrianglesTextureWithTemperature>();
    mShaderManager.SetProgramParameter<ProgramType::ShipTrianglesTextureWithTemperature, ProgramParameterType::WaterLevelThreshold>(
        waterLevelThreshold);

    mShaderManager.ActivateProgram<ProgramType::ShipPointsColor>();
    mShaderManager.SetProgramParameter<ProgramType::ShipPointsColor, ProgramParameterType::WaterLevelThreshold>(
        waterLevelThreshold);

    mShaderManager.ActivateProgram<ProgramType::ShipPointsColorWithTemperature>();
    mShaderManager.SetProgramParameter<ProgramType::ShipPointsColorWithTemperature, ProgramParameterType::WaterLevelThreshold>(
        waterLevelThreshold);
}

void ShipRenderContext::ApplyHeatOverlayTransparencyChanges(RenderParameters const & renderParameters)
{
    //
    // Set parameter in all programs
    //

    mShaderManager.ActivateProgram<ProgramType::ShipRopesWithTemperature>();
    mShaderManager.SetProgramParameter<ProgramType::ShipRopesWithTemperature, ProgramParameterType::HeatOverlayTransparency>(
        renderParameters.HeatOverlayTransparency);

    mShaderManager.ActivateProgram<ProgramType::ShipSpringsColorWithTemperature>();
    mShaderManager.SetProgramParameter<ProgramType::ShipSpringsColorWithTemperature, ProgramParameterType::HeatOverlayTransparency>(
        renderParameters.HeatOverlayTransparency);

    mShaderManager.ActivateProgram<ProgramType::ShipSpringsTextureWithTemperature>();
    mShaderManager.SetProgramParameter<ProgramType::ShipSpringsTextureWithTemperature, ProgramParameterType::HeatOverlayTransparency>(
        renderParameters.HeatOverlayTransparency);

    mShaderManager.ActivateProgram<ProgramType::ShipTrianglesColorWithTemperature>();
    mShaderManager.SetProgramParameter<ProgramType::ShipTrianglesColorWithTemperature, ProgramParameterType::HeatOverlayTransparency>(
        renderParameters.HeatOverlayTransparency);

    mShaderManager.ActivateProgram<ProgramType::ShipTrianglesTextureWithTemperature>();
    mShaderManager.SetProgramParameter<ProgramType::ShipTrianglesTextureWithTemperature, ProgramParameterType::HeatOverlayTransparency>(
        renderParameters.HeatOverlayTransparency);

    mShaderManager.ActivateProgram<ProgramType::ShipPointsColorWithTemperature>();
    mShaderManager.SetProgramParameter<ProgramType::ShipPointsColorWithTemperature, ProgramParameterType::HeatOverlayTransparency>(
        renderParameters.HeatOverlayTransparency);
}

}