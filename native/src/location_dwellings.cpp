#include "h3/locations.hpp"
#include <algorithm>

namespace h3::locations {
void initializeDwelling(Game&g,Object&o){auto&p=o.properties;int cls=o.sourceClass,week=(g.day-1)/7;
 bool first=!p.contains("locationUnits");
 if(first){
  std::vector<int>units;auto key=std::to_string(cls)+":"+std::to_string(o.sourceSubtype);
  if(facts()["dwellings"].contains(key)){for(const auto&row:facts()["dwellings"][key]["units"])if(!row.empty())units.push_back(row[0]);}
  else if(cls>=216&&cls<=218){
   int faction=cls==218?std::clamp(o.sourceSubtype,0,8):-1;
   if(p.value("town_absod_id",uint64_t(0))!=0)for(const auto&town:g.objects)if(town.type==ObjectType::Town&&town.properties.contains("absod_id")&&town.properties["absod_id"]==p["town_absod_id"]&&g.settlements.contains(town.id)){faction=g.settlements.at(town.id).faction;break;}
   if(faction<0){std::vector<int>pool;const char*names[]={"castle","rampart","tower","inferno","necropolis","dungeon","stronghold","fortress","conflux"};for(int i=0;i<9;++i)if(!p.contains("alignment")||p["alignment"].value(names[i],false))pool.push_back(i);faction=pool.empty()?0:pool[int(g.random()*pool.size())];}
   int low=cls==217?std::clamp(o.sourceSubtype,0,6):std::clamp(p.value("min_level",0),0,6);int high=cls==217?low:std::clamp(p.value("max_level",6),low,6);
   units.push_back(faction*14+2*(low+int(g.random()*(high-low+1))));
  }
  if(cls==78){std::vector<int>pool;for(int unit=0;unit<UnitCount;++unit)if(Units[unit].growth>0)pool.push_back(unit);units={pool[int(g.random()*pool.size())]};}
  p["locationUnits"]=units;p["locationPool"]=std::vector<int>(units.size());
 }
 if(p.value("locationStockWeek",-1)!=week){
  auto units=p["locationUnits"].get<std::vector<int>>();if(cls==78&&!first){std::vector<int>pool;for(int unit=0;unit<UnitCount;++unit)if(Units[unit].growth>0)pool.push_back(unit);units={pool[int(g.random()*pool.size())]};p["locationUnits"]=units;}
  std::vector<int>stock;for(int unit:units)stock.push_back(Units[unit].growth);p["locationPool"]=stock;p["locationStockWeek"]=week;
  if(cls!=78){
   o.army={};int slot=0;auto key=std::to_string(cls)+":"+std::to_string(o.sourceSubtype);
   if(first&&facts()["dwellings"].contains(key))for(const auto&s:facts()["dwellings"][key]["guards"])if(slot<ArmySlots)o.army[slot++]={s["unit"],s["count"]};
   if(!slot&&std::any_of(units.begin(),units.end(),[](int u){return Units[u].level>=5;}))for(int u:units)if(slot<ArmySlots)o.army[slot++]={u,3*Units[u].growth};
  }
 }
}
std::string dwelling(Game&g,int id){auto&o=*g.objectById(id);
 if(armyCount(o.army)>0&&o.owner!=g.currentOwner())return "巢穴有守卫驻防。击败守卫后可以占领并招募。";
 if(o.sourceClass!=78)o.owner=g.currentOwner();o.visited=true;
 auto units=o.properties["locationUnits"].get<std::vector<int>>();std::string text=o.sourceClass==78?"难民营每周迎来一批不同的生物。":"巢穴已归你控制，兵源每周更新。一级生物免费加入，其他生物按原价招募。";
 if(units.empty())return "这座巢穴没有配置可招募的生物。";
 for(size_t i=0;i<units.size();++i)text+="\n"+Units[units[i]].name+"："+std::to_string(o.properties["locationPool"][i].get<int>());
 return text;
}
std::vector<LocationOffer> dwellingOffers(const Game&g,const Object&o){std::vector<LocationOffer>offers;if(o.sourceClass!=78&&o.owner!=g.currentOwner())return offers;
 auto units=o.properties.value("locationUnits",std::vector<int>{}),stock=o.properties.value("locationPool",std::vector<int>{});if(units.size()!=stock.size())return offers;
 for(size_t i=0;i<units.size();++i){int unit=units[i];LocationOffer f;f.action=LocationAction::Hire;f.parameter=int(i);f.unit=unit;f.cost=(Units[unit].level==1&&o.sourceClass!=78)?Resources{}:unitCost(unit);f.maximum=stock[i];for(int r=0;r<ResourceCount;++r)if(f.cost[r])f.maximum=std::min(f.maximum,g.resources[r]/f.cost[r]);f.label="招募"+Units[unit].name;f.description="每周产量 "+std::to_string(Units[unit].growth)+"；剩余 "+std::to_string(stock[i]);Army trial=g.hero.army;consolidate(trial);f.enabled=f.maximum>0&&merge(trial,unit,1);if(!f.enabled&&f.maximum>0)f.description+="；部队栏位不足";offers.push_back(f);}
 return offers;
}
std::string hire(Game&g,Object&o,const LocationOffer&f,int amount){Army army=g.hero.army;
 // Classic recruitment merges identical stacks before reporting a full army.
 if(!merge(army,f.unit,amount)){army=g.hero.army;for(int a=0;a<ArmySlots;++a)for(int b=a+1;b<ArmySlots;++b)if(!army[a].empty()&&army[a].unit==army[b].unit&&army[a].count<=MaxArmyStack-army[b].count){army[a].count+=army[b].count;army[b]={};}if(!merge(army,f.unit,amount))return "部队栏位不足，请先合并或转移部队。";}
 for(int r=0;r<ResourceCount;++r)g.resources[r]-=f.cost[r]*amount;g.hero.army=army;o.properties["locationPool"][f.parameter]=o.properties["locationPool"][f.parameter].get<int>()-amount;
 return Units[f.unit].name+" × "+std::to_string(amount)+" 加入了部队。";
}
}
