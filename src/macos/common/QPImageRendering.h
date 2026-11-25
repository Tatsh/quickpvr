#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/** Error domain of QPCreateImage(). */
extern NSErrorDomain const QPErrorDomain;

/** Error codes in QPErrorDomain. */
typedef NS_ENUM(NSInteger, QPErrorCode) {
    QPErrorCodeDecodingFailed = 1,      /*!< The texture could not be decoded. */
    QPErrorCodeImageCreationFailed = 2, /*!< Core Graphics could not create the image. */
    QPErrorCodeEncodingFailed = 3,      /*!< The image could not be encoded. */
};

/**
 * Decode PVR texture data into an image. A texture pack becomes a contact sheet of all its
 * textures.
 *
 * @param data The file contents.
 * @param url Location of the file. When it is readable, a `.pvp` palette with the same name is used
 * for palettised Dreamcast textures.
 * @param summary Receives a one-line description such as `Dreamcast PVR · 256×256 RGB565, VQ`.
 * @param error Receives the reason when decoding fails.
 * @return A new image the caller must release, or `NULL` on failure.
 */
CGImageRef _Nullable QPCreateImage(NSData *data,
                                   NSURL *_Nullable url,
                                   NSString *_Nullable *_Nullable summary,
                                   NSError *_Nullable *_Nullable error) CF_RETURNS_RETAINED;

/**
 * Enlarge an image by a whole factor, without smoothing, until its longer side is at least
 * @p minimumSide pixels.
 *
 * @return A new image, or a retained @p image when no enlargement is needed.
 */
CGImageRef _Nullable QPCreateUpscaledImage(CGImageRef image,
                                           size_t minimumSide) CF_RETURNS_RETAINED;

/**
 * Encode an image as PNG.
 *
 * @return The PNG data, or `nil` when encoding fails.
 */
NSData *_Nullable QPCreatePNGData(CGImageRef image);

NS_ASSUME_NONNULL_END

// code: language=Objective-C++
// kate: hl Objective-C++;
// vim: set ft=objcpp :
