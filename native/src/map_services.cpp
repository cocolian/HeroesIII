#include "h3/game.hpp"
#include <algorithm>

namespace h3 {
int Game::seaMovementBonus(int owner)const{
 int count=0;
 for(const auto&o:objects)if(o.sourceClass==42&&o.owner==owner)++count;
 for(const auto&[id,stored]:settlements){
  auto o=std::find_if(objects.begin(),objects.end(),[&](const auto&o){return o.id==id;});
  const auto&t=id==activeTownId?town:stored;
  if(o!=objects.end()&&o->owner==owner&&t.faction==0&&t.built[Special1])++count;
 }
 return std::min(98500,count*500);
}
std::vector<GuildPlayer> Game::thievesGuild()const{
 std::vector<GuildPlayer> result;
 for(int owner=0;owner<8;++owner){
  GuildPlayer p;p.owner=owner;double bestPower=-1,bestUnitPower=-1;
  auto addArmy=[&](const Army&a,bool heroArmy){
   p.totals[7]+=int64_t(armyPower(a));
   if(heroArmy)for(const auto&s:a)if(!s.empty()){
    Army single{};single[0]={s.unit,1};double power=armyPower(single);
    if(power>bestUnitPower){bestUnitPower=power;p.bestCreature=s.unit;}
   }
  };
  for(const auto&[id,h]:heroes)if(h.owner==owner){
   const auto&value=id==activeHeroId?hero:h.value;
   ++p.totals[1];p.totals[6]+=value.artifactIds.size();addArmy(value.army,true);
   double power=armyPower(value.army)*(1.+.05*(value.attack+value.defense));
   if(power>bestPower){bestPower=power;p.bestHero=value;}
   if(value.originalId==142)p.totals[8]+=350;
  }
  for(const auto&o:objects)if(o.owner==owner){
   if(o.type==ObjectType::Town&&settlements.contains(o.id)){
    ++p.totals[0];const auto&t=o.id==activeTownId?town:settlements.at(o.id);
    p.totals[8]+=t.built[Capitol]?4000:t.built[CityHall]?2000:t.built[TownHall]?1000:500;
    addArmy(t.garrison,false);
   }else if(o.type==ObjectType::Mine&&o.resource==Gold)p.totals[8]+=mineYield(Gold);
  }
  if(!p.totals[0]&&!p.totals[1])continue;
  const auto&bank=owner==currentOwner()?resources:playerResources[owner];
  p.totals[2]=bank[Gold];p.totals[3]=int64_t(bank[Wood])+bank[Ore];
  p.totals[4]=int64_t(bank[Mercury])+bank[Sulfur]+bank[Crystal]+bank[Gems];
  for(const auto&o:objects)if(o.type==ObjectType::Obelisk){
   // Older saves recorded visits globally; new visits retain a player bitmask.
   int mask=o.properties.is_object()?o.properties.value("visitedOwners",o.visited?1:0):(o.visited?1:0);
   if(mask&(1<<owner))++p.totals[5];
  }
  result.push_back(std::move(p));
 }
 // Internal owner zero denotes the human player; the classic table uses color order.
 int human=scenarioRules.is_object()?scenarioRules.value("originalPlayer",0):0;
 auto original=[&](int owner){return owner==0?human:owner<=human?owner-1:owner;};
 std::sort(result.begin(),result.end(),[&](const auto&a,const auto&b){return original(a.owner)<original(b.owner);});
 return result;
}
}
