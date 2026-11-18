#include "h3/game.hpp"
#include <algorithm>
#include <set>

namespace h3 {
namespace {
std::array<std::vector<int>,2> rosters(const Game&game){
 std::array<std::vector<int>,2> out;
 if(game.battle){for(const auto&u:game.battle->units)if(u.origin==StackOrigin::Army)out[u.team].push_back(u.unit);}
 else for(const auto&stack:game.hero.army)if(!stack.empty())out[0].push_back(stack.unit);
 return out;
}
}
int Game::moraleBase(int team)const{
 if(team<0||team>1)return 0;auto armies=rosters(*this);std::set<int> factions;bool undead=false,affected=false;int allied=0,enemy=0;
 for(int id:armies[team]){const auto&u=Units[id];factions.insert(u.faction);undead|=u.undead;affected|=!u.nonLiving;allied=std::max(allied,u.allyMorale);}
 for(int id:armies[1-team])enemy=std::min(enemy,Units[id].enemyMorale);
 const Hero*bearer=team==0?&hero:battle&&heroes.contains(battle->targetId)?&heroes.at(battle->targetId).value:nullptr;if(bearer&&wearsArtifact(*bearer,129)){std::set<int>aligned;for(int faction:factions)aligned.insert(faction==0||faction==1||faction==2||faction==6||faction==7||faction==8?0:faction);factions=std::move(aligned);}
 int composition=factions.empty()?0:2-int(factions.size());if(factions.size()==1&&*factions.begin()==4)composition=0;
 // Unled monsters do not get the hero's single-town army bonus. Innate
 // creature floors and army-wide creature bonuses still apply.
 if(team==1&&(!battle||!heroes.contains(battle->targetId)))composition=std::min(0,composition);
 const Hero*leader=team==0?&hero:battle&&heroes.contains(battle->targetId)?&heroes.at(battle->targetId).value:nullptr;
 int artifacts=0;if(leader)for(int id:equippedArtifacts(*leader))if(id==45||(id>=49&&id<=51))++artifacts;else if(id==108)artifacts+=3;
 return artifacts+(leader?grailBonus(team==0?currentOwner():heroes.at(battle->targetId).owner,0):0)+siegeBonus(team,3)+composition+(undead&&affected?-1:0)+(team==0?hero.skills[Leadership]:battle?battle->defenderSkills[Leadership]:0)+allied+enemy+(leader?adventureModifier(*leader,0):0);
}
int Game::luckBase(int team)const{
 if(team<0||team>1)return 0;auto armies=rosters(*this);int enemy=0;for(int id:armies[1-team])enemy=std::min(enemy,Units[id].enemyLuck);const Hero*leader=team==0?&hero:battle&&heroes.contains(battle->targetId)?&heroes.at(battle->targetId).value:nullptr;int artifacts=0;if(leader)for(int id:equippedArtifacts(*leader))if(id==45||(id>=46&&id<=48))++artifacts;else if(id==108)artifacts+=3;return artifacts+(leader?grailBonus(team==0?currentOwner():heroes.at(battle->targetId).owner,1):0)+siegeBonus(team,4)+(team==0?hero.skills[LuckSkill]:battle?battle->defenderSkills[LuckSkill]:0)+enemy+(leader?adventureModifier(*leader,1):0);
}
int Game::armyMorale(int team)const{return std::clamp(moraleBase(team),-3,3);}
int Game::armyLuck(int team)const{return std::clamp(luckBase(team),0,3);}
int Game::morale(const CombatStack&u)const{
 const auto&def=Units[u.unit];if(def.nonLiving)return 0;return std::clamp(std::max(def.moraleFloor,moraleBase(u.team)+spellEffect(u,Mirth)-spellEffect(u,Sorrow)),-3,3);
}
int Game::luck(const CombatStack&u)const{return std::clamp(std::max(Units[u.unit].luckFloor,luckBase(u.team)+spellEffect(u,Fortune)-spellEffect(u,Misfortune)),0,3);}
void Game::completeAction(bool allowMorale){
 if(finishBattle())return;auto*u=current();
 if(allowMorale&&u&&u->hp>0&&!u->moraleUsed){int value=morale(*u);if(value>0&&random()*24<value){u->moraleUsed=true;battle->log.push_back(Units[u->unit].name+"士气高涨，可以再次行动。");emit({GameEventType::CombatMorale,u->unit,1,{u->x,u->y},u->id,-1,{},*u});return;}}
 nextAction();
}
}
