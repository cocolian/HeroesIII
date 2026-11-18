#include "h3/game.hpp"
#include "h3/locations.hpp"
#include <algorithm>
#include <cmath>

namespace h3 {
namespace {
int learnLevel(const Hero&h){return std::min(5,h.skills[Wisdom]+2);}
bool close(Point a,Point b){return std::max(std::abs(a.x-b.x),std::abs(a.y-b.y))<=1;}
}
std::string Game::meetHero(int other){
 auto at=heroes.find(other);if(battle||report||result||at==heroes.end()||other==activeHeroId||at->second.owner!=currentOwner()||at->second.level!=mapLevel||!close(hero.pos,at->second.value.pos))return "两名己方英雄必须在相邻位置会面。";
 auto&h=at->second.value;int limit=std::max(hero.skills[Scholar],h.skills[Scholar])+1;if(limit==1||!hero.spellbook||!h.spellbook)return "英雄已经会面，可以交换部队和宝物。";
 std::vector<std::string>learned,taught;auto a=hero.spells,b=h.spells;
 for(int spell=0;spell<SpellCount;++spell){const auto&s=Spells[spell];if(s.special||bannedSpells[spell]||s.level>limit)continue;
  if(a[spell]&&!b[spell]&&s.level<=learnLevel(h)){h.spells[spell]=true;taught.push_back(s.name);}
  if(b[spell]&&!a[spell]&&s.level<=learnLevel(hero)){hero.spells[spell]=true;learned.push_back(s.name);}
 }
 std::string message="学术交流完成。";for(const auto&name:learned)message+="\n学会："+name;for(const auto&name:taught)message+="\n传授："+name;
 if(learned.empty()&&taught.empty())return "学术交流完成，没有符合等级、智慧术和魔法书条件的新法术。";storeHero();this->message(message);return message;
}
int Game::necromancyRate(const Hero&who,int owner)const{return necromancyBasisPoints(who,owner)/100;}
int Game::necromancyBasisPoints(const Hero&who,int owner)const{
 bool cloak=std::find(who.wornArtifacts.begin(),who.wornArtifacts.end(),130)!=who.wornArtifacts.end();int rate=1000*who.skills[Necromancy]+(cloak?3000:0);if(secondarySpecialty(who.originalId)==Necromancy)rate+=50*who.skills[Necromancy]*who.level;if(!rate)return 0;
 for(int item:who.wornArtifacts){if(item==54)rate+=500;else if(item==55)rate+=1000;else if(item==56)rate+=1500;}
 for(const auto&o:objects)if(o.type==ObjectType::Town&&o.owner==owner&&settlements.contains(o.id)){const auto&t=o.id==activeTownId?town:settlements.at(o.id);if(t.faction==4){if(t.built[Special2])rate+=1000;if(t.built[GrailBuilding])rate+=2000;}}
 return std::min(10000,rate);
}
void Game::resolveBattleSkills(BattleReport&report,const Battle&combat){
 int winner=report.won?0:1;Hero*who=winner==0?&hero:heroes.contains(combat.targetId)?&heroes.at(combat.targetId).value:nullptr;if(!who)return;
 int mastery=who->skills[EagleEye];if(mastery&&who->spellbook)for(int spell=0;spell<SpellCount;++spell){const auto&s=Spells[spell];if(!combat.spellHistory[1-winner][spell]||who->spells[spell]||s.special||s.adventure||bannedSpells[spell]||s.level>mastery+1||s.level>learnLevel(*who))continue;
  int chance=(30+10*mastery)*(100+(secondarySpecialty(who->originalId)==EagleEye?5*who->level:0));for(int item:who->wornArtifacts)if(item>=63&&item<=65)chance+=500*(item-62);if(random()*10000<std::min(10000,chance)){who->spells[spell]=true;if(winner==0)report.learnedSpells.push_back(spell);message(who->name+"通过鹰眼术学会了"+s.name+"。");}
 }
 int owner=winner==0?currentOwner():heroes.at(combat.targetId).owner,rate=necromancyBasisPoints(*who,owner);if(!rate)return;
 int unit=raisedUnit(*who);int base=unit;bool skeleton=std::any_of(who->army.begin(),who->army.end(),[unit](auto&s){return !s.empty()&&s.unit==unit;}),free=std::any_of(who->army.begin(),who->army.end(),[](auto&s){return s.empty();});
 if(!skeleton&&!free&&std::any_of(who->army.begin(),who->army.end(),[unit](auto&s){return !s.empty()&&s.unit==unit+1;}))unit+=1;
 int count=0,eligible=0;for(const auto&u:combat.units)if(u.team!=winner&&u.origin==StackOrigin::Army&&Units[u.unit].living){int lost=std::max(0,u.initialCount-std::min(u.count,u.permanentCount));eligible+=lost;int64_t numerator=int64_t(lost)*std::min(Units[u.unit].hp,Units[unit].hp)*rate;int64_t denominator=10000*Units[unit].hp;if(unit==base+1){numerator*=2;denominator*=3;}int64_t raised=numerator/denominator;count+=int(raised);}
 if(eligible>0){count=std::clamp(count,1,MaxArmyStack);Army army=who->army;bool fits=locations::merge(army,unit,count);if(!fits&&winner==1){int weak=0;for(int i=1;i<ArmySlots;++i){Army a{},b{};a[0]=army[i];b[0]=army[weak];if(armyPower(a)<armyPower(b))weak=i;}Army lost{},gained{};lost[0]=army[weak];gained[0]={unit,count};if(std::max(armyPower(gained),armyPower(army)*.1*who->skills[Necromancy])>armyPower(lost)){army[weak]={};fits=locations::merge(army,unit,count);}}if(winner==0){report.raised={unit,count};report.raisedPending=!fits;}if(fits){who->army=army;message("招魂术复生了 "+std::to_string(count)+" 名"+Units[unit].name+"。");}}
}
void Game::setupTactics(){if(!battle)return;auto&b=*battle;auto*target=objectById(b.targetId);if(target&&bankLocation(target->sourceClass))return;int difference=hero.skills[Tactics]-b.defenderSkills[Tactics];if(!difference)return;b.tacticsTeam=difference>0?0:1;b.tacticsRange=2*std::abs(difference)+1;for(const auto&u:b.units)if(u.team==b.tacticsTeam&&u.origin==StackOrigin::Army){b.tacticsStack=u.id;break;}if(b.tacticsStack<0)b.tacticsTeam=-1;}
bool Game::tacticsMove(int id,Point p){if(!battle||battle->tacticsTeam<0)return false;auto&b=*battle;auto at=std::find_if(b.units.begin(),b.units.end(),[id](auto&u){return u.id==id;});if(at==b.units.end()||at->team!=b.tacticsTeam||at->hp<=0||at->origin!=StackOrigin::Army)return false;
 for(auto cell:footprint(*at,p)){int x=at->team?BattleW-1-cell.x:cell.x;if(x<0||x>=b.tacticsRange||cell.y<0||cell.y>=BattleH||(!siegeCellAllowed(*at,cell)||obstacleBlocks(cell)))return false;auto*other=combatAt(cell);if(other&&other->id!=id)return false;}
 Point from{at->x,at->y};at->x=p.x;at->y=p.y;b.tacticsStack=id;emit({GameEventType::CombatMove,at->unit,0,p,id,-1,{from,p}});return true;
}
void Game::endTactics(){if(!battle||battle->tacticsTeam<0)return;battle->tacticsTeam=-1;battle->tacticsRange=0;battle->tacticsStack=-1;siegeTowers();if(battle&&!finishBattle())prepareCurrent();}
void Game::autoTactics(){if(!battle||battle->tacticsTeam<0)return;int team=battle->tacticsTeam,range=battle->tacticsRange;std::vector<int>ids;for(const auto&u:battle->units)if(u.team==team&&u.origin==StackOrigin::Army&&u.hp>0)ids.push_back(u.id);for(int id:ids){auto at=std::find_if(battle->units.begin(),battle->units.end(),[id](auto&u){return u.id==id;});if(Units[at->unit].shots)continue;int forward=range-1;for(int x=forward;x>=0;--x)if(tacticsMove(id,{team?BattleW-1-x:x,at->y}))break;}endTactics();}
}
namespace h3 {
std::string skillDescription(int skill,int mastery){
 mastery=std::clamp(mastery,0,3);if(!mastery)return "尚未掌握这项技能。";
 switch(skill){
 case Diplomacy:return "提高野兵加入的机会；交涉还取决于兵力、同类部队和野兵性情。投降费用降低 "+std::to_string(20*mastery)+"%。";
 case Ballistics:return "可手动瞄准城门、城墙和箭塔；每轮发射 "+std::to_string(mastery>=2?2:1)+" 次。"+(mastery==3?std::string("命中城门和城墙的几率为 100%，每次造成 2 点城防伤害。"):std::string("命中率和伤害随技能等级提高。"));
 case EagleEye:return "战斗胜利后，以 "+std::to_string(30+10*mastery)+"% 概率学会对方实际施放的 "+std::to_string(mastery+1)+" 级及以下战斗法术；需要魔法书及对应智慧术。";
 case Necromancy:return "战斗胜利后，将阵亡活物的 "+std::to_string(10*mastery)+"% 复生为骷髅；按每支敌军的生命值和数量分别计算。招魂宝物与招魂塔可以提高比例。";
 case Scholar:return "两名己方英雄相邻会面时，双向交流 "+std::to_string(mastery+1)+" 级及以下已知法术；双方需要魔法书，接受者需满足智慧术等级。";
 case Tactics:return "开战前可在己方 "+std::to_string(2*mastery+1)+" 列内调整部队；双方都有战术时按等级差计算，宝屋战不进行布阵。完成布阵后开始第一轮。";
 default:return SkillLevels[mastery]+SkillNames[skill];
 }
}
}
namespace h3 {
int Game::siegeBonus(int team,int stat)const{
 if(!battle||!battle->siege.fort||team==battle->siege.attackerTeam)return 0;int id=battle->siege.townId;auto at=settlements.find(id);if(at==settlements.end())return 0;const auto&t=id==activeTownId?town:at->second;
 if(t.built[GrailBuilding]){if(stat==0&&t.faction==6)return 20;if(stat==2&&t.faction==5)return 12;if((stat==0||stat==1)&&t.faction==7)return 10+(t.built[stat==0?Special2:Special3]?2:0);}
 if(stat==0&&t.faction==7&&t.built[Special2])return 2;
 if(stat==1&&t.faction==7&&t.built[Special3])return 2;
 if(stat==2&&t.faction==3&&t.built[Special2])return 2;
 if(stat==3)return (t.built[Tavern]?1:0)+(t.faction==0&&t.built[Special3]?2:0);
 if(stat==4&&t.faction==1&&t.built[Special2])return 2;
 return 0;
}
}
namespace h3 {
int Game::raisedUnit(const Hero&who)const{return std::find(who.wornArtifacts.begin(),who.wornArtifacts.end(),130)!=who.wornArtifacts.end()?std::array<int,4>{Skeleton,Skeleton+2,Skeleton+4,Skeleton+8}[who.skills[Necromancy]]:Skeleton;}
std::string Game::answerNecromancy(int slot){
 if(!report||!report->raisedPending)return "没有等待接收的招魂部队。";if(slot<-1||slot>=ArmySlots)return "请选择有效的部队栏位。";if(slot<0){report->raisedPending=false;report->raised={};return "已放弃本次招魂部队。";}
 Army next=hero.army;next[slot]={};auto raised=report->raised;if(!locations::merge(next,raised.unit,raised.count))return "当前栏位无法接收招魂部队。";hero.army=next;report->raisedPending=false;storeHero();return "已接收"+Units[raised.unit].name+" × "+std::to_string(raised.count)+"。";
}
void Game::autoNecromancy(){
 if(!report||!report->raisedPending)return;int weakest=0;for(int i=1;i<ArmySlots;++i){Army a{},b{};a[0]=hero.army[i];b[0]=hero.army[weakest];if(armyPower(a)<armyPower(b))weakest=i;}Army lost{},gained{};lost[0]=hero.army[weakest];gained[0]=report->raised;double future=armyPower(hero.army)*.1*hero.skills[Necromancy];answerNecromancy(std::max(future,armyPower(gained))>armyPower(lost)?weakest:-1);
}
}

namespace h3 {int Game::grailBonus(int owner,int faction)const{int bonus=0;for(const auto&o:objects)if(o.type==ObjectType::Town&&o.owner==owner&&settlements.contains(o.id)){const auto&t=o.id==activeTownId?town:settlements.at(o.id);if(t.faction==faction&&t.built[GrailBuilding])bonus+=2;}if(!imported()&&town.faction==faction&&town.built[GrailBuilding])bonus=2;return bonus;}}
