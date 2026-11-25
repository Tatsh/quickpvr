#import "QPDocument.h"

#import "QPImageRendering.h"

// Layout of the document window, in points.
static const CGFloat kWindowMargin = 8;
static const CGFloat kLabelSpacing = 6;
static const CGFloat kLabelMargin = 10;
static const CGFloat kLabelAreaHeight = 40;
static const CGFloat kMinimumWindowWidth = 320;
static const CGFloat kMinimumContentWidth = 160;
static const CGFloat kMinimumContentHeight = 120;
static const CGFloat kMaximumScreenFraction = 0.9;
// Textures smaller than this open at twice their size.
static const CGFloat kSmallTextureSide = 256;

@interface QPDocument ()
@property(nonatomic, nullable) NSImage *image;
@property(nonatomic, copy, nullable) NSString *summary;
@end

@implementation QPDocument

+ (BOOL)autosavesInPlace {
    return NO;
}

#pragma mark - Reading

- (BOOL)readFromData:(NSData *)data ofType:(NSString *)typeName error:(NSError **)outError {
    NSString *summary;
    CGImageRef cgImage = QPCreateImage(data, self.fileURL, &summary, outError);
    if (cgImage == nullptr) {
        return NO;
    }
    const NSSize pixels = NSMakeSize(CGImageGetWidth(cgImage), CGImageGetHeight(cgImage));
    self.image = [[NSImage alloc] initWithCGImage:cgImage size:pixels];
    CGImageRelease(cgImage);
    self.summary = summary;
    return YES;
}

#pragma mark - Window

- (NSView *)makeContentView {
    NSImageView *imageView = [NSImageView imageViewWithImage:self.image ?: [[NSImage alloc] init]];
    imageView.imageScaling = NSImageScaleProportionallyUpOrDown;
    imageView.imageFrameStyle = NSImageFrameNone;
    imageView.translatesAutoresizingMaskIntoConstraints = NO;
    [imageView setContentCompressionResistancePriority:NSLayoutPriorityDefaultLow
                                        forOrientation:NSLayoutConstraintOrientationHorizontal];
    [imageView setContentCompressionResistancePriority:NSLayoutPriorityDefaultLow
                                        forOrientation:NSLayoutConstraintOrientationVertical];

    NSTextField *label = [NSTextField labelWithString:self.summary ?: @""];
    label.textColor = NSColor.secondaryLabelColor;
    label.font = [NSFont systemFontOfSize:NSFont.smallSystemFontSize];
    label.lineBreakMode = NSLineBreakByTruncatingMiddle;
    label.translatesAutoresizingMaskIntoConstraints = NO;

    NSView *content = [[NSView alloc] init];
    [content addSubview:imageView];
    [content addSubview:label];
    [NSLayoutConstraint activateConstraints:@[
        [imageView.topAnchor constraintEqualToAnchor:content.topAnchor constant:kWindowMargin],
        [imageView.leadingAnchor constraintEqualToAnchor:content.leadingAnchor
                                                constant:kWindowMargin],
        [imageView.trailingAnchor constraintEqualToAnchor:content.trailingAnchor
                                                 constant:-kWindowMargin],
        [label.topAnchor constraintEqualToAnchor:imageView.bottomAnchor constant:kLabelSpacing],
        [label.leadingAnchor constraintEqualToAnchor:content.leadingAnchor constant:kLabelMargin],
        [label.trailingAnchor constraintLessThanOrEqualToAnchor:content.trailingAnchor
                                                       constant:-kLabelMargin],
        [label.bottomAnchor constraintEqualToAnchor:content.bottomAnchor constant:-kWindowMargin],
    ]];
    return content;
}

- (void)makeWindowControllers {
    const NSSize pixels = self.image.size;
    const CGFloat zoom = MAX(pixels.width, pixels.height) < kSmallTextureSide ? 2 : 1;
    const NSRect visible = NSScreen.mainScreen.visibleFrame;
    const NSSize size = NSMakeSize(
        MIN(MAX(pixels.width * zoom + 2 * kWindowMargin, kMinimumWindowWidth),
            visible.size.width * kMaximumScreenFraction),
        MIN(pixels.height * zoom + kLabelAreaHeight, visible.size.height * kMaximumScreenFraction));
    NSWindow *window = [[NSWindow alloc]
        initWithContentRect:NSMakeRect(0, 0, size.width, size.height)
                  styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                            NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable
                    backing:NSBackingStoreBuffered
                      defer:YES];
    window.contentView = [self makeContentView];
    window.contentMinSize = NSMakeSize(kMinimumContentWidth, kMinimumContentHeight);
    window.tabbingMode = NSWindowTabbingModePreferred;
    [window center];
    [self addWindowController:[[NSWindowController alloc] initWithWindow:window]];
}

@end
