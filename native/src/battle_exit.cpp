#include "h3/game.hpp"
#include <algorithm>

namespace h3 {
bool Game::canLeaveBattle(bool surrender)const{
 if(!battle||battle->tacticsTeam>=0||!current()||current()->team!=0||!activeHeroId||!imported())return false;
 bool enemyHero=heroes.contains(battle->targetId);
 auto shackles=[](const Hero&h){return std::find(h.wornArtifacts.begin(),h.wornArtifacts.end(),125)!=h.wornArtifacts.end();};
 if(enemyHero&&(shackles(hero)||shackles(heroes.at(battle->targetId).value)))return false;
 if(battle->siege.fort&&battle->siege.attackerTeam==1){auto at=settlements.find(battle->siege.townId);if(at==settlements.end())return false;const auto&t=battle->siege.townId==activeTownId?town:at->second;if(t.faction!=6||!t.built[Special1])return false;}
 return !surrender||enemyHero;
}
int Game::surrenderCost()const{
 if(!battle)return 0;int discount=20*hero.skills[Diplomacy];for(int id:hero.wornArtifacts)if(id>=66&&id<=68)discount+=10;int64_t cost=0;for(const auto&u:battle->units)if(u.id/100==0&&u.origin==StackOrigin::Army)cost+=int64_t(u.count)*Units[u.unit].cost;return int(std::min<int64_t>(1000000000,(cost/2)*std::max(0,100-discount)/100));
}
std::string Game::leaveBattle(bool surrender){
 if(!canLeaveBattle(surrender))return surrender?"当前无法投降；需要对方英雄，且不能受到战争枷锁限制。":"当前无法撤退；守城时需要逃跑地道，战争枷锁禁止撤退。";
 int cost=surrender?surrenderCost():0;if(resources[Gold]<cost)return "金币不足，无法支付投降费用。";
 Hero returning=hero;returning.levelChoices.clear();returning.army={};if(surrender){for(const auto&u:battle->units)if(u.id/100==0&&u.origin==StackOrigin::Army&&std::min(u.count,u.permanentCount)>0)returning.army[u.slot]={u.unit,std::min(u.count,u.permanentCount)};}else returning.army[0]={heroRetreatUnit(hero),1};
 if(!armyCount(returning.army))return "没有可以带离战场的部队。";resources[Gold]-=cost;if(surrender&&heroes.contains(battle->targetId)){int owner=heroes.at(battle->targetId).owner;playerResources[owner][Gold]=int(std::min<int64_t>(1000000000,int64_t(playerResources[owner][Gold])+cost));}
 int owner=currentOwner();battle->exitType=surrender?2:1;finishBattle();retiredHeroes.push_back({returning,owner,(day-1)/7});storeHero();message(surrender?"已支付投降费用；英雄保留部队、宝物和经验，可在本周酒馆重新招募。":"英雄已经撤退；宝物和经验保留，可在本周酒馆重新招募。");return {};
}
std::string Game::retreat(){return leaveBattle(false);}
}
