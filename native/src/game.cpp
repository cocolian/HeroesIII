#include "h3/game.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <sstream>

namespace h3 {
const std::array<std::string,ResourceCount> ResourceNames={"金币","木材","矿石","水银","硫磺","水晶","宝石"};
const std::array<std::string,SkillCount> SkillNames={"进攻术","箭术","防御术","后勤学","智慧术","智力","神秘术","领导术","气系魔法","火系魔法","水系魔法","土系魔法","幸运术"};
const std::array<std::string,4> SkillLevels={"未掌握","初级","中级","高级"};
static const std::array<int,7> Dwellings={Guardhouse,ArcherTower,GriffinTower,Barracks,Monastery,TrainingGrounds,Portal};
static const std::array<int,7> Upgrades={UpGuardhouse,UpArcherTower,UpGriffinTower,UpBarracks,UpMonastery,UpTrainingGrounds,UpPortal};
int armyCount(const Army& a){int n=0;for(const auto&s:a)n+=std::max(0,s.count);return n;}
int armyStacks(const Army& a){return int(std::count_if(a.begin(),a.end(),[](auto&s){return !s.empty();}));}
int baseTier(int u){return u>=0&&u<UnitCount?Units[u].tier:-1;}
int baseUnit(int u){return u>=0&&u<UnitCount&&Units[u].upgraded?u-1:u;}
double Game::random(){randomState=randomState*1664525u+1013904223u;return double(randomState)/4294967296.0;}
void Game::emit(GameEvent event){if(actingOwner>0)return;if(events_.size()>=256)events_.erase(events_.begin());events_.push_back(event);}
std::vector<GameEvent> Game::takeEvents(){std::vector<GameEvent> result;result.swap(events_);return result;}
void Game::message(const std::string&s){log.push_back(s);if(log.size()>80)log.erase(log.begin());}
Tile* Game::tile(Point p){return p.x>=0&&p.y>=0&&p.x<mapWidth&&p.y<mapHeight?&tiles[(mapLevel*mapHeight+p.y)*mapWidth+p.x]:nullptr;}
const Tile* Game::tile(Point p)const{return p.x>=0&&p.y>=0&&p.x<mapWidth&&p.y<mapHeight?&tiles[(mapLevel*mapHeight+p.y)*mapWidth+p.x]:nullptr;}
Object* Game::objectAt(Point p){Object*found=nullptr;for(auto&o:objects)if(o.level==mapLevel&&o.type!=ObjectType::MapEvent&&o.visitableAt(p)){if(o.type==ObjectType::EnemyHero)return &o;if(!found||o.type==ObjectType::Town)found=&o;}return found;}
const Object* Game::objectAt(Point p)const{const Object*found=nullptr;for(const auto&o:objects)if(o.level==mapLevel&&o.type!=ObjectType::MapEvent&&o.visitableAt(p)){if(o.type==ObjectType::EnemyHero)return &o;if(!found||o.type==ObjectType::Town)found=&o;}return found;}
Object* Game::objectById(int id){auto i=std::find_if(objects.begin(),objects.end(),[id](auto&o){return o.id==id;});return i==objects.end()?nullptr:&*i;}
Game Game::create(uint32_t initialSeed,int faction){Game g;g.seed=g.randomState=initialSeed;g.town.faction=std::clamp(faction,0,FactionCount-1);
 for(int y=0;y<MapH;y++)for(int x=0;x<MapW;x++){auto&t=*g.tile({x,y});bool sea=(x>25&&y>16)||(x>31&&y>8)||(x>28&&y>13)||(y>24&&x>18);t.terrain=sea?Terrain::Water:x>23&&y<13?Terrain::Dirt:x<8&&y>17?Terrain::Sand:Terrain::Grass;t.decor=sea?Decor::None:g.random()<.27?Decor::Tree:g.random()<.12?Decor::Mountain:Decor::None;t.variant=int(g.random()*10000);}
 for(int x=3;x<=29;x++){int y=int(std::round(20-(x-3)*.5));for(int d=-1;d<=1;d++){auto&t=*g.tile({x,y+d});t.terrain=Terrain::Grass;t.decor=Decor::None;}g.tile({x,y})->road=true;}
 for(int y=13;y<=20;y++){g.tile({5,y})->decor=Decor::None;g.tile({5,y})->road=true;}
 auto add=[&](ObjectType type,Point p,std::string name,int owner=-1,int resource=Gold,int amount=0){Object o;o.id=int(g.objects.size())+1;o.type=type;o.pos=p;o.name=std::move(name);o.owner=owner;o.resource=resource;o.amount=amount;g.objects.push_back(o);};
 add(ObjectType::Town,{4,20},"狮鹫之巢",0);add(ObjectType::Town,{29,7},"黑鸦堡",1);g.objects.back().army={Stack{Swordsman,12},Stack{Archer,18},Stack{Griffin,6}};
 add(ObjectType::Mine,{8,18},"伐木场",-1,Wood);add(ObjectType::Mine,{4,14},"矿石采掘场",-1,Ore);add(ObjectType::Mine,{17,12},"金矿",-1,Gold);add(ObjectType::Mine,{22,10},"水晶矿",-1,Crystal);
 add(ObjectType::Treasure,{7,21},"宝箱",-1,Gold,1500);add(ObjectType::Treasure,{10,15},"宝箱",-1,Gold,1200);add(ObjectType::Treasure,{16,17},"宝箱",-1,Gold,2000);add(ObjectType::Treasure,{24,8},"宝箱",-1,Gold,2500);
 add(ObjectType::Resource,{6,17},"木材",-1,Wood,8);add(ObjectType::Resource,{11,20},"矿石",-1,Ore,8);add(ObjectType::Resource,{19,10},"宝石",-1,Gems,5);add(ObjectType::Resource,{15,14},"水晶",-1,Crystal,5);
 add(ObjectType::Monster,{12,16},"骷髅巡逻队");g.objects.back().army={Stack{Skeleton,30}};
 add(ObjectType::Monster,{19,12},"恶狼群");g.objects.back().army={Stack{Wolf,20}};
 add(ObjectType::Monster,{24,10},"半兽人哨兵");g.objects.back().army={Stack{Orc,20},Stack{Wolf,12}};
 add(ObjectType::Shrine,{9,12},"一级魔法神殿");add(ObjectType::Well,{14,19},"魔法井");add(ObjectType::Artifact,{21,16},"骑士之剑");add(ObjectType::Obelisk,{3,8},"先知方尖碑");
 add(ObjectType::TradingPost,{10,19},"贸易站");
 add(ObjectType::Windmill,{9,23},"风车");add(ObjectType::WaterWheel,{20,22},"水车");
 for(Point p:std::array<Point,13>{{{16,24},{16,23},{17,23},{18,23},{19,23},{20,23},{21,23},{22,23},{23,23},{23,22},{24,22},{25,22},{26,22}}})g.tile(p)->river=true;
 for(int y=21;y<=23;++y)for(int x=6;x<=10;++x)g.tile({x,y})->decor=Decor::None;
 for(int y=20;y<=22;++y)for(int x=18;x<=20;++x)g.tile({x,y})->decor=Decor::None;
 for(const auto&o:g.objects){auto*t=g.tile(o.pos);t->decor=Decor::None;if(t->terrain==Terrain::Water)t->terrain=Terrain::Grass;for(Point d:std::array<Point,4>{{{1,0},{-1,0},{0,1},{0,-1}}})if(auto*n=g.tile({o.pos.x+d.x,o.pos.y+d.y});n&&n->terrain!=Terrain::Water)n->decor=Decor::None;}
 std::vector<Point> approaches;
 for(int y=0;y<MapH;++y)for(int x=0;x<MapW;++x)if(g.tile({x,y})->road)approaches.push_back({x,y});
 for(const auto&o:g.objects){
  const auto&t=adventureTemplate(g.mapObjectAsset(o));Point at{o.pos.x-t.visitOffset.x,o.pos.y-t.visitOffset.y};
  for(int i=0;i<48;++i)if(t.blocked&(uint64_t(1)<<i))approaches.push_back({at.x-i%8,at.y-i/8});
  for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx)approaches.push_back({o.pos.x+dx,o.pos.y+dy});
 }
 approaches.push_back(g.grailPos);g.clearObstacleFootprints(approaches);
 int offset=g.town.faction*14;g.hero.army={Stack{offset,Units[offset].growth*2},Stack{offset+2,g.town.faction==0?16:Units[offset+2].growth*2},Stack{offset+4,4}};g.hero.skills[Leadership]=g.hero.skills[Offense]=1;
 for(int id:{TownHall,Fort,Tavern,Guardhouse,ArcherTower})g.town.built[id]=true;g.town.built[VillageHall]=true;g.town.pool={Units[offset].growth,Units[offset+2].growth,0,0,0,0,0};g.hero.maxMovement=g.movementAllowance();g.hero.movement=g.hero.maxMovement;g.generateGuild();g.reveal();g.message("凯瑟琳抵达边境。发展城镇、探索矿场，最终占领黑鸦堡。");return g;
}
void Game::reveal(){for(int y=std::max(0,hero.pos.y-6);y<std::min(mapHeight,hero.pos.y+6);y++)for(int x=std::max(0,hero.pos.x-6);x<std::min(mapWidth,hero.pos.x+6);x++)if(std::hypot(x-hero.pos.x,y-hero.pos.y)<6)tile({x,y})->seen=true;}
std::string Game::visit(int id){
 if(battle||report||result||!hero.levelChoices.empty())return "请先完成战斗、战报或升级选择";
 auto*object=objectById(id);if(!object)return "地点不存在";
 if(!object->visitableAt(hero.pos))return "英雄必须先抵达访问位置";
 if(hero.pos==object->pos&&object->type!=ObjectType::Monster)if(const auto*guard=guardingMonster(hero.pos)){
  startBattle(guard->id);if(battle)battle->deferredVisitId=id;return "必须先击败这里的守军！";
 }
 return applyVisit(id);
}
std::string Game::applyVisit(int id){if(auto handled=importedVisit(id))return *handled;auto*ptr=objectById(id);if(!ptr)return "地点不存在";Object o=*ptr;auto previousResources=resources;std::string msg;bool remove=false;
 switch(o.type){case ObjectType::Town:if(o.owner==1){startBattle(id);return "黑鸦堡守军迎战！";}visitTownBuildings();msg="进入城镇，可以建设、招募和管理驻军。";break;
 case ObjectType::Mine:if(ptr->owner!=currentOwner()){ptr->owner=currentOwner();msg="占领"+o.name+"，从明天起每日产出 "+std::to_string(mineYield(o.resource))+" "+ResourceNames[o.resource]+"。";}else msg=o.name+"：每日产出 "+std::to_string(mineYield(o.resource))+" "+ResourceNames[o.resource]+"，次日自动入库。";break;
 case ObjectType::Resource:case ObjectType::Treasure:resources[o.resource]+=o.amount;msg="获得 "+std::to_string(o.amount)+" "+ResourceNames[o.resource]+"。";remove=true;break;
 case ObjectType::Monster:startBattle(id);return "遭遇敌军！";
 case ObjectType::Well:
  if(hero.wellDay==day)msg="同一天再次饮用任何魔法井，都不能恢复更多魔法值。";
  else if(hero.mana>=maxMana())msg="你的魔法值已经充满，无需饮用井水。";
  else{hero.mana=maxMana();hero.wellDay=day;msg="饮下井水后，魔法值已恢复至上限。";}break;
 case ObjectType::Artifact:hero.attack+=2;hero.artifacts.push_back(o.name);msg="获得骑士之剑，攻击 +2。";remove=true;break;
 case ObjectType::Shrine:{
  int spell=o.amount>0?o.amount-1:MagicArrow;
  if(spell<0||spell>=SpellCount||Spells[spell].level!=1)msg="此神殿没有可传授的一级法术。";
  else if(!hero.spellbook)msg="神殿愿意传授"+Spells[spell].name+"，但你需要先购买魔法书。";
  else if(hero.spells[spell])msg="你已经学会"+Spells[spell].name+"，神殿没有更多内容可以传授。";
  else{hero.spells[spell]=true;ptr->visited=true;msg="神殿的侍僧传授了"+Spells[spell].name+"。";}
  break;}
 case ObjectType::TradingPost:if(hero.pos!=o.pos)return "英雄必须访问贸易站";msg="贸易站开放交易，使用五个市场的兑换价格。";break;
 case ObjectType::Windmill:if(o.visited){msg="风车本周已经领过资源。";break;}ptr->visited=true;{int r=Ore+int(random()*(ResourceCount-Ore)),n=3+int(random()*4);resources[r]+=n;msg="风车提供 "+std::to_string(n)+" "+ResourceNames[r]+"。";}break;
 case ObjectType::WaterWheel:if(o.visited){msg="水车本周已经领过金币。";break;}ptr->visited=true;{int n=day<=7?500:1000;resources[Gold]+=n;msg="水车提供 "+std::to_string(n)+" 金币。";}break;
 case ObjectType::Obelisk:if(!ptr->properties.is_object())ptr->properties=nlohmann::json::object();ptr->properties["visitedOwners"]=ptr->properties.value("visitedOwners",ptr->visited?1:0)|(1<<currentOwner());ptr->visited=true;msg=o.visited?"你已经辨认过这座方尖碑，藏宝图保持原有进度。":"方尖碑上的古老铭文揭开了藏宝图的一部分。";break;}
 if(o.type==ObjectType::Well||o.type==ObjectType::Shrine||o.type==ObjectType::Windmill||o.type==ObjectType::WaterWheel||o.type==ObjectType::Obelisk){
  AdventureVisit notice{o.type,o.name,msg};
  for(int r=0;r<ResourceCount;++r)notice.reward[r]=resources[r]-previousResources[r];
  if(o.type==ObjectType::Shrine&&hero.spellbook){int spell=o.amount>0?o.amount-1:MagicArrow;if(spell>=0&&spell<SpellCount&&hero.spells[spell])notice.spell=spell;}
  adventureVisit_=std::move(notice);
 }
 if(o.type!=ObjectType::Mine||o.owner!=0)emit({GameEventType::Visit,-1,int(o.type),o.pos});if(remove)std::erase_if(objects,[id](auto&v){return v.id==id;});message(msg);return msg;}
