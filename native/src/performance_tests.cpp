#include "h3/renderer.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace h3 {
void Desktop::performancePreview(const std::filesystem::path&directory,const std::filesystem::path&source){
 auto check=[](bool ok,const char*m){if(!ok)throw std::runtime_error(m);};
 std::filesystem::create_directories(directory);saveDir_=directory/"test-saves";settings_=ClientSettings{};fixedAnimationTime_=1000;SDL_SetWindowSize(window_,1280,800);mouseX_=mouseY_=-1;std::string error;
 check(source.empty()?game.importScenario(root_/"assets/demo/scenarios/wayfarer-1.json",error):game.load(source,error),"Performance map load failed");
 scene=Scene::Adventure;camX_=game.hero.pos.x+.5f;camY_=game.hero.pos.y+.5f;zoom_=1;route_.clear();marching_=false;visitPopup_.reset();toastUntil_=0;
 auto original=game.toJson();std::vector<Point>goals;
 for(int y=0;y<game.mapHeight;++y)for(int x=0;x<game.mapWidth;++x)if(game.tile({x,y})->seen&&Point{x,y}!=game.hero.pos)goals.push_back({x,y});
 std::sort(goals.begin(),goals.end(),[&](Point a,Point b){auto distance=[&](Point p){return abs(p.x-game.hero.pos.x)+abs(p.y-game.hero.pos.y);};return distance(a)>distance(b);});
 Point goal=game.hero.pos;for(auto p:goals)if(!game.path(p).empty()){goal=p;break;}
 auto measure=[](int count,const auto&fn){auto start=std::chrono::steady_clock::now();for(int i=0;i<count;++i)fn();return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/count;};
 double pathMs=measure(8,[&]{game.path(goal);});
 render();double idleMs=measure(8,[&]{render();});
 auto pointer=mapScreen(goal);mouseX_=pointer.x;mouseY_=pointer.y;render();double hoverMs=measure(8,[&]{render();});screenshot(directory/"adventure.png");
 mouseX_=mouseY_=-1;scene=Scene::Hero;render();double heroMs=measure(8,[&]{render();});check(screenshot(directory/"hero-window.png"),"Hero screenshot failed");
 check(buttons_.back().rect.x==673&&buttons_.back().rect.y==523&&buttons_.back().rect.w==52&&buttons_.back().rect.h==36,"Hero exit does not match original hsbtns geometry");
 nlohmann::json exits=nlohmann::json::array();
 for(auto size:{Point{640,480},Point{1512,982},Point{2388,1668}}){
  SDL_SetWindowSize(window_,size.x,size.y);scene=Scene::Hero;focus_=-1;render();
  auto tap=[&](Point p){int w,h;SDL_GetWindowSize(window_,&w,&h);auto layout=displayLayout(w,h,w,h,false);p={int(std::lround(layout.pointX+(p.x+.5)*layout.pointScale)),int(std::lround(layout.pointY+(p.y+.5)*layout.pointScale))};for(Uint32 type:{SDL_MOUSEBUTTONDOWN,SDL_MOUSEBUTTONUP}){SDL_Event e{};e.type=type;e.button.button=SDL_BUTTON_LEFT;e.button.x=p.x;e.button.y=p.y;handleNativeEvent(e,true);}};
  tap({699,567});check(scene==Scene::Hero,"Empty space below the original exit still closed the hero window");render();tap({699,541});check(scene==Scene::Adventure,"Original hero exit hit area failed at a supported size");
  scene=Scene::Hero;focus_=-1;render();key(SDLK_RETURN,KMOD_NONE);check(scene==Scene::Adventure,"Hero Return shortcut did not close the window");scene=Scene::Hero;render();key(SDLK_ESCAPE,KMOD_NONE);check(scene==Scene::Adventure,"Hero Escape shortcut failed");
  exits.push_back({{"window",{size.x,size.y}},{"originalExitHitbox",true},{"oldOversizedHitboxRemoved",true},{"returnAndEscape",true}});
 }
 SDL_SetWindowSize(window_,1280,800);scene=Scene::Adventure;
 double saveMs=measure(3,[&]{check(game.save(saveDir_/"benchmark.json",error),"Benchmark save failed");});
 asynchronousSave_=true;marching_=true;nextAutosave_=0;fixedAnimationTime_=5000;
 double enqueueMs=measure(20,[&]{persist();});marching_=false;persist();flushAutosave();
 Game restored;check(restored.load(saveDir_/"autosave.json",error)&&restored.toJson()==game.toJson(),"Background save did not preserve the latest game");uint64_t writes=autosave_->writes();autosave_.reset();asynchronousSave_=false;
 scene=Scene::Adventure;std::vector<int>contribution;int active=game.activeHeroId;auto center=mapScreen(game.hero.pos);
 auto pixels=[&]{SDL_Rect r{int((center.x-48)*pixelScale_),int((center.y-48)*pixelScale_),int(96*pixelScale_),int(64*pixelScale_)};std::vector<Uint32>data(size_t(r.w)*r.h);SDL_SetRenderTarget(renderer_,canvas_.texture);check(SDL_RenderReadPixels(renderer_,&r,SDL_PIXELFORMAT_RGBA32,data.data(),r.w*4)==0,"Hero readback failed");SDL_SetRenderTarget(renderer_,nullptr);return data;};
 const Point directions[]={{0,1},{1,1},{1,0},{1,-1},{0,-1},{-1,-1},{-1,0},{-1,1}};
 for(auto direction:directions)for(int f=0;f<8;++f){heroDirection_=direction;marching_=true;route_={{goal.x,goal.y,100}};fixedAnimationTime_=f*80;render();auto present=pixels();if(contribution.empty())screenshot(directory/"walking.png");game.activeHeroId=0;render();auto absent=pixels();game.activeHeroId=active;int count=0;for(size_t i=0;i<present.size();++i)count+=present[i]!=absent[i];contribution.push_back(count);}
 marching_=false;route_.clear();check(game.toJson()==original,"Render performance probe changed the save state");
 check(*std::min_element(contribution.begin(),contribution.end())>50,"Walking hero is hidden by an incorrectly ordered ground overlay");
 SDL_RendererInfo info{};SDL_GetRendererInfo(renderer_,&info);
 std::ofstream(directory/"performance-audit.json")<<nlohmann::json{{"renderer",info.name?info.name:""},{"window",{1280,800}},{"map",game.scenarioId},{"objects",game.objects.size()},{"decorations",game.decorations.size()},{"idleFrameMs",idleMs},{"hoverFrameMs",hoverMs},{"heroFrameMs",heroMs},{"pathMs",pathMs},{"saveMs",saveMs},{"autosaveEnqueueMeanMs",enqueueMs},{"writesFor20MarchingRequestsAndFinalFlush",writes},{"latestBackgroundSaveMatches",true},{"movingHeroPixelContributions",contribution},{"minimumMovingHeroPixels",*std::min_element(contribution.begin(),contribution.end())},{"exitChecks",exits},{"originalSaveWritten",false}}.dump(2)<<'\n';
 std::cout<<"Performance and 64 moving-hero frame probes completed\n";
}
}
