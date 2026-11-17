#include "h3/renderer.hpp"
#include "h3/battle_layout.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace h3 {
std::optional<Desktop::CursorVisual> Desktop::combatCursor(){
 if(scene!=Scene::Battle||background_||touchPrimary_||touchPinching_||mouseX_<0||mouseY_<0||mouseX_>=800||mouseY_>=556)return {};
 Point point{mouseX_,mouseY_};
 // Hexes and creature bodies are also registered as buttons. The command
 // controls live below y=556, outside this field; do not suppress their targets.
 if(battleMotion_.active()||autoCombat_||(game.current()&&game.current()->team!=0))return CursorVisual{"cradvntr.def",1,{0,0}};
 if(!game.battle||!game.current())return {};
 const auto*active=game.current();const auto*target=battleTargetAt(point);auto hex=battleHexAt(point);
 auto combat=[](int frame)->CursorVisual{
  static constexpr Point pivots[]={{12,12},{10,14},{14,14},{12,12},{12,12},{8,12},{0,0},{21,0},{31,5},{21,21},{0,21},{0,5},{0,0},{6,0},{6,31},{14,0},{12,12},{12,12},{12,12},{14,20}};
  return {"crcombat.def",frame,pivots[frame]};
 };
 if(pendingSpell_>=0){
  if(!hex&&!target)return combat(0);
  Point cell=target?Point{target->x,target->y}:*hex;
  bool valid=game.hero.spellbook&&game.hero.spells[pendingSpell_]&&game.hero.mana>=game.spellCost(pendingSpell_)&&game.battle->spellRound!=game.battle->round;
  if(pendingSpell_==Sacrifice&&sacrificeTarget_>=0){
   auto victims=game.sacrificeVictims(sacrificeTarget_);valid=valid&&target&&std::find(victims.begin(),victims.end(),target->id)!=victims.end();
   return combat(valid?18:0);
  }
  valid=valid&&!game.spellTargets(pendingSpell_,cell).empty();
  if(!valid)return combat(0);
  int count=frameCount("crspell.def");if(!count)return combat(6);
  return CursorVisual{"crspell.def",int(animationTime_/80%count),{18,28}};
 }
 if(target){
  if(target->team==active->team)return combat(5);
  auto plan=game.attackPlan(*active,*target);if(!plan)return combat(0);
  if(plan->ranged)return combat(!Units[active->unit].noDistancePenalty&&game.distance(*active,*target)>10?15:3);
  // Point the blade from the actual approach hex to the closest occupied
  // target hex, including either half of a two-hex creature.
  Point from=plan->from,to{target->x,target->y};int best=100;
  for(auto a:game.footprint(*active,plan->from))for(auto b:game.footprint(*target,{target->x,target->y})){
   int distance=Game::hexDistance(a,b);if(distance<best){best=distance;from=a;to=b;}
  }
  auto a=battleHexCenter(from),b=battleHexCenter(to);
  double angle=std::atan2(double(b.y-a.y),double(b.x-a.x));
  int sector=(int(std::lround(angle/(std::acos(-1.)/4)))+8)%8;
  static constexpr int blades[]={8,9,14,10,11,12,13,7};return combat(blades[sector]);
 }
 if(!hex)return combat(6);
 auto cost=game.battlePath(*active,*hex);
 return combat(cost&&*cost>0&&*cost<=game.speed(*active)?Units[active->unit].flying?2:1:0);
}
void Desktop::combatCursorRenderTests(const std::filesystem::path&directory){
 auto check=[](bool ok,const char*message){if(!ok)throw std::runtime_error(message);};
 int width,height;SDL_GetWindowSize(window_,&width,&height);SDL_SetWindowSize(window_,800,600);
 auto setup=[&](int unit=Pikeman){
  cancelPointer();battleMotion_.clear();settings_=ClientSettings{};settings_.hdArtwork=false;game=Game::create();
  game.hero.army={Stack{unit,20}};game.objectById(15)->army={Stack{Skeleton,50}};game.startBattle(15);
  game.battle->units[0].x=5;game.battle->units[0].y=5;game.battle->units[1].x=6;game.battle->units[1].y=5;
  game.battle->queue={0,100};scene=Scene::Battle;fixedAnimationTime_=0;animationTime_=0;pendingSpell_=sacrificeTarget_=-1;
  autoCombat_=false;nextAI_=0;focus_=-1;mouseX_=mouseY_=-1;toastUntil_=0;cursorPreview_=true;render();
 };
 auto hover=[&](Point cell){movePointer(battleHexCenter(cell));return combatCursor();};
 auto expect=[&](int frame,const char*message){auto cursor=combatCursor();check(cursor&&cursor->asset=="crcombat.def"&&cursor->frame==frame,message);};
 auto shot=[&](const char*name){toastUntil_=0;render();check(screenshot(directory/(std::string("cursor-battle-")+name+".png")),"Combat cursor screenshot failed");};
 setup();auto initial=game.toJson();hover({6,5});auto sword=combatCursor();
 check(sword&&sword->frame>=7&&sword->frame<=14,"Melee target did not show an original directional sword");shot("sword");
 check(adventureCursorHidden_&&game.toJson()==initial,"Cursor hover mutated battle or retained system arrow");
 auto plan=game.attackPlan(*game.current(),game.battle->units[1]);check(plan&&!plan->ranged&&plan->traveled==0,"Adjacent sword disagreed with attack plan");
 int hp=game.battle->units[1].hp;click(battleHexCenter({6,5}),false);check(game.battle->units[1].hp<hp,"Sword click did not damage enemy");
 setup();game.battle->units[1].x=14;hover({14,5});expect(0,"Out-of-range target advertised attack");shot("out-of-range");
 setup();hover({5,5});expect(5,"Friendly stack did not show information cursor");shot("friendly");
 hover({4,5});expect(1,"Reachable empty hex did not show walking cursor");shot("walk");
 hover({0,0});expect(0,"Unreachable empty hex advertised movement");
 setup(Griffin);hover({3,7});expect(2,"Flying creature did not show flying cursor");shot("fly");
 setup(Archer);game.battle->units[1].x=9;hover({9,5});expect(3,"Ranged target did not show shooting cursor");shot("shoot");
 game.battle->units[0].x=0;game.battle->units[1].x=14;hover({14,5});expect(15,"Long-range damage penalty did not show broken shooting cursor");shot("shoot-penalty");
 setup(Archer);hover({6,5});sword=combatCursor();check(sword&&sword->frame>=7&&sword->frame<=14,"Blocked archer advertised shooting instead of melee");shot("blocked-archer-melee");
 setup();game.battle->units[1].x=8;hover({8,5});plan=game.attackPlan(*game.current(),game.battle->units[1]);
 check(plan&&plan->traveled>0,"Approach attack fixture invalid");auto destination=plan->from;hp=game.battle->units[1].hp;
 click(battleHexCenter({8,5}),false);check(game.battle->units[0].x==destination.x&&game.battle->units[0].y==destination.y&&game.battle->units[1].hp<hp,"Sword approach differed from executed move attack");
 setup();game.hero.spellbook=true;game.hero.spells[MagicArrow]=true;pendingSpell_=MagicArrow;hover({6,5});initial=game.toJson();
 auto spell=combatCursor();check(spell&&spell->asset=="crspell.def","Valid spell target did not show spell cursor");shot("spell-0");
 fixedAnimationTime_=160;render();auto next=combatCursor();check(next&&next->asset=="crspell.def"&&next->frame!=spell->frame&&game.toJson()==initial,"Spell cursor did not animate or mutated gameplay");shot("spell-2");
 hover({5,5});expect(0,"Enemy spell advertised a friendly target");game.hero.mana=0;hover({6,5});expect(0,"Unaffordable spell advertised valid cast");
 pendingSpell_=-1;autoCombat_=true;hover({6,5});auto busy=combatCursor();check(busy&&busy->asset=="cradvntr.def"&&busy->frame==1,"Auto combat did not show waiting cursor");shot("busy");
 autoCombat_=false;movePointer({-1,-1});render();check(!combatCursor()&&!adventureCursorHidden_,"Leaving combat canvas retained software cursor");
 scene=Scene::Report;game.battle.reset();game.report.emplace();render();check(!adventureCursorHidden_,"Battle report retained combat cursor");
 std::ofstream(directory/"combat-cursor-audit.json")<<nlohmann::json{{"originalAssets",{"crcombat.def","crspell.def","cradvntr.def"}},{"directionalMeleeSword",true},{"shootAndDistancePenalty",true},{"blockedArcherUsesMelee",true},{"movementAndFlight",true},{"unreachableTargetsBlocked",true},{"spellTargetValidationAndAnimation",true},{"hoverPreservesGameAndRng",true},{"sharedPlanWithExecutedAttack",true},{"restoresSystemCursor",true}}.dump(2)<<'\n';
 cursorPreview_=false;restoreSystemCursor();game=Game::create();scene=Scene::Town;battleMotion_.clear();fixedAnimationTime_=0;mouseX_=mouseY_=-1;SDL_SetWindowSize(window_,width,height);
}
}
