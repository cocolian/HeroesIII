#include "h3/locations.hpp"
#include <algorithm>
#include <cmath>

namespace h3::locations {
const nlohmann::json& facts(){static const auto value=nlohmann::json::parse(
#include "../generated/adventure_locations.inc"
);return value;}
int originalUnit(int id){for(int i=0;i<UnitCount;++i)if(Units[i].originalId==id)return i;return -1;}
int originalSpell(int id){for(int i=0;i<SpellCount;++i)if(Spells[i].originalId==id)return i;return -1;}
int artifactPrice(int id){const auto&p=facts()["artifactPrices"];return id>=0&&id<int(p.size())?p[id].get<int>():0;}
int artifactExperience(int id){const auto&r=facts()["artifactRarities"];if(id<7||id>=int(r.size()))return 0;std::string rarity=r[id];return rarity=="T"?1000:rarity=="N"?1500:rarity=="J"?3000:rarity=="R"?6000:0;}
int creatureExperience(int unit){return unit>=0&&unit<int(facts()["aiValues"].size())?(facts()["aiValues"][unit].get<int>()/40)*5:0;}
bool merge(Army&a,int unit,int count){if(unit<0||unit>=UnitCount||count<1||count>MaxArmyStack)return false;for(auto&s:a)if(!s.empty()&&s.unit==unit&&s.count<=MaxArmyStack-count){s.count+=count;return true;}for(auto&s:a)if(s.empty()){s={unit,count};return true;}return false;}
void consolidate(Army&army){for(int a=0;a<ArmySlots;++a)for(int b=a+1;b<ArmySlots;++b)if(!army[a].empty()&&army[a].unit==army[b].unit&&army[a].count<=MaxArmyStack-army[b].count){army[a].count+=army[b].count;army[b]={};}}
double estimatedGuards(const Object&o){double power=Game::armyPower(o.army);if(!Game::bankLocation(o.sourceClass)||(o.properties.is_object()&&o.properties.value("locationBankReady",false)))return power;
 if(o.sourceClass==63){Army guards{Stack{originalUnit(116),40},Stack{originalUnit(117),20}};return Game::armyPower(guards);}
 auto key=std::to_string(o.sourceClass)+":"+std::to_string(std::max(0,o.sourceSubtype));if(!facts()["banks"].contains(key))return power;
 for(const auto&row:facts()["banks"][key]){Army guards{};int slot=0;for(const auto&s:row["guards"])if(slot<ArmySlots)guards[slot++]={s["unit"],s["count"]};power=std::max(power,Game::armyPower(guards));}return power;
}
bool learnable(const Hero&h,int skill){return skill>=0&&skill<SkillCount&&!h.skills[skill]&&std::count_if(h.skills.begin(),h.skills.end(),[](int n){return n>0;})<8;}
bool spellLearnable(const Game&g,int spell){return spell>=0&&spell<SpellCount&&g.hero.spellbook&&!g.bannedSpells[spell]&&Spells[spell].level<=std::max(2,g.hero.skills[Wisdom]+2);}
std::string resourcesText(const Resources&r){std::string text;for(int i=0;i<ResourceCount;++i)if(r[i])text+=(text.empty()?"":"、")+std::to_string(r[i])+" "+ResourceNames[i];return text;}
void giveArtifact(Game&g,int id){g.hero.artifactIds.push_back(id);g.hero.artifacts.push_back(artifactName(id));g.equipPickedArtifact(id);}
void removeArtifact(Hero&h,int id){auto item=std::find(h.artifactIds.begin(),h.artifactIds.end(),id);if(item==h.artifactIds.end())return;h.artifactIds.erase(item);auto worn=std::find(h.wornArtifacts.begin(),h.wornArtifacts.end(),id);if(worn!=h.wornArtifacts.end()){h.wornArtifacts.erase(worn);for(auto&[slot,value]:h.equipmentSlots)if(value==id){value=-1;break;}}auto n=std::find(h.artifacts.begin(),h.artifacts.end(),artifactName(id));if(n!=h.artifacts.end())h.artifacts.erase(n);}
void bonus(Game&g,const Object&o,int morale,int luck){if(!g.hasAdventureBonus(g.hero,o.sourceClass))g.hero.adventureBonuses.push_back({o.sourceClass,o.id,morale,luck,0,0,true});}
std::string name(int cls,int subtype){switch(cls){
 case 2:return "献祭祭坛";case 4:return "竞技场";case 7:return "黑市";case 13:return "制图师";case 15:return "黑暗天幕";
 case 16:{const char*names[]={"独眼巨人宝屋","矮人宝屋","狮鹫宝屋","小怪物宝屋","美杜莎宝屋","娜迦宝屋","龙蝇宝屋"};return subtype>=0&&subtype<7?names[subtype]:"生物宝屋";}
 case 17:case 20:case 216:case 217:case 218:return "野外生物巢穴";case 22:return "尸体";case 24:return "废弃船只";case 25:return "龙之国";case 27:return "魔法师之眼";case 29:return "漂浮物";
 case 35:return "山丘堡垒";case 37:return "魔法师小屋";case 39:return "窝棚";case 47:return "魔法学校";case 50:return "时空市场";case 58:return "红木瞭望塔";case 60:return "火柱";case 62:return "监狱";case 63:return "金字塔";
 case 78:return "难民营";case 80:return "避难所";case 81:return "学者";case 84:return "墓穴";case 85:return "沉船";case 86:return "海难幸存者";case 95:return "野外酒馆";case 102:return "知识之树";case 104:return "大学";case 105:return "废弃货车";case 106:return "战争机器工厂";case 107:return "战争学校";case 108:return "勇士之墓";case 113:return "女巫小屋";case 213:return "自由职业者公会";case 221:return "贸易站";default:return {};}}
}
namespace h3 {
bool Game::extendedLocation(int cls){return !locations::name(cls).empty();}
bool Game::dwellingLocation(int cls){return cls==17||cls==20||cls==78||(cls>=216&&cls<=218);}
bool Game::bankLocation(int cls){return cls==16||cls==24||cls==25||cls==63||cls==84||cls==85;}
void Game::initializeLocation(int id){auto*o=objectById(id);if(!o||!extendedLocation(o->sourceClass))return;if(!o->properties.is_object())o->properties=nlohmann::json::object();
 o->name=locations::name(o->sourceClass,o->sourceSubtype);
 if(dwellingLocation(o->sourceClass))locations::initializeDwelling(*this,*o);
 else if(bankLocation(o->sourceClass))locations::initializeBank(*this,*o);
 else{locations::initializeTraining(*this,*o);locations::initializeService(*this,*o);}
}
void Game::locationNotice(int id,const std::string&message){auto*o=objectById(id);if(!o)return;adventureVisit_=AdventureVisit{o->type,o->name,message};adventureVisit_->sourceClass=o->sourceClass;adventureVisit_->objectId=id;recordStory(o->name,message);this->message(message);emit({GameEventType::Visit,o->sourceClass,int(o->type),o->pos});}
std::optional<std::string> Game::visitLocation(int id){auto*o=objectById(id);if(!o||!extendedLocation(o->sourceClass))return {};initializeLocation(id);int cls=o->sourceClass;std::string text;
 if(dwellingLocation(cls))text=locations::dwelling(*this,id);
 else if(bankLocation(cls))text=locations::bank(*this,id);
 else if(cls==22||cls==29||cls==39||cls==86||cls==105||cls==108)text=locations::pickup(*this,id);
 else if(cls==13||cls==15||cls==27||cls==37||cls==58||cls==60)text=locations::scout(*this,id);
 else if(cls==4||cls==47||cls==81||cls==102||cls==104||cls==107||cls==113)text=locations::training(*this,id);
 else{text=locations::service(*this,id);if(auto*site=objectById(id))site->visited=true;}
 if(objectById(id))locationNotice(id,text);if(actingOwner>0&&objectById(id))autoVisitLocation(id);checkScenarioVictory();return text;
}
std::vector<LocationOffer> Game::locationOffers(int id)const{auto at=std::find_if(objects.begin(),objects.end(),[id](auto&o){return o.id==id;});if(at==objects.end()||!extendedLocation(at->sourceClass))return {};const auto&o=*at;
 if(!o.properties.is_object())return {};
 if(armyCount(o.army)>0&&o.owner!=currentOwner()){LocationOffer offer;offer.action=LocationAction::Fight;offer.label="迎战守卫";offer.description="击败守卫后才能访问和领取奖励。";return {offer};}
 if(dwellingLocation(o.sourceClass))return locations::dwellingOffers(*this,o);
 if(bankLocation(o.sourceClass)){if(o.properties.value("locationCleared",false))return {};LocationOffer offer;offer.action=LocationAction::Claim;offer.label="领取战利品";Army trial=hero.army;if(o.properties.contains("locationLoot"))for(const auto&s:o.properties["locationLoot"]["creatures"])if(!locations::merge(trial,s["unit"],s["count"]))offer.enabled=false;return {offer};}
 if(o.sourceClass==4||o.sourceClass==47||o.sourceClass==81||o.sourceClass==102||o.sourceClass==104||o.sourceClass==107||o.sourceClass==113)return locations::trainingOffers(*this,o);
 return locations::serviceOffers(*this,o);
}
std::string Game::locationAction(int id,LocationAction action,int parameter,int amount){
 if(battle||report||result||!hero.levelChoices.empty())return "请先完成战斗、战报或升级。";
 auto*o=objectById(id);if(!o||!extendedLocation(o->sourceClass)||o->level!=mapLevel||!canVisitLocation(*o))return "英雄必须先抵达访问地点。";
 initializeLocation(id);auto offers=locationOffers(id);auto at=std::find_if(offers.begin(),offers.end(),[&](auto&offer){return offer.action==action&&offer.parameter==parameter;});
 if(at==offers.end()||!at->enabled||amount<1||amount>at->maximum)return "当前选择已失效，请重新选择。";
 auto offer=*at;Resources cost{};for(int r=0;r<ResourceCount;++r){int64_t n=int64_t(offer.cost[r])*amount;if(n>1000000000||n<0)return "数量超出交易范围。";cost[r]=int(n);}if(!canAfford(cost))return "资源不足，无法完成这项交易。";
 if(action==LocationAction::Fight){adventureVisit_.reset();if(const auto*guard=guardingMonster(o->pos)){return encounterMonster(guard->id,id);}else startBattle(id);return "守卫迎战！";}
 std::string response;
 if(action==LocationAction::Claim){if(o->sourceClass==108){o->properties["locationSearchAccepted"]=true;response=locations::pickup(*this,id);}else response=locations::bank(*this,id);}
 else if(action==LocationAction::Hire)response=locations::hire(*this,*o,offer,amount);
 else if(action==LocationAction::Primary||action==LocationAction::Learn||action==LocationAction::Level)response=locations::performTraining(*this,*o,offer,amount);
 else response=locations::performService(*this,*o,offer,amount);
 if(auto*current=objectById(id)){current->visited=true;locationNotice(id,response);}checkScenarioVictory();return response;
}
void Game::locationBattleWon(int id){if(auto*o=objectById(id);o&&extendedLocation(o->sourceClass)){o->properties["locationGuardsDefeated"]=true;}}
bool Game::sanctuaryProtected(int heroId)const{auto at=heroes.find(heroId);Point pos=heroId==activeHeroId?hero.pos:at==heroes.end()?Point{-999,-999}:at->second.value.pos;int level=heroId==activeHeroId?mapLevel:at==heroes.end()?-1:at->second.level;return std::any_of(objects.begin(),objects.end(),[&](auto&o){return o.sourceClass==80&&o.level==level&&o.pos==pos;});}
void Game::locationNewDay(){for(size_t i=0;i<objects.size();++i)if(dwellingLocation(objects[i].sourceClass))initializeLocation(objects[i].id);}
bool Game::canVisitLocation(const Object&o)const{return o.level==mapLevel&&(o.visitableAt(hero.pos)||((bankLocation(o.sourceClass)||dwellingLocation(o.sourceClass))&&std::max(std::abs(o.pos.x-hero.pos.x),std::abs(o.pos.y-hero.pos.y))==1&&canStep(hero.pos,o.pos)));}
bool Game::locationBlocksPassage(const Object&o)const{if(o.type!=ObjectType::Monster&&o.type!=ObjectType::EnemyHero&&o.type!=ObjectType::MapEvent&&o.owner!=currentOwner()&&armyCount(o.army)>0)return true;return o.type==ObjectType::Site&&((bankLocation(o.sourceClass)&&(!o.properties.is_object()||!o.properties.value("locationCleared",false)))||(dwellingLocation(o.sourceClass)&&o.owner!=currentOwner()&&armyCount(o.army)>0));}
int Game::externalDwellingGrowth(int faction,int tier,int owner)const{int count=0;for(const auto&o:objects)if(o.owner==owner&&dwellingLocation(o.sourceClass)&&o.properties.is_object())for(int unit:o.properties.value("locationUnits",std::vector<int>{}))if(unit>=0&&unit<CreatureUnitCount&&Units[unit].faction==faction&&Units[unit].tier==tier)++count;return count;}
void Game::autoVisitLocation(int id){if(battle||report||result||!hero.levelChoices.empty())return;auto*o=objectById(id);if(!o)return;if(canVisitLocation(*o)&&std::find(hero.aiVisitedSites.begin(),hero.aiVisitedSites.end(),id)==hero.aiVisitedSites.end())hero.aiVisitedSites.push_back(id);
 auto offers=locationOffers(id);if(offers.empty())return;
 if(offers.front().action==LocationAction::Fight){if(armyPower(hero.army)>armyPower(o->army)*1.4)locationAction(id,LocationAction::Fight);return;}
 // Computer kingdoms make useful visits without sacrificing their main army or artifacts.
 for(const auto&offer:offers)if(offer.enabled){
  if(offer.action==LocationAction::Hire){locationAction(id,offer.action,offer.parameter,offer.maximum);continue;}
  if(offer.action==LocationAction::Upgrade){locationAction(id,offer.action,offer.parameter,offer.maximum);continue;}
  if(offer.action==LocationAction::Primary||offer.action==LocationAction::Level||offer.action==LocationAction::Learn||offer.action==LocationAction::Claim||offer.action==LocationAction::BuyMap||offer.action==LocationAction::FreeHero){locationAction(id,offer.action,offer.parameter);break;}
 }
}
}
