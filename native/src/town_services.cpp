#include "h3/game.hpp"
#include "h3/locations.hpp"
#include <algorithm>
#include <cmath>

namespace h3 {
namespace {
Object serviceObject(const Game&g,int building){Object o;o.id=g.activeTownId;o.owner=g.currentOwner();o.pos=g.town.pos;o.level=g.town.level;o.properties=nlohmann::json::object();if(building==Blacksmith||(g.town.faction==6&&building==Special3))o.sourceClass=106;else if(g.town.faction==6&&building==Special2)o.sourceClass=213;else o.sourceClass=7;o.properties["blackMarketStock"]=g.town.merchantStock;return o;}
int machine(int faction){constexpr int ids[]={4,6,5,5,6,4,5,6,4};return ids[faction];}
bool merchant(int faction,int building){return building==Special1&&(faction==2||faction==5||faction==8);}
}
void Game::refreshTownServices(){
 if(!hasTown())return;
 if(town.faction==5&&town.built[Special3]&&town.portalUnit<0){std::vector<int>pool;for(const auto&o:objects)if(o.owner==currentOwner()&&dwellingLocation(o.sourceClass)&&o.sourceClass!=78&&o.properties.is_object())for(int unit:o.properties.value("locationUnits",std::vector<int>{}))pool.push_back(unit);if(!pool.empty()){town.portalUnit=pool[size_t(random()*pool.size())];town.portalStock=Units[town.portalUnit].growth;}}
 int month=(day-1)/28;if(merchant(town.faction,Special1)&&town.built[Special1]&&town.merchantMonth!=month){town.merchantStock.clear();for(int cls:{66,66,66,67,67,67,68})town.merchantStock.push_back(rollArtifact(cls));town.merchantMonth=month;}
}
std::vector<LocationOffer> Game::townOffers(int building)const{
 std::vector<LocationOffer>out;if(!visiting()||building<0||building>=BuildingCount||!town.built[building]||battle||report||neutralOffer||result)return out;
 if(building==Blacksmith||(town.faction==6&&(building==Special2||building==Special3))||merchant(town.faction,building)){
  auto o=serviceObject(*this,building);out=locations::serviceOffers(*this,o);
  if(o.sourceClass==106){int id=town.faction==6&&building==Special3?4:machine(town.faction);std::erase_if(out,[id](auto&f){return f.artifact!=id;});}
  if(o.sourceClass==7){std::vector<LocationOffer>priced;constexpr int prices[]={1,250,250,500,500,500,500};double effectiveness=std::min((marketplaces()+3.)/20.,.6);for(auto f:out)for(int r=0;r<ResourceCount;++r){auto offer=f;offer.parameter=f.parameter*ResourceCount+r;offer.cost={};offer.cost[r]=std::max(1,int(std::lround(locations::artifactPrice(f.artifact)/effectiveness/(prices[r]*(r?0.5:1.)))));offer.label+=" · "+ResourceNames[r];offer.enabled=canAfford(offer.cost);priced.push_back(offer);}out=std::move(priced);}
  if(o.sourceClass==7){std::vector<int>items=hero.artifactIds;std::sort(items.begin(),items.end());items.erase(std::unique(items.begin(),items.end()),items.end());for(int id:items)if(id>=7&&id<=127&&locations::artifactPrice(id)>0)for(int r=0;r<ResourceCount;++r){LocationOffer f;f.action=LocationAction::SellArtifact;f.artifact=id;f.parameter=id*ResourceCount+r;f.maximum=int(std::count(hero.artifactIds.begin(),hero.artifactIds.end(),id));f.label=artifactName(id)+" → "+ResourceNames[r];constexpr int prices[]={1,250,250,500,500,500,500};int n=std::max(1,int(std::lround(locations::artifactPrice(id)*std::min((marketplaces()+3.)/20.,.6)/prices[r])));f.description="每件换取 "+std::to_string(n)+" "+ResourceNames[r];out.push_back(f);}}
  return out;
 }
 if(town.faction==4&&building==Special3)for(int slot=0;slot<ArmySlots;++slot){auto s=hero.army[slot];if(s.empty()||s.unit==Skeleton)continue;LocationOffer f;f.action=LocationAction::Transform;f.parameter=slot;f.unit=(std::find(locations::facts()["transformerDragons"].begin(),locations::facts()["transformerDragons"].end(),s.unit)!=locations::facts()["transformerDragons"].end())?Skeleton+12:Skeleton;f.maximum=s.count;f.label=Units[s.unit].name+" → "+Units[f.unit].name;Army trial=hero.army;trial[slot]={};f.enabled=locations::merge(trial,f.unit,s.count);out.push_back(f);}
 if(town.faction==3&&building==Special3)for(const auto&o:objects)if(o.id!=activeTownId&&o.type==ObjectType::Town&&o.owner==currentOwner()&&settlements.contains(o.id)&&settlements.at(o.id).faction==3&&settlements.at(o.id).built[Special3]){LocationOffer f;f.action=LocationAction::Travel;f.parameter=o.id;f.label="前往"+o.name;f.enabled=std::none_of(heroes.begin(),heroes.end(),[&](auto&h){return h.second.level==o.level&&h.second.value.pos==o.pos;});out.push_back(f);}
 if(town.faction==5&&building==Special3&&town.portalUnit>=0){LocationOffer f;f.action=LocationAction::Hire;f.unit=town.portalUnit;f.parameter=town.portalUnit;f.maximum=town.portalStock;f.cost=unitCost(f.unit);for(int r=0;r<ResourceCount;++r)if(f.cost[r])f.maximum=std::min(f.maximum,resources[r]/f.cost[r]);f.label="召唤"+Units[f.unit].name;Army trial=hero.army;f.enabled=f.maximum>0&&locations::merge(trial,f.unit,1);out.push_back(f);}
 return out;
}
std::string Game::townService(int building,LocationAction action,int parameter,int amount){
 refreshTownServices();auto offers=townOffers(building);auto at=std::find_if(offers.begin(),offers.end(),[&](auto&f){return f.action==action&&f.parameter==parameter;});if(at==offers.end()||!at->enabled||amount<1||amount>at->maximum)return "当前选择已失效，或英雄尚未进入城镇。";auto f=*at;Resources cost{};for(int r=0;r<ResourceCount;++r){int64_t n=int64_t(f.cost[r])*amount;if(n>1000000000)return "交易数量过大。";cost[r]=int(n);}if(!canAfford(cost))return "资源不足。";
 std::string text;if(action==LocationAction::Transform){Army next=hero.army;next[parameter].count-=amount;if(!next[parameter].count)next[parameter]={};if(!locations::merge(next,f.unit,amount))return "部队栏位不足。";hero.army=next;text="转换完成："+Units[f.unit].name+" × "+std::to_string(amount)+"。";}
 else if(action==LocationAction::SellArtifact){int r=parameter%ResourceCount,id=parameter/ResourceCount;constexpr int prices[]={1,250,250,500,500,500,500};int n=std::max(1,int(std::lround(locations::artifactPrice(id)*std::min((marketplaces()+3.)/20.,.6)/prices[r])));if(int64_t(n)*amount+resources[r]>1000000000)return "资源超过上限。";for(int i=0;i<amount;++i)locations::removeArtifact(hero,id);resources[r]+=n*amount;text="出售完成，获得 "+std::to_string(n*amount)+" "+ResourceNames[r]+"。";}
 else if(action==LocationAction::Travel){auto*o=objectById(parameter);if(!o)return "目标城镇不存在。";hero.pos=o->pos;mapLevel=o->level;selectTown(o->id);reveal();visitTownBuildings();text="通过城堡之门抵达"+o->name+"。";}
 else if(action==LocationAction::Hire){Army next=hero.army;if(!locations::merge(next,f.unit,amount))return "部队栏位不足。";hero.army=next;town.portalStock-=amount;for(int r=0;r<ResourceCount;++r)resources[r]-=cost[r];text="召唤了"+Units[f.unit].name+" × "+std::to_string(amount)+"。";}
 else{auto o=serviceObject(*this,building);if(action==LocationAction::BuyArtifact)f.parameter/=ResourceCount;text=locations::performService(*this,o,f,amount);if(action==LocationAction::BuyArtifact)town.merchantStock=o.properties["blackMarketStock"].get<std::vector<int>>();}
 storeHero();syncHeroObjects();message(text);return text;
}
void Game::townServiceWeek(){
 if((day-1)%7)return;storeHero();auto oldTown=town;int oldId=activeTownId,oldLevel=mapLevel;auto apply=[&](int owner){
  int week=(day-1)/7;if(town.bonusWeek==week)return;town.bonusWeek=week;auto&bank=owner==currentOwner()?resources:playerResources[owner];
  if(town.faction==1){if(town.built[Special3])bank[Gold]=int(std::min<int64_t>(1000000000,int64_t(bank[Gold])*11/10));if(town.built[Special1]){int r=Mercury+int(random()*4),n=1+int(random()*4);bank[r]=std::min(1000000000,bank[r]+n);town.pondResource=r;town.pondAmount=n;}}
  if(town.faction==5&&town.built[Special3]){std::vector<int>pool;for(const auto&o:objects)if(o.owner==owner&&dwellingLocation(o.sourceClass)&&o.sourceClass!=78&&o.properties.is_object())for(int unit:o.properties.value("locationUnits",std::vector<int>{}))pool.push_back(unit);town.portalUnit=pool.empty()?-1:pool[size_t(random()*pool.size())];town.portalStock=town.portalUnit<0?0:Units[town.portalUnit].growth;}
 };
 if(imported())for(auto&[id,value]:settlements){auto*o=objectById(id);if(!o||o->owner<0)continue;town=value;activeTownId=id;apply(o->owner);value=town;}else apply(currentOwner());if(imported()){activeTownId=oldId;town=settlements.contains(oldId)?settlements.at(oldId):oldTown;}mapLevel=oldLevel;
}
std::string Game::giftResources(int owner,int resource,int amount){
 if(battle||report||neutralOffer||result||!imported()||owner<0||owner>7||owner==currentOwner()||resource<0||resource>=ResourceCount||amount<=0||resources[resource]<amount)return "赠送的势力、资源或数量无效。";
 bool exists=std::any_of(heroes.begin(),heroes.end(),[owner](auto&h){return h.second.owner==owner;})||std::any_of(objects.begin(),objects.end(),[owner](auto&o){return o.type==ObjectType::Town&&o.owner==owner;});if(!exists||int64_t(playerResources[owner][resource])+amount>1000000000)return "该势力已被消灭，或资源超过上限。";resources[resource]-=amount;playerResources[owner][resource]+=amount;message("已赠送 "+std::to_string(amount)+" "+ResourceNames[resource]+"。");return log.back();
}
}

