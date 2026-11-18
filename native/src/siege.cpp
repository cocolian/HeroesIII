#include "h3/game.hpp"
#include <algorithm>
#include <cmath>
namespace h3 {
namespace {
constexpr Point Sections[]={{14,2},{11,0},{12,10},{9,5},{11,1},{9,4},{10,7},{11,10}};
constexpr Point Static[]={{11,0},{10,2},{10,3},{9,6},{10,8},{11,9}};
constexpr int Moat[]={11,28,44,61,77,111,129,146,164,181};
Point canonical(const Siege&s,Point p){if(s.attackerTeam)p.x=BattleW-1-p.x;return p;}
}
void Game::setupSiege(int townId,int attacker){
 if(!battle||!settlements.contains(townId))return;const auto&t=townId==activeTownId?town:settlements.at(townId);int fort=t.built[Castle]?3:t.built[Citadel]?2:t.built[Fort]?1:0;const Terrain native[]={Terrain::Grass,Terrain::Grass,Terrain::Snow,Terrain::Lava,Terrain::Dirt,Terrain::Subterranean,Terrain::Rough,Terrain::Swamp,Terrain::Grass};battle->terrain=native[t.faction];if(!fort)return;
 auto&s=battle->siege;s.faction=t.faction;s.fort=fort;s.townId=townId;s.attackerTeam=attacker;s.buildings=int(std::count(t.built.begin(),t.built.end(),true));s.hp.fill(fort);s.hp[0]=fort>=2?2:0;s.hp[1]=s.hp[2]=fort>=3?2:0;battle->log.push_back("攻城战：城墙、城门"+std::string(fort>=2?"、护城河与箭塔":"")+"已部署。");
}
int Game::siegePartAt(Point p)const{
 if(!battle||!battle->siege.fort)return -1;p=canonical(battle->siege,p);
 if(p==Point{9,5}||p==Point{10,5})return 3;
 for(int i=4;i<8;++i)if(p==Sections[i])return i;
 if(std::find(std::begin(Static),std::end(Static),p)!=std::end(Static))return -2;
 return -1;
}
bool Game::gateOpen()const{
 if(!battle||!battle->siege.fort)return true;const auto&s=battle->siege;if(s.hp[3]==0)return true;
 for(const auto&u:battle->units)if(u.hp>0)for(auto p:footprint(u,{u.x,u.y})){p=canonical(s,p);if(p==Point{9,5}||p==Point{10,5})return true;}
 return false;
}
bool Game::siegeCellAllowed(const CombatStack&u,Point p)const{
 if(!battle||!battle->siege.fort)return true;const auto&s=battle->siege;int part=siegePartAt(p);if(part==-2)return false;if(part<0||s.hp[part]==0)return true;
 if(part==3){if(gateOpen())return true;if(u.team==s.attackerTeam)return false;for(const auto&other:battle->units)if(other.hp>0&&other.team==s.attackerTeam)for(auto q:footprint(other,{other.x,other.y}))if(canonical(s,q)==Point{8,5})return false;return true;}
 return false;
}
bool Game::inMoat(const CombatStack&u,Point p)const{
 if(!battle||battle->siege.fort<2)return false;const auto&s=battle->siege;
 for(auto cell:footprint(u,p)){cell=canonical(s,cell);int original=cell.y*17+cell.x+1;bool moat=std::find(std::begin(Moat),std::end(Moat),original)!=std::end(Moat);
  if(s.faction==7)moat|=std::find(std::begin(Moat),std::end(Moat),original+1)!=std::end(Moat)||(original==94&&!gateOpen());
  if(moat&&!(s.faction==2&&std::find(s.spentMines.begin(),s.spentMines.end(),cell)!=s.spentMines.end()))return true;
 }return false;
}
void Game::moatHit(int id){
 if(!battle)return;auto it=std::find_if(battle->units.begin(),battle->units.end(),[&](const auto&u){return u.id==id;});if(it==battle->units.end()||it->hp<=0||!inMoat(*it,{it->x,it->y}))return;auto&s=battle->siege;
 if(s.faction==2){if(it->team!=s.attackerTeam)return;auto p=canonical(s,{it->x,it->y});s.spentMines.push_back(p);if(spellImmune(*it,LandMine))return;}
 constexpr int Damage[]={70,70,150,90,70,90,70,90,90};battle->log.push_back(s.faction==2?"触发塔楼地雷。":"护城河造成伤害。");hurt(*it,Damage[s.faction]);
}
bool Game::wallPenalty(const CombatStack&a,const CombatStack&t)const{
 if(!battle||!battle->siege.fort||a.team!=battle->siege.attackerTeam)return false;const auto&s=battle->siege;auto from=canonical(s,{a.x,a.y}),to=canonical(s,{t.x,t.y});constexpr int line[]={11,11,10,10,9,9,9,10,10,11,11};if(from.x>=line[from.y]||to.x<line[to.y])return false;
 int row=std::clamp((from.y+to.y)/2,0,10),part=row==5?3:row<3?4:row<6?5:row<9?6:7;return s.hp[part]>0;
}
int Game::catapultPartAt(Point p)const{if(!battle||!battle->siege.fort)return -1;p=canonical(battle->siege,p);for(int i=0;i<8;++i)if(p==Sections[i])return i;return p==Point{10,5}?3:-1;}
void Game::catapultShots(int selected){
 if(!battle||!battle->siege.fort||battle->catapultRound==battle->round)return;auto&b=*battle;auto&s=b.siege;b.catapultRound=b.round;int mastery=s.attackerTeam?b.defenderSkills[Ballistics]:hero.skills[Ballistics];int shots=mastery>=2?2:1;
 const int accuracy[4][4]={{5,10,25,50},{5,10,50,75},{5,10,50,75},{10,20,100,100}};
 for(int shot=0;shot<shots;++shot){std::vector<int>parts;for(int i=0;i<8;++i)if(s.hp[i]>0)parts.push_back(i);if(parts.empty())return;int part=selected>=0&&selected<8&&s.hp[selected]>0?selected:parts[int(random()*parts.size())];int kind=part==0?0:part<3?1:part==3?2:3;auto actor=std::find_if(b.units.begin(),b.units.end(),[&](auto&u){return u.unit==CatapultUnit&&u.team==s.attackerTeam;});Point aim=canonical(s,Sections[part]);if(actor!=b.units.end())emit({GameEventType::CombatAttack,CatapultUnit,2,aim,actor->id,-1,{},*actor});
  if(random()*100>=accuracy[mastery][kind]){b.log.push_back("投石车未命中城防。");emit({GameEventType::SiegeHit,-1,s.hp[part],aim,-1,part});continue;}int roll=int(random()*100);int damage=mastery==3?2:mastery?roll<50?1:2:roll<10?0:roll<70?1:2;s.hp[part]=std::max(0,s.hp[part]-damage);emit({GameEventType::SiegeHit,-1,s.hp[part],aim,-1,part});b.log.push_back("投石车命中"+std::string(part==3?"城门":part<3?"箭塔":"城墙")+"，造成 "+std::to_string(damage)+" 点城防伤害。");
 }
}
std::string Game::fireCatapult(int part){if(!battle||!current()||current()->unit!=CatapultUnit||battle->tacticsTeam>=0)return "当前不是投石车行动。";if(part<0||part>7||battle->siege.hp[part]<=0)return "请选择尚未摧毁的城门、城墙或箭塔。";catapultShots(part);if(current())current()->acted=true;completeAction(false);return {};}
void Game::siegeCatapult(){if(battle&&!std::any_of(battle->units.begin(),battle->units.end(),[](const auto&u){return u.unit==CatapultUnit;}))catapultShots();}
void Game::siegeTowers(){
 if(!battle||battle->siege.fort<2)return;for(int tower=0;tower<3&&battle;++tower){auto&s=battle->siege;if(!s.hp[tower])continue;CombatStack*target=nullptr;double best=-1;for(auto&u:battle->units)if(u.hp>0&&u.team==s.attackerTeam){double score=u.count*(Units[u.unit].minDamage+Units[u.unit].maxDamage)*(Units[u.unit].shots?2.:1.);if(score>best){best=score;target=&u;}}
  if(!target)return;int base=tower?6:10,buildings=tower?s.buildings/2:s.buildings;int damage=base+buildings+int(random()*(base+buildings+1));battle->log.push_back(tower?"侧箭塔射击。":"主箭塔射击。");hurt(*target,damage);if(finishBattle())return;
 }
}
}
