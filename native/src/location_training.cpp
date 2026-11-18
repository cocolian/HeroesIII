#include "h3/locations.hpp"
#include <algorithm>
#include <numeric>

namespace h3::locations {
namespace {
bool visited(const Hero&h,int id){return std::find(h.visitedSites.begin(),h.visitedSites.end(),id)!=h.visitedSites.end();}
void mark(Hero&h,int id){if(!visited(h,id))h.visitedSites.push_back(id);}
}
void initializeTraining(Game&g,Object&o){auto&p=o.properties;int cls=o.sourceClass;
 if(cls==102&&!p.contains("treePayment"))p["treePayment"]=int(g.random()*3);
 if(cls==113&&!p.contains("locationSkill")){
  const char*keys[]={"pathfinding","archery","logistics","scouting","diplomacy","navigation","leadership","wisdom","mysticism","luck","ballistics","eagle_eye","necromancy","estates","fire_magic","air_magic","water_magic","earth_magic","scholar","tactics","artillery","learning","offense","armorer","intelligence","sorcery","resistance","first_aid"};
  std::vector<int>pool;for(int original=0;original<28;++original){bool allowed=p.contains("potential_skills")?p["potential_skills"].value(keys[original],false):original!=6&&original!=12;if(allowed)pool.push_back(OriginalSkills[original]);}
  p["locationSkill"]=pool.empty()?-1:pool[int(g.random()*pool.size())];
 }
 if(cls==104&&!p.contains("universitySkills")){std::vector<int>pool;for(int skill=0;skill<SkillCount;++skill)if(skill!=Necromancy)pool.push_back(skill);std::vector<int>skills;for(int i=0;i<4&&!pool.empty();++i){int at=int(g.random()*pool.size());skills.push_back(pool[at]);pool.erase(pool.begin()+at);}p["universitySkills"]=skills;}
 if(cls==81&&!p.contains("scholarReward")){
  int type=p.value("reward_type",255),value=p.value("reward_value",255);
  if(type==255){type=int(g.random()*3);value=type==0?int(g.random()*4):type==1?int(g.random()*28):Spells[int(g.random()*int(SpellCount))].originalId;}
  p["scholarReward"]={{"type",type},{"value",value}};
 }
}
std::string training(Game&g,int id){auto&o=*g.objectById(id);int cls=o.sourceClass;
 if(cls==4||cls==47||cls==107||cls==102){if(visited(g.hero,id))return "这位英雄已经完成过这里的训练，不能重复领取。";return cls==102?"知识之树可以让每位英雄提升一级。请确认是否支付所需费用。":cls==4?"请选择攻击或防御训练，主要属性增加 2 点。每位英雄只能选择一次。":"支付 1000 金币，选择一项主要属性增加 1 点。每位英雄只能训练一次。";}
 if(cls==104)return "大学教授四项辅助技能。每项初级课程学费 2000 金币；每位英雄最多掌握八项辅助技能。";
 if(cls==113){int skill=o.properties.value("locationSkill",-1);if(skill<0)return "地图作者没有为这座小屋允许任何技能。";if(g.hero.skills[skill])return "小屋教授"+SkillNames[skill]+"，你已经掌握这项技能。";return "女巫愿意传授初级"+SkillNames[skill]+"。"+(learnable(g.hero,skill)?"":"你的八个辅助技能位置已经用完。");}
 if(cls==81){
  auto r=o.properties["scholarReward"];int type=r["type"],value=r["value"];std::string result;
  if(type==1&&value>=0&&value<28){int skill=OriginalSkills[value];if(g.hero.skills[skill]<3&&(g.hero.skills[skill]||learnable(g.hero,skill))){++g.hero.skills[skill];result="学者传授了"+SkillLevels[g.hero.skills[skill]]+SkillNames[skill]+"。";}}
  else if(type==2){int spell=originalSpell(value);if(spellLearnable(g,spell)&&!g.hero.spells[spell]){g.hero.spells[spell]=true;result="学者传授了"+Spells[spell].name+"。";}}
  if(result.empty()){int stat=type==0?std::clamp(value,0,3):int(g.random()*4);int*stats[]={&g.hero.attack,&g.hero.defense,&g.hero.power,&g.hero.knowledge};*stats[stat]=std::min(1000,*stats[stat]+1);const char*names[]={"攻击","防御","力量","知识"};result="学者使你的"+std::string(names[stat])+"增加 1 点。";}
  g.locationNotice(id,result);std::erase_if(g.objects,[id](auto&o){return o.id==id;});return result;
 }
 return "";
}
std::vector<LocationOffer> trainingOffers(const Game&g,const Object&o){std::vector<LocationOffer>offers;int cls=o.sourceClass;if((cls==4||cls==47||cls==107||cls==102)&&visited(g.hero,o.id))return offers;
 if(cls==4||cls==47||cls==107){int start=cls==47?2:0;for(int stat=start;stat<start+2;++stat){LocationOffer f;f.action=LocationAction::Primary;f.parameter=stat;const char*names[]={"攻击","防御","力量","知识"};f.label=std::string(names[stat])+" +"+(cls==4?"2":"1");if(cls!=4)f.cost[Gold]=1000;f.enabled=g.canAfford(f.cost);offers.push_back(f);}}
 else if(cls==102){LocationOffer f;f.action=LocationAction::Level;f.label="接受知识之树的指引";int payment=o.properties.value("treePayment",0);if(payment==1)f.cost[Gold]=2000;if(payment==2)f.cost[Gems]=10;f.enabled=g.hero.level<70&&g.canAfford(f.cost);offers.push_back(f);}
 else if(cls==104||cls==113){std::vector<int>skills=cls==104?o.properties.value("universitySkills",std::vector<int>{}):std::vector<int>{o.properties.value("locationSkill",-1)};for(int skill:skills)if(skill>=0&&skill<SkillCount){LocationOffer f;f.action=LocationAction::Learn;f.parameter=skill;f.label="学习"+SkillNames[skill];if(cls==104)f.cost[Gold]=2000;f.enabled=learnable(g.hero,skill)&&g.canAfford(f.cost);f.description=g.hero.skills[skill]?"已经掌握":"初级辅助技能";offers.push_back(f);}}
 return offers;
}
std::string performTraining(Game&g,Object&o,const LocationOffer&f,int){for(int r=0;r<ResourceCount;++r)g.resources[r]-=f.cost[r];
 if(f.action==LocationAction::Primary){int*stats[]={&g.hero.attack,&g.hero.defense,&g.hero.power,&g.hero.knowledge};int amount=o.sourceClass==4?2:1;*stats[f.parameter]=std::min(1000,*stats[f.parameter]+amount);mark(g.hero,o.id);return "训练完成："+f.label+"。";}
 if(f.action==LocationAction::Level){mark(g.hero,o.id);g.gainXP(std::max(0,Game::experienceForLevel(g.hero.level+1)-g.hero.xp),false);return "知识之树使英雄提升一级。";}
 g.hero.skills[f.parameter]=1;g.hero.maxMovement=g.movementAllowance();return "掌握了初级"+SkillNames[f.parameter]+"。";
}
}
