#include "h3/game.hpp"
#include <algorithm>
#include <set>
namespace h3 {
namespace {
#include "../generated/battle_obstacles.inc"
}
bool Game::obstacleBlocks(Point p)const{
 return battle&&std::any_of(battle->obstacles.begin(),battle->obstacles.end(),[p](const auto&o){return (o.spell==ForceField||o.spell<0)&&std::find(o.cells.begin(),o.cells.end(),p)!=o.cells.end();});
}
void Game::clearHypnosis(CombatStack&u){u.effects[Hypnotize]=u.effectMastery[Hypnotize]=0;u.team=u.id/100;}
std::string Game::castBattleSpecial(int spell,Point point,Point destination){
 auto&b=*battle;int mastery=spellMastery(spell),power=casterPower(),active=current()->id;std::vector<BattleObstacle>created;
 if(spell==Teleport){auto*u=combatAt(point);if(!u||u->team!=spellTeam_||u->origin==StackOrigin::WarMachine||spellImmune(*u,spell))return "请选择可传送的己方部队。";for(auto p:footprint(*u,destination)){if(p.x<0||p.x>=BattleW||p.y<0||p.y>=BattleH||obstacleBlocks(p)||!siegeCellAllowed(*u,p))return "此处不能落脚。";auto*other=combatAt(p);if(other&&other->id!=u->id)return "目标格已有部队。";}
  if(b.siege.fort){auto side=[&](Point p){int x=b.siege.attackerTeam?BattleW-1-p.x:p.x;constexpr int line[]={11,11,10,10,9,9,9,10,10,11,11};return x>=line[p.y];};if(mastery<3&&side(point)!=side(destination))return "专家水系才能越过城墙。";if(mastery<2&&inMoat(*u,point)!=inMoat(*u,destination))return "高级水系才能越过护城河。";}
  std::vector<Point>route{{u->x,u->y},destination};u->x=destination.x;u->y=destination.y;emit({GameEventType::CombatMove,u->unit,0,destination,u->id,-1,route});obstacleHit(*u,true);
 }else if(spell==RemoveObstacle){auto at=std::find_if(b.obstacles.begin(),b.obstacles.end(),[&](auto&o){return std::find(o.cells.begin(),o.cells.end(),point)!=o.cells.end();});if(at==b.obstacles.end())return "请选择可驱除的障碍。";if(at->spell>=0&&(mastery<2||(mastery==2&&at->spell!=FireWall)))return "高级可驱除烈火魔墙，专家可驱除其他魔法障碍。";b.obstacles.erase(at);
 }else if(spell==Quicksand||spell==LandMine){std::vector<Point>free;for(int y=0;y<BattleH;++y)for(int x=2;x<BattleW-2;++x){Point p{x,y};if(!combatAt(p)&&!obstacleBlocks(p)&&siegePartAt(p)==-1&&std::none_of(b.obstacles.begin(),b.obstacles.end(),[p](auto&o){return std::find(o.cells.begin(),o.cells.end(),p)!=o.cells.end();}))free.push_back(p);}int count=std::array<int,4>{4,4,6,8}[mastery];if(free.empty())return "没有可放置陷阱的空地。";for(int i=0;i<count&&!free.empty();++i){size_t k=size_t(random()*free.size());created.push_back({spell,spellTeam_,power,mastery,-1,1<<spellTeam_,{free[k]}});free.erase(free.begin()+k);}
 }else if(spell==ForceField||spell==FireWall){std::vector<Point>cells{point};Point p=point;int length=mastery>=2?3:2;for(int i=1;i<length;++i){int dx=i==1?(spellTeam_?p.y%2?-1:0:p.y%2?0:1):(spellTeam_?p.y%2?0:1:p.y%2?-1:0);p={p.x+dx,p.y-1};cells.push_back(p);}for(auto cell:cells)if(cell.x<0||cell.x>=BattleW||cell.y<0||cell.y>=BattleH||combatAt(cell)||obstacleBlocks(cell)||siegePartAt(cell)!=-1)return "墙体需要连续的空闲格。";created.push_back({spell,spellTeam_,power,mastery,2,3,cells});
 }else return "无效特殊法术。";
 casterMana()-=spellCost(spell);casterRound()=b.round;for(auto&o:created)b.obstacles.push_back(o);b.log.push_back((spellTeam_?std::string("对方英雄"):hero.name)+"施放"+Spells[spell].name+"。");emit({GameEventType::SpellCast,-1,spell,point});finishCast(active,spell);return {};
}
bool Game::obstacleHit(CombatStack&u,bool entering){
 if(!battle||u.hp<=0)return false;bool stopped=false;auto cells=footprint(u,{u.x,u.y});for(auto&o:battle->obstacles){if(o.cells.empty()||std::none_of(cells.begin(),cells.end(),[&](Point p){return std::find(o.cells.begin(),o.cells.end(),p)!=o.cells.end();}))continue;
  if((o.spell==Quicksand||o.spell==LandMine)&&(u.team==o.team||!entering||Units[u.unit].flying))continue;
  if(o.spell==Quicksand){o.revealed=3;stopped=true;battle->log.push_back(Units[u.unit].name+"陷入流沙，停止移动。");}
  if(o.spell==LandMine||o.spell==FireWall){int spell=o.spell;if(!spellImmune(u,spell)){int damage=o.power*10+(spell==LandMine?std::array<int,4>{25,25,50,100}[o.mastery]:std::array<int,4>{10,10,20,50}[o.mastery]);int heroId=o.team==0?hero.originalId:heroes.contains(battle->targetId)?heroes.at(battle->targetId).value.originalId:-1;if(spell==FireWall&&heroId==136)damage*=2;int previous=spellTeam_;spellTeam_=o.team;hurt(u,spellDamage(u,spell,damage));spellTeam_=previous;}if(spell==LandMine){o.cells.clear();o.revealed=3;}}
  if(u.hp<=0)break;
 }std::erase_if(battle->obstacles,[](auto&o){return o.cells.empty();});return stopped;
}
bool Game::moveCombatStack(CombatStack&u,std::vector<Point>&route){
 std::vector<Point>actual;if(route.empty())return false;actual.push_back(route.front());bool stopped=false;for(size_t i=1;i<route.size();++i){u.x=route[i].x;u.y=route[i].y;actual.push_back(route[i]);if(obstacleHit(u,true)||u.hp<=0){stopped=true;break;}}route=std::move(actual);return !stopped;
}
}

