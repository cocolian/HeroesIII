#include "h3/game.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <tuple>

namespace h3 {
bool Game::canStep(Point from,Point to)const{
 int dx=to.x-from.x,dy=to.y-from.y;
 if(std::max(std::abs(dx),std::abs(dy))!=1||!passable(to))return false;
 if(imported()){
  auto*a=tile(from);auto*b=tile(to);if(!a||!b)return false;
  if(a->terrain!=Terrain::Water&&b->terrain==Terrain::Water){auto*o=objectAt(to);if(!o||o->type!=ObjectType::Boat)return false;}
  if(a->terrain==Terrain::Water&&b->terrain!=Terrain::Water&&!(b->originalFlags&64))return false;
  return true;
 }
 return !dx||!dy||(passable({from.x+dx,from.y})&&passable({from.x,from.y+dy}));
}
int Game::moveCost(Point from,Point to)const{
 auto*t=tile(to);auto*f=tile(from);if(!t||!f)return 999999;
 // The map currently draws dirt roads. A discount requires both endpoints
 // to lie on that road; merely stepping onto a road is ordinary terrain.
 int penalty=t->terrain==Terrain::Sand||t->terrain==Terrain::Snow?50:t->terrain==Terrain::Swamp?75:t->terrain==Terrain::Rough?25:0;
 const Terrain native[]={Terrain::Grass,Terrain::Grass,Terrain::Snow,Terrain::Lava,Terrain::Dirt,Terrain::Subterranean,Terrain::Rough,Terrain::Swamp,Terrain::Grass};bool nativeArmy=armyCount(hero.army)>0;for(const auto&s:hero.army)if(!s.empty())nativeArmy&=Units[s.unit].faction>=0&&native[Units[s.unit].faction]==t->terrain;
 if(nativeArmy)penalty=0;penalty=std::max(0,penalty-std::array<int,4>{0,25,50,100}[hero.skills[Pathfinding]]);int cost=f->road&&t->road?75:100+penalty;
 return from.x!=to.x&&from.y!=to.y?int(std::floor(cost*std::sqrt(2.))):cost;
}
const Object* Game::guardingMonster(Point p,bool visibleOnly)const{
 const auto*ground=tile(p);if(!ground)return nullptr;
 const Object*selected=nullptr;
 for(const auto&o:objects){
  if(o.level!=mapLevel||o.type!=ObjectType::Monster||o.owner==currentOwner()||armyCount(o.army)==0)continue;
  auto*t=tile(o.pos);if(!t||(visibleOnly&&!t->seen))continue;
  if((t->terrain==Terrain::Water)!=(ground->terrain==Terrain::Water))continue;
  if(std::max(std::abs(o.pos.x-p.x),std::abs(o.pos.y-p.y))>1)continue;
  if(o.pos==p)return &o;
  // Stable original-style column ordering for overlapping guard areas.
  if(!selected||std::tie(o.pos.x,o.pos.y,o.id)<std::tie(selected->pos.x,selected->pos.y,selected->id))selected=&o;
 }
 return selected;
}
std::vector<Step> Game::path(Point goal)const{
 auto allowed=movementMask();
 // Each search owns a fresh spatial index. It cannot retain stale pointers
 // after a pickup, hero movement, capture, or switching map levels.
 std::vector<const Object*>occupants(mapWidth*mapHeight),guards(mapWidth*mapHeight);
 auto cell=[&](Point p){return p.y*mapWidth+p.x;};
 auto insert=[&](const Object&o,Point p){if(!tile(p))return;auto&old=occupants[cell(p)];if(!old||(old->type!=ObjectType::EnemyHero&&(o.type==ObjectType::EnemyHero||o.type==ObjectType::Town)))old=&o;};
 for(const auto&o:objects)if(o.level==mapLevel&&o.type!=ObjectType::MapEvent){insert(o,o.pos);if(o.sourceClass==48)insert(o,{o.pos.x-1,o.pos.y});}
 for(const auto&o:objects)if(o.level==mapLevel&&o.type==ObjectType::Monster&&o.owner!=currentOwner()&&armyCount(o.army)>0){
  auto*origin=tile(o.pos);if(!origin||!origin->seen)continue;
  for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){Point p{o.pos.x+dx,o.pos.y+dy};auto*t=tile(p);if(!t||(t->terrain==Terrain::Water)!=(origin->terrain==Terrain::Water))continue;auto&old=guards[cell(p)];
   if(old&&old->pos==p)continue;
   if(!old||o.pos==p||std::tie(o.pos.x,o.pos.y,o.id)<std::tie(old->pos.x,old->pos.y,old->id))old=&o;
  }
 }
 auto object=[&](Point p){return tile(p)?occupants[cell(p)]:nullptr;};
 auto guardAt=[&](Point p){return tile(p)?guards[cell(p)]:nullptr;};
 auto open=[&](Point p){return tile(p)&&allowed[p.y*mapWidth+p.x];};
 auto step=[&](Point a,Point b){int dx=b.x-a.x,dy=b.y-a.y;if(std::max(std::abs(dx),std::abs(dy))!=1||!open(b)||(!imported()&&(dx&&dy)&&(!open({a.x+dx,a.y})||!open({a.x,a.y+dy}))))return false;if(imported()){auto*f=tile(a);auto*t=tile(b);if(f->terrain!=Terrain::Water&&t->terrain==Terrain::Water){auto*o=object(b);if(!o||o->type!=ObjectType::Boat)return false;}if(f->terrain==Terrain::Water&&t->terrain!=Terrain::Water&&!(t->originalFlags&64))return false;}return true;};
 if(!open(goal)||!tile(goal)->seen||goal==hero.pos)return {};
 const auto*target=object(goal);if(target&&target->type==ObjectType::EnemyHero&&sanctuaryProtected(target->id))return {};
 bool attack=target&&(target->type==ObjectType::Monster||target->type==ObjectType::EnemyHero)&&target->owner!=currentOwner()&&armyCount(target->army)>0;
 if(attack&&step(hero.pos,goal))return {{goal.x,goal.y,moveCost(hero.pos,goal)}};
 constexpr int Inf=std::numeric_limits<int>::max();
 std::vector<int> distance(mapWidth*mapHeight,Inf),parent(mapWidth*mapHeight,-1),hops(mapWidth*mapHeight,Inf);
 auto index=[&](Point p){return p.y*mapWidth+p.x;};int start=index(hero.pos),end=-1;
 // Cost first, then fewer moves and stable coordinates. No gameplay RNG.
 using Node=std::tuple<int,int,int>;std::priority_queue<Node,std::vector<Node>,std::greater<Node>> queue;
 distance[start]=hops[start]=0;queue.emplace(0,0,start);
 while(!queue.empty()){
  auto[c,count,i]=queue.top();queue.pop();if(c!=distance[i]||count!=hops[i])continue;
  Point p{i%mapWidth,i/mapWidth};const auto*guard=guardAt(p);
  if(i!=start){
   if((!attack&&p==goal)||(attack&&guard&&guard->id==target->id)||(attack&&target->type==ObjectType::EnemyHero&&step(p,target->pos))){end=i;break;}
   // A route cannot silently continue through an encounter or enemy town.
   const auto*at=object(p);
   if(guard||(at&&((at->type==ObjectType::Town&&at->owner!=currentOwner())||at->type==ObjectType::Gate||at->type==ObjectType::Portal||locationBlocksPassage(*at))))continue;
  }
  for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){
   Point next{p.x+dx,p.y+dy};if(!step(p,next)||!tile(next)->seen)continue;
   const auto*monster=object(next);if(monster&&(monster->type==ObjectType::Monster||monster->type==ObjectType::EnemyHero))continue;
   auto*g=guardAt(next);
   if(g&&!((!attack&&next==goal)||(attack&&g->id==target->id)))continue;
   int ni=index(next),cost=c+moveCost(p,next),steps=count+1;
   if(std::tie(cost,steps)<std::tie(distance[ni],hops[ni])){
    distance[ni]=cost;hops[ni]=steps;parent[ni]=i;queue.emplace(cost,steps,ni);
   }
  }
 }
 if(end<0)return {};
 std::vector<Step> result;
 for(int i=end;i!=start;i=parent[i]){
  if(parent[i]<0)return {};
  result.push_back({i%mapWidth,i/mapWidth,distance[i]-distance[parent[i]]});
 }
 std::reverse(result.begin(),result.end());if(attack&&target->type==ObjectType::EnemyHero&&!result.empty())result.push_back({goal.x,goal.y,moveCost({result.back().x,result.back().y},goal)});return result;
}
std::string Game::move(Point p){
 if(imported()&&!activeHeroId)return "请先从城镇酒馆招募英雄。";
 if(battle||report||result||!hero.levelChoices.empty())return "请先完成当前战斗、战报或升级选择";
 if(imported()&&p==hero.pos){for(const auto&o:objects)if(o.type==ObjectType::Portal&&o.level==mapLevel&&o.pos==p){if(hero.movement<100)return "行动力不足，请结束回合";hero.movement-=100;return visit(o.id);}}
 if(!canStep(hero.pos,p)||!tile(p)->seen)return "无法通行";
 int cost=moveCost(hero.pos,p);if(hero.movement<cost)return "行动力不足，请结束回合";
 hero.movement-=cost;
 auto*object=objectAt(p);
 if(object&&(object->type==ObjectType::Monster||object->type==ObjectType::EnemyHero)&&object->owner!=currentOwner()&&armyCount(object->army)>0){
  startBattle(object->id);return "向相邻野兵发起战斗！";
 }
 if(object&&object->type==ObjectType::Gate){if(const auto*guard=guardingMonster(p)){startBattle(guard->id);return "必须先击败关卡附近的守军。";}return applyVisit(object->id);}
 if(object&&locationBlocksPassage(*object))return visit(object->id);
 auto previous=hero.pos;bool waterBefore=tile(previous)->terrain==Terrain::Water,waterAfter=tile(p)->terrain==Terrain::Water;
 if(imported()&&waterBefore!=waterAfter){
  if(waterAfter){hero.lastEmbark=previous;hero.onBoat=true;if(object&&object->type==ObjectType::Boat){int id=object->id;std::erase_if(objects,[id](const auto&o){return o.id==id;});object=nullptr;}}
  else{hero.onBoat=false;Object boat;for(const auto&o:objects)boat.id=std::max(boat.id,o.id);++boat.id;boat.type=ObjectType::Boat;boat.pos=boat.anchor=previous;boat.level=mapLevel;boat.sourceClass=8;boat.sprite="avxboat1.def";boat.name="船只";int id=object?object->id:0;objects.push_back(boat);object=id?objectById(id):nullptr;}
  hero.movement=0;
 }
 hero.pos=p;emit({GameEventType::HeroMove,-1,int(tile(p)->terrain),p});reveal();
 if(const auto*guard=guardingMonster(p)){
  int deferred=object&&object->type!=ObjectType::Monster?object->id:0;
  startBattle(guard->id);if(battle)battle->deferredVisitId=deferred;
  return "进入野兵守卫范围，必须先击败守军！";
 }
 auto message=object?visit(object->id):"";if(imported()&&!battle)triggerMapEvents();return message;
}
}