int Game::income()const{if(!hasTown())return 0;return town.built[Capitol]?4000:town.built[CityHall]?2000:town.built[TownHall]?1000:500;}
int Game::mineYield(int resource){return resource<0||resource>=ResourceCount?0:resource==Gold?1000:resource==Wood||resource==Ore?2:1;}
Resources Game::mineIncome()const{Resources result{};for(const auto&o:objects)if(o.type==ObjectType::Mine&&o.owner==currentOwner()&&o.resource>=0&&o.resource<ResourceCount)result[o.resource]+=mineYield(o.resource);return result;}
Resources Game::dailyIncome()const{auto result=mineIncome();if(imported()){for(const auto&o:objects)if(o.type==ObjectType::Town&&o.owner==currentOwner()){auto it=settlements.find(o.id);if(it==settlements.end())continue;const auto&t=o.id==activeTownId?town:it->second;result[Gold]+=t.built[Capitol]?4000:t.built[CityHall]?2000:t.built[TownHall]?1000:500;if(t.built[ResourceSilo])for(int r=0;r<ResourceCount;r++)result[r]+=TownDefs[t.faction].silo[r];}for(const auto&[id,h]:heroes)if(h.owner==currentOwner()&&h.value.originalId==142)result[Gold]+=350;return result;}result[Gold]+=income();if(town.built[ResourceSilo])for(int r=0;r<ResourceCount;r++)result[r]+=townDef().silo[r];return result;}
int Game::weeklyGrowth(int tier)const{if(tier<0||tier>=7)return 0;int n=Units[town.faction*14+tier*2].growth;n=town.built[Castle]?n*2:town.built[Citadel]?n*3/2:n;for(int i=0;i<2;i++)if(townDef().hordeTier[i]==tier&&town.built[i==0?GriffinBastion:Horde2])n+=townDef().hordeGrowth[i];return n;}
int Game::maxMana()const{constexpr double bonus[]={1,1.25,1.5,2};return int(primarySkill(3)*10*bonus[hero.skills[Intelligence]]);}
int Game::movementAllowance(int owner)const{if(imported()&&hero.onBoat)return 1500+seaMovementBonus(owner<0?currentOwner():owner);int lowest=11;for(auto&s:hero.army)if(!s.empty())lowest=std::min(lowest,Units[s.unit].speed);constexpr int base[]={1300,1300,1300,1500,1560,1630,1700,1760,1830,1900,1960,2000};return int(base[std::clamp(lowest,0,11)]*(1+.1*hero.skills[Logistics]))+std::max(town.stableWeek==(day-1)/7?400:0,adventureModifier(hero,2))+artifactLandMovement();}
bool Game::canAfford(const Resources&cost)const{for(int i=0;i<ResourceCount;i++)if(resources[i]<cost[i])return false;return true;}
std::string Game::endDay(){if(battle||report||result||!hero.levelChoices.empty())return "请先完成战斗或升级";auto production=dailyIncome(),mines=mineIncome();day++;emit({GameEventType::NewDay,-1,day});for(int r=0;r<ResourceCount;r++)resources[r]=int(std::min<int64_t>(1000000000,int64_t(resources[r])+production[r]));
 if(visiting())visitTownBuildings();hero.maxMovement=movementAllowance();hero.movement=hero.maxMovement;if(hero.mana<maxMana())hero.mana=std::min(maxMana(),hero.mana+1+hero.skills[Mysticism]);if(visiting()&&town.built[MageGuild])hero.mana=std::max(hero.mana,maxMana());
 if((day-1)%7==0){for(auto&o:objects)if(o.type==ObjectType::Windmill||o.type==ObjectType::WaterWheel)o.visited=false;for(int i=0;i<7;i++)if(town.built[Dwellings[i]])town.pool[i]+=weeklyGrowth(i);message("新的一周开始，城镇兵源已增加。");}if(imported()){importedNewDay();mapNewDay();}std::string msg="第 "+std::to_string(day)+" 天：城镇 +"+std::to_string(imported()?production[Gold]-mines[Gold]:income())+" 金币";for(int r=0;r<ResourceCount;r++)if(mines[r])msg+="，矿山 +"+std::to_string(mines[r])+" "+ResourceNames[r];msg+="，行动力恢复。";message(msg);return msg;}
