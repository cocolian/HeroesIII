#include "h3/game.hpp"
#include <algorithm>
namespace h3 {
Resources startingResources(int difficulty,bool computer){
 const Resources human[5]={{30000,30,30,15,15,15,15},{20000,20,20,10,10,10,10},{15000,15,15,7,7,7,7},{10000,10,10,4,4,4,4},{0,0,0,0,0,0,0}};
 const Resources ai[5]={{5000,5,5,2,2,2,2},{7500,10,10,4,4,4,4},{10000,15,15,7,7,7,7},{10000,15,15,7,7,7,7},{10000,15,15,7,7,7,7}};
 difficulty=difficulty<0?1:std::clamp(difficulty,0,4);return computer?ai[difficulty]:human[difficulty];
}
Resources Game::incomeForOwner(int owner)const{
 Resources income{};for(const auto&o:objects)if(o.owner==owner){if(o.type==ObjectType::Mine)income[o.resource]+=mineYield(o.resource);if(o.type==ObjectType::Town&&settlements.contains(o.id)){const auto&t=o.id==activeTownId?town:settlements.at(o.id);income[Gold]+=(t.built[Capitol]?4000:t.built[CityHall]?2000:t.built[TownHall]?1000:500)+(t.built[GrailBuilding]?5000:0);if(t.built[ResourceSilo])for(int r=0;r<ResourceCount;++r)income[r]+=TownDefs[t.faction].silo[r];}}
 for(const auto&[id,h]:heroes)if(h.owner==owner){const auto&who=id==activeHeroId?hero:h.value;income[Gold]+=who.originalId==142?350:0;income[Gold]+=std::array<int,4>{0,125,250,500}[who.skills[Estates]];}
 return income;
}
void Game::payComputerIncome(){
 int difficulty=std::clamp(scenarioRules.value("difficulty",1),0,4);const int weekly[5][7]={{-175,0,0,0,0,0,0},{0,0,0,0,0,0,0},{0,0,0,0,0,0,0},{175,275,275,100,100,100,100},{350,375,375,200,200,200,200}};int part=(day-2)%7+1;
 for(int owner=1;owner<8;++owner){auto income=incomeForOwner(owner);for(int r=0;r<ResourceCount;++r){auto bonus=[&](int n){int64_t value=int64_t(income[r])*weekly[difficulty][r]*n;return value>=0?value/700:-((-value+699)/700);};int64_t payment=int64_t(income[r])+bonus(part)-bonus(part-1);playerResources[owner][r]=int(std::clamp<int64_t>(int64_t(playerResources[owner][r])+payment,0,1000000000));}}
}
}
