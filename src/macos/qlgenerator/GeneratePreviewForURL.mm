#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <QuickLook/QuickLook.h>

#import "QPImageRendering.h"

// Small textures are enlarged without smoothing until their longer side is at least this length.
static const size_t kMinimumPreviewSide = 512;

extern "C" OSStatus GeneratePreviewForURL(void *thisInterface,
                                          QLPreviewRequestRef preview,
                                          CFURLRef url,
                                          CFStringRef contentTypeUTI,
                                          CFDictionaryRef options);
extern "C" void CancelPreviewGeneration(void *thisInterface, QLPreviewRequestRef preview);

extern "C" OSStatus GeneratePreviewForURL(
    void *, QLPreviewRequestRef preview, CFURLRef url, CFStringRef, CFDictionaryRef) {
    @autoreleasepool {
        NSURL *fileURL = (__bridge NSURL *)url;
        NSData *data = [NSData dataWithContentsOfURL:fileURL
                                             options:NSDataReadingMappedIfSafe
                                               error:nil];
        if (data == nil || QLPreviewRequestIsCancelled(preview)) {
            return noErr;
        }
        NSString *summary;
        CGImageRef decoded = QPCreateImage(data, fileURL, &summary, nil);
        if (decoded == nullptr) {
            return noErr;
        }
        CGImageRef image = QPCreateUpscaledImage(decoded, kMinimumPreviewSide);
        CGImageRelease(decoded);
        if (image == nullptr) {
            return noErr;
        }
        const CGSize size = CGSizeMake(CGImageGetWidth(image), CGImageGetHeight(image));
        NSDictionary *properties = @{
            (__bridge NSString *)kQLPreviewPropertyDisplayNameKey :
                [NSString stringWithFormat:@"%@ (%@)", fileURL.lastPathComponent, summary],
        };
        CGContextRef context = QLPreviewRequestCreateContext(
            preview, size, true, (__bridge CFDictionaryRef)properties);
        if (context != nullptr) {
            CGContextSetInterpolationQuality(context, kCGInterpolationNone);
            CGContextDrawImage(context, CGRectMake(0, 0, size.width, size.height), image);
            QLPreviewRequestFlushContext(preview, context);
            CGContextRelease(context);
        }
        CGImageRelease(image);
    }
    return noErr;
}

extern "C" void CancelPreviewGeneration(void *, QLPreviewRequestRef) {
}
