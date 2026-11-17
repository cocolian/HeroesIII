#include "h3/renderer.hpp"
#include "h3/battle_layout.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace h3 {
void Desktop::interactionPreview(const std::filesystem::path&directory){std::filesystem::create_directories(directory);saveDir_=directory/"test-saves";scenarioRenderTests(directory);pointerRenderTests(directory);adventureVisitRenderTests(directory);cursorRenderTests(directory);combatCursorRenderTests(directory);outcomeRenderTests(directory);hintRenderTests(directory);encounterRenderTests(directory);runtimeFlowRenderTests(directory);campaignFlowRenderTests(directory);std::cout<<"Interaction, runtime and campaign flow passed\n";}
void Desktop::hintRenderTests(const std::filesystem::path&directory){
 auto check=[](bool ok,const std::string&message){if(!ok)throw std::runtime_error(message);};
 int width,height;SDL_GetWindowSize(window_,&width,&height);SDL_SetWindowSize(window_,1000,750);
 nlohmann::json coverage=nlohmann::json::array();uint64_t clock=1000;
 auto setup=[&](Scene target){
  cancelPointer();battleMotion_.clear();game=Game::create();settings_=ClientSettings{};scene=target;menuView_=MenuView::Main;
  game.town.built.fill(true);game.hero.spellbook=true;game.hero.spells.fill(true);spellAdventure_=false;spellFilter_=spellPage_=0;
  route_.clear();destination_.reset();marching_=autoCombat_=false;pendingSpell_=sacrificeTarget_=-1;focus_=-1;mouseX_=mouseY_=-1;toastUntil_=0;fixedAnimationTime_=clock+=1000;
  sourceSlot_=0;targetSlot_=1;sourceGarrison_=targetGarrison_=false;splitAmount_=1;splitReturn_=armyReturn_=Scene::Town;
  recruitReturn_=Scene::Town;recruitUnit_=Pikeman;recruitAmount_=1;recruitGarrison_=false;
  selectedBuilding_=Marketplace;buildingReturn_=Scene::Build;optionsReturn_=Scene::Town;marketReturn_=Scene::Town;marketObject_=0;marketSold_=Wood;marketBought_=Ore;marketLots_=0;
  if(target==Scene::Maps)selectMap(0);
  if(target==Scene::Battle)game.startBattle(15);
  if(target==Scene::Outcome)game.result=1;
  visitPopup_.reset();if(target==Scene::Visit)visitPopup_=AdventureVisit{ObjectType::Well,"魔法井","井水已恢复魔法值。"};
  if(target==Scene::Level)game.gainXP(1000);
  if(target==Scene::Report){game.report.emplace();game.report->won=true;game.report->before=game.report->after=game.hero.army;}
 };
 for(int sceneIndex=0;sceneIndex<=int(Scene::Maps);++sceneIndex){
  auto target=Scene(sceneIndex);setup(target);render();check(scene==target,"Help coverage fixture changed scenes: "+std::to_string(sceneIndex));
  check(!buttons_.empty(),"Scene has no controls: "+std::to_string(sceneIndex));int disabled=0;
  for(size_t i=0;i<buttons_.size();++i){check(!buttons_[i].hint.empty(),"Missing button help in scene "+std::to_string(sceneIndex)+", button "+std::to_string(i));disabled+=!buttons_[i].enabled;}
  coverage.push_back({{"scene",sceneIndex},{"buttons",buttons_.size()},{"disabled",disabled}});
 }
 auto hover=[&](Point p){movePointer(p);fixedAnimationTime_=clock+=1000;render();check(!hintBounds_,"Hover help appeared without a dwell");fixedAnimationTime_=clock+=500;render();check(hintBounds_.has_value(),"Hover help did not appear after dwell");auto r=*hintBounds_;auto size=canvasLogicalSize();check(r.x>=0&&r.y>=0&&r.x+r.w<=size.x&&r.y+r.h<=size.y,"Help overflowed the canvas");};
 auto shot=[&](const char*name){check(screenshot(directory/(std::string("help-")+name+".png")),"Help screenshot failed");};
 setup(Scene::Adventure);render();auto before=game.toJson();auto r=buttons_.back().rect;hover({r.x+r.w/2,r.y+r.h/2});shot("adventure");check(game.toJson()==before,"Hover changed gameplay or RNG");
 setup(Scene::Options);render();hover({415,155});check(hintText_.find("尚未实现")!=std::string::npos&&hintText_.find("不可用")!=std::string::npos,"Disabled option did not explain its state");shot("disabled-option");
 setup(Scene::Battle);render();before=game.toJson();SDL_Event e{};e.type=SDL_MOUSEBUTTONDOWN;e.button.button=SDL_BUTTON_RIGHT;e.button.x=667;e.button.y=578;handleEvent(e);render();check(rightHelp_&&hintBounds_&&game.toJson()==before&&scene==Scene::Battle,"Right-click help activated the combat control");shot("battle-right-click");
 e.type=SDL_MOUSEBUTTONUP;handleEvent(e);render();check(!rightHelp_&&!hintBounds_&&game.toJson()==before,"Right release activated a control or retained its popup");
 pendingSpell_=MagicArrow;e.type=SDL_MOUSEBUTTONDOWN;handleEvent(e);check(pendingSpell_<0&&!rightHelp_&&game.toJson()==before,"Right-click no longer cancels spell targeting");
 setup(Scene::Market);marketLots_=1;render();auto expected=game;expected.trade(marketSold_,marketBought_,marketLots_,marketObject_);key(SDLK_RETURN,KMOD_NONE);check(scene==Scene::Market&&marketLots_==0&&game.toJson()==expected.toJson(),"Market Enter did not perform the advertised exchange");
 setup(Scene::Battle);render();before=game.toJson();key(SDLK_a,KMOD_NONE);check(autoCombat_&&game.toJson()==before,"Auto combat shortcut advanced combat before ticking");key(SDLK_a,KMOD_NONE);check(!autoCombat_&&game.toJson()==before,"Auto combat shortcut failed to stop");
 setup(Scene::Recruit);render();hover({recruitTrack().x+recruitTrack().w/2,recruitTrack().y+8});shot("recruit-slider");
 setup(Scene::Spells);render();hover({200,135});shot("spell-description");
 setup(Scene::Adventure);SDL_SetWindowSize(window_,1512,982);render();r=buttons_.back().rect;hover({r.x+r.w-2,r.y+r.h-2});shot("wide-map-edge");
 drag_=false;dragged_=true;fixedAnimationTime_=clock+=100;render();check(hintBounds_.has_value(),"Finished map drag suppressed button help");
 SDL_Event lost{};lost.type=SDL_WINDOWEVENT;lost.window.event=SDL_WINDOWEVENT_FOCUS_LOST;handleEvent(lost);check(!rightHelp_&&hintText_.empty(),"Focus loss retained help capture");
 std::ofstream(directory/"button-help-audit.json")<<nlohmann::json{{"sceneCoverage",coverage},{"hoverDelayMs",450},{"rightClickDoesNotActivate",true},{"disabledControlsExplainState",true},{"spellCancelPreserved",true},{"helpClampedToCanvas",true},{"hoverPreservesGameAndRng",true}}.dump(2)<<'\n';
 cancelPointer();hintBounds_.reset();game=Game::create();scene=Scene::Town;settings_=ClientSettings{};fixedAnimationTime_=0;mouseX_=mouseY_=-1;sourceSlot_=-1;SDL_SetWindowSize(window_,width,height);
}

