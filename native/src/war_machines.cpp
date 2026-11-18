#include "h3/locations.hpp"
#include <algorithm>

namespace h3 {
void Game::setupWarMachines(){if(!battle)return;auto*target=objectById(battle->targetId);if(target&&bankLocation(target->sourceClass))return;
 for(int team=0;team<2;++team){const Hero*owner=team==0?&hero:heroes.contains(battle->targetId)?&heroes.at(battle->targetId).value:nullptr;if(!owner)continue;
  const int artifacts[]={4,6,5,3},layoutIndex[]={0,2,1,3};for(int kind=0;kind<4;++kind){if(kind==3){if(!battle->siege.fort||team!=battle->siege.attackerTeam)continue;}else if(std::find(owner->artifactIds.begin(),owner->artifactIds.end(),artifacts[kind])==owner->artifactIds.end())continue;if(kind==0&&battle->siege.fort&&team!=battle->siege.attackerTeam)continue;
   CombatStack machine;machine.origin=StackOrigin::WarMachine;machine.slot=kind==3?22:23+kind;machine.id=team*100+machine.slot;machine.team=team;machine.unit=CreatureUnitCount+kind;machine.count=machine.initialCount=1;machine.hp=Units[machine.unit].hp;machine.permanentCount=0;machine.shots=Units[machine.unit].shots;
   auto place=[&](Point p){for(auto cell:footprint(machine,p))if(cell.x<0||cell.x>=BattleW||cell.y<0||cell.y>=BattleH||combatAt(cell)||!siegeCellAllowed(machine,cell))return false;machine.x=p.x;machine.y=p.y;return true;};
   int raw=locations::facts()["warMachineLayouts"][team?"defenderWarMachines":"attackerWarMachines"][layoutIndex[kind]];
   bool placed=place({std::clamp(raw%17-1,team?0:Units[machine.unit].size-1,team?BattleW-Units[machine.unit].size:BattleW-1),raw/17});
   for(int column=0;column<4&&!placed;++column)for(int y=0;y<BattleH&&!placed;++y)placed=place({team==0?column+1:BattleW-2-column,y});
   if(placed)battle->units.push_back(machine);
  }
 }
}
bool Game::prepareWarMachine(){auto*u=current();if(!battle||!u||u->origin!=StackOrigin::WarMachine)return false;
 if(u->unit==FirstAidTentUnit){int skill=u->team==0?hero.skills[FirstAid]:battle->defenderSkills[FirstAid];CombatStack*best=nullptr;int missing=0;
  for(auto&ally:battle->units)if(ally.team==u->team&&ally.origin==StackOrigin::Army&&ally.hp>0){int n=ally.count*Units[ally.unit].hp-ally.hp;if(n>missing){missing=n;best=&ally;}}
  if(best&&missing>0){int healing=std::min(missing,1+int(random()*25*(skill+1)));best->hp+=healing;battle->log.push_back("急救帐篷恢复了 "+std::to_string(healing)+" 点生命。");emit({GameEventType::CombatSpellEffect,best->unit,Cure,{best->x,best->y},best->id,u->id,{},*best});}
  u->acted=true;nextAction(false);return true;
 }
 if(u->unit==AmmoCartUnit){nextAction(false);return true;}
 if(u->unit==CatapultUnit){int skill=u->team==0?hero.skills[Ballistics]:battle->defenderSkills[Ballistics];if(u->team!=0||!skill){catapultShots();if(battle){u=current();if(u)u->acted=true;nextAction(false);}return true;}return false;}
 int skill=u->team==0?hero.skills[Artillery]:battle->defenderSkills[Artillery];if(u->unit==BallistaUnit&&(u->team!=0||skill==0)){
  const CombatStack*target=nullptr;double value=-1;for(const auto&enemy:battle->units)if(enemy.hp>0&&enemy.team!=u->team&&attackPlan(*u,enemy)){double score=enemy.count*(Units[enemy.unit].maxDamage+Units[enemy.unit].hp*.2);if(score>value){value=score;target=&enemy;}}
  if(target)act("attack",{target->x,target->y});else nextAction(false);return true;
 }
 return false;
}
}