bool Game::buildingRequirementsMet(int id)const{if(id<0||id>=BuildingCount)return false;const auto&b=townDef().buildings[id];return std::any_of(b.requirements.begin(),b.requirements.end(),[&](const auto&option){return std::all_of(option.begin(),option.end(),[&](int need){return town.built[need];});});}
void Game::visitTownBuildings(){if(!visiting())return;learnTownSpells();if(town.faction==0&&town.built[Special2]&&town.stableWeek!=(day-1)/7){town.stableWeek=(day-1)/7;if(!hasAdventureBonus(hero,94))hero.movement+=400;hero.maxMovement=movementAllowance();message("马厩为本周提供 400 额外行动力。");}
 int lesson=town.faction==2||town.faction==3||town.faction==5||town.faction==6?3:town.faction==7?0:-1;if(lesson>=0&&town.built[Special1+lesson]&&!town.lessons[lesson]){town.lessons[lesson]=true;if(town.faction==2)hero.knowledge++;else if(town.faction==3)hero.power++;else if(town.faction==5)gainXP(1000);else if(town.faction==6)hero.attack++;else if(town.faction==7)hero.defense++;message("英雄获得"+townDef().buildings[Special1+lesson].name+"的永久奖励。");}
 if(town.faction==2&&town.built[Special2])for(int y=0;y<mapHeight;y++)for(int x=0;x<mapWidth;x++)if(std::hypot(x-town.pos.x,y-town.pos.y)<=20)tile({x,y})->seen=true;
}
BuildState Game::constructionState(int id)const{
 if(!hasTown())return BuildState::Unavailable;
 if(battle||report||result||!hero.levelChoices.empty())return BuildState::Busy;
 if(id<0||id>=BuildingCount)return BuildState::Invalid;
 const auto&building=townDef().buildings[id];
 if(!building.available)return BuildState::Unavailable;if(town.built[id])return BuildState::Owned;
 if(!building.implemented&&!(imported()&&id==Shipyard&&coastalTown()))return BuildState::Unsupported;
 if(town.builtDay==day)return BuildState::AlreadyBuiltToday;
 if(!buildingRequirementsMet(id))return BuildState::MissingRequirements;
 if(!canAfford(building.cost))return BuildState::NoResources;
 return BuildState::Ready;
}
int Game::hallBuilding(int row,int column)const{
 if(row<0||row>=int(townDef().hallSlots.size())||column<0||column>=int(townDef().hallSlots[row].size()))return -1;
 int last=-1;for(int id:townDef().hallSlots[row][column])if(id>=0&&id<BuildingCount&&townDef().buildings[id].available){last=id;if(!town.built[id])return id;}
 return last;
}
int Game::hallIcon(int building)const{
 if(building<0||building>=BuildingCount)return -1;
 const auto&definition=townDef().buildings[building];int horde=building==GriffinBastion?0:building==Horde2?1:-1;
 if(horde>=0&&definition.upIcon>=0){int tier=townDef().hordeTier[horde];if(tier>=0&&town.built[UpGuardhouse+tier])return definition.upIcon;}
 return definition.icon;
}
std::string Game::build(int id){
 switch(constructionState(id)){
 case BuildState::Busy:return "当前无法建设";
 case BuildState::Invalid:return "无效建筑";
 case BuildState::Unavailable:return "该城镇没有此建筑";
 case BuildState::Owned:return "已经建造";
 case BuildState::Unsupported:return "该建筑的功能仍在实现";
 case BuildState::AlreadyBuiltToday:return "城镇每天只能建设一次";
 case BuildState::MissingRequirements:return "尚未满足前置建筑要求";
 case BuildState::NoResources:return "资源不足";
 case BuildState::Ready:break;
 }
 const auto&b=townDef().buildings[id];for(int i=0;i<ResourceCount;i++)resources[i]-=b.cost[i];town.built[id]=true;town.builtDay=day;emit({GameEventType::Build,-1,id,town.pos});
 if(b.unit>=0)town.pool[baseTier(b.unit)]+=weeklyGrowth(baseTier(b.unit));visitTownBuildings();
 std::string msg=b.name+"建造完成。";message(msg);return msg;
}
std::array<int,7> Game::recruitableUnits()const{std::array<int,7>out{};for(int i=0;i<7;i++)out[i]=town.built[Dwellings[i]]?town.faction*14+2*i+(town.built[Upgrades[i]]?1:0):-1;return out;}
int Game::recruitMax(int unit)const{if(!hasTown())return 0;int tier=baseTier(unit);if(tier<0||Units[unit].faction!=town.faction||!town.built[Dwellings[tier]]||(unit%2&&!town.built[Upgrades[tier]]))return 0;int n=town.pool[tier];auto cost=unitCost(unit);for(int r=0;r<ResourceCount;r++)if(cost[r]>0)n=std::min(n,resources[r]/cost[r]);return n;}
std::string Game::recruit(int unit,int count,bool garrison){if(battle||report||result||!hero.levelChoices.empty())return "当前无法招募";if(count<=0||count>recruitMax(unit))return "可招募数量或资源不足";if(!garrison&&!visiting())return "英雄必须进入城镇才能接收部队";Army&a=garrison?town.garrison:hero.army;int slot=-1;for(int i=0;i<ArmySlots;i++)if(!a[i].empty()&&a[i].unit==unit){slot=i;break;}if(slot<0)for(int i=0;i<ArmySlots;i++)if(a[i].empty()){slot=i;break;}if(slot<0)return "七个部队栏位已满";if(count>MaxArmyStack-a[slot].count)return "超过当前部队数量上限";
 auto total=unitCost(unit,count);for(int r=0;r<ResourceCount;r++)resources[r]-=total[r];town.pool[baseTier(unit)]-=count;a[slot]={unit,a[slot].count+count};emit({GameEventType::Recruit,unit,count,town.pos});std::string msg="招募 "+std::to_string(count)+" 名"+Units[unit].name+"。";message(msg);return msg;}
