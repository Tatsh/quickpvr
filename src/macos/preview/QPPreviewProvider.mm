#import "QPPreviewProvider.h"

#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import "QPImageRendering.h"

// Small textures are enlarged without smoothing until their longer side is at least this length.
static const size_t kMinimumPreviewSide = 512;

@implementation QPPreviewProvider

- (void)providePreviewForFileRequest:(QLFilePreviewRequest *)request
                   completionHandler:(void (^)(QLPreviewReply *, NSError *))handler {
    NSError *error;
    NSData *data = [NSData dataWithContentsOfURL:request.fileURL
                                         options:NSDataReadingMappedIfSafe
                                           error:&error];
    if (data == nil) {
        handler(nil, error);
        return;
    }
    NSString *summary;
    CGImageRef decoded = QPCreateImage(data, request.fileURL, &summary, &error);
    if (decoded == nullptr) {
        handler(nil, error);
        return;
    }
    CGImageRef image = QPCreateUpscaledImage(decoded, kMinimumPreviewSide);
    CGImageRelease(decoded);
    NSData *png = image != nullptr ? QPCreatePNGData(image) : nil;
    if (png == nil) {
        CGImageRelease(image);
        handler(nil,
                [NSError errorWithDomain:QPErrorDomain
                                    code:QPErrorCodeEncodingFailed
                                userInfo:@{NSLocalizedDescriptionKey : @"Could not encode PNG."}]);
        return;
    }
    const CGSize size = CGSizeMake(CGImageGetWidth(image), CGImageGetHeight(image));
    CGImageRelease(image);
    QLPreviewReply *reply =
        [[QLPreviewReply alloc] initWithDataOfContentType:UTTypePNG
                                              contentSize:size
                                        dataCreationBlock:^NSData *(QLPreviewReply *, NSError **) {
                                          return png;
                                        }];
    reply.title =
        [NSString stringWithFormat:@"%@ (%@)", request.fileURL.lastPathComponent, summary];
    handler(reply, nil);
}

@end
