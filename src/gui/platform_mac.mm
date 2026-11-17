// macOS integration for the dictation island: notch geometry, window level
// above the menu bar, transparent rounded corners and focus hand-back.
#include "gui/platform.h"

#import <AppKit/AppKit.h>
#import <QuartzCore/QuartzCore.h>

#include <SDL2/SDL_syswm.h>

namespace gui::platform {

namespace {

NSRunningApplication *g_previous_app = nil;

NSWindow *ns_window(SDL_Window *window) {
  SDL_SysWMinfo wm;
  SDL_VERSION(&wm.version);
  if (!SDL_GetWindowWMInfo(window, &wm) || wm.subsystem != SDL_SYSWM_COCOA) {
    return nil;
  }
  return wm.info.cocoa.window;
}

NSScreen *island_screen() {
  const NSPoint mouse = [NSEvent mouseLocation];
  for (NSScreen *screen in [NSScreen screens]) {
    if (NSMouseInRect(mouse, screen.frame, NO)) {
      return screen;
    }
  }
  return [NSScreen mainScreen];
}

// Height of the primary display, used to flip between Cocoa (bottom-left)
// and SDL (top-left) coordinates.
CGFloat primary_height() {
  NSArray<NSScreen *> *screens = [NSScreen screens];
  return screens.count > 0 ? screens[0].frame.size.height : 0;
}

} // namespace

ScreenInfo screen_for_island() {
  ScreenInfo info;
  @autoreleasepool {
    NSScreen *screen = island_screen();
    if (screen == nil) {
      return info;
    }
    const NSRect frame = screen.frame;
    const NSRect visible = screen.visibleFrame;
    info.x = static_cast<int>(frame.origin.x);
    info.y = static_cast<int>(primary_height() - NSMaxY(frame));
    info.w = static_cast<int>(frame.size.width);
    info.h = static_cast<int>(frame.size.height);
    info.top_inset = static_cast<int>(NSMaxY(frame) - NSMaxY(visible));

    if (@available(macOS 12.0, *)) {
      const NSEdgeInsets safe = screen.safeAreaInsets;
      if (safe.top > 0) {
        const NSRect left = screen.auxiliaryTopLeftArea;
        const NSRect right = screen.auxiliaryTopRightArea;
        const CGFloat notch = frame.size.width - left.size.width - right.size.width;
        if (notch > 0 && notch < frame.size.width * 0.5) {
          info.has_notch = true;
          info.notch_w = static_cast<int>(notch);
          info.notch_h = static_cast<int>(safe.top);
        }
      }
    }
  }
  return info;
}

bool configure_island_window(SDL_Window *window) {
  @autoreleasepool {
    NSWindow *win = ns_window(window);
    if (win == nil) {
      return false;
    }
    // Above the menu bar so the island can grow out of the notch.
    win.level = NSStatusWindowLevel + 1;
    win.collectionBehavior = NSWindowCollectionBehaviorCanJoinAllSpaces |
                             NSWindowCollectionBehaviorStationary |
                             NSWindowCollectionBehaviorFullScreenAuxiliary |
                             NSWindowCollectionBehaviorIgnoresCycle;
    win.hasShadow = NO;
    win.opaque = NO;
    win.backgroundColor = [NSColor clearColor];
    win.hidesOnDeactivate = NO;
    win.movable = NO;

    NSView *view = win.contentView;
    view.wantsLayer = YES;
    view.layer.backgroundColor = [NSColor clearColor].CGColor;
    view.layer.opaque = NO;
    view.layer.masksToBounds = YES;
    for (NSView *sub in view.subviews) {
      sub.wantsLayer = YES;
      sub.layer.opaque = NO; // CAMetalLayer / GL layer: keep alpha
    }
    return true;
  }
}

void place_island(SDL_Window *window, int x, int y, int w, int h, float radius,
                  bool round_all_corners) {
  @autoreleasepool {
    NSWindow *win = ns_window(window);
    if (win == nil) {
      SDL_SetWindowSize(window, w, h);
      SDL_SetWindowPosition(window, x, y);
      return;
    }
    // setFrame: is not constrained to below the menu bar for this level.
    const NSRect frame = NSMakeRect(x, primary_height() - y - h, w, h);
    if (!NSEqualRects(frame, win.frame)) {
      [win setFrame:frame display:NO animate:NO];
    }
    CALayer *layer = win.contentView.layer;
    layer.cornerRadius = radius;
    if (@available(macOS 10.13, *)) {
      // Attached to the notch: round only the bottom corners so the top edge
      // melts into the camera housing. Floating: round all four.
      layer.maskedCorners = round_all_corners
                                ? (kCALayerMinXMinYCorner | kCALayerMaxXMinYCorner |
                                   kCALayerMinXMaxYCorner | kCALayerMaxXMaxYCorner)
                                : (kCALayerMinXMinYCorner | kCALayerMaxXMinYCorner);
    }
  }
}

void remember_focus() {
  @autoreleasepool {
    NSRunningApplication *front = [[NSWorkspace sharedWorkspace] frontmostApplication];
    if (front != nil && ![front isEqual:[NSRunningApplication currentApplication]]) {
      g_previous_app = front;
    }
  }
}

void restore_focus() {
  @autoreleasepool {
    if (g_previous_app != nil && !g_previous_app.terminated && !g_previous_app.active) {
      [g_previous_app activateWithOptions:0];
    }
  }
}

} // namespace gui::platform