void Desktop::encounterRenderTests(const std::filesystem::path&directory){
 auto check=[](bool ok,const char*message){if(!ok)throw std::runtime_error(message);};
 uint64_t clock=1000;auto advance=[&]{fixedAnimationTime_=clock+=100000;tick();};
 auto setup=[&]{
  cancelPointer();battleMotion_.clear();game=Game::create();scene=Scene::Adventure;settings_=ClientSettings{};settings_.hdArtwork=false;
  for(auto&t:game.tiles){t.seen=true;t.decor=Decor::None;t.road=false;t.terrain=Terrain::Grass;}
  game.objects.clear();game.hero.pos={10,10};game.hero.army={Stack{Archer,100}};game.hero.spellbook=true;game.hero.spells[MagicArrow]=true;
  Object guard;guard.id=100;guard.pos={13,10};guard.type=ObjectType::Monster;guard.owner=-1;guard.army={Stack{Skeleton,1}};guard.name="骷髅守军";game.objects.push_back(guard);
  Object treasure;treasure.id=101;treasure.pos={12,10};treasure.type=ObjectType::Treasure;treasure.resource=Gold;treasure.amount=750;treasure.name="守卫宝箱";game.objects.push_back(treasure);
  camX_=12.5f;camY_=10.5f;zoom_=1;route_.clear();destination_.reset();marching_=autoCombat_=false;pendingSpell_=sacrificeTarget_=-1;mouseX_=mouseY_=-1;nextStep_=nextAI_=0;fixedAnimationTime_=clock;toastUntil_=0;
 };
 auto shot=[&](const char*name){toastUntil_=0;render();check(screenshot(directory/(std::string("encounter-")+name+".png")),"Encounter screenshot failed");};
 setup();render();movePointer(mapScreen({12,10}));check(adventureCursorFrame()==5,"Guarded treasure must advertise combat with the original sword cursor");
 click(mapScreen({12,10}),false);check(destination_==Point{12,10}&&route_.size()==2,"Guarded treasure route missing");shot("guarded-treasure-route");
 auto gold=game.resources[Gold];key(SDLK_m,KMOD_NONE);for(int i=0;i<10&&marching_;++i)advance();
 check(scene==Scene::Battle&&game.battle&&game.battle->targetId==100&&game.battle->deferredVisitId==101&&(game.hero.pos==Point{12,10})&&!marching_&&route_.empty()&&game.resources[Gold]==gold,"Walking into a guard zone did not interrupt before pickup");shot("adjacent-interception");
 key(SDLK_c,KMOD_NONE);render();click({200,135},false);check(pendingSpell_==MagicArrow&&scene==Scene::Battle,"Encounter spell selection failed");
 render();auto enemy=game.battle->units.back();click(battleHexCenter({enemy.x,enemy.y}),false);
 for(int i=0;i<80&&(game.battle||battleMotion_.active());++i)advance();
 check(scene==Scene::Report&&!game.battle&&game.report&&game.report->won&&game.hero.mana==15&&game.resources[Gold]==gold+750&&!game.objectById(100)&&!game.objectById(101)&&game.validate(),"Spell victory failed to commit guard removal and deferred treasure");shot("victory-and-loot");
 render();click({582,542},false);check(scene==Scene::Adventure&&!game.report,"Encounter report did not return to exploration");
 render();click(mapScreen({14,10}),false);key(SDLK_m,KMOD_NONE);for(int i=0;i<10&&marching_;++i)advance();check((game.hero.pos==Point{14,10})&&!game.battle,"Exploration did not resume across the defeated guard");shot("exploration-resumed");
 setup();game.hero.pos={12,10};std::erase_if(game.objects,[](const auto&o){return o.id==101;});render();click(mapScreen({13,10}),false);key(SDLK_m,KMOD_NONE);advance();
 check(scene==Scene::Battle&&game.battle&&(game.hero.pos==Point{12,10})&&!marching_&&route_.empty(),"Direct adjacent attack stalled in adventure view");shot("direct-attack");
 std::ofstream(directory/"encounter-flow-audit.json")<<nlohmann::json{{"guardedObjectRequiresVictory",true},{"adjacentGuardInterruptsMarch",true},{"directAttackPreservesHeroTile",true},{"spellbookToVictory",true},{"deferredGold",750},{"explorationResumesAfterReport",true},{"originalSwordCursorFrame",5}}.dump(2)<<'\n';
 cancelPointer();battleMotion_.clear();game=Game::create();scene=Scene::Town;settings_=ClientSettings{};fixedAnimationTime_=0;mouseX_=mouseY_=-1;route_.clear();destination_.reset();marching_=false;
}
}
