#pragma once
#include "h3/game.hpp"
namespace h3 {
struct CreatureAnimationTiming {double fidgetInterval=1,walkTime=1,attackTime=1,flightDistance=1;int countOffset=0,attackClimax=0;};
extern const std::array<CreatureAnimationTiming,UnitCount> CreatureAnimationTimings;
Point battleHexTopLeft(Point);
Point battleHexCenter(Point);
bool battleCellContains(Point local);
std::optional<Point> battleHexAt(Point pixel);
Point battleDeployment(int team,int size,int stacks,int rank,bool tight);
}
