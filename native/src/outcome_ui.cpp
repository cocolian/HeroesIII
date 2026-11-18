#include "h3/renderer.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace h3 {
void Desktop::afterReport(){
 if(!game.battle&&!game.report&&game.hero.levelChoices.empty()&&!game.computerQueue.empty())game.computerTurns();
 scene=!game.hero.levelChoices.empty()?Scene::Level:game.result?Scene::Outcome:Scene::Adventure;
 focus_=-1;marching_=false;route_.clear();destination_.reset();
 pendingSpell_=sacrificeTarget_=-1;autoCombat_=false;if(scene==Scene::Adventure)settle();
}
void Desktop::mapDialog(SDL_Rect dialog,Scene background){
 auto*target=SDL_GetRenderTarget(renderer_);auto&backdrop=images_["#outcome-backdrop"];
 if(!backdrop.texture){
  backdrop={SDL_CreateTexture(renderer_,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,800,600),800,600};
  if(!backdrop.texture)throw std::runtime_error(SDL_GetError());
  SDL_SetTextureBlendMode(backdrop.texture,SDL_BLENDMODE_NONE);SDL_SetTextureScaleMode(backdrop.texture,SDL_ScaleModeLinear);
 }
 // Compose tiled art on integer pixels first. SDL's independently rounded
 // copies otherwise leave cracks at fractional 4:3 scales such as 1280x800.
 SDL_SetRenderTarget(renderer_,backdrop.texture);SDL_RenderSetScale(renderer_,1,1);SDL_RenderSetClipRect(renderer_,nullptr);
 SDL_SetRenderDrawColor(renderer_,7,6,5,255);SDL_RenderClear(renderer_);
 // Keep the completed map visible behind a persistent, explicit result.
 if(background==Scene::Battle)battle();else if(background==Scene::Town)town();else adventure();buttons_.clear();box({0,0,800,600},{0,0,0,170});
 box({dialog.x+4,dialog.y+4,dialog.w,dialog.h},{0,0,0,180});
 // Tile the original leather and eight-piece dialogue frame at native size.
 for(int y=dialog.y+14;y<dialog.y+dialog.h-14;y+=256)for(int x=dialog.x+14;x<dialog.x+dialog.w-14;x+=256){
  int w=std::min(256,dialog.x+dialog.w-14-x),h=std::min(256,dialog.y+dialog.h-14-y);
  originalImage("diboxbck.pcx",{x,y,w,h},SDL_Rect{0,0,w,h});
 }
 SDL_Rect horizontal{dialog.x+64,dialog.y,dialog.w-128,dialog.h};SDL_RenderSetClipRect(renderer_,&horizontal);
 for(int x=dialog.x+64;x<dialog.x+dialog.w-64;x+=64){frame("dialgbox.def",0,6,{x,dialog.y,64,64});frame("dialgbox.def",0,7,{x,dialog.y+dialog.h-64,64,64});}
 SDL_Rect vertical{dialog.x,dialog.y+64,dialog.w,dialog.h-128};SDL_RenderSetClipRect(renderer_,&vertical);
 for(int y=dialog.y+64;y<dialog.y+dialog.h-64;y+=64){frame("dialgbox.def",0,4,{dialog.x,y,64,64});frame("dialgbox.def",0,5,{dialog.x+dialog.w-64,y,64,64});}
 SDL_RenderSetClipRect(renderer_,nullptr);
 frame("dialgbox.def",0,0,{dialog.x,dialog.y,64,64});frame("dialgbox.def",0,1,{dialog.x+dialog.w-64,dialog.y,64,64});
 frame("dialgbox.def",0,2,{dialog.x,dialog.y+dialog.h-64,64,64});frame("dialgbox.def",0,3,{dialog.x+dialog.w-64,dialog.y+dialog.h-64,64,64});
 SDL_SetRenderTarget(renderer_,target);SDL_RenderSetScale(renderer_,float(pixelScale_),float(pixelScale_));SDL_RenderSetClipRect(renderer_,nullptr);
 SDL_Rect full{0,0,800,600};SDL_RenderCopy(renderer_,backdrop.texture,nullptr,&full);
}
void Desktop::outcome(){
 mapDialog({158,116,484,368});
 constexpr SDL_Color gold{236,213,130,255},muted{189,171,135,255};
 bool won=game.result>0;
 centerText(won?"地图胜利！":"战役失败",{180,140,440,46},32,gold);
 heroPortrait(game.hero,{204,210,58,64});
 text(game.imported()?(won?"已达成地图的胜利条件":"本次地图任务失败"):(won?"黑鸦堡已被占领":"凯瑟琳的部队已全军覆没"),285,210,19,gold);
 text(game.imported()?(won?"本次地图任务已完成。":"本次冒险已经结束。"):(won?"边境重归和平，任务完成。":"本次冒险已经结束。"),285,250,15,muted);
 centerText(game.imported()?game.scenarioName:"边境练习图",{180,300,440,25},16,gold);
 centerText("用时 "+std::to_string(game.day)+" 天    英雄等级 "+std::to_string(game.hero.level),{180,337,440,25},16);
 centerText("获得经验 "+std::to_string(game.hero.xp),{180,369,440,25},16);
 originalButton({368,412,64,32},"iokay.def",0,[this]{
  scene=Scene::Menu;menuView_=MenuView::Main;focus_=-1;mouseX_=mouseY_=-1;
  battleMotion_.clear();restoreSystemCursor();persist();
 },true,-1,"确认本局胜败结果，返回主菜单（Enter）；不会自动开始下一局");
 centerText("确认后返回主菜单",{180,450,440,16},12,muted);
}
void Desktop::outcomeRenderTests(const std::filesystem::path& directory){
 auto check=[](bool ok,const char*message){if(!ok)throw std::runtime_error(message);};
 int width,height;SDL_GetWindowSize(window_,&width,&height);SDL_SetWindowSize(window_,1000,750);
 cancelPointer();battleMotion_.clear();settings_=ClientSettings{};fixedAnimationTime_=1000;
 auto shot=[&](const char*name){toastUntil_=0;render();check(screenshot(directory/(std::string("outcome-")+name+".png")),"Outcome screenshot failed");};
 nlohmann::json ends=nlohmann::json::array();
 for(bool won:{true,false}){
  game=Game::create();game.hero.army={Stack{Archer,won?100:1}};
  auto*target=game.objectAt({29,7});check(target&&target->type==ObjectType::Town,"Outcome fixture lost enemy town");
  target->army={Stack{won?Skeleton:Archangel,won?1:100}};game.startBattle(target->id);
  if(won){game.battle->queue={0,100};check(game.act("attack",{game.battle->units[1].x,game.battle->units[1].y}).empty(),"Final attack failed");}
  else{game.battle->queue={100,0};game.ai();}
  check(!game.battle&&game.report&&game.result==(won?1:-1),"Final battle did not produce a terminal result");
  if(won)game.gainXP(1000);scene=Scene::Battle;settle();
  check(scene==Scene::Report,"Final battle skipped its casualty report");shot(won?"victory-report":"defeat-report");
  click({582,542},false);
  if(won){check(scene==Scene::Level,"Final victory skipped pending level choices");while(scene==Scene::Level){render();click({200,410},false);}}
  check(scene==Scene::Outcome&&!game.report&&game.hero.levelChoices.empty(),"Report or upgrade returned directly to menu");
  check(audioTheme()==(won?"report.scenario-win":"report.ultimate-loss"),"Terminal result lost its music context");
  auto state=game.toJson();for(int i=0;i<8;++i){fixedAnimationTime_=100000+i*1000;tick();render();}
  for(auto keycode:{SDLK_ESCAPE,SDLK_F1,SDLK_e,SDLK_m})key(keycode,KMOD_NONE);
  check(scene==Scene::Outcome&&game.toJson()==state,"Result dismissed or advanced without acknowledgement");
  click({20,20},false);check(scene==Scene::Outcome,"Clicking outside result dismissed it");shot(won?"victory":"defeat");
  if(won){
   SDL_SetWindowSize(window_,1280,800);render();shot("fractional-victory");
   SDL_SetRenderTarget(renderer_,canvas_.texture);
   for(int x=222;x<578;x+=64){
    SDL_Rect pixel{int(std::lround(x*pixelScale_)),int(std::lround(122*pixelScale_)),1,1};Uint8 rgba[4]{};
    check(SDL_RenderReadPixels(renderer_,&pixel,SDL_PIXELFORMAT_RGBA32,rgba,4)==0,"Could not inspect fractional dialog seam");
    check(int(rgba[0])+rgba[1]+rgba[2]>40,"Fractional dialogue border has a black seam");
   }
   SDL_SetRenderTarget(renderer_,nullptr);SDL_SetWindowSize(window_,1000,750);render();
  }
  key(SDLK_RETURN,KMOD_NONE);check(scene==Scene::Menu&&game.toJson()==state,"Acknowledgement failed or reset the game");
  // Resuming a completed map must show the result again instead of a dead map.
  scene=Scene::Adventure;settle();check(scene==Scene::Outcome,"Completed-map resume skipped its result");
  ends.push_back({{"won",won},{"result",game.result},{"reportBeforeResult",true},{"explicitAcknowledgement",true}});
 }
 std::ofstream(directory/"outcome-audit.json")<<nlohmann::json{{"ends",ends},{"upgradeBeforeResult",true},{"noTimedDismissal",true},{"escapeAndOutsideClickRetainResult",true},{"completedMapResume",true},{"terminalMusic",true},{"fractionalScaleFrameIsContinuous",true}}.dump(2)<<'\n';
 game=Game::create();scene=Scene::Town;battleMotion_.clear();fixedAnimationTime_=0;mouseX_=mouseY_=-1;SDL_SetWindowSize(window_,width,height);
}
}
