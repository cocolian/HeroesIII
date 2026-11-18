#include "h3/battle_layout.hpp"
#include <limits>
namespace h3 {
const std::array<BattleProjectileDef,UnitCount> BattleProjectiles={{
#include "../generated/projectiles.inc"
}};

#include "../generated/battle_layout.inc"
Point battleHexTopLeft(Point p){return {36+p.x*44-(p.y&1)*22,86+p.y*42};}
Point battleHexCenter(Point p){auto top=battleHexTopLeft(p);return {top.x+22,top.y+26};}
bool battleCellContains(Point p){return p.x>=0&&p.y>=0&&p.x<45&&p.y<52&&(cellHitRows[p.y]&(UINT64_C(1)<<p.x));}
std::optional<Point> battleHexAt(Point pixel){
 std::optional<Point> result;int best=std::numeric_limits<int>::max();
 for(int y=0;y<BattleH;y++)for(int x=0;x<BattleW;x++){Point cell{x,y};auto top=battleHexTopLeft(cell);if(!battleCellContains({pixel.x-top.x,pixel.y-top.y}))continue;
  auto center=battleHexCenter(cell);int dx=pixel.x-center.x,dy=pixel.y-center.y,distance=dx*dx+dy*dy;if(distance<best){best=distance;result=cell;}
 }
 return result;
}
Point battleDeployment(int team,int size,int stacks,int rank,bool tight){
 if(team<0||team>1||size<1||size>2||stacks<1||stacks>7||rank<0||rank>=stacks)return {-1,-1};
 return {team==0?size-1:BattleW-size,(tight?tightRows:looseRows)[stacks-1][rank]};
}
}