std::string Game::upgrade(bool garrison,int slot){if(battle||report||result||!hero.levelChoices.empty()||slot<0||slot>=ArmySlots)return "当前无法升级";if(!garrison&&!visiting())return "英雄需要进入城镇";auto&a=garrison?town.garrison:hero.army;auto&s=a[slot];int tier=baseTier(s.unit);if(s.empty()||tier<0||Units[s.unit].faction!=town.faction||Units[s.unit].upgraded||!town.built[Upgrades[tier]])return "该部队无法在城镇升级";const auto&to=Units[s.unit+1];Resources cost{};auto fromCost=unitCost(s.unit),toCost=unitCost(s.unit+1);for(int r=0;r<ResourceCount;r++)cost[r]=int(std::clamp<int64_t>(int64_t(toCost[r]-fromCost[r])*s.count,-std::numeric_limits<int>::max(),std::numeric_limits<int>::max()));if(!canAfford(cost))return "升级资源不足";for(int i=0;i<ResourceCount;i++)resources[i]-=cost[i];s.unit++;emit({GameEventType::Upgrade,s.unit,s.count,town.pos});message("部队升级为"+to.name+"。");return log.back();}
std::string Game::transfer(bool fromGarrison,int fromSlot,bool toGarrison,int toSlot,int amount){if(battle||report||result||!hero.levelChoices.empty())return "当前无法调整部队";if(fromSlot<0||fromSlot>=ArmySlots||toSlot<0||toSlot>=ArmySlots)return "无效栏位";if(fromGarrison!=toGarrison&&!visiting())return "英雄必须进入城镇才能交换驻军";if(fromGarrison==toGarrison&&fromSlot==toSlot)return "请选择不同栏位";
 Army&from=fromGarrison?town.garrison:hero.army;Army&to=toGarrison?town.garrison:hero.army;auto&a=from[fromSlot];auto&b=to[toSlot];if(a.empty()||amount<=0||amount>a.count)return "拆分数量无效";bool swap=!b.empty()&&b.unit!=a.unit;if(swap&&amount!=a.count)return "不同兵种只能整队交换";
 if(!fromGarrison&&toGarrison&&amount==a.count&&armyStacks(from)==1&&!swap)return "英雄至少需要保留一个单位";
 if(!swap&&amount>MaxArmyStack-b.count)return "超过当前部队数量上限";
 if(swap){std::swap(a,b);}else{b={a.unit,b.count+amount};a.count-=amount;if(a.count==0)a={};}message(swap?"部队交换完成。":"部队转移完成。");return log.back();}
