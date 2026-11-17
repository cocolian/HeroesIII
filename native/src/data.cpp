#include "h3/game.hpp"
#include <limits>
#include <algorithm>
namespace h3 {
const std::array<UnitDef,UnitCount> Units={{
#include "../generated/creatures.inc"
}};
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
