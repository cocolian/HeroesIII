#include "h3/renderer.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>

namespace h3 {
namespace {
struct MenuItem {const char*asset;SDL_Rect rect;int labelY;};
// Original DEF canvases have different widths; their text shares one center.
constexpr MenuItem Items[]={
 {"mmenung.def",{540,18,207,121},50},
 {"mmenulg.def",{531,129,226,120},49},
 {"mmenuhs.def",{524,242,239,106},39},
 {"mmenucr.def",{557,346,173,110},42},
 {"mmenuqt.def",{587,459,114,102},29},
};
}
bool Desktop::menuButtonContains(const std::string&name,SDL_Rect bounds,Point point){
 if(point.x<bounds.x||point.y<bounds.y||point.x>=bounds.x+bounds.w||point.y>=bounds.y+bounds.h)return false;
 int x=point.x-bounds.x,y=point.y-bounds.y;
 // Text is a continuous click target, including spaces and letter counters.
 for(const auto&item:Items)if(name==item.asset&&y>=item.labelY&&y<item.labelY+36)return true;
 auto key="#menu-hit#"+name;auto found=masks_.find(key);
 if(found==masks_.end()){
  const auto&animation=demo_["def"][name];const auto&frames=animation["groups"]["0"];
  PixelMask mask{bounds.w,bounds.h,std::vector<uint8_t>(size_t(bounds.w)*bounds.h)};
  for(int index=0;index<3;index++){
   const auto&f=frames[index];auto*page=readSurface(animation["pages"][f["page"].get<int>()]["file"],false,false);if(!page)continue;
   int left=f["offset"][0],top=f["offset"][1],sx=f["rect"][0],sy=f["rect"][1],w=f["rect"][2],h=f["rect"][3];
   for(int row=0;row<h;row++)for(int col=0;col<w;col++)if(left+col<mask.width&&top+row<mask.height&&static_cast<Uint8*>(page->pixels)[(sy+row)*page->pitch+(sx+col)*4+3]>128)mask.pixels[size_t(top+row)*mask.width+left+col]=1;
   SDL_FreeSurface(page);
  }
  found=masks_.emplace(key,std::move(mask)).first;
 }
 return found->second.contains(x,y);
}
void Desktop::menuButton(SDL_Rect bounds,const std::string&name,std::function<void()>action){
 int id=int(buttons_.size());bool hover=menuButtonContains(name,bounds,{mouseX_,mouseY_})||focus_==id;
 bool pressed=mouseLeftDown_&&pressedScene_==scene&&pressedButton_==id&&hover;
 frame(name,0,pressed?1:hover?2:0,bounds);
 const std::map<std::string,std::string> hints={{"mmenung.def","新游戏：选择阵营并开始新的练习地图（N）"},{"mmenulg.def","读取已保存的游戏（L）"},{"mmenuhs.def","查看本局练习图通关成绩（H）"},{"mmenucr.def","查看制作与素材来源说明（C）"},{"mmenuqt.def","退出游戏（Q）"}};
 buttons_.push_back({bounds,std::move(action),true,[this,name,bounds](Point p){return menuButtonContains(name,bounds,p);},hints.at(name)});
}
void Desktop::menu(){
 art("assets/demo/ui/sod-mainmenu.png",{0,0,800,600},{},false);
 if(menuView_!=MenuView::Main){
  constexpr SDL_Color Gold{236,213,130,255},Muted{189,171,135,255};
  panel({100,92,600,416});text(menuView_==MenuView::Scores?"最高得分":"制作组",132,113,24,Gold);
  if(menuView_==MenuView::Scores){
   text("边境练习图 · 本局通关成绩",132,166,16,Gold);
   if(game.result==1){text("英雄：凯瑟琳",132,219,18);text("完成天数："+std::to_string(game.day),132,268,18);text("获得经验："+std::to_string(game.hero.xp),132,317,18);}
   else text("尚未完成本局练习地图",132,230,18,Muted);
  }else{
   text("原作",132,166,17,Gold);text("New World Computing",132,201,19);text("The 3DO Company",132,239,19);
   text("当前实现",132,301,17,Gold);text("C++20 / SDL2 原生开发项目",132,336,18);text("原版图像与音频来自公开试玩资料",132,377,14,Muted);
  }
  originalButton({610,452,64,32},"iokay.def",0,[this]{menuView_=MenuView::Main;focus_=-1;});return;
 }
 menuButton(Items[0].rect,Items[0].asset,[this]{scene=Scene::Scenario;focus_=-1;});
 menuButton(Items[1].rect,Items[1].asset,[this]{load();});
 menuButton(Items[2].rect,Items[2].asset,[this]{menuView_=MenuView::Scores;focus_=-1;});
 menuButton(Items[3].rect,Items[3].asset,[this]{menuView_=MenuView::Credits;focus_=-1;});
 menuButton(Items[4].rect,Items[4].asset,[this]{
#ifdef __IPHONEOS__
  say("请使用系统手势返回主屏幕。");
#else
  running_=false;
#endif
 });
}
void Desktop::menuRenderTests(const std::filesystem::path&directory){
 auto check=[](bool ok,const char*message){if(!ok)throw std::runtime_error(message);};
 int oldW=0,oldH=0;SDL_GetWindowSize(window_,&oldW,&oldH);SDL_SetWindowSize(window_,800,600);auto previous=settings_;
 game=Game::create();scene=Scene::Menu;menuView_=MenuView::Main;fixedAnimationTime_=0;mouseX_=mouseY_=-1000;focus_=-1;toastUntil_=0;cancelPointer();auto before=game.toJson();
 auto shot=[&](const char*name){toastUntil_=0;render();check(screenshot(directory/(std::string(name)+".png")),"Original menu screenshot failed");};
 settings_.hdArtwork=false;shot("menu-original-classic");check(buttons_.size()==5,"Main menu must have its original five entries");
 for(int i=0;i<5;i++)check(buttons_[i].rect.w==Items[i].rect.w&&buttons_[i].rect.h==Items[i].rect.h,"Menu distorted an original DEF canvas");
 check(!menuButtonContains(Items[0].asset,Items[0].rect,{540,18}),"Transparent corner is clickable");
 mouseX_=600;mouseY_=80;shot("menu-original-hover");SDL_Event e{};e.type=SDL_MOUSEBUTTONDOWN;e.button.button=SDL_BUTTON_LEFT;e.button.x=600;e.button.y=80;handleNativeEvent(e,true);check(pressedButton_==0,"Original New Game press was not captured");shot("menu-original-pressed");
 e.type=SDL_MOUSEBUTTONUP;handleNativeEvent(e,true);check(scene==Scene::Scenario,"Original New Game did not open scenario selection");
 scene=Scene::Menu;mouseX_=mouseY_=-1000;render();click({600,295},false);check(scene==Scene::Menu&&menuView_==MenuView::Scores,"High Score was wired to another feature");shot("menu-scores");key(SDLK_ESCAPE,KMOD_NONE);check(menuView_==MenuView::Main,"High Score Escape did not return to the main menu");
 render();click({640,405},false);check(menuView_==MenuView::Credits,"Credits was wired to Continue Game");shot("menu-credits");render();click({642,468},false);check(menuView_==MenuView::Main,"Credits acknowledgement failed");
 std::string error;check(game.save(saveDir_/"manual.json",error),"Menu load fixture could not save");game.resources[Gold]-=1;render();click({640,191},false);check(scene==Scene::Adventure&&game.toJson()==before,"Original Load Game did not restore saved progress");
 scene=Scene::Menu;mouseX_=mouseY_=-1000;toastUntil_=0;render();e.type=SDL_MOUSEBUTTONDOWN;e.button.x=600;e.button.y=80;handleNativeEvent(e,true);e.type=SDL_MOUSEBUTTONUP;e.button.x=450;e.button.y=80;handleNativeEvent(e,true);check(scene==Scene::Menu,"Dragging off New Game still activated it");
 mouseX_=mouseY_=-1000;settings_.hdArtwork=true;SDL_SetWindowSize(window_,2560,1920);shot("menu-original-retina");
 SDL_SetWindowSize(window_,1512,982);shot("menu-original-wide");auto layout=displayLayout(1512,982,1512,982,false);check(layout.width==800&&layout.height==600,"Wide menu lost its original aspect ratio");
 SDL_SetWindowSize(window_,800,600);render();key(SDLK_i,KMOD_NONE);check(scene==Scene::Codex,"Codex lost its menu keyboard access");scene=Scene::Menu;render();key(SDLK_h,KMOD_NONE);check(menuView_==MenuView::Scores,"Original H hotkey did not open High Score");key(SDLK_ESCAPE,KMOD_NONE);key(SDLK_c,KMOD_NONE);check(menuView_==MenuView::Credits,"Original C hotkey did not open Credits");key(SDLK_ESCAPE,KMOD_NONE);
 render();click({640,510},false);
#ifndef __IPHONEOS__
 check(!running_,"Original Quit did not exit");running_=true;
#endif
 check(game.toJson()==before,"Menu navigation changed game state or RNG");
 std::ofstream(directory/"menu-audit.json")<<nlohmann::json{{"entries",{"New Game","Load Game","High Score","Credits","Quit"}},{"originalDEFAssets",true},{"localization","original English demo sprites"},{"normalHoverPressedStates",true},{"nativePressRelease",true},{"transparentCornersExcluded",true},{"dragOffCancels",true},{"loadRestoresProgress",true},{"highScoreAndCreditsCorrectDestinations",true},{"scoresScope","Current practice-run result only; original persistent rankings/scoring remain incomplete"},{"originalCanvasSizes",true},{"retinaScreenshot",{2560,1920}},{"wideWindowPoints",{1512,982}},{"gameStateAndRngUnchanged",true}}.dump(2)<<'\n';
 cancelPointer();menuView_=MenuView::Main;settings_=previous;SDL_SetWindowSize(window_,oldW,oldH);game=Game::create();scene=Scene::Town;fixedAnimationTime_=0;mouseX_=mouseY_=-1;focus_=-1;toastUntil_=0;
}
}
