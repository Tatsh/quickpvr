#include <algorithm>
#include <cmath>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <QuickLook/QuickLook.h>

#import "QPImageRendering.h"

extern "C" OSStatus GenerateThumbnailForURL(void *thisInterface,
                                            QLThumbnailRequestRef thumbnail,
                                            CFURLRef url,
                                            CFStringRef contentTypeUTI,
                                            CFDictionaryRef options,
                                            CGSize maxSize);
extern "C" void CancelThumbnailGeneration(void *thisInterface, QLThumbnailRequestRef thumbnail);

extern "C" OSStatus GenerateThumbnailForURL(void *,
                                            QLThumbnailRequestRef thumbnail,
                                            CFURLRef url,
                                            CFStringRef,
                                            CFDictionaryRef,
                                            CGSize maxSize) {
    @autoreleasepool {
        NSURL *fileURL = (__bridge NSURL *)url;
        NSData *data = [NSData dataWithContentsOfURL:fileURL
                                             options:NSDataReadingMappedIfSafe
                                               error:nil];
        if (data == nil || QLThumbnailRequestIsCancelled(thumbnail)) {
            return noErr;
        }
        CGImageRef image = QPCreateImage(data, fileURL, nil, nil);
        if (image == nullptr) {
            return noErr;
        }
        const CGFloat width = CGImageGetWidth(image);
        const CGFloat height = CGImageGetHeight(image);
        const CGFloat scale = std::min(maxSize.width / width, maxSize.height / height);
        const CGSize size = CGSizeMake(std::max<CGFloat>(1, std::floor(width * scale)),
                                       std::max<CGFloat>(1, std::floor(height * scale)));
        CGContextRef context = QLThumbnailRequestCreateContext(thumbnail, size, true, nullptr);
        if (context != nullptr) {
            CGContextSetInterpolationQuality(
                context, scale > 1 ? kCGInterpolationNone : kCGInterpolationHigh);
            CGContextDrawImage(context, CGRectMake(0, 0, size.width, size.height), image);
            QLThumbnailRequestFlushContext(thumbnail, context);
            CGContextRelease(context);
        }
        CGImageRelease(image);
    }
    return noErr;
}

extern "C" void CancelThumbnailGeneration(void *, QLThumbnailRequestRef) {
}
