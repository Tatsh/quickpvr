#import "QPImageRendering.h"

#include <algorithm>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#import <ImageIO/ImageIO.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include "pvr/pvr.h"

NSErrorDomain const QPErrorDomain = @"sh.tat.quickpvr";

namespace {

constexpr size_t kBitsPerComponent = 8;
constexpr size_t kBitsPerPixel = 32;
constexpr size_t kBytesPerPixel = 4;
constexpr size_t kMaxUpscaleFactor = 16;

NSError *makeError(QPErrorCode code, NSString *message) {
    return [NSError errorWithDomain:QPErrorDomain
                               code:code
                           userInfo:@{NSLocalizedDescriptionKey : message}];
}

std::span<const std::uint8_t> bytesOf(NSData *data) {
    return {static_cast<const std::uint8_t *>(data.bytes), data.length};
}

void releaseImageBuffer(void *info, const void *, size_t) {
    std::unique_ptr<Pvr::Image>{static_cast<Pvr::Image *>(info)};
}

CGImageRef createCGImage(Pvr::Image &&source) {
    auto image = std::make_unique<Pvr::Image>(std::move(source));
    const auto width = image->width;
    const auto height = image->height;
    const auto *pixels = image->rgba.data();
    const auto size = image->rgba.size();
    // The provider frees the image when Core Graphics is done with it.
    CGDataProviderRef provider =
        CGDataProviderCreateWithData(image.get(), pixels, size, releaseImageBuffer);
    if (provider == nullptr) {
        return nullptr;
    }
    image.release();
    CGColorSpaceRef colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    CGImageRef result = CGImageCreate(width,
                                      height,
                                      kBitsPerComponent,
                                      kBitsPerPixel,
                                      static_cast<size_t>(width) * kBytesPerPixel,
                                      colorSpace,
                                      static_cast<CGBitmapInfo>(kCGImageAlphaLast),
                                      provider,
                                      nullptr,
                                      false,
                                      kCGRenderingIntentDefault);
    CGColorSpaceRelease(colorSpace);
    CGDataProviderRelease(provider);
    return result;
}

std::optional<std::vector<Pvr::Color>> siblingPalette(NSURL *_Nullable url) {
    if (url == nil || !url.isFileURL) {
        return std::nullopt;
    }
    for (NSString *extension in @[ @"pvp", @"PVP" ]) {
        NSURL *candidate = [url.URLByDeletingPathExtension URLByAppendingPathExtension:extension];
        NSData *data = [NSData dataWithContentsOfURL:candidate];
        if (data == nil) {
            continue;
        }
        auto palette = Pvr::decodePalette(bytesOf(data));
        if (palette) {
            return std::move(*palette);
        }
        return std::nullopt;
    }
    return std::nullopt;
}

NSString *describe(const Pvr::DecodeResult &result, const Pvr::Image &image, bool sheet) {
    const auto container = std::string(Pvr::containerName(result.container));
    if (!sheet) {
        return [NSString stringWithFormat:@"%s · %u×%u %s",
                                          container.c_str(),
                                          image.width,
                                          image.height,
                                          image.format.c_str()];
    }
    NSString *text =
        [NSString stringWithFormat:@"%s · %zu textures", container.c_str(), result.images.size()];
    if (!result.failures.empty()) {
        text = [text stringByAppendingFormat:@" (%zu unreadable)", result.failures.size()];
    }
    return text;
}

} // namespace

CGImageRef QPCreateImage(NSData *data, NSURL *url, NSString **summary, NSError **error) {
    Pvr::Options options;
    options.palette = siblingPalette(url);
    auto result = Pvr::decode(bytesOf(data), options);
    if (!result) {
        if (error != nullptr) {
            *error = makeError(QPErrorCodeDecodingFailed, @(result.error().message.c_str()));
        }
        return nullptr;
    }
    const bool sheet =
        result->images.size() != 1 || result->container == Pvr::Container::DreamcastPack;
    auto image = sheet ? Pvr::contactSheet(result->images, Pvr::kDefaultSheetPadding, options) :
                         std::expected<Pvr::Image, Pvr::Error>(std::move(result->images.front()));
    if (!image) {
        if (error != nullptr) {
            *error = makeError(QPErrorCodeDecodingFailed, @(image.error().message.c_str()));
        }
        return nullptr;
    }
    if (summary != nullptr) {
        *summary = describe(*result, *image, sheet);
    }
    CGImageRef cgImage = createCGImage(std::move(*image));
    if (cgImage == nullptr && error != nullptr) {
        *error = makeError(QPErrorCodeImageCreationFailed, @"Could not create an image.");
    }
    return cgImage;
}

CGImageRef QPCreateUpscaledImage(CGImageRef image, size_t minimumSide) {
    const size_t width = CGImageGetWidth(image);
    const size_t height = CGImageGetHeight(image);
    const size_t longest = std::max(width, height);
    if (longest == 0 || longest >= minimumSide) {
        return CGImageRetain(image);
    }
    const size_t factor = std::min(kMaxUpscaleFactor, minimumSide / longest);
    if (factor < 2) {
        return CGImageRetain(image);
    }
    CGColorSpaceRef colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    CGContextRef context = CGBitmapContextCreate(nullptr,
                                                 width * factor,
                                                 height * factor,
                                                 kBitsPerComponent,
                                                 0,
                                                 colorSpace,
                                                 kCGImageAlphaPremultipliedLast);
    CGColorSpaceRelease(colorSpace);
    if (context == nullptr) {
        return CGImageRetain(image);
    }
    CGContextSetInterpolationQuality(context, kCGInterpolationNone);
    CGContextDrawImage(
        context,
        CGRectMake(
            0, 0, static_cast<CGFloat>(width * factor), static_cast<CGFloat>(height * factor)),
        image);
    CGImageRef result = CGBitmapContextCreateImage(context);
    CGContextRelease(context);
    return result;
}

NSData *QPCreatePNGData(CGImageRef image) {
    NSMutableData *data = [NSMutableData data];
    CGImageDestinationRef destination = CGImageDestinationCreateWithData(
        (__bridge CFMutableDataRef)data, (__bridge CFStringRef)UTTypePNG.identifier, 1, nullptr);
    if (destination == nullptr) {
        return nil;
    }
    CGImageDestinationAddImage(destination, image, nullptr);
    const bool finalized = CGImageDestinationFinalize(destination);
    CFRelease(destination);
    return finalized ? data : nil;
}
