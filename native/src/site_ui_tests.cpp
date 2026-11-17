#include "h3/renderer.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace h3 {
void Desktop::sitePreview(const std::filesystem::path&directory,const std::filesystem::path&resume){
 auto check=[](bool ok,const char*message){if(!ok)throw std::runtime_error(message);};
 std::filesystem::create_directories(directory);saveDir_=directory/"test-saves";settings_=ClientSettings{};settings_.hdArtwork=false;fixedAnimationTime_=1000;
 SDL_SetWindowSize(window_,1280,800);zoom_=1;mouseX_=mouseY_=-1;focus_=-1;route_.clear();destination_.reset();marching_=autoCombat_=false;visitPopup_.reset();battleMotion_.clear();
 auto shot=[&](const std::string&name){toastUntil_=0;render();check(screenshot(directory/(name+".png")),"Site screenshot failed");};
 auto ack=[&]{render();key(SDLK_RETURN,KMOD_NONE);check(scene==Scene::Adventure&&!visitPopup_,"Visit acknowledgement did not resume exploration");};
 std::string error;bool realPosition=!resume.empty();
 if(realPosition)check(game.load(resume,error),"Reported position cannot be loaded");
 else{check(game.importScenario(root_/"assets/demo/scenarios/wayfarer-1.json",error),"Wayfarer could not import");auto*ring=game.objectById(9979);check(ring&&ring->sourceClass==28,"Original Wayfarer ring is missing");game.hero.pos=ring->pos;game.mapLevel=ring->level;game.reveal();}
 auto*ring=game.objectAt(game.hero.pos);check(ring&&ring->sourceClass==28,"Reported hero is not at the faerie ring");int ringId=ring->id;
 camX_=game.hero.pos.x+.5f;camY_=game.hero.pos.y+.5f;scene=Scene::Adventure;shot("reported-ring-before");auto original=game.toJson();
 click(mapScreen(game.hero.pos),false);check(scene==Scene::Visit&&visitPopup_&&visitPopup_->title=="仙女环"&&visitPopup_->luck==1&&game.armyLuck()==1,"Reported ring still shows a placeholder or grants no luck");shot("reported-ring-reward");ack();
 render();click(mapScreen(game.hero.pos),false);check(scene==Scene::Visit&&game.armyLuck()==1&&game.hero.adventureBonuses.size()==1,"Repeated ring click stacked the bonus");shot("reported-ring-repeat");ack();
 key(SDLK_h,KMOD_NONE);check(scene==Scene::Hero,"Hero details did not open");shot("reported-hero-luck");key(SDLK_ESCAPE,KMOD_NONE);scene=Scene::Adventure;
 // A separate controlled battle fixture verifies the bonus lifetime; this is not a campaign replay.
 game.hero.army={Stack{Marksman,100}};Object enemy;enemy.id=90001;enemy.pos=game.hero.pos;enemy.level=game.mapLevel;enemy.type=ObjectType::Monster;enemy.army={Stack{Skeleton,1}};game.objects.push_back(enemy);game.startBattle(enemy.id);scene=Scene::Battle;check(game.armyLuck()==1,"Luck was lost before battle");shot("site-battle-luck-active");int turns=0;while(game.battle&&turns++<100)game.ai();check(game.report&&game.report->won&&game.armyLuck()==0,"Luck did not expire when the battle ended");game.report.reset();game.hero.levelChoices.clear();battleMotion_.clear();scene=Scene::Hero;shot("site-battle-luck-expired");scene=Scene::Adventure;game.visit(ringId);settle();check(scene==Scene::Visit&&game.armyLuck()==1,"Ring could not be revisited after battle");shot("site-ring-after-battle");ack();
 for(int cls:{11,14,28,30,31,38,48,52,55,56,64,94,96,110}){
  check(game.importScenario(root_/"assets/demo/scenarios/dead-and-buried-1.json",error,8,131,4),"Site fixture import failed");
  Object site;site.id=90000+cls;site.pos=game.hero.pos;site.level=game.mapLevel;site.type=ObjectType::Site;site.sourceClass=cls;site.name=adventureSiteName(cls);site.properties=nlohmann::json::object();game.objects.push_back(site);if(cls==48)game.hero.mana=1;
  game.visit(site.id);scene=Scene::Adventure;camX_=game.hero.pos.x+.5f;camY_=game.hero.pos.y+.5f;visitPopup_.reset();settle();check(scene==Scene::Visit&&visitPopup_&&visitPopup_->message.find("尚未实现")==std::string::npos,"A supported landmark still uses the placeholder");shot("site-"+std::to_string(cls));ack();
 }
 std::ofstream(directory/"site-ui-audit.json")<<nlohmann::json{{"reportedPositionLoaded",realPosition},{"reportedScenario",original["scenarioId"]},{"object",ringId},{"ringMouseVisit",true},{"repeatDoesNotStack",true},{"heroLuckVisible",true},{"battleBonusConsumed",true},{"postBattleRevisit",true},{"supportedSiteClasses",{11,14,28,30,31,38,48,52,55,56,64,94,96,110}},{"originalSaveWritten",false},{"combatFixtureUsesControlledArmy",true}}.dump(2)<<'\n';
 std::cout<<"Reported ring and 14 adventure landmarks UI passed\n";
}
}
