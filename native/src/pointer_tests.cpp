#include "h3/renderer.hpp"
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace h3 {
void Desktop::pointerPreview(const std::filesystem::path&directory){
 std::filesystem::create_directories(directory);saveDir_=directory/"test-saves";
 pointerRenderTests(directory);touchRenderTests(directory);
 std::cout<<"Trackpad, native pointer and touch input passed\n";
}
void Desktop::pointerRenderTests(const std::filesystem::path&directory){
 auto check=[](bool ok,const char*message){if(!ok)throw std::runtime_error(message);};
 auto raw=[this](Point p){int w=0,h=0;SDL_GetWindowSize(window_,&w,&h);auto layout=displayLayout(w,h,w,h,scene==Scene::Adventure);return Point{int(std::lround(layout.pointX+(p.x+.5)*layout.pointScale)),int(std::lround(layout.pointY+(p.y+.5)*layout.pointScale))};};
 auto pointer=[&](Uint32 type,Point p,SDL_Keymod mods=KMOD_NONE){p=raw(p);SDL_Event e{};e.type=type;e.button.button=SDL_BUTTON_LEFT;e.button.x=p.x;e.button.y=p.y;handleNativeEvent(e,true,mods);};
 auto click=[&](Point p,SDL_Keymod mods=KMOD_NONE){pointer(SDL_MOUSEBUTTONDOWN,p,mods);pointer(SDL_MOUSEBUTTONUP,p,mods);};
 auto shot=[&](const char*name){toastUntil_=0;render();check(screenshot(directory/name),"Native pointer regression capture failed");};
 int w=0,h=0;SDL_GetWindowSize(window_,&w,&h);fixedAnimationTime_=0;battleMotion_.clear();settings_=ClientSettings{};autoCombat_=false;
 for(auto size:{Point{640,480},Point{1280,720},Point{997,751}}){
  SDL_SetWindowSize(window_,size.x,size.y);game=Game::create();scene=Scene::Adventure;camX_=4.5f;camY_=20.5f;zoom_=1;route_.clear();destination_.reset();marching_=false;nextStep_=0;mouseLeftDown_=drag_=false;render();auto state=game.toJson();
  click(mapScreen({4,21}));check(destination_==Point{4,21}&&route_.size()==1&&route_[0].cost==100,"Native map click planned the wrong tile");
  auto control=buttons_.back().rect;auto at=raw({control.x+control.w/2,control.y+control.h/2});SDL_Event help{};help.type=SDL_MOUSEBUTTONDOWN;help.button.button=SDL_BUTTON_RIGHT;help.button.x=at.x;help.button.y=at.y;handleNativeEvent(help,true);render();
  check(rightHelp_&&hintBounds_&&game.toJson()==state&&scene==Scene::Adventure,"Native right click failed help or activated end turn");help.type=SDL_MOUSEBUTTONUP;handleNativeEvent(help,true);render();check(!rightHelp_&&!hintBounds_&&game.toJson()==state,"Native right release retained help or advanced the day");
  pointer(SDL_MOUSEBUTTONDOWN,{450,300});pointer(SDL_MOUSEBUTTONUP,{450,260});check(std::abs(zoom_-std::exp(40.f/180))<.015f&&!drag_&&!marching_&&game.toJson()==state,"Release-only map zoom did not use the final position");
  shot(size.x==640?"pointer-map-small.png":size.x==1280?"pointer-map-wide.png":"pointer-map-fractional.png");
  click(mapScreen({4,21}));tick();check(game.hero.pos==Point{4,21}&&game.hero.movement==1460,"Native planned route did not reach the clicked tile");
  if(size.x==1280){state=game.toJson();SDL_Event e{};e.type=SDL_MOUSEBUTTONDOWN;e.button.button=SDL_BUTTON_LEFT;e.button.x=1;e.button.y=360;handleNativeEvent(e,true);e.type=SDL_MOUSEBUTTONUP;handleNativeEvent(e,true);check(game.toJson()==state&&!drag_,"Border click entered the adventure canvas");}
 }
 SDL_SetWindowSize(window_,1000,750);game=Game::create();scene=Scene::Town;sourceSlot_=-1;render();auto state=game.toJson();
 pointer(SDL_MOUSEBUTTONDOWN,{332,516});pointer(SDL_MOUSEBUTTONUP,{332,418});check(game.hero.army[0].empty()&&game.town.garrison[0].count==28&&!armyDrag_,"Release-only army drag missed the destination");shot("pointer-town-release.png");
 render();pointer(SDL_MOUSEBUTTONDOWN,{332,418});pointer(SDL_MOUSEBUTTONUP,{332,516});check(game.hero.army[0].count==28&&game.town.garrison[0].empty()&&game.resources[Gold]==10000,"Release-only return drag changed unit or gold totals");
 render();state=game.toJson();SDL_SetModState(KMOD_NONE);click({332,516});click({394,418},KMOD_SHIFT);check(scene==Scene::Split&&game.toJson()==state&&splitAmount_==14&&!pointerModifiers_,"Native Shift snapshot was replaced by polled modifier state");shot("pointer-shift-split.png");key(SDLK_ESCAPE,KMOD_NONE);
 render();pointer(SDL_MOUSEBUTTONDOWN,{332,516});SDL_Event resize{};resize.type=SDL_WINDOWEVENT;resize.window.event=SDL_WINDOWEVENT_SIZE_CHANGED;handleNativeEvent(resize);pointer(SDL_MOUSEBUTTONUP,{332,418});check(game.toJson()==state&&!armyDrag_&&!mouseLeftDown_&&pressedButton_==-1,"Resize during dragging committed a transfer");
 scene=Scene::Adventure;openOptions();render();auto track=volumeTrack(0);pointer(SDL_MOUSEBUTTONDOWN,{track.x+2,track.y+track.h/2});pointer(SDL_MOUSEBUTTONUP,{track.x+track.w-2,track.y+track.h/2});check(settings_.music==9&&optionsDragging_<0&&!mouseLeftDown_,"Release-only volume drag ignored its final position");shot("pointer-volume-release.png");key(SDLK_ESCAPE,KMOD_NONE);
 game=Game::create();scene=Scene::Town;openRecruit(Pikeman,Scene::Town);render();track=recruitTrack();pointer(SDL_MOUSEBUTTONDOWN,{track.x+2,track.y+track.h/2});pointer(SDL_MOUSEBUTTONUP,{track.x+track.w-2,track.y+track.h/2});check(recruitAmount_==14&&!recruitDragging_,"Release-only recruitment slider ignored its final position");shot("pointer-recruit-release.png");
 mapGestureRenderTests(directory);
 SDL_SetWindowSize(window_,w,h);game=Game::create();scene=Scene::Town;settings_=ClientSettings{};sourceSlot_=-1;nativePointer_.reset();pointerModifiers_.reset();fixedAnimationTime_=0;
}
void Desktop::mapGestureRenderTests(const std::filesystem::path&directory){
 auto check=[](bool ok,const char*message){if(!ok)throw std::runtime_error(message);};
 auto raw=[&](Point p){int w,h;SDL_GetWindowSize(window_,&w,&h);auto layout=displayLayout(w,h,w,h,scene==Scene::Adventure);return Point{int(std::lround(layout.pointX+(p.x+.5)*layout.pointScale)),int(std::lround(layout.pointY+(p.y+.5)*layout.pointScale))};};
 auto pointer=[&](Uint32 type,Point p){auto r=raw(p);SDL_Event e{};e.type=type;if(type==SDL_MOUSEMOTION){e.motion.x=r.x;e.motion.y=r.y;}else{e.button.button=SDL_BUTTON_LEFT;e.button.x=r.x;e.button.y=r.y;}handleNativeEvent(e,true);};
 auto wheel=[&](Point p,float x,float y,bool precise=true,Uint32 direction=SDL_MOUSEWHEEL_NORMAL){auto r=raw(p);SDL_Event e{};e.type=SDL_MOUSEWHEEL;e.wheel.mouseX=r.x;e.wheel.mouseY=r.y;e.wheel.direction=direction;e.wheel.x=int(x);e.wheel.y=int(y);if(precise){e.wheel.preciseX=x;e.wheel.preciseY=y;}handleNativeEvent(e,true);};
 auto shot=[&](const char*name){toastUntil_=0;mouseX_=mouseY_=-1;render();check(screenshot(directory/name),"Map gesture screenshot failed");};
 auto world=[&](Point p){auto map=adventureMapRect();auto camera=mapCamera();return std::array<float,2>{camera[0]+(p.x-map.x-map.w/2)/(32*zoom_),camera[1]+(p.y-map.y-map.h/2)/(32*zoom_)};};
 auto close=[](float a,float b){return std::abs(a-b)<.002f;};
 nlohmann::json cases=nlohmann::json::array();
 for(auto size:{Point{640,480},Point{1280,720},Point{997,751},Point{1512,982}}){
  SDL_SetWindowSize(window_,size.x,size.y);cancelPointer();game=Game::create();scene=Scene::Adventure;fixedAnimationTime_=1000;settings_=ClientSettings{};
  camX_=MapW/2.f;camY_=MapH/2.f;zoom_=1.6f;marching_=false;destination_=Point{7,21};route_=game.path(*destination_);render();
  auto map=adventureMapRect();Point center{map.x+map.w/2,map.y+map.h/2};auto before=game.toJson();auto destination=destination_;auto routeSize=route_.size();
  float x=camX_,y=camY_;
  if(size.x==1280)shot("gesture-before.png");
  wheel(center,.25f,-.375f);check(close(camX_,x+.25f*24/(32*1.6f))&&close(camY_,y+.375f*24/(32*1.6f))&&zoom_==1.6f,"Fractional diagonal trackpad scroll did not pan both axes without zooming");
  auto moved=std::array<float,2>{camX_,camY_};
  if(size.x==1280)shot("gesture-after-scroll.png");
  wheel(center,-.25f,.375f);check(close(camX_,x)&&close(camY_,y),"Reverse trackpad scroll did not return the camera");
  wheel(center,.25f,-.375f,true,SDL_MOUSEWHEEL_FLIPPED);check(close(camX_,moved[0])&&close(camY_,moved[1]),"System natural-scroll deltas were inverted twice");
  wheel(center,-.25f,.375f);wheel(center,1,-1,false);check(close(camX_,x+24/(32*1.6f))&&close(camY_,y+24/(32*1.6f)),"Integer mouse wheel fallback did not pan");
  wheel(center,-1,1,false);check(close(camX_,x)&&close(camY_,y),"Integer wheel reverse left camera drift");
  auto mini=adventureMinimapRect();wheel({mini.x+mini.w/2,mini.y+mini.h/2},2,-2);wheel({map.x+10,map.y+map.h+10},2,-2);
  check(close(camX_,x)&&close(camY_,y)&&zoom_==1.6f,"Wheel over sidebar or resources moved the adventure map");
  for(auto other:{Scene::Town,Scene::Battle,Scene::Options,Scene::Help}){scene=other;wheel({300,300},1,-1);check(close(camX_,x)&&close(camY_,y)&&zoom_==1.6f,"Wheel changed the adventure camera from a different scene");}scene=Scene::Adventure;
  // Zoom uses the initial drag state, so intermediate motion cannot compound it.
  zoom_=1;camX_=x;camY_=y;render();Point anchor{center.x+36,center.y-18};auto origin=world(anchor);
  pointer(SDL_MOUSEBUTTONDOWN,anchor);wheel(center,1,-1);check(zoom_==1&&close(camX_,x)&&close(camY_,y),"Wheel mixed panning into an active zoom drag");
  pointer(SDL_MOUSEMOTION,{anchor.x,anchor.y-20});pointer(SDL_MOUSEMOTION,{anchor.x,anchor.y-40});pointer(SDL_MOUSEBUTTONUP,{anchor.x,anchor.y-40});
  check(std::abs(zoom_-std::exp(40.f/180))<.015f&&!drag_&&!mouseLeftDown_,"Press-drag failed to zoom from the initial position or retain release capture");
  auto anchored=world(anchor);check(std::abs(origin[0]-anchored[0])<.06f&&std::abs(origin[1]-anchored[1])<.06f,"Zoom lost its original map anchor");
  if(size.x==1280)shot("gesture-after-drag-zoom.png");
  float upZoom=zoom_;pointer(SDL_MOUSEBUTTONDOWN,center);pointer(SDL_MOUSEBUTTONUP,{center.x,center.y+40});check(zoom_<upZoom,"Press-drag down did not zoom out");
  float stableZoom=zoom_;auto stable=mapCamera();pointer(SDL_MOUSEBUTTONDOWN,center);pointer(SDL_MOUSEBUTTONUP,{center.x-35,center.y});check(zoom_==stableZoom&&mapCamera()==stable,"Horizontal press-drag still panned the desktop map");
  pointer(SDL_MOUSEBUTTONDOWN,center);pointer(SDL_MOUSEBUTTONUP,{center.x,center.y-1000});check(zoom_==1.8f,"Zoom exceeded maximum");
  pointer(SDL_MOUSEBUTTONDOWN,center);pointer(SDL_MOUSEBUTTONUP,{center.x,center.y+1000});check(zoom_==.65f,"Zoom exceeded minimum");
  pointer(SDL_MOUSEBUTTONDOWN,center);SDL_Event lost{};lost.type=SDL_WINDOWEVENT;lost.window.event=SDL_WINDOWEVENT_FOCUS_LOST;handleNativeEvent(lost);pointer(SDL_MOUSEBUTTONUP,{center.x,center.y-100});check(zoom_==.65f&&!drag_&&!mouseLeftDown_,"Focus loss committed a zoom on release");
  pointer(SDL_MOUSEBUTTONDOWN,center);lost.window.event=SDL_WINDOWEVENT_SIZE_CHANGED;handleNativeEvent(lost);pointer(SDL_MOUSEBUTTONUP,{center.x,center.y-100});check(zoom_==.65f&&!drag_,"Resize committed a zoom on release");
  zoom_=1.6f;wheel(center,1000,-1000);auto edge=mapCamera();check(close(camX_,edge[0])&&close(camY_,edge[1]),"Wheel left invisible camera overscroll at the map boundary");wheel(center,-1,1);check(camX_<edge[0]&&camY_<edge[1],"Camera remained stuck after reversing at map boundary");
  check(game.toJson()==before&&destination_==destination&&route_.size()==routeSize&&!marching_,"Trackpad scroll or drag zoom moved the hero, changed its route, or triggered an event");
  cases.push_back({{"window",{size.x,size.y}},{"fractionalTwoAxisPan",true},{"wheelDoesNotZoom",true},{"systemDirectionPreserved",true},{"pressDragZoom",true},{"zoomAnchor",true},{"dragReleaseDoesNotClick",true},{"focusAndResizeCancel",true},{"mapBounds",true},{"sidebarAndOtherScenesIgnored",true},{"gameAndRouteUnchanged",true}});
 }
 std::ofstream(directory/"map-gesture-audit.json")<<nlohmann::json{{"desktopScroll","two-finger or mouse wheel pans"},{"desktopDrag","hold and drag up to zoom in, down to zoom out"},{"touchscreen","single-finger pan and pinch unchanged"},{"windows",cases}}.dump(2)<<'\n';
 cancelPointer();route_.clear();destination_.reset();zoom_=1;
}
}
