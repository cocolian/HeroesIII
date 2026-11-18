#include "h3/game.hpp"
#include <algorithm>

namespace h3 {
std::optional<AdventureVisit> Game::takeAdventureVisit(){auto result=std::move(adventureVisit_);adventureVisit_.reset();return result;}
int Game::obelisksVisited()const{return int(std::count_if(objects.begin(),objects.end(),[](const auto&o){return o.type==ObjectType::Obelisk&&o.visited;}));}
int Game::obelisksTotal()const{return int(std::count_if(objects.begin(),objects.end(),[](const auto&o){return o.type==ObjectType::Obelisk;}));}
std::string Game::dig(){
 if(battle||report||result||!hero.levelChoices.empty())return "请先完成当前战斗或升级";
 if(hero.movement!=hero.maxMovement)return "挖掘需要一整天，请在尚未移动的新一天进行。";
 if(objectAt(hero.pos)||!passable(hero.pos))return "这里无法挖掘。";
 hero.movement=0;
 std::string msg="挖掘了一整天，这里没有发现圣杯。";
 if(!grailFound&&hero.pos==grailPos&&(!imported()||(scenarioRules.value("grailExists",false)&&mapLevel==scenarioRules.value("grailLevel",0)))){grailFound=true;hero.artifacts.push_back("圣杯");hero.artifactIds.push_back(2);msg="你挖出了圣杯！圣杯已交由英雄保管。";}
 message(msg);return msg;
}
}
