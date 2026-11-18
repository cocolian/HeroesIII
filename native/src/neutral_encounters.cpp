#include "h3/game.hpp"
#include "h3/locations.hpp"
#include <algorithm>
#include <cmath>

namespace h3 {
NeutralOffer Game::neutralReaction(int id,bool allowJoin)const{
 NeutralOffer offer;offer.objectId=id;auto at=std::find_if(objects.begin(),objects.end(),[id](auto&o){return o.id==id;});if(at==objects.end()||at->type!=ObjectType::Monster)return offer;const auto&o=*at;for(auto s:o.army)if(!s.empty()){offer.unit=s.unit;offer.count+=s.count;}
 if(armyStacks(o.army)!=1||!o.properties.is_object()||!o.properties.contains("disposition"))return offer;int disposition=std::clamp(o.properties.value("disposition",4),0,4);uint32_t roll=seed^(uint32_t(id)*2654435761u);roll=roll*1664525u+1013904223u;int mood=disposition==0?-4:disposition==1?1+int(roll%7):disposition==2?1+int(roll%10):disposition==3?4+int(roll%7):10;mood=o.properties.value("neutralMood",mood);
 auto value=[](const Army&a){double n=0;for(auto s:a)if(!s.empty()&&s.unit<CreatureUnitCount)n+=double(locations::facts()["aiValues"][s.unit].get<int>())*s.count;return n;};
 double relative=value(hero.army)*std::sqrt((1+.05*primarySkill(0))*(1+.05*primarySkill(1)))/std::max(1.,value(o.army));int power=relative>=7?11:relative>=1?int(2*(relative-1)):relative>=.5?-1:relative>=1./3?-2:-3;int same=0,total=armyCount(hero.army);for(auto s:hero.army)if(!s.empty()&&baseUnit(s.unit)==baseUnit(offer.unit))same+=s.count;int sympathy=(same>0)+(same*2>total);int diplomacy=hero.skills[Diplomacy];if(currentOwner()==0&&scenarioRules.is_object()&&scenarioRules.value("difficulty",-1)==0)++diplomacy;int charisma=power+diplomacy+1+sympathy;
 if(charisma<mood)return offer;
 if(allowJoin){if(diplomacy+sympathy+1>=mood){offer.intent=NeutralIntent::JoinFree;return offer;}if(diplomacy*2+sympathy+1>=mood){offer.intent=NeutralIntent::JoinPaid;offer.cost=int(std::min<int64_t>(1000000000,int64_t(Units[offer.unit].cost)*offer.count));return offer;}}
 if(charisma>mood&&!o.properties.value("never_flees",0))offer.intent=NeutralIntent::Flee;return offer;
}
std::string Game::encounterMonster(int id,int deferred){auto offer=neutralReaction(id);offer.deferredVisitId=deferred;if(offer.intent==NeutralIntent::Fight){startBattle(id);if(battle)battle->deferredVisitId=deferred;return "野兵迎战！";}neutralOffer=offer;return offer.intent==NeutralIntent::Flee?"野兵畏惧你的实力，准备逃走。是否放他们离开？":offer.intent==NeutralIntent::JoinFree?"野兵愿意免费加入你的军队。是否接受？":"野兵愿意支付招募费用后加入。是否接受？";}
std::string Game::answerNeutral(bool accept,int replaceSlot){if(!neutralOffer)return "当前没有待处理的野兵交涉。";if(replaceSlot<-1||replaceSlot>=ArmySlots)return "请选择有效的替换部队。";auto offer=*neutralOffer;auto*o=objectById(offer.objectId);if(!o||o->type!=ObjectType::Monster)return "野兵已经不在这里。";
 if(!accept){neutralOffer.reset();if(offer.intent!=NeutralIntent::Flee){auto next=neutralReaction(offer.objectId,false);next.deferredVisitId=offer.deferredVisitId;if(next.intent==NeutralIntent::Flee){neutralOffer=next;return "野兵因请求被拒而准备逃走。是否放他们离开？";}}startBattle(offer.objectId);if(battle)battle->deferredVisitId=offer.deferredVisitId;return "野兵发动进攻！";}
 if(offer.intent!=NeutralIntent::Flee){Army army=hero.army;if(replaceSlot>=0)army[replaceSlot]={};if(resources[Gold]<offer.cost)return "金币不足，无法支付招募费用。";if(!locations::merge(army,offer.unit,offer.count))return "部队栏位不足，请拒绝招募或先整理部队。";hero.army=army;resources[Gold]-=offer.cost;}
 if(imported()&&o->properties.contains("absod_id"))scenarioRules["defeated"].push_back(o->properties["absod_id"]);int deferred=offer.deferredVisitId;neutralOffer.reset();std::erase_if(objects,[&](auto&obj){return obj.id==offer.objectId;});std::string text=offer.intent==NeutralIntent::Flee?"野兵已经离开。":Units[offer.unit].name+" × "+std::to_string(offer.count)+" 加入你的军队。";message(text);if(deferred)if(auto*target=objectById(deferred);target&&!guardingMonster(target->pos)&&(target->visitableAt(hero.pos)||(extendedLocation(target->sourceClass)&&canVisitLocation(*target))))applyVisit(deferred);checkScenarioVictory();return text;
}
void Game::autoNeutral(){for(int i=0;neutralOffer&&i<3;++i){auto offer=*neutralOffer;Army trial=hero.army;int replace=-1;bool fits=locations::merge(trial,offer.unit,offer.count);if(!fits&&resources[Gold]>=offer.cost){int weakest=0;for(int s=1;s<ArmySlots;++s){Army a{},b{};a[0]=hero.army[s];b[0]=hero.army[weakest];if(armyPower(a)<armyPower(b))weakest=s;}Army old{},gain{};old[0]=hero.army[weakest];gain[0]={offer.unit,offer.count};if(armyPower(gain)>armyPower(old)*1.3){replace=weakest;fits=true;}}bool accept=offer.intent==NeutralIntent::Flee||(resources[Gold]>=offer.cost&&fits);answerNeutral(accept,replace);}}
}
