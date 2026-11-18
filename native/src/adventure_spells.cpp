#include "h3/game.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>

namespace h3 {
bool Game::adventureSpellActive(int spell)const{return spell>=0&&spell<SpellCount&&hero.adventureSpellDay[spell]==day;}
std::vector<int> Game::portalTowns()const{
 std::vector<int> towns;for(const auto&o:objects)if(o.type==ObjectType::Town&&o.owner==currentOwner()&&settlements.contains(o.id))towns.push_back(o.id);
 std::stable_sort(towns.begin(),towns.end(),[&](int a,int b){auto find=[&](int id)->const Object&{return *std::find_if(objects.begin(),objects.end(),[id](auto&o){return o.id==id;});};const auto&x=find(a);const auto&y=find(b);auto distance=[&](const Object&o){return std::hypot(o.pos.x-hero.pos.x,o.pos.y-hero.pos.y)+(o.level==mapLevel?0:1000);};return distance(x)<distance(y);});
 if(spellMastery(TownPortal)<2&&towns.size()>1)towns.resize(1);return towns;
}
std::string Game::castAdventure(int spell,Point point,int townId){
 if(battle||report||neutralOffer||result||!hero.levelChoices.empty())return "请先完成当前事件。";
 if(spell<0||spell>=SpellCount||!Spells[spell].adventure||!hero.spellbook||!heroKnowsSpell(hero,spell)||bannedSpells[spell])return "尚未学会此冒险法术。";
 int cost=spellCost(spell),mastery=spellMastery(spell);if(hero.mana<cost)return "魔法值不足。";
 auto occupied=[&](Point p,int level){return std::any_of(heroes.begin(),heroes.end(),[&](auto&entry){return entry.first!=activeHeroId&&entry.second.level==level&&entry.second.value.pos==p;});};
 auto validLanding=[&](Point p){auto*t=tile(p);if(!t||t->blocked||t->decor!=Decor::None||t->terrain==Terrain::Rock||(t->terrain==Terrain::Water)!=hero.onBoat||occupied(p,mapLevel))return false;auto*o=objectAt(p);return !o||o->type==ObjectType::MapEvent;};
 auto relocate=[&](Point p,int level,bool guards){hero.pos=p;mapLevel=level;hero.adventureGoal=0;reveal();storeHero();syncHeroObjects();emit({GameEventType::HeroMove,-1,int(tile(p)->terrain),p});if(guards)if(const auto*g=guardingMonster(p))encounterMonster(g->id);};
 if(spell==TownPortal){if(hero.onBoat)return "船上不能使用时空之门。";auto towns=portalTowns();if(towns.empty())return "没有可返回的己方城镇。";if(!townId)townId=towns.front();if(std::find(towns.begin(),towns.end(),townId)==towns.end())return "此等级不能选择该城镇。";auto*o=objectById(townId);if(occupied(o->pos,o->level))return "城门已有英雄，无法传送。";int movement=mastery==3?200:300;if(hero.movement<movement)return "行动力不足。";auto p=o->pos;int level=o->level;hero.mana-=cost;hero.movement-=movement;relocate(p,level,false);selectTown(townId);visitTownBuildings();}
 else if(spell==DimensionDoor){if(std::abs(point.x-hero.pos.x)>9||std::abs(point.y-hero.pos.y)>8||point==hero.pos)return "目标必须在横向 9 格、纵向 8 格范围内。";int casts=hero.doorDay==day?hero.doorCasts:0;if(casts>=mastery+1)return "已达到今天的异次元之门次数上限。";int movement=mastery==3?200:300;if(hero.movement<=0)return "行动力不足。";if(!validLanding(point))return "传送目标不可落脚。";hero.mana-=cost;hero.movement=std::max(0,hero.movement-movement);hero.doorDay=day;hero.doorCasts=casts+1;relocate(point,mapLevel,false);}
 else if(spell==ScuttleBoat){auto*o=objectAt(point);if(!o||o->type!=ObjectType::Boat||!tile(point)->seen||std::abs(point.x-hero.pos.x)>9||std::abs(point.y-hero.pos.y)>8)return "请选择视野中的一艘空船。";int id=o->id;hero.mana-=cost;if(random()*100<std::array<int,4>{50,50,75,100}[mastery])std::erase_if(objects,[id](auto&o){return o.id==id;});else{message("毁船术施放失败。");return {};}}
 else if(spell==SummonBoat){if(hero.onBoat)return "请在岸边召唤空船。";Point berth{-1,-1};for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){Point p{hero.pos.x+dx,hero.pos.y+dy};auto*t=tile(p);if(berth.x<0&&t&&t->terrain==Terrain::Water&&!t->blocked&&!objectAt(p))berth=p;}if(berth.x<0)return "岸边没有可用泊位。";int nearest=0;double best=1e20;for(const auto&o:objects)if(o.type==ObjectType::Boat&&o.level==mapLevel){double distance=std::hypot(o.pos.x-hero.pos.x,o.pos.y-hero.pos.y);if(distance<best){nearest=o.id;best=distance;}}if(!nearest&&mastery<2)return "没有可召来的空船；高级水系可以创造船只。";hero.mana-=cost;if(random()*100>=std::array<int,4>{50,50,75,100}[mastery]){message("召船术施放失败。");return {};}if(nearest){auto*boat=objectById(nearest);boat->pos=boat->anchor=berth;}else{Object boat;for(const auto&o:objects)boat.id=std::max(boat.id,o.id);for(const auto&[id,h]:heroes)boat.id=std::max(boat.id,id);++boat.id;boat.type=ObjectType::Boat;boat.sourceClass=8;boat.level=mapLevel;boat.pos=boat.anchor=berth;boat.sprite="avxboat1.def";boat.name="船只";objects.push_back(boat);}}
 else {if((spell==Fly||spell==WaterWalk)&&hero.onBoat)return "请先登岸。";hero.mana-=cost;hero.adventureSpellDay[spell]=day;hero.adventureSpellMastery[spell]=mastery;}
 emit({GameEventType::SpellCast,-1,spell,hero.pos});message(hero.name+"施放了"+Spells[spell].name+"。");return {};
}
std::vector<Step> Game::magicRoute(Point goal)const{
 bool fly=adventureSpellActive(Fly),water=adventureSpellActive(WaterWalk);if((!fly&&!water)||hero.onBoat||goal==hero.pos||!tile(goal)||!tile(goal)->seen||!passable(goal)||tile(goal)->terrain==Terrain::Water)return {};
 if(const auto*o=objectAt(goal);o&&(o->type==ObjectType::EnemyHero||o->type==ObjectType::Monster||o->type==ObjectType::Boat||o->type==ObjectType::Gate||locationBlocksPassage(*o)))return {};
 const int n=mapWidth*mapHeight,inf=std::numeric_limits<int>::max();std::vector<int>dist(n,inf),parent(n,-1);using Node=std::pair<int,int>;std::priority_queue<Node,std::vector<Node>,std::greater<Node>>q;auto index=[&](Point p){return p.y*mapWidth+p.x;};int start=index(hero.pos),end=index(goal);dist[start]=0;q.emplace(0,start);
 int mastery=hero.adventureSpellMastery[fly?Fly:WaterWalk],penalty=std::array<int,4>{40,40,20,0}[mastery];auto mask=movementMask();
 while(!q.empty()){auto[c,i]=q.top();q.pop();if(c!=dist[i])continue;if(i==end)break;Point from{i%mapWidth,i/mapWidth};for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){Point p{from.x+dx,from.y+dy};auto*t=tile(p);if((!dx&&!dy)||!t||!t->seen||t->terrain==Terrain::Rock||(!fly&&((t->terrain!=Terrain::Water&&!mask[index(p)])||t->blocked||t->decor!=Decor::None)))continue;if(auto*o=objectAt(p);o&&o->type!=ObjectType::MapEvent&&!fly)continue;int move=(dx&&dy?141:100)*(100+penalty)/100,total=c+move,j=index(p);if(total<dist[j]&&total<=hero.movement){dist[j]=total;parent[j]=i;q.emplace(total,j);}}}
 if(dist[end]==inf)return {};std::vector<Step>route;for(int i=end;i!=start;i=parent[i]){if(parent[i]<0)return {};route.push_back({i%mapWidth,i/mapWidth,dist[i]-dist[parent[i]]});}std::reverse(route.begin(),route.end());return route;
}
}
