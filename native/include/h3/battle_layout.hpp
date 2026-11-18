#pragma once
#include "h3/game.hpp"
namespace h3 {
struct CreatureAnimationTiming {double fidgetInterval=1,walkTime=1,attackTime=1,flightDistance=1;int countOffset=0,attackClimax=0;};
extern const std::array<CreatureAnimationTiming,UnitCount> CreatureAnimationTimings;
struct BattleProjectileDef {const char*animation=nullptr;int frames=0,width=0,height=0;std::array<double,6> offsets{};std::array<double,12> angles{};};
extern const std::array<BattleProjectileDef,UnitCount> BattleProjectiles;
Point battleHexTopLeft(Point);
Point battleHexCenter(Point);
bool battleCellContains(Point local);
std::optional<Point> battleHexAt(Point pixel);
Point battleDeployment(int team,int size,int stacks,int rank,bool tight);
}
