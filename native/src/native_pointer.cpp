#include "h3/native_pointer.hpp"
namespace h3 {
struct NativePointerInput::Impl{};
NativePointerInput::NativePointerInput(SDL_Window*):impl_(std::make_unique<Impl>()){}
NativePointerInput::~NativePointerInput()=default;
bool NativePointerInput::enabled()const{return false;}
Uint32 NativePointerInput::eventType()const{return 0;}
std::optional<NativePointerEvent>NativePointerInput::take(){return {};}
}
