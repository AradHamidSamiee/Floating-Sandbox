/***************************************************************************************
* Original Author:      Gabriele Giuseppini
* Created:              2019-01-26
* Copyright:            Gabriele Giuseppini  (https://github.com/GabrieleGiuseppini)
***************************************************************************************/
#include "ImageFileTools.h"

#include <GameCore/GameException.h>

#include <IL/il.h>
#include <IL/ilu.h>

#include <algorithm>
#include <cassert>
#include <cstring>
#include <regex>

bool ImageFileTools::mIsInitialized = false;

ImageSize ImageFileTools::GetImageSize(std::filesystem::path const & filepath)
{
    //
    // Load image
    //

    ILuint imgHandle = InternalLoadImage(filepath);

    //
    // Get size
    //

    int const width = ilGetInteger(IL_IMAGE_WIDTH);
    int const height = ilGetInteger(IL_IMAGE_HEIGHT);


    //
    // Delete image
    //

    ilDeleteImage(imgHandle);


    //
    // Check
    //

    if (width == 0 || height == 0)
    {
        throw GameException("Could not load image \"" + filepath.string() + "\": image is empty");
    }

    return ImageSize(width, height);
}

RgbaImageData ImageFileTools::LoadImageRgba(std::filesystem::path const & filepath)
{
    return InternalLoadImage<rgbaColor>(
        filepath,
        IL_RGBA,
        IL_ORIGIN_LOWER_LEFT,
        std::nullopt);
}

RgbImageData ImageFileTools::LoadImageRgb(std::filesystem::path const & filepath)
{
    return InternalLoadImage<rgbColor>(
        filepath,
        IL_RGB,
        IL_ORIGIN_LOWER_LEFT,
        std::nullopt);
}

RgbaImageData ImageFileTools::LoadImageRgbaAndMagnify(
    std::filesystem::path const & filepath,
    int magnificationFactor)
{
    return InternalLoadImage<rgbaColor>(
        filepath,
        IL_RGBA,
        IL_ORIGIN_LOWER_LEFT,
        ResizeInfo(
            [magnificationFactor](ImageSize const & originalImageSize)
            {
                return ImageSize(
                    originalImageSize.Width * magnificationFactor,
                    originalImageSize.Height * magnificationFactor);
            },
            ILU_NEAREST));
}

RgbaImageData ImageFileTools::LoadImageRgbaAndResize(
    std::filesystem::path const & filepath,
    int resizedWidth)
{
    return InternalLoadImage<rgbaColor>(
        filepath,
        IL_RGBA,
        IL_ORIGIN_LOWER_LEFT,
        ResizeInfo(
            [resizedWidth](ImageSize const & originalImageSize)
            {
                return ImageSize(
                    resizedWidth,
                    static_cast<int>(
                        round(
                            static_cast<float>(originalImageSize.Height)
                            / static_cast<float>(originalImageSize.Width)
                            * static_cast<float>(resizedWidth))));
            },
            ILU_BILINEAR));
}

RgbaImageData ImageFileTools::LoadImageRgbaAndResize(
    std::filesystem::path const & filepath,
    ImageSize const & maxSize)
{
    return InternalLoadImageAndResize<rgbaColor>(
        filepath,
        IL_RGBA,
        maxSize);
}

RgbImageData ImageFileTools::LoadImageRgbAndResize(
    std::filesystem::path const & filepath,
    ImageSize const & maxSize)
{
    return InternalLoadImageAndResize<rgbColor>(
        filepath,
        IL_RGB,
        maxSize);
}

void ImageFileTools::SaveImage(
    std::filesystem::path filepath,
    RgbaImageData const & image)
{
    InternalSaveImage(
        image.Size,
        image.Data.get(),
        4,
        IL_RGBA,
        filepath);
}

void ImageFileTools::SaveImage(
    std::filesystem::path filepath,
    RgbImageData const & image)
{
    InternalSaveImage(
        image.Size,
        image.Data.get(),
        3,
        IL_RGB,
        filepath);
}

////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////

void ImageFileTools::CheckInitialized()
{
    if (!mIsInitialized)
    {
        // Initialize DevIL
        ilInit();
        iluInit();

        mIsInitialized = true;
    }
}

unsigned int ImageFileTools::InternalLoadImage(std::filesystem::path const & filepath)
{
    CheckInitialized();

    ILuint imghandle;
    ilGenImages(1, &imghandle);
    ilBindImage(imghandle);

    //
    // Load image
    //

    std::string const filepathStr = filepath.string();
    ILconst_string ilFilename(filepathStr.c_str());
    if (!ilLoadImage(ilFilename))
    {
        ILint const devilError = ilGetError();

        // First check if the file is missing altogether
        if (!std::filesystem::exists(filepath))
        {
            throw GameException("Could not load image \"" + filepathStr + "\": the file does not exist");
        }

        // Provide DevIL's error message now
        std::string const devilErrorMessage(iluErrorString(devilError));
        throw GameException("Could not load image \"" + filepathStr + "\": " + devilErrorMessage);
    }

    return static_cast<unsigned int>(imghandle);
}

