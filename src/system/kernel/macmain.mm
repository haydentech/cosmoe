// macOS main() wrapper
// This helps us define a main() that works on Linux and Mac systems with
// a minimal amount of platform-specific code.

#import <Cocoa/Cocoa.h>
#include <crt_externs.h>
#include <thread>

extern "C" int cosmoe_main(int argc, char** argv);


@interface CosmoeAppDelegate : NSObject <NSApplicationDelegate>
@end

@implementation CosmoeAppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    // Create a minimal menu bar to satisfy macOS requirements
    NSMenu *menubar = [[NSMenu alloc] init];
    NSMenuItem *appMenuItem = [[NSMenuItem alloc] init];
    [menubar addItem:appMenuItem];
    [NSApp setMainMenu:menubar];
    
    NSMenu *appMenu = [[NSMenu alloc] init];
    NSString *appName = [[NSProcessInfo processInfo] processName];
    NSString *quitTitle = [@"Quit " stringByAppendingString:appName];
    NSMenuItem *quitMenuItem = [[NSMenuItem alloc] initWithTitle:quitTitle
                                                          action:@selector(terminate:)
                                                   keyEquivalent:@"q"];
    [appMenu addItem:quitMenuItem];
    [appMenuItem setSubmenu:appMenu];
    
    // Launch Cosmoe event loop in a separate thread so we don't block
    // the main thread that Cocoa uses for its own event loop.
    int argc = *_NSGetArgc();
    char** argv = *_NSGetArgv();
    
    std::thread([argc, argv]{
        cosmoe_main(argc, argv);
    }).detach();
}

@end

extern "C" int mac_main(int argc, char** argv)
{
    static CosmoeAppDelegate *delegate = nil;
    delegate = [[CosmoeAppDelegate alloc] init];
    [NSApplication sharedApplication].delegate = delegate;

    return NSApplicationMain(argc, (const char**)argv);
}




