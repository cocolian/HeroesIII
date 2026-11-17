#include "h3/renderer.hpp"
#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace h3 {
void Desktop::mineIncomeRenderTests(const std::filesystem::path& directory){
 auto check=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
 int oldW=0,oldH=0;SDL_GetWindowSize(window_,&oldW,&oldH);auto settings=settings_;settings_.hdArtwork=false;
 SDL_SetWindowSize(window_,800,600);game=Game::create();scene=Scene::Adventure;zoom_=1;camX_=4.5f;camY_=20.5f;
 route_.clear();destination_.reset();marching_=false;cancelPointer();mouseX_=mouseY_=-1;toastUntil_=0;
 // Earlier flow tests have advanced the simulated clock. Continue past their
 // timers so a pending movement deadline cannot stall this independent run.
 uint64_t clock=std::max(nextStep_,nextAI_)+1000;fixedAnimationTime_=clock;
 auto walk=[&](Point goal){
  render();click(mapScreen(goal),false);check(destination_==goal&&!route_.empty(),"Mine visit did not plan a route");key(SDLK_m,KMOD_NONE);
  for(int step=0;step<100&&marching_;step++){fixedAnimationTime_=clock+=1000;tick();}
  check(game.hero.pos==goal&&scene==Scene::Adventure,"Mine visit failed to reach its entrance");
 };
 // Isolate production UI from fog scouting; full navigation is covered by the campaign run.
 game.hero.pos={5,16};game.reveal();camX_=5.5f;camY_=15.5f;
 int ore=game.resources[Ore];walk({4,14});check(game.objectAt({4,14})->owner==0&&game.resources[Ore]==ore,"Ore mine capture paid immediately or did not flag the mine");
 walk({5,15});int capturedDay=game.day;auto rng=game.randomState;nlohmann::json mornings=nlohmann::json::array();
 auto numberPixels=[&]{
  auto size=canvasLogicalSize();int cell=(size.x-200)/ResourceCount;
  SDL_Rect region{int((5+2*cell+25)*pixelScale_),int((size.y-20)*pixelScale_),int((cell-27)*pixelScale_),int(15*pixelScale_)};
  std::vector<Uint32> pixels(size_t(region.w)*region.h);check(SDL_SetRenderTarget(renderer_,canvas_.texture)==0,"Mine income canvas unavailable");
  bool ok=SDL_RenderReadPixels(renderer_,&region,SDL_PIXELFORMAT_RGBA32,pixels.data(),region.w*4)==0;SDL_SetRenderTarget(renderer_,nullptr);check(ok,"Mine resource number readback failed");return pixels;
 };
 for(auto size:{Point{800,600},Point{1512,982}}){
  SDL_SetWindowSize(window_,size.x,size.y);mouseX_=mouseY_=-1;toastUntil_=0;render();auto before=numberPixels();int expected=game.resources[Ore]+2,day=game.day;
  check(screenshot(directory/("mine-income-before-"+std::to_string(size.x)+".png")),"Mine before screenshot failed");
  SDL_Event e{};e.type=SDL_MOUSEBUTTONDOWN;e.button.button=SDL_BUTTON_LEFT;e.button.x=711+size.x-800;e.button.y=372;handleNativeEvent(e,true);
  check(game.day==day,"End turn triggered on pointer down");e.type=SDL_MOUSEBUTTONUP;handleNativeEvent(e,true);
  check(game.day==day+1&&game.resources[Ore]==expected,"End turn did not deposit the occupied ore mine's two ore");
  mouseX_=mouseY_=-1;toastUntil_=0;render();check(numberPixels()!=before,"Ore inventory changed but the visible number remained static");
  check(screenshot(directory/("mine-income-after-"+std::to_string(size.x)+".png")),"Mine after screenshot failed");
  mornings.push_back({{"window",{size.x,size.y}},{"day",game.day},{"ore",game.resources[Ore]},{"visibleCounterUpdated",true}});
 }
 key(SDLK_e,KMOD_NONE);check(game.day==capturedDay+3&&game.resources[Ore]==ore+6,"Keyboard end turn did not continue ore production");
 check(game.randomState==rng&&game.validate(),"Mine production consumed RNG or invalidated game state");
 std::ofstream(directory/"mine-income-audit.json")<<nlohmann::json{{"captureViaMapRoute",true},{"startAtMineApproach",true},{"captureDoesNotPayImmediately",true},{"heroLeftMine",true},{"pointerDownDoesNotAdvanceDay",true},{"nativePointerReleaseDepositsIncome",true},{"keyboardDepositsIncome",true},{"initialOre",ore},{"finalOre",game.resources[Ore]},{"elapsedDays",3},{"mornings",mornings},{"rngUnchanged",true}}.dump(2)<<'\n';
 cancelPointer();game=Game::create();scene=Scene::Town;settings_=settings;fixedAnimationTime_=0;mouseX_=mouseY_=-1;SDL_SetWindowSize(window_,oldW,oldH);
}
}