template <typename TColor>
ImageData<TColor> ImageFileTools::InternalLoadImageAndResize(
    std::filesystem::path const & filepath,
    int targetFormat,
    ImageSize const & maxSize)
{
    return InternalLoadImage<TColor>(
        filepath,
        targetFormat,
        IL_ORIGIN_LOWER_LEFT,
        ResizeInfo(
            [maxSize](ImageSize const & originalImageSize)
            {
                float wShrinkFactor = static_cast<float>(maxSize.Width) / static_cast<float>(originalImageSize.Width);
                float hShrinkFactor = static_cast<float>(maxSize.Height) / static_cast<float>(originalImageSize.Height);
                float shrinkFactor = std::min(
                    std::min(wShrinkFactor, hShrinkFactor),
                    1.0f);

                return ImageSize(
                    static_cast<int>(round(static_cast<float>(originalImageSize.Width) * shrinkFactor)),
                    static_cast<int>(round(static_cast<float>(originalImageSize.Height) * shrinkFactor)));
            },
            ILU_BILINEAR));
}

template <typename TColor>
ImageData<TColor> ImageFileTools::InternalLoadImage(
    std::filesystem::path const & filepath,
    int targetFormat,
    int targetOrigin,
    std::optional<ResizeInfo> resizeInfo)
{
    //
    // Load image
    //

    ILuint imgHandle = InternalLoadImage(filepath);

    //
    // Check if we need to convert it
    //

    int imageFormat = ilGetInteger(IL_IMAGE_FORMAT);
    int imageType = ilGetInteger(IL_IMAGE_TYPE);
    if (targetFormat != imageFormat || IL_UNSIGNED_BYTE != imageType)
    {
        if (!ilConvertImage(targetFormat, IL_UNSIGNED_BYTE))
        {
            ILint devilError = ilGetError();
            std::string devilErrorMessage(iluErrorString(devilError));
            throw GameException("Could not convert image \"" + filepath.string() + "\": " + devilErrorMessage);
        }
    }

    int imageOrigin = ilGetInteger(IL_IMAGE_ORIGIN);
    if (targetOrigin != imageOrigin)
    {
        iluFlipImage();
    }


    //
    // Get metadata
    //

    ImageSize imageSize(
        ilGetInteger(IL_IMAGE_WIDTH),
        ilGetInteger(IL_IMAGE_HEIGHT));
    int const depth = ilGetInteger(IL_IMAGE_DEPTH);
    int const bpp = ilGetInteger(IL_IMAGE_BYTES_PER_PIXEL);

    assert(bpp == sizeof(TColor));


    //
    // Resize it
    //

    if (!!resizeInfo)
    {
        iluImageParameter(ILU_FILTER, resizeInfo->FilterType);

        auto newImageSize = resizeInfo->ResizeHandler(imageSize);

        if (!iluScale(newImageSize.Width, newImageSize.Height, depth))
        {
            ILint devilError = ilGetError();
            std::string devilErrorMessage(iluErrorString(devilError));
            throw GameException("Could not resize image: " + devilErrorMessage);
        }

        imageSize = newImageSize;
    }


    //
    // Create data
    //

    ILubyte const * imageData = ilGetData();
    auto data = std::make_unique<TColor[]>(imageSize.Width * imageSize.Height);
    std::memcpy(static_cast<void*>(data.get()), imageData, imageSize.Width * imageSize.Height * bpp);

    //
    // Delete image
    //

    ilDeleteImage(imgHandle);


    return ImageData<TColor>(
        imageSize,
        std::move(data));
}

void ImageFileTools::InternalSaveImage(
    ImageSize imageSize,
    void const * imageData,
    int bpp,
    int format,
    std::filesystem::path filepath)
{
    CheckInitialized();

    ILuint imghandle;
    ilGenImages(1, &imghandle);
    ilBindImage(imghandle);

    ilTexImage(
        imageSize.Width,
        imageSize.Height,
        1,
        static_cast<ILubyte>(bpp),
        format,
        IL_UNSIGNED_BYTE,
        const_cast<void *>(imageData));

    ilEnable(IL_FILE_OVERWRITE);
    ilSave(IL_PNG, filepath.string().c_str());

    ilDeleteImage(imghandle);
}