namespace h3 {
void Game::townServiceDay(){
 if(!imported())return;storeHero();for(size_t i=0;i<tiles.size();++i)playerFog[currentOwner()][i]=tiles[i].seen;int previous=actingOwner;
 for(const auto&o:objects)if(o.type==ObjectType::Town&&o.owner>=0&&settlements.contains(o.id)){const auto&t=o.id==activeTownId?town:settlements.at(o.id);if(t.faction==2&&t.built[GrailBuilding]){playerFog[o.owner].assign(tiles.size(),true);continue;}if(t.faction!=4||!t.built[Special1])continue;actingOwner=o.owner;auto*cover=objectById(o.id);int cls=cover->sourceClass;bool visited=cover->visited;cover->sourceClass=15;locations::scout(*this,cover->id);cover=objectById(o.id);cover->sourceClass=cls;cover->visited=visited;}
 actingOwner=previous;for(size_t i=0;i<tiles.size();++i)tiles[i].seen=playerFog[currentOwner()][i];
}
}
namespace h3 {
std::string Game::transferGarrison(int id,bool fromGarrison,int fromSlot,bool toGarrison,int toSlot,int amount){
 if(battle||report||neutralOffer||result||!hero.levelChoices.empty())return "当前不能调整驻军。";auto*o=objectById(id);if(!o||(o->sourceClass!=33&&o->sourceClass!=219)||o->owner!=currentOwner()||o->level!=mapLevel||!o->visitableAt(hero.pos))return "英雄需要抵达己方驻防要塞。";
 if(fromSlot<0||fromSlot>=ArmySlots||toSlot<0||toSlot>=ArmySlots||(fromGarrison==toGarrison&&fromSlot==toSlot))return "请选择有效的不同栏位。";Army&from=fromGarrison?o->army:hero.army;Army&to=toGarrison?o->army:hero.army;auto&a=from[fromSlot];auto&b=to[toSlot];if(a.empty()||amount<1||amount>a.count)return "转移数量无效。";bool swap=!b.empty()&&a.unit!=b.unit;if(swap&&amount!=a.count)return "不同兵种只能整队交换。";
 if(fromGarrison!=toGarrison&&!o->properties.value("can_remove_units",1)&&(fromGarrison||swap))return "地图规定这座要塞的驻军不能撤出。";
 if(!fromGarrison&&toGarrison&&amount==a.count&&armyStacks(from)==1&&!swap)return "英雄至少需要保留一名士兵。";if(!swap&&b.count>MaxArmyStack-amount)return "部队数量超过上限。";
 if(swap)std::swap(a,b);else{b={a.unit,b.count+amount};a.count-=amount;if(!a.count)a={};}storeHero();message("驻防部队交换完成。");return log.back();
}
}
