#include "h3/game.hpp"
#include <algorithm>
#include <cmath>

namespace h3 {
namespace {
using Json=nlohmann::json;
constexpr const char*ResourceKeys[]={"gold","wood","ore","mercury","sulfur","crystal","gems"};
constexpr int ResourceIds[]={Wood,Mercury,Ore,Sulfur,Crystal,Gems,Gold};
constexpr int SkillIds[]={-1,Archery,Logistics,-1,-1,-1,Leadership,Wisdom,Mysticism,LuckSkill,-1,-1,-1,-1,FireMagic,AirMagic,WaterMagic,EarthMagic,-1,-1,-1,-1,Offense,Armorer,Intelligence,-1,-1,-1};
int unitId(int original){for(int i=0;i<UnitCount;++i)if(Units[i].originalId==original)return i;return -1;}
bool merge(Army&army,int unit,int count){if(unit<0||count<=0)return false;for(auto&s:army)if(!s.empty()&&s.unit==unit){if(count>MaxArmyStack-s.count)return false;s.count+=count;return true;}for(auto&s:army)if(s.empty()){s={unit,count};return count<=MaxArmyStack;}return false;}
int count(const Army&army,int original){int id=unitId(original),n=0;for(auto s:army)if(s.unit==id)n+=s.count;return n;}
void remove(Army&army,int original,int n){int id=unitId(original);for(auto&s:army)if(s.unit==id){int used=std::min(n,s.count);s.count-=used;n-=used;if(!s.count)s={};}}
int bounded(int64_t n){return int(std::clamp<int64_t>(n,0,1000000000));}
bool questMet(const Game&g,const Json&q){
 int type=q.value("type",0);const auto&d=q.value("details",Json::object());if(q.value("deadline",uint64_t(0xffffffff))!=0xffffffff&&uint64_t(g.day)>q["deadline"].get<uint64_t>())return false;
 switch(type){case 0:return true;case 1:return g.hero.level>=d.value("level",1);case 2:{const auto&s=d.at("skills");return g.primarySkill(0)>=s.value("attack",0)&&g.primarySkill(1)>=s.value("defense",0)&&g.primarySkill(2)>=s.value("spell_power",0)&&g.primarySkill(3)>=s.value("knowledge",0);}
 case 3:case 4:{for(const auto&id:g.scenarioRules["defeated"])if(id==d["absod_id"])return true;return false;}
 case 5:{auto items=g.hero.artifactIds;for(int id:d["artifacts"]){auto at=std::find(items.begin(),items.end(),id);if(at==items.end())return false;items.erase(at);}return true;}
 case 6:for(const auto&s:d["creatures"])if(count(g.hero.army,s["type"])<s["quantity"].get<int>())return false;return true;
 case 7:for(int r=0;r<ResourceCount;++r)if(g.resources[r]<d["resources"].value(ResourceKeys[r],0))return false;return true;
 case 8:return g.hero.originalId==d.value("hero",-1);case 9:return g.scenarioRules["originalPlayer"]==d["player"];default:return false;}
}
std::string questDescription(const Json&q){const auto&d=q.value("details",Json::object());std::string text=q.value("proposal",std::string{});if(text.empty())text="完成关卡要求后可以继续。";switch(q.value("type",0)){
 case 1:text+="\n需要英雄等级 "+std::to_string(d.value("level",1));break;
 case 5:text+="\n需要宝物：";for(int id:d["artifacts"])text+=" #"+std::to_string(id);break;
 case 6:for(const auto&s:d["creatures"]){int u=unitId(s["type"]);text+="\n需要 "+(u>=0?Units[u].name:"未知兵种")+" × "+std::to_string(s["quantity"].get<int>());}break;
 case 7:for(int r=0;r<ResourceCount;++r)if(int n=d["resources"].value(ResourceKeys[r],0);n>0)text+="\n需要 "+std::to_string(n)+" "+ResourceNames[r];break;
 case 3:case 4:text+="\n需要先击败指定英雄或守军。";break;default:break;}return text;
}
void consumeQuest(Game&g,const Json&q){const auto&d=q.value("details",Json::object());switch(q.value("type",0)){
 case 5:for(int id:d["artifacts"]){auto at=std::find(g.hero.artifactIds.begin(),g.hero.artifactIds.end(),id);if(at!=g.hero.artifactIds.end())g.hero.artifactIds.erase(at);auto worn=std::find(g.hero.wornArtifacts.begin(),g.hero.wornArtifacts.end(),id);if(worn!=g.hero.wornArtifacts.end()){g.hero.wornArtifacts.erase(worn);for(auto&[slot,item]:g.hero.equipmentSlots)if(item==id){item=-1;break;}}auto name=artifactName(id);auto a=std::find(g.hero.artifacts.begin(),g.hero.artifacts.end(),name);if(a!=g.hero.artifacts.end())g.hero.artifacts.erase(a);}break;
 case 6:for(const auto&s:d["creatures"])remove(g.hero.army,s["type"],s["quantity"]);break;
 case 7:for(int r=0;r<ResourceCount;++r)g.resources[r]-=d["resources"].value(ResourceKeys[r],0);break;default:break;}}
void spell(Game&g,int original){for(int i=0;i<SpellCount;++i)if(Spells[i].originalId==original)g.hero.spells[i]=true;}
bool rewardFits(const Game&g,const Json&event){if(event.contains("resources"))for(int r=0;r<ResourceCount;++r)if(int64_t(g.resources[r])+event["resources"].value(ResourceKeys[r],0)<0)return false;Army next=g.hero.army;for(const auto&s:event.value("creatures",Json::array()))if(s.value("quantity",0)>0&&!merge(next,unitId(s.value("type",-1)),s.value("quantity",0)))return false;return true;}
void eventReward(Game&g,const Json&event){
 if(event.contains("resources"))for(int r=0;r<ResourceCount;++r)g.resources[r]=bounded(int64_t(g.resources[r])+event["resources"].value(ResourceKeys[r],0));
 if(event.contains("primary_skills")){const auto&s=event["primary_skills"];g.hero.attack=std::clamp(g.hero.attack+s.value("attack",0),0,1000);g.hero.defense=std::clamp(g.hero.defense+s.value("defense",0),0,1000);g.hero.power=std::clamp(g.hero.power+s.value("spell_power",0),0,1000);g.hero.knowledge=std::clamp(g.hero.knowledge+s.value("knowledge",0),0,1000);}
 for(const auto&s:event.value("secondary_skills",Json::array())){int id=s.value("type",-1);if(id<0||id>=28||SkillIds[id]<0)continue;int skill=SkillIds[id],level=std::clamp(s.value("level",0),0,3);if(!g.hero.skills[skill]&&level>0&&std::count_if(g.hero.skills.begin(),g.hero.skills.end(),[](int n){return n>0;})>=8){g.message("已掌握八项辅助技能，无法再学习"+SkillNames[skill]+"。");continue;}g.hero.skills[skill]=std::max(g.hero.skills[skill],level);}
 for(int id:event.value("artifacts",std::vector<int>{})){g.hero.artifactIds.push_back(id);g.hero.artifacts.push_back(artifactName(id));if(id==0)g.hero.spellbook=true;g.equipPickedArtifact(id);}
 for(int id:event.value("spells",std::vector<int>{}))spell(g,id);
 for(const auto&s:event.value("creatures",Json::array()))merge(g.hero.army,unitId(s["type"]),s["quantity"]);
 if(int xp=event.value("experience",0);xp>0)g.gainXP(xp);
 if(int delta=event.value("spell_points",0))g.hero.mana=int(std::clamp<int64_t>(int64_t(g.hero.mana)+delta,0,1000000));
}
Json rewardEvent(const Json&r){Json event=Json::object();const auto&d=r.value("details",Json::object());switch(r.value("type",0)){
 case 1:event["experience"]=d.value("experience",0);break;
 case 2:event["spell_points"]=d.value("spell_points",0);break;
 case 5:{const auto&s=d["resource"];int id=s.value("type",-1);if(id>=0&&id<7)event["resources"][ResourceKeys[ResourceIds[id]]]=s["quantity"];}break;
 case 6:{const char*keys[]={"attack","defense","spell_power","knowledge"};int id=d.value("type",-1);if(id>=0&&id<4)event["primary_skills"][keys[id]]=d["value"];}break;
 case 7:event["secondary_skills"]=Json::array({d["secondary_skill"]});break;
 case 8:event["artifacts"]=Json::array({d["artifact"]});break;
 case 9:event["spells"]=Json::array({d["spell"]});break;
 case 10:event["creatures"]=Json::array({d["creatures"]});break;default:break;}return event;}
}
bool Game::questReady(const Object&o)const{if(o.sourceClass==9||o.sourceClass==212)return o.sourceSubtype>=0&&o.sourceSubtype<8&&hero.keys[o.sourceSubtype];return o.properties.contains("quest")&&questMet(*this,o.properties["quest"]);}
std::optional<std::string> Game::importedVisit(int id){
 if(!imported())return {};auto*ptr=objectById(id);if(!ptr)return std::string{};if(ptr->type==ObjectType::Site&&ptr->name=="地图物件")ptr->name=adventureSiteName(ptr->sourceClass);Object o=*ptr;auto previous=resources;if(o.type==ObjectType::MapEvent){const char*names[]={"red","blue","tan","green","orange","purple","teal","pink"};int original=scenarioRules.value("originalPlayer",0);int recipient=actingOwner==0?original:actingOwner<=original?actingOwner-1:actingOwner;if(o.properties.contains("affected_players")&&!o.properties["affected_players"].value(names[recipient],false))return std::string{};if(actingOwner&&!o.properties.value("applies_to_computer",1))return std::string{};}
 auto finish=[&](std::string message,bool erase=false,bool mark=true){if(auto*p=objectById(id);p&&mark)p->visited=true;if(erase)std::erase_if(objects,[id](const auto&o){return o.id==id;});if(mark&&(o.sourceClass==23||o.sourceClass==32||o.sourceClass==51||o.sourceClass==61||o.sourceClass==100||o.sourceClass==41))hero.visitedSites.push_back(id);recordStory(o.name,message);AdventureVisit notice{o.type,o.name,message};for(int r=0;r<ResourceCount;++r)notice.reward[r]=resources[r]-previous[r];adventureVisit_=notice;this->message(message);emit({GameEventType::Visit,o.type==ObjectType::Site?o.sourceClass:-1,int(o.type),o.pos});checkScenarioVictory();return message;};
 if(o.type==ObjectType::EnemyHero&&o.owner==currentOwner())return finish("与 "+o.name+" 会面，可以交换部队或切换英雄。",false,false);
 if(o.type!=ObjectType::Town&&o.type!=ObjectType::Monster&&o.type!=ObjectType::EnemyHero&&armyCount(o.army)>0&&!((o.sourceClass==33||o.sourceClass==219)&&o.owner==actingOwner)){startBattle(id);return "必须先击败守卫。";}
 if(o.sourceClass==33||o.sourceClass==219){if(o.owner==actingOwner)return finish("这是己方驻防要塞。",false,false);if(!armyCount(o.army)){ptr->owner=actingOwner;return finish("驻防要塞已占领。",false,false);}}
 if(o.type==ObjectType::Town){if(o.owner!=currentOwner()&&armyCount(o.army)){startBattle(id);return o.name+"的守军迎战！";}ptr->owner=currentOwner();selectTown(id);visitTownBuildings();checkScenarioVictory();return "进入"+o.name+"，可建设、招募和管理驻军。";}
 if(o.type==ObjectType::Portal){if(!o.properties.contains("destination"))return finish("这个出口没有可用的传送目的地。");const auto&d=o.properties["destination"];Point target{d["x"],d["y"]};int level=d["level"];if(level<0||level>=mapLevels)return finish("传送目的地无效。");for(const auto&[other,h]:heroes)if(other!=activeHeroId&&h.level==level&&h.value.pos==target)return finish("通道出口已被英雄占用，请先移开出口处的英雄。",false,false);hero.pos=target;mapLevel=level;reveal();return finish("穿过"+o.name+"，抵达"+(mapLevel?std::string("地下"):std::string("地上"))+"层。");}
 if(o.type==ObjectType::Gate||o.sourceClass==83){
  if(o.sourceClass==9||o.sourceClass==212){if(o.sourceSubtype<0||o.sourceSubtype>=8||!hero.keys[o.sourceSubtype])return finish("必须先访问相同颜色的钥匙大师帐篷。",false,false);return finish("同色钥匙已验证，关卡开放。",true);}
  if(o.visited&&o.sourceClass==83)return finish("这项任务已经完成。");const auto&q=o.properties.value("quest",Json::object());
  if(!questMet(*this,q))return finish(questDescription(q),false,false);
  auto reward=rewardEvent(o.properties.value("reward",Json::object()));if(!rewardFits(*this,reward))return finish("部队栏位不足，请先整理军队后再领取奖励。",false,false);
  auto trial=hero.army;if(q.value("type",0)==6)for(const auto&s:q["details"]["creatures"])remove(trial,s["type"],s["quantity"]);if(!armyCount(trial)&&reward.value("creatures",Json::array()).empty())return finish("交付后英雄将没有部队，请先补充军队。",false,false);
  consumeQuest(*this,q);eventReward(*this,reward);return finish(q.value("completion",std::string{}).empty()?"任务完成，可以继续前进。":q["completion"].get<std::string>(),o.type==ObjectType::Gate);
 }
 if(o.type==ObjectType::Artifact){if(o.sourceSubtype==0)hero.spellbook=true;hero.artifactIds.push_back(o.sourceSubtype);hero.artifacts.push_back(o.name);equipPickedArtifact(o.sourceSubtype);if(o.sourceClass==93)spell(*this,o.properties.value("spell",-1));return finish("获得"+o.name+"。",true);}
 if(o.type==ObjectType::MapEvent||o.sourceClass==6){if(o.visited&&o.properties.value("cancel_after_first_visit",0))return std::string{};if(o.properties.contains("affected_players")){const char*keys[]={"red","blue","tan","green","orange","purple","teal","pink"};int original=scenarioRules.value("originalPlayer",0);int player=actingOwner==0?original:actingOwner<=original?actingOwner-1:actingOwner;if(!o.properties["affected_players"].value(keys[player],false))return std::string{};}if(actingOwner!=0&&!o.properties.value("applies_to_computer",1))return std::string{};if(!rewardFits(*this,o.properties))return finish("资源不足或部队栏位不足，尚未触发这项交换/奖励。",false,false);eventReward(*this,o.properties);return finish(o.properties.value("message",o.properties.value("guardians",Json::object()).value("message",std::string{})).empty()?"触发地图事件并领取奖励。":o.properties.value("message",o.properties.value("guardians",Json::object()).value("message",std::string{})),o.sourceClass==6||o.properties.value("cancel_after_first_visit",0)!=0,o.sourceClass==6||o.properties.value("cancel_after_first_visit",0)!=0);}
 if(o.type==ObjectType::Site){
  bool bonusWasActive=hasAdventureBonus(hero,o.sourceClass);
  if(auto response=visitAdventureSite(*ptr)){auto result=finish(*response,false,false);if(!bonusWasActive&&adventureVisit_)for(const auto&bonus:hero.adventureBonuses)if(bonus.sourceClass==o.sourceClass){adventureVisit_->morale=bonus.morale;adventureVisit_->luck=bonus.luck;break;}return result;}
  if(o.sourceClass==87)return finish(buyBoat(id),false,false);
  if(o.sourceClass==10){if(o.sourceSubtype>=0&&o.sourceSubtype<8)hero.keys[o.sourceSubtype]=true;for(auto&[heroId,h]:heroes)if(h.owner==actingOwner)h.value.keys=hero.keys;return finish("钥匙大师已登记你的来访，同色边境关卡可以通行。");}
  if(o.sourceClass==91||o.sourceClass==59)return finish(o.properties.value("message",std::string("这里没有留下文字。")));
  if(o.sourceClass==23||o.sourceClass==32||o.sourceClass==51||o.sourceClass==61||o.sourceClass==100||o.sourceClass==41)o.visited=std::find(hero.visitedSites.begin(),hero.visitedSites.end(),id)!=hero.visitedSites.end();if(o.visited)return finish("英雄已经访问过这里。");
  switch(o.sourceClass){case 12:{resources[Gold]=bounded(int64_t(resources[Gold])+400+100*int(random()*3));int r=Wood+int(random()*6);resources[r]=bounded(int64_t(resources[r])+4+int(random()*3));return finish("营火中留有金币与资源。",true);}case 23:++hero.defense;return finish("防御力 +1。");case 32:++hero.knowledge;return finish("知识 +1。");case 51:++hero.attack;return finish("攻击力 +1。");case 61:++hero.power;return finish("魔法力量 +1。");case 100:gainXP(1000);return finish("获得 1000 点经验。");case 41:if(hero.level<10)return finish("英雄需要达到 10 级。",false,false);hero.attack+=2;hero.defense+=2;hero.power+=2;hero.knowledge+=2;return finish("四项主要属性各增加 2。");default:return finish("此物件的完整规则尚未实现；原图位置与阻挡保持不变。",false,false);}
 }
 return {};
}
void Game::triggerMapEvents(){std::vector<int>ids;for(const auto&o:objects)if(o.level==mapLevel&&o.pos==hero.pos&&o.type==ObjectType::MapEvent&&!o.visited)ids.push_back(o.id);for(int id:ids){if(battle)break;importedVisit(id);}}
void Game::checkScenarioVictory(){
 if(!imported()||result||actingOwner!=0)return;if(!activeHeroId&&!hasTown()){result=-1;message("所有城镇和英雄均已失去。任务失败。");return;}const auto&loss=scenarioRules["additional"]["loss_condition"];int lossType=loss.value("type",255),lossTarget=scenarioRules.value("lossTarget",0);bool lost=lossType==2&&day>loss.value("details",Json::object()).value("days",100000);if(lossTarget&&lossType==0){auto*o=objectById(lossTarget);lost=!o||o->owner!=0;}if(lossTarget&&lossType==1&&scenarioRules.value("lossTargetWasOwned",false))lost=!heroes.contains(lossTarget);if(lost){result=-1;message("触发地图设定的失败条件。");return;}const auto&v=scenarioRules["additional"]["victory_condition"];int type=v.value("type",255);const auto&d=v.value("details",Json::object());bool win=false;
 auto allDefeated=[&]{return std::none_of(objects.begin(),objects.end(),[](const auto&o){return o.owner>0&&(o.type==ObjectType::Town||o.type==ObjectType::EnemyHero);});};
 if(type==255||d.value("allow_normal_win",0))win=scenarioRules.value("hadEnemy",false)&&allDefeated();
 if(type==6&&d.contains("coordinates")){auto p=d["coordinates"];for(const auto&o:objects)if(o.type==ObjectType::Town&&o.owner==0&&o.anchor.x==p["x"]&&o.anchor.y==p["y"]&&o.level==p["z"])win=true;}
 if(type==10&&d.contains("destination")){const auto&p=d["destination"];int artifact=d.value("artifact_type",-1);bool carries=std::find(hero.artifactIds.begin(),hero.artifactIds.end(),artifact)!=hero.artifactIds.end();for(const auto&o:objects)if(carries&&o.type==ObjectType::Town&&o.owner==0&&hero.pos==o.pos&&mapLevel==o.level&&o.anchor.x==p["x"]&&o.anchor.y==p["y"]&&o.level==p["z"])win=true;}
 if(type==11)win=std::none_of(objects.begin(),objects.end(),[](const auto&o){return o.type==ObjectType::Monster;});
 if(type==8)win=std::all_of(objects.begin(),objects.end(),[](const auto&o){return !((o.sourceClass>=17&&o.sourceClass<=20)||(o.sourceClass>=216&&o.sourceClass<=218))||o.owner==0;});
 if(type==9)win=std::all_of(objects.begin(),objects.end(),[](const auto&o){return o.type!=ObjectType::Mine||o.owner==0;});
 if(type==5||type==7)if(d.contains("coordinates")){auto initial=scenarioRules.value("victoryTarget",0);win=initial>0&&std::none_of(objects.begin(),objects.end(),[&](const auto&o){return o.id==initial;});}
 if(win){result=1;message("完成地图「"+scenarioName+"」的胜利条件！");}
}
void Game::importedNewDay(){
 if(!imported())return;
 if((day-1)%7==0)for(auto&[id,t]:settlements){if(id==activeTownId)continue;auto*o=objectById(id);if(!o||o->owner<0)continue;for(int tier=0;tier<7;++tier)if(t.built[Guardhouse+tier]){int growth=Units[t.faction*14+tier*2].growth;if(t.built[Castle])growth*=2;else if(t.built[Citadel])growth=growth*3/2;t.pool[tier]=std::min(MaxArmyStack,t.pool[tier]+growth);}}
 runScenarioEvents();
 checkScenarioVictory();
}
}
