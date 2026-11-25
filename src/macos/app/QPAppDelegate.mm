#import "QPAppDelegate.h"

@implementation QPAppDelegate

#pragma mark - NSApplicationDelegate

- (void)applicationWillFinishLaunching:(NSNotification *)notification {
    [self buildMainMenu];
}

- (BOOL)applicationShouldOpenUntitledFile:(NSApplication *)sender {
    return NO;
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    // Wait one run-loop turn so documents passed at launch are open before checking.
    dispatch_async(dispatch_get_main_queue(), ^{
      if (NSDocumentController.sharedDocumentController.documents.count == 0) {
          [self showWelcome];
      }
    });
}

#pragma mark - Private

- (void)showWelcome {
    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = @"QuickPVR is installed";
    alert.informativeText =
        @"Finder now shows previews and thumbnails of PowerVR (.pvr) and Sega Dreamcast textures "
        @"and texture packs. If they do not appear, enable the QuickPVR extensions in System "
        @"Settings > General > Login Items & Extensions > Quick Look, or in System Settings > "
        @"Privacy & Security > Extensions on older systems.\n\nYou can also open .pvr files here.";
    [alert addButtonWithTitle:@"Open a File…"];
    [alert addButtonWithTitle:@"Quit"];
    if ([alert runModal] == NSAlertFirstButtonReturn) {
        [NSDocumentController.sharedDocumentController openDocument:nil];
    } else {
        [NSApp terminate:nil];
    }
}

- (void)buildMainMenu {
    NSString *name = NSProcessInfo.processInfo.processName;
    NSMenu *mainMenu = [[NSMenu alloc] init];

    NSMenu *appMenu = [[NSMenu alloc] initWithTitle:name];
    [appMenu addItemWithTitle:[@"About " stringByAppendingString:name]
                       action:@selector(orderFrontStandardAboutPanel:)
                keyEquivalent:@""];
    [appMenu addItem:NSMenuItem.separatorItem];
    [appMenu addItemWithTitle:[@"Hide " stringByAppendingString:name]
                       action:@selector(hide:)
                keyEquivalent:@"h"];
    [appMenu addItemWithTitle:[@"Quit " stringByAppendingString:name]
                       action:@selector(terminate:)
                keyEquivalent:@"q"];
    [mainMenu addItemWithTitle:name action:nil keyEquivalent:@""].submenu = appMenu;

    NSMenu *fileMenu = [[NSMenu alloc] initWithTitle:@"File"];
    [fileMenu addItemWithTitle:@"Open…" action:@selector(openDocument:) keyEquivalent:@"o"];
    NSMenuItem *recentItem = [fileMenu addItemWithTitle:@"Open Recent"
                                                 action:nil
                                          keyEquivalent:@""];
    recentItem.submenu = [[NSMenu alloc] initWithTitle:@"Open Recent"];
    [recentItem.submenu addItemWithTitle:@"Clear Menu"
                                  action:@selector(clearRecentDocuments:)
                           keyEquivalent:@""];
    [fileMenu addItem:NSMenuItem.separatorItem];
    [fileMenu addItemWithTitle:@"Close" action:@selector(performClose:) keyEquivalent:@"w"];
    [mainMenu addItemWithTitle:@"File" action:nil keyEquivalent:@""].submenu = fileMenu;

    NSMenu *windowMenu = [[NSMenu alloc] initWithTitle:@"Window"];
    [windowMenu addItemWithTitle:@"Minimize"
                          action:@selector(performMiniaturize:)
                   keyEquivalent:@"m"];
    [windowMenu addItemWithTitle:@"Zoom" action:@selector(performZoom:) keyEquivalent:@""];
    [mainMenu addItemWithTitle:@"Window" action:nil keyEquivalent:@""].submenu = windowMenu;
    NSApp.windowsMenu = windowMenu;

    NSApp.mainMenu = mainMenu;
}

@end
