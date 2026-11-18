#include "h3/game.hpp"
#include <algorithm>
#include <cmath>

namespace h3 {
namespace {
struct Specialty {int kind,unit,spell,resource,amount;std::array<int,4> bonus;std::vector<int> targets;int upgrade;};
const std::array<Specialty,156> Specialties={{
#include "../generated/hero_specialties.inc"
}};
const Specialty* specialty(const Hero& h){return h.originalId>=0&&h.originalId<156?&Specialties[h.originalId]:nullptr;}
bool applies(const Specialty&s,int unit){return std::find(s.targets.begin(),s.targets.end(),unit)!=s.targets.end();}
}
const Hero* Game::combatHero(int team)const {
 if(team==0)return &hero;
 if(battle){auto at=heroes.find(battle->targetId);if(at!=heroes.end())return &at->second.value;}
 return nullptr;
}
int Game::nativeTerrainBonus(const CombatStack&u)const{if(!battle||Units[u.unit].faction<0||u.origin==StackOrigin::WarMachine)return 0;const Terrain native[]={Terrain::Grass,Terrain::Grass,Terrain::Snow,Terrain::Lava,Terrain::Dirt,Terrain::Subterranean,Terrain::Rough,Terrain::Swamp,Terrain::Grass};return battle->terrain==native[Units[u.unit].faction]?1:0;}
int Game::attackSkill(const CombatStack&u,bool ranged)const{return std::max(0,siegeBonus(u.team,0)+Units[u.unit].attack+nativeTerrainBonus(u)+(combatHero(u.team)?heroUnitBonus(*combatHero(u.team),u.unit,0):0)+(u.disease>0?-2:0)+(u.team==0?primarySkill(0):battle?battle->defenderPrimary[0]:0)+spellEffect(u,Prayer)-spellEffect(u,Weakness)+(ranged?spellEffect(u,Precision):spellEffect(u,Bloodlust)));}
double secondaryEffect(const Hero& h,int skill,double base){
 return secondarySpecialty(h.originalId)==skill&&h.skills[skill]?base*(1.+.05*h.level):base;
}
int heroUnitBonus(const Hero& h,int unit,int stat){
 if(unit<0||unit>=UnitCount||stat<0||stat>3)return 0;auto*s=specialty(h);if(!s)return 0;
 if(s->kind==7)return s->bonus[stat];
 if(s->kind==5&&applies(*s,unit))return s->bonus[stat];
 if(s->kind!=1||(unit!=s->unit&&!(s->unit<126&&s->unit%2==0&&unit==s->unit+1)))return 0;
 if(stat==3)return 1;if(stat==2)return 0;
 int tier=s->amount>0?s->amount:std::max(1,Units[s->unit].level);
 return int(std::ceil((stat==0?Units[unit].attack:Units[unit].defense)*double(h.level/tier)/20.));
}
int heroSpellEffect(const Hero& h,int spell,int unit,int raw){
 auto*s=specialty(h);if(!s||s->spell!=spell||unit<0||unit>=UnitCount)return raw;
 int level=std::max(1,Units[unit].level);
 if(s->kind==2){int percent=spell==MeteorShower||spell==ChainLightning||spell==Resurrection||spell==AnimateDead?5:3;return int(int64_t(raw)*(100*level+percent*h.level)/(100*level));}
 if(s->kind==3){if(spell==Mirth||spell==Fortune)return 3;int add=spell==Slayer?std::max(0,5-level):level<=2?3:level<=4?2:level<=6?1:0;return raw+add;}
 if(s->kind==10||s->kind==11)return int(int64_t(raw)*(100+s->amount)/100);
 return raw;
}
Resources heroSpecialtyIncome(const Hero& h){Resources result{};auto*s=specialty(h);if(s&&s->kind==6&&s->resource>=0)result[s->resource]+=s->amount;result[Gold]+=int(secondaryEffect(h,Estates,std::array<int,4>{0,125,250,500}[h.skills[Estates]]));return result;}
int heroSpecialUpgrade(const Hero&h,int unit){auto*s=specialty(h);return s&&s->kind==8&&applies(*s,unit)?s->upgrade:-1;}
std::string heroSpecialtyDescription(const Hero& h){
 auto*s=specialty(h);if(!s)return "自定义英雄";
 if(s->kind==1)return Units[s->unit].name+"及升级兵种速度 +1，攻击与防御随英雄等级提高。";
 if(s->kind==2||s->kind==3||s->kind==4||s->kind==10||s->kind==11)return "强化"+Spells[s->spell].name+"；依原版分别计算等级、目标兵种和固定加成。";
 if(s->kind==5)return "对专长兵种提供固定的攻击、防御、伤害或速度加成。";
 if(s->kind==6)return "每日额外产出 "+std::to_string(s->amount)+" "+ResourceNames[s->resource]+"。";
 if(s->kind==7)return "所有部队速度 +2。";
 if(s->kind==8)return "可将对应部队升级为"+Units[s->upgrade].name+"，支付正常升级差价。";
 if(s->kind==9)return "初始掌握专家级火系魔法。";
 if(s->kind==12)return "航海移动加成随英雄等级提高。";
 int skill=secondarySpecialty(h.originalId);return skill>=0?SkillNames[skill]+"效果每级额外提高 5%。":"原版特殊英雄";
}
}
