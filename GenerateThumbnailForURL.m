/*******************************************************************************
  Copyright (c) 2009, Limbic Software, Inc.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:
      * Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
      * Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.
      * Neither the name of the Limbic Software, Inc. nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY LIMBIC SOFTWARE, INC. ''AS IS'' AND ANY
  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL LIMBIC SOFTWARE, INC. BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ******************************************************************************/
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <QuickLook/QuickLook.h>

#include "pvr.h"
#include <Cocoa/Cocoa.h>

extern "C" OSStatus GenerateThumbnailForURL(void *thisInterface,
                                            QLThumbnailRequestRef thumbnail,
                                            CFURLRef url,
                                            CFStringRef contentTypeUTI,
                                            CFDictionaryRef options,
                                            CGSize maxSize);
extern "C" void CancelThumbnailGeneration(void *thisInterface, QLThumbnailRequestRef thumbnail);

/* -----------------------------------------------------------------------------
    Generate a thumbnail for file

   This function's job is to create thumbnail for designated file as fast as possible
   ----------------------------------------------------------------------------- */

extern "C" OSStatus GenerateThumbnailForURL(void *thisInterface,
                                            QLThumbnailRequestRef thumbnail,
                                            CFURLRef url,
                                            CFStringRef contentTypeUTI,
                                            CFDictionaryRef options,
                                            CGSize maxSize) {
    @autoreleasepool {
        (void)thisInterface;
        (void)contentTypeUTI;
        (void)options;
        (void)maxSize;

    // Read the PVR file
    PVRTexture pvr;

    NSString *targetCFS = [[(__bridge NSURL *)url absoluteURL] path];
    int res = pvr.load(targetCFS.UTF8String);
    if (res != PVR_LOAD_OKAY && res != PVR_LOAD_UNKNOWN_TYPE) {
        return noErr;
    }

    // create the render context
    NSSize canvasSize = NSMakeSize(pvr.width, pvr.height);
    CGContextRef cgContext = QLThumbnailRequestCreateContext(thumbnail, *(CGSize *)&canvasSize, false, NULL);
    if (cgContext) {
        NSGraphicsContext *context = [NSGraphicsContext graphicsContextWithGraphicsPort:(void *)cgContext flipped:NO];

        if (context) {
            [NSGraphicsContext saveGraphicsState];
            [NSGraphicsContext setCurrentContext:context];
            [context saveGraphicsState];

            unsigned int w = pvr.width;
            unsigned int h = pvr.height;
            if (pvr.data) {
                uint8_t *buffer = pvr.data;

                CGDataProviderRef provider = CGDataProviderCreateWithData(NULL, buffer, (w * h * 4), NULL);

                unsigned int bitsPerComponent = 8;
                unsigned int bitsPerPixel = 32;
                unsigned int bytesPerRow = 4 * w;
                CGColorSpaceRef colorSpaceRef = CGColorSpaceCreateDeviceRGB();
                CGBitmapInfo bitmapInfo = kCGBitmapByteOrderDefault | kCGImageAlphaLast;
                CGColorRenderingIntent renderingIntent = kCGRenderingIntentDefault;
                CGImageRef image = CGImageCreate(w,
                                                 h,
                                                 bitsPerComponent,
                                                 bitsPerPixel,
                                                 bytesPerRow,
                                                 colorSpaceRef,
                                                 bitmapInfo,
                                                 provider,
                                                 NULL,
                                                 NO,
                                                 renderingIntent);
                if (pvr.should_flip == true) {
                    CGContextTranslateCTM(cgContext, 0.0f, h);
                    CGContextScaleCTM(cgContext, 1.0f, -1.0f);
                }
                CGContextDrawImage((CGContext *)[context graphicsPort], CGRectMake(0, 0, w - 1, h - 1), image);
                if (pvr.should_flip == true) {
                    CGContextScaleCTM(cgContext, 1.0f, -1.0f);
                    CGContextTranslateCTM(cgContext, 0.0f, -h);
                }
            }
            [context restoreGraphicsState];
            [NSGraphicsContext restoreGraphicsState];
        }

        QLThumbnailRequestFlushContext(thumbnail, cgContext);
        CFRelease(cgContext);
    }

    return noErr;
    }
}

extern "C" void CancelThumbnailGeneration(void *thisInterface, QLThumbnailRequestRef thumbnail) {
    // implement only if supported
    (void)thisInterface;
    (void)thumbnail;
}