std::string Game::dismiss(bool garrison,int slot){if(battle||report||result||!hero.levelChoices.empty()||slot<0||slot>=ArmySlots)return "无效操作";auto&a=garrison?town.garrison:hero.army;if(a[slot].empty())return "空栏位";if(!garrison&&armyStacks(a)==1)return "英雄必须保留一支部队";a[slot]={};message("部队已遣散。");return log.back();}
int Game::experienceForLevel(int level){constexpr int known[]={0,0,1000,2000,3200,4600,6200,8000,10000,12200,14700,17500,20860};if(level<1)return 0;if(level<=12)return known[level];int total=known[12],increment=3360;for(int l=13;l<=level;l++){increment=int(increment*1.2);if(total>1000000000-increment)return 1000000000;total+=increment;}return total;}
void Game::gainXP(int amount){if(amount<=0)return;hero.xp=int(std::min<int64_t>(int64_t(hero.xp)+amount,1000000000));while(hero.level<70&&hero.xp>=experienceForLevel(hero.level+1)){hero.level++;emit({GameEventType::Level,-1,hero.level,hero.pos});int r=int(random()*100);int primary=r<35?0:r<80?1:r<90?2:3;int*stats[]={&hero.attack,&hero.defense,&hero.power,&hero.knowledge};++*stats[primary];LevelChoice choice;choice.level=hero.level;choice.primary=primary;std::vector<int>up,newSkills;int known=int(std::count_if(hero.skills.begin(),hero.skills.end(),[](int n){return n>0;}));for(int i=0;i<SkillCount;i++)if(hero.skills[i]>0&&hero.skills[i]<3)up.push_back(i);else if(hero.skills[i]==0&&known<8)newSkills.push_back(i);
 if(!up.empty()){int i=int(random()*up.size());choice.skills.push_back(up[i]);up.erase(up.begin()+i);}if(!newSkills.empty())choice.skills.push_back(newSkills[int(random()*newSkills.size())]);else if(!up.empty())choice.skills.push_back(up[int(random()*up.size())]);if(!choice.skills.empty())hero.levelChoices.push_back(choice);message("凯瑟琳升至 "+std::to_string(hero.level)+" 级，请选择一项技能。");}}
