#include "h3/locations.hpp"
#include <algorithm>
#include <cmath>
#include <set>

namespace h3::locations {
namespace {
int alignment(const Game&g){int faction=g.hero.originalId>=0&&g.hero.originalId<144?g.hero.originalId/16:g.town.faction;return faction<3?1:faction<6?-1:0;}
Resources upgradeCost(int unit){auto old=unitCost(unit),next=unitCost(unit+1);Resources cost{};int percent=std::clamp(Units[unit].level-1,0,4)*25;for(int r=0;r<ResourceCount;++r)cost[r]=std::max(0,next[r]-old[r])*percent/100;return cost;}
std::pair<int,int> sale(int unit,int resource){constexpr int prices[]={1,250,250,500,500,500,500};double given=Units[unit].cost,wanted=prices[resource]/.7;return given>wanted?std::pair{1,int(std::ceil(given/wanted))}:std::pair{std::max(1,int(wanted/given+.5)),1};}
}
void initializeService(Game&g,Object&o){auto&p=o.properties;
 if(o.sourceClass==7&&!p.contains("blackMarketStock")){std::vector<int>stock;for(int cls:{66,66,66,67,67,67,68})stock.push_back(g.rollArtifact(cls));p["blackMarketStock"]=stock;}
 if(o.sourceClass==95&&p.value("tavernWeek",-1)!=(g.day-1)/7){std::vector<int>pool;for(int i=0;i<144;++i)if(std::none_of(g.heroes.begin(),g.heroes.end(),[&](auto&h){return h.second.value.originalId==i;}))pool.push_back(i);std::vector<int>offer;for(int i=0;i<2&&!pool.empty();++i){int at=int(g.random()*pool.size());offer.push_back(pool[at]);pool.erase(pool.begin()+at);}p["tavernHeroes"]=offer;p["tavernWeek"]=(g.day-1)/7;}
}
Point spawn(const Game&g,const Object&o){for(int radius=1;radius<=3;++radius)for(int dy=-radius;dy<=radius;++dy)for(int dx=-radius;dx<=radius;++dx){if(std::max(std::abs(dx),std::abs(dy))!=radius)continue;Point p{o.pos.x+dx,o.pos.y+dy};if(p==g.hero.pos)continue;size_t i=(o.level*g.mapHeight+p.y)*g.mapWidth+p.x;if(p.x<0||p.y<0||p.x>=g.mapWidth||p.y>=g.mapHeight)continue;auto&t=g.tiles[i];if(t.blocked||t.terrain==Terrain::Water||t.terrain==Terrain::Rock)continue;if(std::any_of(g.objects.begin(),g.objects.end(),[&](auto&n){return n.level==o.level&&n.pos==p;}))continue;if(std::any_of(g.heroes.begin(),g.heroes.end(),[&](auto&h){return h.second.level==o.level&&h.second.value.pos==p;}))continue;return p;}return {-1,-1};}
Hero prisoner(const Game&g,const Object&o){const auto&p=o.properties;Hero h;heroDefaults(h,p.value("type",0));h.name=p.value("name",h.name);h.xp=std::clamp(p.value("experience",0),0,1000000000);while(h.level<70&&h.xp>=Game::experienceForLevel(h.level+1))++h.level;
 if(p.contains("primary_skills")){const auto&s=p["primary_skills"];h.attack=s.value("attack",h.attack);h.defense=s.value("defense",h.defense);h.power=s.value("spell_power",h.power);h.knowledge=s.value("knowledge",h.knowledge);}
 for(const auto&s:p.value("secondary_skills",nlohmann::json::array())){int original=s.value("type",-1);if(original>=0&&original<28)h.skills[OriginalSkills[original]]=std::clamp(s.value("level",0),0,3);}
 int slot=0;for(const auto&s:p.value("creatures",nlohmann::json::array())){if(slot>=ArmySlots)break;int unit=originalUnit(s.value("type",-1)),count=s.value("quantity",0);if(unit>=0&&count>0)h.army[slot]={unit,std::min(MaxArmyStack,count)};++slot;}
 if(!armyCount(h.army))h.army={Stack{std::clamp(h.originalId/16,0,8)*14,1}};
 if(p.contains("spells"))for(int spell=0;spell<SpellCount;++spell){int original=Spells[spell].originalId;if(original>=0&&g.scenarioRules.contains("spellNames")&&original<int(g.scenarioRules["spellNames"].size()))h.spells[spell]=p["spells"].value(g.scenarioRules["spellNames"][original].get<std::string>(),false);}
 if(p.contains("artifacts")){const auto&a=p["artifacts"];if(a.contains("equipped"))for(const auto&[slot,value]:a["equipped"].items()){int id=value;if(slot=="spellbook"){h.spellbook=id>=0;continue;}h.equipmentSlots[slot]=id;if(id>=0){h.artifactIds.push_back(id);h.wornArtifacts.push_back(id);h.artifacts.push_back(artifactName(id));}}for(int id:a.value("backpack",std::vector<int>{}))if(id>=0){h.artifactIds.push_back(id);h.artifacts.push_back(artifactName(id));}}
 h.keys=g.hero.keys;return h;
}
std::string service(Game&g,int id){auto&o=*g.objectById(id);switch(o.sourceClass){
 case 2:return "将部队或宝物献给祭坛以换取经验。善良英雄可以献祭宝物，邪恶英雄可以献祭部队，中立英雄两者均可。英雄必须保留至少一名士兵。";
 case 7:return "黑市出售七件宝物，库存属于这座市场，售出后不会每周刷新。";
 case 35:return "山丘堡垒可以升级部队：一级免费，二级按差价的 25%，三级 50%，四级 75%，五级及以上按全部差价。";
 case 50:return "时空市场是原图保留的未启用建筑。在死亡阴影原版中不提供交易服务。";
 case 62:return "监狱中关押着"+o.properties.value("name",std::string("一位英雄"))+"。打开监狱即可释放并招入麾下；最多同时拥有八名冒险英雄。";
 case 80:return "英雄在避难所中免受其他英雄攻击。离开后保护立即结束。";
 case 95:return "野外酒馆可以花费 2500 金币招募一名英雄。可供招募的人选每周更新，同一英雄不会重复出现。";
 case 106:return "战争机器工厂出售弩车（2500 金币）、弹药车（1000 金币）和急救帐篷（750 金币）。每位英雄每种机器只能携带一件。";
 case 108:return "确认搜索勇士之墓？获得宝物，同时士气 -3，持续到下一场战斗。";
 case 213:return "将部队交给自由职业者公会，换取金币或其他资源。英雄必须保留最后一名士兵。";
 case 221:return "贸易站提供五座市场的兑换效率，可以交换资源。";
 default:return "";}}
std::vector<LocationOffer> serviceOffers(const Game&g,const Object&o){std::vector<LocationOffer>offers;int cls=o.sourceClass;
 if(cls==35){for(int slot=0;slot<ArmySlots;++slot){auto s=g.hero.army[slot];if(s.empty()||Units[s.unit].faction<0||Units[s.unit].upgraded||s.unit+1>=UnitCount)continue;LocationOffer f;f.action=LocationAction::Upgrade;f.parameter=slot;f.unit=s.unit+1;f.cost=upgradeCost(s.unit);f.maximum=s.count;for(int r=0;r<ResourceCount;++r)if(f.cost[r])f.maximum=std::min(f.maximum,g.resources[r]/f.cost[r]);f.label=Units[s.unit].name+" → "+Units[s.unit+1].name;f.enabled=f.maximum>0;offers.push_back(f);}}
 if(cls==7){auto stock=o.properties.value("blackMarketStock",std::vector<int>{});for(size_t i=0;i<stock.size();++i)if(stock[i]>=0){LocationOffer f;f.action=LocationAction::BuyArtifact;f.parameter=int(i);f.artifact=stock[i];f.cost[Gold]=int(std::ceil(artifactPrice(stock[i])*2.5));f.label=artifactName(stock[i]);f.enabled=g.canAfford(f.cost);offers.push_back(f);}}
 if(cls==106)for(int id:{4,5,6}){LocationOffer f;f.action=LocationAction::BuyMachine;f.parameter=id;f.artifact=id;f.cost[Gold]=artifactPrice(id);f.label=artifactName(id);f.enabled=std::find(g.hero.artifactIds.begin(),g.hero.artifactIds.end(),id)==g.hero.artifactIds.end()&&g.canAfford(f.cost);offers.push_back(f);}
 if(cls==95)for(int id:o.properties.value("tavernHeroes",std::vector<int>{})){if(id<0)continue;LocationOffer f;f.action=LocationAction::RecruitHero;f.parameter=id;f.cost[Gold]=2500;Hero h;heroDefaults(h,id);f.label=h.name;f.enabled=g.ownedHeroes().size()<8&&g.canAfford(f.cost)&&spawn(g,o).x>=0&&std::none_of(g.heroes.begin(),g.heroes.end(),[&](auto&h){return h.second.value.originalId==id;});offers.push_back(f);}
 if(cls==62){LocationOffer f;f.action=LocationAction::FreeHero;f.label="释放英雄";f.enabled=g.ownedHeroes().size()<8&&spawn(g,o).x>=0;offers.push_back(f);}
 if(cls==13){int subtype=std::clamp(o.sourceSubtype,0,2);const auto&rules=g.scenarioRules;bool visited=false;auto owner=std::to_string(g.currentOwner());if(rules.contains("cartographers")&&rules["cartographers"].contains(owner))for(int v:rules["cartographers"][owner])visited|=v==subtype;if(!visited){LocationOffer f;f.action=LocationAction::BuyMap;f.parameter=subtype;f.cost[Gold]=1000;f.label="购买地图";f.enabled=g.canAfford(f.cost);offers.push_back(f);}}
 if(cls==108&&!o.visited){LocationOffer f;f.action=LocationAction::Claim;f.label="搜索墓穴";f.description="士气 -3，持续到下一场战斗";offers.push_back(f);}
 if(cls==221){LocationOffer f;f.action=LocationAction::Trade;f.label="交换资源";offers.push_back(f);}
 if(cls==2||cls==213){if(cls==213||alignment(g)<=0)for(int slot=0;slot<ArmySlots;++slot){auto s=g.hero.army[slot];if(s.empty())continue;int available=std::min(s.count,armyCount(g.hero.army)-1);if(cls==2){LocationOffer f;f.action=LocationAction::SacrificeCreature;f.parameter=slot;f.unit=s.unit;f.maximum=available;f.label="献祭"+Units[s.unit].name;f.description="每名士兵换取 "+std::to_string(g.experienceAward(creatureExperience(s.unit)))+" 经验";f.enabled=available>0&&creatureExperience(s.unit)>0;offers.push_back(f);}else for(int r=0;r<ResourceCount;++r){auto[give,receive]=sale(s.unit,r);LocationOffer f;f.action=LocationAction::SellCreature;f.parameter=slot*ResourceCount+r;f.unit=s.unit;f.maximum=available/give;f.label=Units[s.unit].name+" → "+ResourceNames[r];f.description=std::to_string(give)+" 名换取 "+std::to_string(receive)+" "+ResourceNames[r];f.enabled=f.maximum>0;offers.push_back(f);}}}
 if(cls==2&&alignment(g)>=0){std::set<int>seen;for(int id:g.hero.artifactIds)if(artifactExperience(id)>0&&seen.insert(id).second){LocationOffer f;f.action=LocationAction::SacrificeArtifact;f.parameter=id;f.artifact=id;f.maximum=int(std::count(g.hero.artifactIds.begin(),g.hero.artifactIds.end(),id));f.label="献祭"+artifactName(id);f.description="每件换取 "+std::to_string(g.experienceAward(artifactExperience(id)))+" 经验";offers.push_back(f);}}
 return offers;
}
std::string performService(Game&g,Object&o,const LocationOffer&f,int amount){int id=o.id;
 if(f.action==LocationAction::Trade)return "贸易站已经开放，可以按五座市场的效率交换资源。";
 if(f.action==LocationAction::Upgrade){auto&s=g.hero.army[f.parameter];if(s.count!=amount){Army next=g.hero.army;s.count-=amount;next=g.hero.army;if(!merge(next,f.unit,amount)){s.count+=amount;return "部分升级需要一个空部队栏位，或可以合并的同类部队。";}g.hero.army=next;}else s.unit=f.unit;for(int r=0;r<ResourceCount;++r)g.resources[r]-=f.cost[r]*amount;return "已升级 "+Units[f.unit].name+" × "+std::to_string(amount)+"。";}
 if(f.action==LocationAction::BuyArtifact||f.action==LocationAction::BuyMachine){for(int r=0;r<ResourceCount;++r)g.resources[r]-=f.cost[r];giveArtifact(g,f.artifact);if(f.action==LocationAction::BuyArtifact)o.properties["blackMarketStock"][f.parameter]=-1;return "获得"+artifactName(f.artifact)+"。";}
 if(f.action==LocationAction::SacrificeArtifact){int xp=g.experienceAward(artifactExperience(f.parameter)*amount);for(int i=0;i<amount;++i)removeArtifact(g.hero,f.parameter);g.gainXP(xp,false);return "献祭宝物，获得 "+std::to_string(xp)+" 点经验。";}
 if(f.action==LocationAction::SacrificeCreature){auto&s=g.hero.army[f.parameter];int xp=g.experienceAward(int(std::min<int64_t>(1000000000,int64_t(creatureExperience(s.unit))*amount)));s.count-=amount;if(!s.count)s={};g.gainXP(xp,false);return "献祭部队，获得 "+std::to_string(xp)+" 点经验。";}
 if(f.action==LocationAction::SellCreature){int slot=f.parameter/ResourceCount,r=f.parameter%ResourceCount;auto&s=g.hero.army[slot];auto[give,receive]=sale(s.unit,r);if(int64_t(g.resources[r])+int64_t(receive)*amount>1000000000)return "资源已达到储存上限。";s.count-=give*amount;if(!s.count)s={};g.resources[r]+=receive*amount;return "交换完成，获得 "+std::to_string(receive*amount)+" "+ResourceNames[r]+"。";}
 if(f.action==LocationAction::BuyMap){g.resources[Gold]-=1000;auto&seen=g.scenarioRules["cartographers"][std::to_string(g.currentOwner())];if(!seen.is_array())seen=nlohmann::json::array();seen.push_back(f.parameter);for(int level=0;level<g.mapLevels;++level)for(int y=0;y<g.mapHeight;++y)for(int x=0;x<g.mapWidth;++x){auto&t=g.tiles[(level*g.mapHeight+y)*g.mapWidth+x];bool water=t.terrain==Terrain::Water,rock=t.terrain==Terrain::Rock;bool show=f.parameter==0?water:f.parameter==1?level==0&&!water&&!rock:level>0&&!water&&!rock;if(show)t.seen=true;}return "购买完成，对应区域的战争迷雾已经揭开。";}
 if(f.action==LocationAction::FreeHero||f.action==LocationAction::RecruitHero){Point pos=spawn(g,o);if(pos.x<0)return "入口附近没有可容纳新英雄的空地。";Hero h;if(f.action==LocationAction::FreeHero)h=prisoner(g,o);else{heroDefaults(h,f.parameter);h.army={Stack{(f.parameter/16)*14,20},Stack{(f.parameter/16)*14+2,6}};g.resources[Gold]-=2500;}h.pos=pos;h.mana=h.knowledge*10;h.keys=g.hero.keys;auto previous=g.hero;g.hero=h;h.maxMovement=h.movement=g.movementAllowance();g.hero=previous;int next=id;for(const auto&n:g.objects)next=std::max(next,n.id);for(const auto&[hid,hero]:g.heroes)next=std::max(next,hid);g.storeHero();g.heroes[++next]={h,g.currentOwner(),o.level};g.heroes[next].properties=o.properties;
  std::string text=h.name+"加入你的队伍，出现在入口附近。";if(f.action==LocationAction::FreeHero){g.locationNotice(id,text);std::erase_if(g.objects,[id](auto&n){return n.id==id;});}else for(auto&value:o.properties["tavernHeroes"])if(value==f.parameter)value=-1;g.syncHeroObjects();return text;}
 return "交易没有执行。";
}
}
