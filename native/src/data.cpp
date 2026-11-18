#include "h3/game.hpp"
#include <limits>
#include <algorithm>
namespace h3 {
const std::array<UnitDef,UnitCount> Units=[]{std::array<UnitDef,UnitCount> units={{
#include "../generated/creatures.inc"
}};
 const char*names[]={"弩车","急救帐篷","弹药车"};const char*animations[]={"smbal.def","smtent.def","smcart.def"};
 const int hp[]={250,75,100},cost[]={2500,750,1000},defense[]={10,0,5};
 for(int i=0;i<3;++i){auto&u=units[CreatureUnitCount+i];u.name=names[i];u.animation=animations[i];u.originalId=146+i;u.portraitFrame=148+i;u.hp=hp[i];u.attack=i==0?10:0;u.defense=defense[i];u.minDamage=i==0?2:0;u.maxDamage=i==0?3:0;u.speed=1;u.cost=cost[i];u.shots=i==0?24:0;u.size=i==2?1:2;u.retaliations=0;u.nonLiving=true;u.living=false;u.mindImmune=true;u.noDistancePenalty=true;u.noMeleePenalty=true;u.noRetaliation=true;u.portraitOnly=true;}
 return units;}();
const std::array<TownDef,FactionCount> TownDefs={{
#include "../generated/towns.inc"
}};
const std::array<BuildingDef,BuildingCount>& Buildings=TownDefs[0].buildings;
const std::array<FactionDef,FactionCount> Factions={{
 {"castle","城堡","castle-town/TBcsback.png","towns-dwellings/AVCcasx0.png"},
 {"rampart","壁垒","rampart-town/tbrmback.png","towns-dwellings/AVCramx0.png"},
 {"tower","塔楼","tower-town/tbtwback.png","towns-dwellings/AVCtowx0.png"},
 {"inferno","地狱","inferno-town/tbinback.png","towns-dwellings/AVCinfx0.png"},
 {"necropolis","墓园","necropolis-town/tbncback.png","towns-dwellings/AVCnecx0.png"},
 {"dungeon","地下城","dungeon-town/tbdnback.png","towns-dwellings/AVCdunx0.png"},
 {"stronghold","据点","stronghold-town/tbstback.png","towns-dwellings/AVCstrx0.png"},
 {"fortress","要塞","fortress-town/tbfrback.png","towns-dwellings/AVCftrx0.png"},
 {"conflux","元素城","conflux-town/TBELBack.png","towns-dwellings/AVChforx.png"}
}};
Resources unitCost(int id,int count){Resources cost{};if(id<0||id>=UnitCount||count<0||count>1000000)return cost;cost=Units[id].extraCost;cost[Gold]=Units[id].cost;for(int&r:cost)r=int(std::min<int64_t>(int64_t(r)*count,std::numeric_limits<int>::max()));return cost;}
}
