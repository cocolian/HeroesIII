#include "h3/renderer.hpp"
#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace h3 {
void Desktop::locationPreview(const std::filesystem::path&directory,const std::filesystem::path&resume){
 auto check=[](bool ok,const std::string&why){if(!ok)throw std::runtime_error(why);};std::filesystem::create_directories(directory);saveDir_=directory/"test-saves";
 settings_=ClientSettings{};settings_.hdArtwork=false;fixedAnimationTime_=1000;SDL_SetWindowSize(window_,1280,800);
 Game baseline;std::string error;if(resume.empty())check(baseline.importScenario(root_/"assets/demo/scenarios/wayfarer-1.json",error),error);else check(baseline.load(resume,error),error);
 std::map<int,Object> originals;std::vector<std::filesystem::path> maps;for(const auto&entry:std::filesystem::directory_iterator(root_/"assets/demo/scenarios"))if(entry.path().extension()==".json")maps.push_back(entry.path());std::sort(maps.begin(),maps.end());for(const auto&map:maps){Game candidate;check(candidate.importScenario(map,error),error);for(const auto&o:candidate.objects)if(Game::extendedLocation(o.sourceClass)&&!originals.contains(o.sourceClass))originals[o.sourceClass]=o;}
 const int classes[]={2,4,7,13,15,16,17,20,22,24,25,27,29,35,37,39,47,50,58,60,62,63,78,80,81,84,85,86,95,102,104,105,106,107,108,113,213,216,217,218,221};
 nlohmann::json results=nlohmann::json::array();
 auto shot=[&](const std::string&name){toastUntil_=0;render();check(screenshot(directory/(name+".png")),"Location screenshot failed");};
 auto resolve=[&]{for(int i=0;scene==Scene::Level&&i<70;++i){check(!game.hero.levelChoices.empty(),"Empty level dialog");game.chooseSkill(game.hero.levelChoices.front().skills.front());scene=Scene::Adventure;settle();}};
 for(int cls:classes){
  check(originals.contains(cls),"Original location class absent: "+std::to_string(cls));game=baseline;game.objects.clear();game.decorations.clear();game.settlements.clear();game.activeTownId=0;game.result=0;game.scenarioRules["hadEnemy"]=false;game.scenarioRules["additional"]["victory_condition"]={{"type",255}};game.scenarioRules["additional"]["loss_condition"]={{"type",255}};
  game.hero.pos={18,18};game.mapLevel=0;game.hero.onBoat=false;game.hero.army={Stack{Archangel,2000},Stack{Pikeman,20}};game.hero.levelChoices.clear();game.hero.skills.fill(0);game.hero.skills[Wisdom]=3;game.hero.originalId=96;game.hero.spellbook=true;game.hero.artifactIds={7};game.hero.artifacts={artifactName(7)};game.hero.wornArtifacts.clear();game.hero.equipmentSlots.clear();game.resources={1000000,10000,10000,10000,10000,10000,10000};game.heroes.clear();game.heroes[game.activeHeroId]={game.hero,0,0};game.computerQueue.clear();
  for(auto&t:game.tiles){t.terrain=Terrain::Grass;t.decor=Decor::None;t.blocked=false;t.seen=true;t.road=t.river=false;t.originalView=-1;}
  Object site=originals.at(cls);site.id=90000+cls;site.type=ObjectType::Site;site.level=0;site.pos=game.hero.pos;const auto&geometry=adventureTemplate(site.sprite);site.anchor={site.pos.x-geometry.visitOffset.x,site.pos.y-geometry.visitOffset.y};site.owner=-1;site.visited=false;site.army={};if(!site.properties.is_object())site.properties=nlohmann::json::object();game.objects.push_back(site);
  if(cls==13||cls==29||cls==86)game.hero.onBoat=true;
  camX_=18.5f;camY_=18.5f;zoom_=1;scene=Scene::Adventure;visitPopup_.reset();locationObject_=0;locationSelected_=-1;route_.clear();destination_.reset();marching_=autoCombat_=false;battleMotion_.clear();mouseX_=mouseY_=-1;focus_=-1;
  render();click(mapScreen(game.hero.pos),false);resolve();check(scene==Scene::Visit&&visitPopup_,"Location did not open a visit dialog: "+std::to_string(cls));check(visitPopup_->message.find("完整规则尚未实现")==std::string::npos,"Location retained placeholder");
  shot("location-"+std::to_string(cls)+"-visit");auto state=game.toJson();render();check(game.toJson()==state,"Rendering changed location state");auto offers=game.locationOffers(site.id);
  if(offers.size()>4){render();click({640,438},false);check(locationPage_==1,"Location next page failed");shot("location-"+std::to_string(cls)+"-page2");click({158,438},false);check(locationPage_==0,"Location previous page failed");}
  auto enabled=std::find_if(offers.begin(),offers.end(),[](auto&f){return f.enabled;});bool performed=false;
  if(enabled!=offers.end()){
   int index=int(enabled-offers.begin());locationPage_=index/4;render();click({400,205+60*(index%4)},false);render();check(locationSelected_==index,"Location offer could not be selected");if(enabled->maximum>1){render();click({354,475},false);check(locationAmount_==enabled->maximum,"Location maximum quantity failed");}shot("location-"+std::to_string(cls)+"-selected");click({600,477},false);performed=true;
   if(scene==Scene::Battle){shot("location-"+std::to_string(cls)+"-battle");int turns=0;while(game.battle&&turns++<3000)game.ai();check(!game.battle&&game.report&&game.report->won,"Location guard fight stalled");game.report.reset();battleMotion_.clear();scene=Scene::Adventure;settle();}
   resolve();check(scene==Scene::Visit||scene==Scene::Market||scene==Scene::Adventure,"Location action did not settle");shot("location-"+std::to_string(cls)+"-result");
  }
  if(cls==106){
   visitPopup_.reset();scene=Scene::Adventure;for(int artifact:{5,6})game.locationAction(site.id,LocationAction::BuyMachine,artifact);game.takeAdventureVisit();
   Object foe;foe.id=99999;foe.type=ObjectType::Monster;foe.pos=foe.anchor={21,18};foe.level=0;foe.army={Stack{Pikeman,1000}};foe.name="守军";game.objects.push_back(foe);game.hero.skills[Artillery]=3;game.startBattle(foe.id);scene=Scene::Battle;battleMotion_.clear();game.takeEvents();shot("factory-war-machines-battle");check(game.validate(),"Factory battle state invalid");
   int turns=0;while(game.battle&&turns++<3000)game.ai();check(!game.battle&&game.report&&game.report->won,"Factory-equipped army cannot finish battle");game.report.reset();game.hero.levelChoices.clear();game.takeEvents();battleMotion_.clear();scene=Scene::Adventure;
  }
  if(scene==Scene::Visit){render();key(SDLK_RETURN,KMOD_NONE);}else if(scene==Scene::Market)key(SDLK_ESCAPE,KMOD_NONE);
  check(scene==Scene::Adventure,"Closing location did not resume exploration");
  results.push_back({{"sourceClass",cls},{"name",adventureSiteName(cls)},{"offers",offers.size()},{"performed",performed},{"originalSprite",site.sprite}});
 }
 std::ofstream(directory/"locations-ui-audit.json")<<nlohmann::json{{"classes",41},{"tests",results},{"controlledFixture",true},{"originalProgressWritten",false}}.dump(2)<<'\n';
}
}
