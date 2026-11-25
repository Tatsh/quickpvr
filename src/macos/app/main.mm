#import <AppKit/AppKit.h>

#import "QPAppDelegate.h"

int main() {
    @autoreleasepool {
        NSApplication *application = NSApplication.sharedApplication;
        QPAppDelegate *delegate = [[QPAppDelegate alloc] init];
        application.delegate = delegate;
        [application setActivationPolicy:NSApplicationActivationPolicyRegular];
        [application run];
    }
    return 0;
}
