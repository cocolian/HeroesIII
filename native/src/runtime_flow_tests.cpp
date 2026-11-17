#include "h3/renderer.hpp"
#include "h3/battle_layout.hpp"
#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <iostream>

namespace h3 {
void Desktop::runtimePreview(const std::filesystem::path&directory){std::filesystem::create_directories(directory);saveDir_=directory/"test-saves";runtimeFlowRenderTests(directory);campaignFlowRenderTests(directory);std::cout<<"Runtime and practice campaign flow passed\n";}
void Desktop::runtimeFlowRenderTests(const std::filesystem::path& directory){
 auto check=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
 uint64_t clock=1000;
 auto advance=[&]{fixedAnimationTime_=clock+=100000;tick();};
 auto ready=[&]{
  for(int i=0;i<80;i++){
   if(!battleMotion_.active()&&(!game.battle||!game.current()||game.current()->team==0))break;
   advance();
  }
  check(!battleMotion_.active()&&(!game.battle||(game.current()&&game.current()->team==0)),"Runtime flow did not return control to the player");
  check(game.validate(),"Runtime flow produced invalid game state");
 };
 auto shot=[&](const char* name){toastUntil_=0;render();check(screenshot(directory/(std::string("runtime-")+name+".png")),"Runtime flow screenshot failed");};
 auto walk=[&](Point goal){
  check(scene==Scene::Adventure&&game.tile(goal)&&game.tile(goal)->seen,"Runtime route target has not been explored");
  render();click(mapScreen(goal),false);key(SDLK_m,KMOD_NONE);
  for(int i=0;i<100&&game.hero.pos!=goal&&!game.battle;i++){
   if(scene==Scene::Market){key(SDLK_ESCAPE,KMOD_NONE);key(SDLK_m,KMOD_NONE);}
   if(scene==Scene::Visit||scene==Scene::Puzzle){render();key(SDLK_RETURN,KMOD_NONE);}
   if(scene==Scene::Adventure&&!marching_){
    check(!game.path(goal).empty(),"Runtime target has no route around original object footprints");
    auto remaining=game.path(goal);if(game.hero.movement<remaining.front().cost)key(SDLK_e,KMOD_NONE);render();click(mapScreen(goal),false);key(SDLK_m,KMOD_NONE);
   }
   advance();
  }
  const auto*object=game.objectAt(goal);check(game.hero.pos==goal||(game.battle&&object&&object->type==ObjectType::Monster&&game.battle->targetId==object->id),"Runtime route did not reach its destination or its guard encounter");
  if(!game.battle){advance();if(scene==Scene::Visit||scene==Scene::Puzzle){render();key(SDLK_RETURN,KMOD_NONE);}}
  check(game.validate(),"Runtime movement produced invalid game state");
 };
 auto visiblePoint=[&](Point cell){
  auto*unit=game.combatAt(cell);check(unit,"Runtime target is absent");int id=unit->id;auto center=battleHexCenter(cell);auto*hit=battleTargetAt(center);if(hit&&hit->id==id)return center;
  auto visual=combatVisual(*unit);check(bool(visual),"Runtime target has no sprite");
  for(int y=std::max(0,visual->canvas.y);y<std::min(556,visual->canvas.y+visual->canvas.h);++y)for(int x=std::max(0,visual->canvas.x);x<std::min(800,visual->canvas.x+visual->canvas.w);++x){Point p{x,y};auto*at=battleTargetAt(p);if(at&&at->id==id&&combatBodyContains(*unit,p))return p;}
  throw std::runtime_error("Runtime target is fully occluded");
 };
 auto castFromBook=[&](Point bookCell,Point target){
  key(SDLK_c,KMOD_NONE);check(scene==Scene::Spells,"Runtime combat spellbook did not open");render();click(bookCell,false);
  check(scene==Scene::Battle&&pendingSpell_>=0,"Runtime spell selection failed");render();click(visiblePoint(target),false);ready();
 };
 battleMotion_.clear();game=Game::create();scene=Scene::Adventure;autoCombat_=false;
 route_.clear();destination_.reset();marching_=false;focus_=-1;mouseX_=mouseY_=-1;
 camX_=4.5f;camY_=20.5f;zoom_=1;fixedAnimationTime_=clock;
 const int initialSeen=int(std::count_if(game.tiles.begin(),game.tiles.end(),[](const auto& tile){return tile.seen;}));
 walk({7,21});check(game.resources[Gold]==11500&&!game.objectAt({7,21}),"Runtime treasure was not collected exactly once");shot("chest-collected");
 walk({4,20});render();click(mapScreen(game.hero.pos),false);check(scene==Scene::Town,"Runtime hero did not enter town");
 key(SDLK_h,KMOD_NONE);render();click({400,180},false);check(scene==Scene::BuildConfirm,"Runtime guild construction confirmation did not open");
 render();click({310,490},false);check(game.town.built[MageGuild]&&game.resources[Gold]==9500&&game.resources[Wood]==15&&game.resources[Ore]==15,"Runtime guild construction charged incorrect resources");
 townBuilding(MageGuild);check(scene==Scene::MageGuild,"Runtime guild service did not open");render();click({470,530},false);
 check(game.hero.spellbook&&game.hero.spells[MagicArrow]&&game.resources[Gold]==9000,"Runtime spellbook purchase or guild learning failed");shot("guild-learned");
 key(SDLK_ESCAPE,KMOD_NONE);key(SDLK_ESCAPE,KMOD_NONE);
 walk({8,18});check(game.objectAt({8,18})->owner==0,"Runtime mine capture failed");walk({11,20});check(game.resources[Ore]==23,"Runtime resource pickup failed");
 int priorDay=game.day,priorGold=game.resources[Gold],priorWood=game.resources[Wood];key(SDLK_e,KMOD_NONE);check(game.day==priorDay+1&&game.resources[Gold]==priorGold+1000&&game.resources[Wood]==priorWood+2&&game.hero.movement==1560,"Runtime day income or movement reset failed");
 key(SDLK_SPACE,KMOD_NONE);walk({14,19});int treasureGold=game.resources[Gold];walk({16,17});check(game.resources[Gold]==treasureGold+2000,"Runtime second treasure failed");
 key(SDLK_SPACE,KMOD_NONE);check(frameCount("ava0008.def")==1,"Runtime sword map sprite is missing");shot("artifact-visible");
 walk({21,16});check(game.hero.attack==4&&game.hero.artifacts.size()==1&&!game.objectAt({21,16}),"Runtime artifact pickup or attack bonus failed");
 key(SDLK_h,KMOD_NONE);shot("artifact-collected");key(SDLK_ESCAPE,KMOD_NONE);
 auto artifactState=game.toJson();key(SDLK_s,KMOD_NONE);key(SDLK_l,KMOD_NONE);check(game.toJson()==artifactState,"Runtime artifact did not survive save/load");
 key(SDLK_SPACE,KMOD_NONE);check(game.hero.attack==4&&game.hero.artifacts.size()==1,"Runtime artifact bonus was applied twice");
 key(SDLK_e,KMOD_NONE);walk({19,12});check(scene==Scene::Battle&&game.battle&&game.battle->round==1,"Runtime map encounter did not enter battle");
 auto battleStart=game.toJson();key(SDLK_c,KMOD_NONE);render();click({200,135},false);key(SDLK_ESCAPE,KMOD_NONE);
 check(game.toJson()==battleStart,"Runtime cancelled spell changed combat or mana");
 key(SDLK_c,KMOD_NONE);render();click({200,135},false);SDL_Event cancel{};cancel.type=SDL_MOUSEBUTTONDOWN;cancel.button.button=SDL_BUTTON_RIGHT;handleNativeEvent(cancel,true);check(pendingSpell_==-1&&game.toJson()==battleStart,"Runtime right-click spell cancellation changed combat");
 key(SDLK_c,KMOD_NONE);render();click({200,135},false);render();click(battleHexCenter({1,8}),false);
 check(game.toJson()==battleStart&&pendingSpell_==MagicArrow,"Runtime invalid spell target changed combat or lost selection");
 click(battleHexCenter({13,5}),false);ready();
 check(game.hero.mana==15&&game.battle->units.back().hp==180&&game.battle->spellRound==1,"Runtime Magic Arrow damage or payment failed");shot("magic-arrow");
 auto afterArrow=game.toJson();key(SDLK_s,KMOD_NONE);key(SDLK_l,KMOD_NONE);check(game.toJson()==afterArrow,"Runtime battle save/load changed RNG, mana, units or queue");
 key(SDLK_c,KMOD_NONE);render();click({200,135},false);check(scene==Scene::Spells&&game.toJson()==afterArrow,"Runtime repeat spell was not disabled");key(SDLK_ESCAPE,KMOD_NONE);
 render();click(battleHexCenter({5,7}),false);ready();key(SDLK_d,KMOD_NONE);ready();
 render();click(battleHexCenter({7,5}),false);ready();check(game.battle&&game.battle->round==2&&game.battle->units[1].shots==11,"Runtime ranged action or next round failed");
 castFromBook({310,135},{5,7});check(game.hero.mana==10&&game.battle->units[2].effects[Shield]>0,"Runtime Shield effect or payment failed");shot("shield");
 int combatRounds=2,spellsCast=2;
 // Follow the live enemy position: the tactical AI no longer stays on (7,5).
 for(int action=0;action<200&&game.battle;++action){
  ready();if(!game.battle)break;combatRounds=game.battle->round;
  auto enemy=std::find_if(game.battle->units.begin(),game.battle->units.end(),[](const auto&u){return u.team==1&&u.hp>0;});
  check(enemy!=game.battle->units.end(),"Runtime battle has no living opponent");
  if(game.hero.mana>=game.spellCost(MagicArrow)&&game.battle->spellRound!=game.battle->round){Point target{enemy->x,enemy->y};castFromBook({200,135},target);++spellsCast;if(!game.battle)break;}
  auto*actor=game.current();check(actor&&actor->team==0,"Runtime combat lost human control");
  auto target=std::find_if(game.battle->units.begin(),game.battle->units.end(),[&](const auto&u){return u.team==1&&u.hp>0&&game.attackPlan(*actor,u).has_value();});
  if(target!=game.battle->units.end()){Point at{target->x,target->y};render();click(visiblePoint(at),false);}else key(SDLK_d,KMOD_NONE);
  ready();
 }
 std::ofstream(directory/"runtime-finish-state.json")<<game.toJson().dump(2);
 check(!game.battle&&game.report&&game.report->won&&game.report->xp==200&&game.hero.xp==200&&spellsCast>=3&&game.hero.mana<=5,"Runtime spell and attack victory did not settle into a report");
 check(std::equal(game.report->after.begin(),game.report->after.end(),game.hero.army.begin(),[](const Stack&a,const Stack&b){return a.unit==b.unit&&a.count==b.count;})&&armyCount(game.hero.army)>0&&!game.objectAt({19,12}),"Runtime casualties or defeated map guard did not persist");
 auto beforeReportDraw=game.toJson();shot("victory-report");check(game.toJson()==beforeReportDraw,"Report drawing changed committed casualties or victory");check(buttons_.size()==1&&buttons_.front().contains({582,542}),"Runtime report must expose only its acknowledgement control");
 auto reportState=game.toJson();click({730,35},false);check(game.toJson()==reportState&&scene==Scene::Report,"Runtime report has a hidden dismissal action");
 mouseX_=582;mouseY_=542;click({582,542},false);
 check(scene==Scene::Adventure&&!game.report&&mapPoint(mapScreen(game.hero.pos))==game.hero.pos,"Runtime report did not return to the hero on the map");
 check(mouseX_<0&&mouseY_<0&&scrollTick_==0,"Runtime report left the old modal pointer in the map edge scroll region");
 float cameraX=camX_,cameraY=camY_;advance();check(camX_==cameraX&&camY_==cameraY,"Runtime report exit scrolled the map without fresh pointer input");shot("map-resumed");
 walk({19,12});int movement=game.hero.movement;walk({20,12});check(game.hero.movement==movement-75&&game.hero.xp==200&&game.hero.attack==4,"Runtime post-battle movement or progression failed");shot("after-victory-move");
 auto finalState=game.toJson();key(SDLK_s,KMOD_NONE);key(SDLK_l,KMOD_NONE);check(game.toJson()==finalState&&!game.report&&!game.battle,"Runtime post-battle save/load failed");
 const int finalSeen=int(std::count_if(game.tiles.begin(),game.tiles.end(),[](const auto& tile){return tile.seen;}));check(finalSeen>initialSeen,"Runtime exploration did not reveal fog");
 std::ofstream(directory/"runtime-flow-audit.json")<<nlohmann::json{{"seed",1999},{"naturalAdventureProgression",true},{"initialSeenTiles",initialSeen},{"finalSeenTiles",finalSeen},{"day",game.day},{"resources",game.resources},{"artifactAttackBefore",2},{"artifactAttackAfter",game.hero.attack},{"artifactCount",game.hero.artifacts.size()},{"manaSpent",20-game.hero.mana},{"spellsCast",spellsCast},{"battleRounds",combatRounds},{"experience",game.hero.xp},{"survivingArmyCounts",{game.hero.army[0].count,game.hero.army[1].count,game.hero.army[2].count}},{"cancelledAndInvalidSpellStateUnchanged",true},{"battleSaveLoadStateUnchanged",true},{"defeatedGuardRemoved",true},{"postBattleMovementCost",75},{"reportExitCameraStable",true},{"postBattleSaveLoadStateUnchanged",true}}.dump(2)<<'\n';
 game=Game::create();game.gainXP(1000);scene=Scene::Level;shot("level-choice");check(buttons_.size()==game.hero.levelChoices.front().skills.size(),"Runtime level window exposes an extra dismissal control");
 key(SDLK_ESCAPE,KMOD_NONE);check(scene==Scene::Level&&!game.hero.levelChoices.empty(),"Runtime level choice can be skipped");render();click({200,410},false);check(game.hero.levelChoices.empty()&&scene==Scene::Adventure,"Runtime level choice did not resume adventure");
 battleMotion_.clear();game=Game::create();scene=Scene::Town;fixedAnimationTime_=0;route_.clear();destination_.reset();marching_=false;mouseX_=mouseY_=-1;
}
}
