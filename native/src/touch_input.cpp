#include "h3/renderer.hpp"
#include "h3/battle_layout.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace h3 {
void Desktop::cancelPointer(bool clearTouch){
 restoreSystemCursor();rightHelp_=false;hintText_.clear();
 mouseLeftDown_=minimapDragging_=marketDragging_=recruitDragging_=drag_=false;
 pressedButton_=-1;armyDrag_.reset();optionsDragging_=-1;nativePointer_.reset();
 battleTouch_.reset();
 if(clearTouch){touchPoints_.clear();touchPrimary_.reset();touchPinching_=false;touchDistance_=0;}
}
void Desktop::touchEvent(const SDL_TouchFingerEvent& e){
 int width=0,height=0;SDL_GetWindowSize(window_,&width,&height);
 Point raw{std::clamp(int(std::lround(e.x*width)),0,width-1),std::clamp(int(std::lround(e.y*height)),0,height-1)};
 if(e.type==SDL_FINGERDOWN){
  touchPoints_[e.fingerId]=raw;
  if(touchPoints_.size()==1&&!touchPinching_){touchPrimary_=e.fingerId;touchOrigin_=raw;touchStarted_=e.timestamp;}
  else{cancelPointer(false);touchPrimary_.reset();touchPinching_=true;touchDistance_=0;}
 }else{
  auto found=touchPoints_.find(e.fingerId);if(found==touchPoints_.end())return;found->second=raw;
 }
 if(touchPinching_){
  if(touchPoints_.size()==2&&scene==Scene::Adventure){
   auto a=touchPoints_.begin(),b=std::next(a);double distance=std::hypot(a->second.x-b->second.x,a->second.y-b->second.y);
   if(e.type==SDL_FINGERMOTION&&touchDistance_>1&&distance>1)zoom_=std::clamp(float(zoom_*std::clamp(distance/touchDistance_,.8,1.25)),.65f,1.8f);
   touchDistance_=distance;
  }
  if(e.type==SDL_FINGERUP){touchPoints_.erase(e.fingerId);if(touchPoints_.empty()){touchPinching_=false;touchDistance_=0;}}
  return;
 }
 if(!touchPrimary_||*touchPrimary_!=e.fingerId)return;
 Point p=windowPoint(raw);
 if(e.type==SDL_FINGERDOWN&&scene==Scene::Battle&&game.battle&&
    !battleMotion_.active()&&!autoCombat_&&game.battle->tacticsTeam<0&&
    pendingSpell_<0&&!pendingCreature_){
  const auto* actor=game.current();
  if(actor&&actor->team==0&&actor->unit!=CatapultUnit){
   const auto* target=battleTargetAt(p,true);
   if(target&&target->team==1)
    battleTouch_=BattleTouch{actor->id,target->id,game.battle->round,p};
  }
 }
 if(battleTouch_){
  auto& press=*battleTouch_;
  constexpr int tapSlop=10;
  press.dragged|=std::hypot(p.x-press.origin.x,p.y-press.origin.y)>tapSlop;
  if(e.type==SDL_FINGERUP){
   auto captured=press;
   battleTouch_.reset();
   touchPoints_.erase(e.fingerId);
   touchPrimary_.reset();
   mouseX_=mouseY_=-1;
   const auto* actor=game.current();
   if(!captured.dragged&&scene==Scene::Battle&&game.battle&&actor&&
      actor->id==captured.actor&&game.battle->round==captured.round&&
      pendingSpell_<0&&!pendingCreature_){
    const auto& units=game.battle->units;
    auto target=std::find_if(units.begin(),units.end(),[&](const CombatStack& unit){
     return unit.id==captured.target&&unit.team==1&&unit.hp>0;
    });
    if(target!=units.end())battleCellAction({target->x,target->y});
   }
  }
  return;
 }
 SDL_Event mouse{};
 bool held=e.type==SDL_FINGERUP&&Uint32(e.timestamp-touchStarted_)>=550&&std::hypot(raw.x-touchOrigin_.x,raw.y-touchOrigin_.y)<9;
 auto previous=pointerModifiers_;pointerModifiers_=held&&(scene==Scene::Army||scene==Scene::Town)?KMOD_SHIFT:KMOD_NONE;
 if(e.type==SDL_FINGERMOTION){mouse.type=SDL_MOUSEMOTION;mouse.motion.x=p.x;mouse.motion.y=p.y;}
 else{mouse.type=e.type==SDL_FINGERDOWN?SDL_MOUSEBUTTONDOWN:SDL_MOUSEBUTTONUP;mouse.button.button=SDL_BUTTON_LEFT;mouse.button.x=p.x;mouse.button.y=p.y;}
 if(held&&scene==Scene::Battle&&(pendingSpell_>=0||pendingCreature_)){cancelPointer(false);mouse.type=SDL_MOUSEBUTTONDOWN;mouse.button.button=SDL_BUTTON_RIGHT;}
 handleEvent(mouse);pointerModifiers_=previous;
 if(e.type==SDL_FINGERUP){touchPoints_.erase(e.fingerId);touchPrimary_.reset();mouseX_=mouseY_=-1;}
}
void Desktop::touchRenderTests(const std::filesystem::path& directory){
 auto check=[](bool ok,const char*message){if(!ok)throw std::runtime_error(message);};
 int oldW=0,oldH=0;SDL_GetWindowSize(window_,&oldW,&oldH);SDL_SetWindowSize(window_,1133,744);
 auto raw=[&](Point p){auto layout=displayLayout(1133,744,1133,744,scene==Scene::Adventure);return Point{int(std::lround(layout.pointX+(p.x+.5)*layout.pointScale)),int(std::lround(layout.pointY+(p.y+.5)*layout.pointScale))};};
 Uint32 time=100;auto finger=[&](Uint32 type,SDL_FingerID id,Point p,int delay=50){SDL_Event e{};e.type=type;e.tfinger.type=type;e.tfinger.fingerId=id;e.tfinger.timestamp=time+=delay;e.tfinger.x=float(p.x)/1133;e.tfinger.y=float(p.y)/744;handleNativeEvent(e);};
 auto tap=[&](Point p,int duration=50){auto r=raw(p);finger(SDL_FINGERDOWN,1,r);finger(SDL_FINGERUP,1,r,duration);};
 game=Game::create();battleMotion_.clear();scene=Scene::Menu;fixedAnimationTime_=1000;nextStep_=nextAI_=scrollTick_=0;cancelPointer();render();tap({600,80});check(scene==Scene::Scenario,"Touch New Game failed");render();tap({630,530});check(scene==Scene::Adventure,"Touch scenario start failed");
 camX_=4.5;camY_=20.5;zoom_=1;render();auto tile=mapScreen({7,21});tap(tile);tap(tile);check(marching_&&!route_.empty(),"Touch route selection/second tap did not march");
 for(int step=0;step<30&&marching_;step++){fixedAnimationTime_=*fixedAnimationTime_+10000;tick();}
 check(game.hero.pos==Point{7,21}&&game.resources[Gold]==11500,"Touch exploration/chest pickup failed");
 auto state=game.toJson();route_.clear();destination_.reset();render();auto map=adventureMapRect();Point start{map.x+map.w/2,map.y+map.h/2};auto camera=mapCamera();finger(SDL_FINGERDOWN,1,start);finger(SDL_FINGERUP,1,{start.x-64,start.y});render();check(!drag_&&!mouseLeftDown_&&std::abs(mapCamera()[0]-camera[0])>.5&&game.toJson()==state,"Touch final release did not pan map cleanly");
 auto mini=adventureMinimapRect();finger(SDL_FINGERDOWN,1,{mini.x+mini.w/4,mini.y+mini.h/4});finger(SDL_FINGERUP,1,{mini.x+mini.w*3/4,mini.y+mini.h*3/4});check(!minimapDragging_,"Touch minimap retained capture");
 finger(SDL_FINGERDOWN,1,start);finger(SDL_FINGERDOWN,2,{start.x+100,start.y});float z=zoom_;finger(SDL_FINGERMOTION,2,{start.x+120,start.y});finger(SDL_FINGERUP,1,start);finger(SDL_FINGERUP,2,{start.x+120,start.y});check(zoom_>z&&touchPoints_.empty()&&!drag_&&!mouseLeftDown_&&game.toJson()==state,"Pinch triggered a map click or did not zoom");
 toastUntil_=0;render();check(screenshot(directory/"touch-ipad-mini-map.png"),"Touch map screenshot failed");
 game=Game::create();scene=Scene::Army;armyReturn_=Scene::Adventure;sourceSlot_=-1;render();tap({80,195});render();tap({320,195},650);check(scene==Scene::Split&&splitAmount_==14,"Long press did not open touch army splitting");toastUntil_=0;render();check(screenshot(directory/"touch-split.png"),"Touch split screenshot failed");key(SDLK_ESCAPE,KMOD_NONE);
 game=Game::create();game.hero.spellbook=true;game.hero.spells[MagicArrow]=true;game.startBattle(15);scene=Scene::Battle;pendingSpell_=MagicArrow;battleMotion_.clear();render();int mana=game.hero.mana;tap({400,300},650);check(pendingSpell_<0&&game.hero.mana==mana,"Touch hold failed to cancel spell without payment");toastUntil_=0;render();check(screenshot(directory/"touch-cancel-spell.png"),"Touch spell cancel screenshot failed");

 auto prepareAttack = [&](int unit, Point enemy) {
  cancelPointer();
  battleMotion_.clear();
  game = Game::create();
  game.hero.army = {Stack{unit, 20}};
  game.objectById(15)->army = {Stack{Skeleton, 100}};
  game.startBattle(15);
  auto& battle = *game.battle;
  battle.obstacles.clear();
  battle.units[0].x = 5;
  battle.units[0].y = 5;
  battle.units[1].x = enemy.x;
  battle.units[1].y = enemy.y;
  battle.queue = {0, 100};
  game.takeEvents();
  scene = Scene::Battle;
  pendingSpell_ = -1;
  pendingCreature_ = false;
  autoCombat_ = false;
  render();
  return battle.units[1].hp;
 };
 auto enemyPlate = [](Point cell) {
  auto center = battleHexCenter(cell);
  return Point{center.x - 37, center.y + 7};
 };
 int hp = prepareAttack(Pikeman, {7,5});
 auto plate = enemyPlate({7,5});
 check(battleHexAt(plate) != Point{7,5}, "Touch attack fixture must hit a neighboring hex");
 tap(plate);
 check(game.battle->units[1].hp < hp && battleMotion_.active(),
       "Tapping the enemy count moved instead of attacking");

 hp = prepareAttack(Archer, {11,5});
 plate = enemyPlate({11,5});
 finger(SDL_FINGERDOWN, 1, raw({plate.x-12, plate.y}));
 finger(SDL_FINGERMOTION, 1, raw({plate.x-18, plate.y}));
 finger(SDL_FINGERUP, 1, raw({plate.x-18, plate.y}));
 check(game.battle->units[1].hp < hp && battleMotion_.active(),
       "Slight finger movement lost a ranged attack");

 hp = prepareAttack(Archer, {11,5});
 plate = enemyPlate({11,5});
 finger(SDL_FINGERDOWN, 1, raw(plate));
 finger(SDL_FINGERUP, 1, raw({plate.x-60, plate.y}));
 check(game.battle->units[1].hp == hp && !battleMotion_.active(),
       "Dragging away triggered a ranged attack");

 hp = prepareAttack(Pikeman, {14,5});
 tap(enemyPlate({14,5}));
 check(game.battle->units[1].hp == hp && !battleMotion_.active() &&
       toast_ == "敌军不在移动攻击范围内", "Unreachable tap should report its attack restriction");

 hp = prepareAttack(Archangel, {14,10});
 tap(battleHexCenter({14,10}));
 check(game.battle->units[1].hp < hp, "Creature spell button covered an enemy on the battlefield");

 prepareAttack(Archangel, {7,5});
 tap({580,579});
 check(pendingCreature_, "Touch creature casting button did not select cast mode");
 render();
 tap({771,579});
 check(!pendingCreature_, "A creature's cast mode carried over after its action ended");
 battleMotion_.clear();
 render();

 finger(SDL_FINGERDOWN,1,raw({400,300}));SDL_Event lost{};lost.type=SDL_WINDOWEVENT;lost.window.event=SDL_WINDOWEVENT_FOCUS_LOST;handleEvent(lost);finger(SDL_FINGERUP,1,raw({400,300}));check(!mouseLeftDown_&&touchPoints_.empty(),"Focus loss retained touch capture");
 marching_=true;SDL_Event background{};background.type=SDL_APP_WILLENTERBACKGROUND;handleEvent(background);check(background_&&!marching_&&touchPoints_.empty(),"Background did not pause interaction");background.type=SDL_APP_DIDENTERFOREGROUND;handleEvent(background);check(!background_,"Foreground did not resume frames");
 std::ofstream(directory/"touch-layout-audit.json")<<nlohmann::json{{"target","iPad mini 7"},{"windowPoints",{1133,744}},{"nativeDeviceTested",false},{"hostSyntheticTouch",true},{"newGameAndChest",true},{"finalReleaseMapPan",true},{"minimapRelease",true},{"pinchDoesNotClick",true},{"longPressSplit",true},{"longPressCancelSpell",true},{"countTapAttacks",true},{"rangedTapWithDriftAttacks",true},{"dragAwayDoesNotAttack",true},{"unreachableTapReportsRestriction",true},{"creatureCastDoesNotCoverEnemies",true},{"castModeResetsAfterAction",true},{"focusLossRelease",true},{"backgroundForeground",true}}.dump(2)<<'\n';
 cancelPointer();game=Game::create();scene=Scene::Town;fixedAnimationTime_=0;zoom_=1;sourceSlot_=-1;pendingSpell_=-1;SDL_SetWindowSize(window_,oldW,oldH);
}
}
