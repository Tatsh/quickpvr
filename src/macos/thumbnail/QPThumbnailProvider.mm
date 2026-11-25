#import "QPThumbnailProvider.h"

#include <algorithm>
#include <cmath>

#import <AppKit/AppKit.h>

#import "QPImageRendering.h"

@implementation QPThumbnailProvider

- (void)provideThumbnailForFileRequest:(QLFileThumbnailRequest *)request
                     completionHandler:(void (^)(QLThumbnailReply *, NSError *))handler {
    NSError *error;
    NSData *data = [NSData dataWithContentsOfURL:request.fileURL
                                         options:NSDataReadingMappedIfSafe
                                           error:&error];
    if (data == nil) {
        handler(nil, error);
        return;
    }
    CGImageRef image = QPCreateImage(data, request.fileURL, nil, &error);
    if (image == nullptr) {
        handler(nil, error);
        return;
    }
    const CGFloat width = CGImageGetWidth(image);
    const CGFloat height = CGImageGetHeight(image);
    const CGSize bounds = request.maximumSize;
    // Fill the thumbnail by shrinking smoothly or enlarging without smoothing.
    const CGFloat scale = std::min(bounds.width / width, bounds.height / height);
    const CGSize size = CGSizeMake(std::max<CGFloat>(1, std::floor(width * scale)),
                                   std::max<CGFloat>(1, std::floor(height * scale)));
    id retainedImage = CFBridgingRelease(image);
    handler([QLThumbnailReply replyWithContextSize:size
                        currentContextDrawingBlock:^BOOL {
                          CGContextRef context = NSGraphicsContext.currentContext.CGContext;
                          if (context == nullptr) {
                              return NO;
                          }
                          CGContextSetInterpolationQuality(
                              context, scale > 1 ? kCGInterpolationNone : kCGInterpolationHigh);
                          CGContextDrawImage(context,
                                             CGRectMake(0, 0, size.width, size.height),
                                             (__bridge CGImageRef)retainedImage);
                          return YES;
                        }],
            nil);
}

@end