namespace h3 {void Game::setupBattleObstacles(){
 if(!imported()||!battle||battle->siege.fort)return;const auto*target=objectById(battle->targetId);if(target&&bankLocation(target->sourceClass))return;auto*t=tile(hero.pos);if(!t||t->terrain==Terrain::Water)return;std::vector<const NaturalObstacleDef*>choices;for(const auto&def:NaturalObstacles)if(def.terrain&(1<<int(t->terrain)))choices.push_back(&def);if(choices.empty())return;
 int tiles=0,patches=3+int(random()*3);for(int attempt=0;attempt<100&&patches>0&&tiles<18;++attempt){auto&def=*choices[size_t(random()*choices.size())];Point anchor{3+int(random()*8),2+int(random()*8)};BattleObstacle obstacle;obstacle.spell=-1;obstacle.revealed=3;obstacle.asset=def.asset;obstacle.anchor=anchor;bool valid=true;for(int offset:def.blocked){int index=anchor.y*17+anchor.x+1+offset;Point p{index%17-1,index/17};if((anchor.y&1)&&!(p.y&1))--p.x;if(p.x<3||p.x>BattleW-4||p.y<1||p.y>=BattleH-1||combatAt(p)||obstacleBlocks(p)){valid=false;break;}obstacle.cells.push_back(p);}if(valid&&!obstacle.cells.empty()){tiles+=obstacle.cells.size();battle->obstacles.push_back(obstacle);--patches;}}
}}
