#include "h3/renderer.hpp"
#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace h3 {
void Desktop::mapServicesPreview(const std::filesystem::path&directory,const std::filesystem::path&resume){
 auto check=[](bool ok,const char*message){if(!ok)throw std::runtime_error(message);};
 std::filesystem::create_directories(directory);saveDir_=directory/"test-saves";
 settings_=ClientSettings{};settings_.hdArtwork=false;fixedAnimationTime_=1000;
 SDL_SetWindowSize(window_,1280,800);zoom_=1;mouseX_=mouseY_=-1;
 std::string error;Game original;
 if(!resume.empty())check(original.load(resume,error),error.c_str());
 else check(original.importScenario(root_/"assets/demo/scenarios/wayfarer-1.json",error),error.c_str());
 nlohmann::json results=nlohmann::json::array();
 auto shot=[&](const std::string&name){toastUntil_=0;render();check(screenshot(directory/(name+".png")),"Map service screenshot failed");};
 for(int cls:{97,92,42}){
  game=original;std::erase_if(game.objects,[](const auto&o){return o.type==ObjectType::Monster;});
  auto it=std::find_if(game.objects.begin(),game.objects.end(),[&](const auto&o){return o.sourceClass==cls;});
  check(it!=game.objects.end(),"Reported map service not present");int id=it->id;
  game.hero.pos=it->pos;game.mapLevel=it->level;game.hero.onBoat=cls==92;game.hero.levelChoices.clear();game.reveal();
  scene=Scene::Adventure;camX_=game.hero.pos.x+.5f;camY_=game.hero.pos.y+.5f;
  visitPopup_.reset();route_.clear();destination_.reset();marching_=autoCombat_=false;battleMotion_.clear();focus_=-1;
  int xp=game.hero.xp,troops=armyCount(game.hero.army),movement=game.hero.movement;
  // Use the actual map hit-testing and visit dispatch, rather than constructing a popup.
  render();click(mapScreen(game.hero.pos),false);
  int levels=0;while(scene==Scene::Level&&!game.hero.levelChoices.empty()&&levels++<70){game.chooseSkill(game.hero.levelChoices.front().skills.front());scene=Scene::Adventure;settle();}
  check(scene==Scene::Visit&&visitPopup_&&visitPopup_->sourceClass==cls,"Map service click did not open its result");
  check(visitPopup_->message.find("尚未实现")==std::string::npos,"Map service retained placeholder");
  auto state=game.toJson();shot("service-"+std::to_string(cls));check(game.toJson()==state,"Drawing service changed state");
  if(cls==97){SDL_SetWindowSize(window_,1920,1200);shot("service-97-retina");check(game.toJson()==state,"Retina intelligence rendering changed state");SDL_SetWindowSize(window_,1280,800);}
  if(cls==42)check(game.objectById(id)->owner==0&&game.hero.movement==movement,"Lighthouse failed capture or added same-day movement");
  results.push_back({{"sourceClass",cls},{"object",id},{"name",game.objectById(id)->name},{"troopsBefore",troops},{"troopsAfter",armyCount(game.hero.army)},{"experienceGained",game.hero.xp-xp},{"message",visitPopup_->message},{"levelChoicesResolved",levels}});
  render();key(SDLK_RETURN,KMOD_NONE);check(scene==Scene::Adventure&&!visitPopup_,"Service acknowledgement failed");
  shot("service-"+std::to_string(cls)+"-map");
  if(cls==42){
   const auto&o=*game.objectById(id);auto sprite=o.sprite;
   check(ownerArt_["assets"].contains(sprite),"Lighthouse owner-color artwork not exported");
   const auto&frames=demo_["def"][sprite]["groups"]["0"];size_t index=(animationTime_/100)%frames.size();
   auto canvas=mapSpriteCanvas(sprite,o.anchor,false);const auto&f=frames[index];
   const auto&point=ownerArt_["assets"][sprite]["samples"][index]["point"];
   Point flag{canvas.x+int((point[0].get<int>()+.5)*canvas.w/f["canvas"][0].get<int>()),canvas.y+int((point[1].get<int>()+.5)*canvas.h/f["canvas"][1].get<int>())};
   bool found=false;auto expected=ownerColor(o.owner);SDL_SetRenderTarget(renderer_,canvas_.texture);
   for(int dy=-2;dy<=2;++dy)for(int dx=-2;dx<=2;++dx){
    SDL_Rect area{int((flag.x+dx+.5)*pixelScale_),int((flag.y+dy+.5)*pixelScale_),1,1};Uint8 rgb[4]{};
    check(SDL_RenderReadPixels(renderer_,&area,SDL_PIXELFORMAT_RGBA32,rgb,4)==0,"Lighthouse pixel readback failed");
    found|=rgb[0]==expected.r&&rgb[1]==expected.g&&rgb[2]==expected.b;
   }
   SDL_SetRenderTarget(renderer_,nullptr);check(found,"Captured lighthouse flag does not match its owner");
  }
  // Every service remains available; repeat visits must not duplicate destructive/reward effects.
  auto repeatArmy=game.hero.army;int repeatXP=game.hero.xp;render();click(mapScreen(game.hero.pos),false);
  check(scene==Scene::Visit&&game.hero.xp==repeatXP&&armyCount(game.hero.army)==armyCount(repeatArmy),"Repeated service visit duplicated reward/loss");
  shot("service-"+std::to_string(cls)+"-repeat");key(SDLK_ESCAPE,KMOD_NONE);check(scene==Scene::Adventure,"Escape did not return to map");
 }
 std::ofstream(directory/"map-services-ui-audit.json")<<nlohmann::json{{"scenario",original.scenarioId},{"privateSaveCopy",!resume.empty()},{"originalSaveWritten",false},{"guardIsolation",true},{"tests",results}}.dump(2)<<'\n';
}
}