std::string Game::chooseSkill(int id){if(hero.levelChoices.empty())return "没有待选择的技能";const auto&choices=hero.levelChoices.front().skills;if(std::find(choices.begin(),choices.end(),id)==choices.end()||hero.skills[id]>=3)return "无效技能选择";hero.skills[id]++;hero.levelChoices.erase(hero.levelChoices.begin());while(!hero.levelChoices.empty()){auto&next=hero.levelChoices.front();next.skills.clear();std::vector<int>up,fresh;for(int i=0;i<SkillCount;i++)if(hero.skills[i]>0&&hero.skills[i]<3)up.push_back(i);else if(hero.skills[i]==0&&std::count_if(hero.skills.begin(),hero.skills.end(),[](int n){return n>0;})<8)fresh.push_back(i);if(!up.empty()){int at=int(random()*up.size());next.skills.push_back(up[at]);up.erase(up.begin()+at);}if(!fresh.empty())next.skills.push_back(fresh[int(random()*fresh.size())]);else if(!up.empty())next.skills.push_back(up[int(random()*up.size())]);if(!next.skills.empty())break;hero.levelChoices.erase(hero.levelChoices.begin());}learnTownSpells();hero.maxMovement=movementAllowance();message("掌握"+SkillLevels[hero.skills[id]]+SkillNames[id]+"。");return log.back();}
}
