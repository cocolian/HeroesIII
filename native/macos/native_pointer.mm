#include "h3/native_pointer.hpp"
#include <SDL_syswm.h>
#import <Cocoa/Cocoa.h>
#include <cmath>
#include <algorithm>
#include <deque>

namespace h3 {
struct NativePointerInput::Impl {
 id monitor=nil;
 Uint32 type=0,windowId=0;
 float wheelRemainderX=0,wheelRemainderY=0;
 std::deque<NativePointerEvent> pending;
 explicit Impl(SDL_Window*window){
  SDL_SysWMinfo info{};SDL_VERSION(&info.version);
  if(!SDL_GetWindowWMInfo(window,&info)||info.subsystem!=SDL_SYSWM_COCOA||!info.info.cocoa.window)return;
  NSWindow*ownWindow=info.info.cocoa.window;windowId=SDL_GetWindowID(window);
  type=SDL_RegisterEvents(1);if(type==Uint32(-1)){type=0;return;}
  NSEventMask mask=NSEventMaskMouseMoved|NSEventMaskLeftMouseDown|NSEventMaskLeftMouseUp|NSEventMaskLeftMouseDragged|NSEventMaskRightMouseDown|NSEventMaskRightMouseUp|NSEventMaskRightMouseDragged|NSEventMaskOtherMouseDown|NSEventMaskOtherMouseUp|NSEventMaskOtherMouseDragged|NSEventMaskScrollWheel;
  Impl*self=this;
  monitor=[NSEvent addLocalMonitorForEventsMatchingMask:mask handler:^NSEvent*(NSEvent*event){
   if(event.window!=ownWindow)return event;
   NSView*view=ownWindow.contentView;if(!view)return event;
   NSPoint point=[view convertPoint:event.locationInWindow fromView:nil];NSRect bounds=view.bounds;
   double x=point.x-bounds.origin.x,y=point.y-bounds.origin.y;if(!view.isFlipped)y=bounds.size.height-y;
   SDL_Event raw{};raw.common.timestamp=SDL_GetTicks();
   bool down=event.type==NSEventTypeLeftMouseDown||event.type==NSEventTypeRightMouseDown||event.type==NSEventTypeOtherMouseDown;
   bool up=event.type==NSEventTypeLeftMouseUp||event.type==NSEventTypeRightMouseUp||event.type==NSEventTypeOtherMouseUp;
   if(down&&(x<0||y<0||x>=bounds.size.width||y>=bounds.size.height))return event;
   if(event.type==NSEventTypeScrollWheel){
    raw.type=SDL_MOUSEWHEEL;raw.wheel.windowID=self->windowId;
    // Match SDL's Cocoa axes and preserve AppKit's system scroll direction.
    float wheelX=-event.deltaX,wheelY=event.deltaY;
    if(!event.hasPreciseScrollingDeltas){wheelX=wheelX>0?std::ceil(wheelX):std::floor(wheelX);wheelY=wheelY>0?std::ceil(wheelY):std::floor(wheelY);}
    auto integral=[](float delta,float&remainder){if(delta*remainder<0)remainder=0;remainder+=delta;int value=int(remainder);remainder-=value;return value;};
    raw.wheel.preciseX=wheelX;raw.wheel.preciseY=wheelY;
    raw.wheel.x=integral(wheelX,self->wheelRemainderX);raw.wheel.y=integral(wheelY,self->wheelRemainderY);
    raw.wheel.direction=event.isDirectionInvertedFromDevice?SDL_MOUSEWHEEL_FLIPPED:SDL_MOUSEWHEEL_NORMAL;
    raw.wheel.mouseX=int(std::floor(x));raw.wheel.mouseY=int(std::floor(y));
   }
   else if(down||up){raw.type=down?SDL_MOUSEBUTTONDOWN:SDL_MOUSEBUTTONUP;raw.button.windowID=self->windowId;raw.button.button=event.buttonNumber==0?SDL_BUTTON_LEFT:event.buttonNumber==1?SDL_BUTTON_RIGHT:Uint8(event.buttonNumber+1);raw.button.state=down?SDL_PRESSED:SDL_RELEASED;raw.button.clicks=Uint8(std::min<NSInteger>(255,event.clickCount));raw.button.x=int(std::floor(x));raw.button.y=int(std::floor(y));}
   else{raw.type=SDL_MOUSEMOTION;raw.motion.windowID=self->windowId;raw.motion.x=int(std::floor(x));raw.motion.y=int(std::floor(y));raw.motion.state=event.type==NSEventTypeLeftMouseDragged?SDL_BUTTON_LMASK:0;}
   auto flags=event.modifierFlags;int mods=KMOD_NONE;if(flags&NSEventModifierFlagShift)mods|=KMOD_SHIFT;if(flags&NSEventModifierFlagControl)mods|=KMOD_CTRL;if(flags&NSEventModifierFlagOption)mods|=KMOD_ALT;if(flags&NSEventModifierFlagCommand)mods|=KMOD_GUI;if(flags&NSEventModifierFlagCapsLock)mods|=KMOD_CAPS;
   self->pending.push_back({raw,SDL_Keymod(mods)});SDL_Event queued{};queued.type=self->type;queued.user.windowID=self->windowId;
   if(SDL_PushEvent(&queued)<=0)self->pending.pop_back();
   return event;
  }];
 }
 ~Impl(){if(monitor)[NSEvent removeMonitor:monitor];}
};
NativePointerInput::NativePointerInput(SDL_Window*window):impl_(std::make_unique<Impl>(window)){}
NativePointerInput::~NativePointerInput()=default;
bool NativePointerInput::enabled()const{return impl_->monitor!=nil;}
Uint32 NativePointerInput::eventType()const{return impl_->type;}
std::optional<NativePointerEvent>NativePointerInput::take(){if(impl_->pending.empty())return {};auto event=impl_->pending.front();impl_->pending.pop_front();return event;}
}
