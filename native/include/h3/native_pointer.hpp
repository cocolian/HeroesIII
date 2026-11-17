#pragma once
#include <SDL.h>
#include <memory>
#include <optional>

namespace h3 {
struct NativePointerEvent {SDL_Event event{};SDL_Keymod modifiers=KMOD_NONE;};
// A local source for this application's own Cocoa pointer and scroll events. A private
// SDL event preserves ordering with keyboard, focus and window events.
class NativePointerInput {
public:
 explicit NativePointerInput(SDL_Window*);
 ~NativePointerInput();
 bool enabled()const;
 Uint32 eventType()const;
 std::optional<NativePointerEvent> take();
private:
 struct Impl;
 std::unique_ptr<Impl> impl_;
};
}
