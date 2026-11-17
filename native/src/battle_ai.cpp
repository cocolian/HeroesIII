#include "h3/game.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
namespace h3 {
namespace {
double worth(const CombatStack&u){Army a{};a[0]={u.unit,1};return Game::armyPower(a)/Units[u.unit].hp;}
double threat(const CombatStack&u){return u.count*(Units[u.unit].minDamage+Units[u.unit].maxDamage)/2.;}
}
bool Game::aiSpell(){
 if(!battle||!current())return false;struct Restore{int&value;int old;~Restore(){value=old;}}restore{spellTeam_,spellTeam_};spellTeam_=current()->team;
 const auto&known=spellTeam_==1?battle->defenderSpells:hero.spells;bool book=spellTeam_==1?battle->defenderSpellbook:hero.spellbook;if(!book||casterRound()==battle->round)return false;
 double best=3;int chosen=-1;Point aim{};
 for(int spell:{MagicArrow,LightningBolt,IceBolt,Implosion,Slow,Haste,Bless,Curse,Shield,AirShield,Prayer,Cure,Dispel,StoneSkin,Resurrection,AnimateDead}){
  if((spellTeam_==1&&(spell==Resurrection||spell==AnimateDead))||!known[spell]||casterMana()<spellCost(spell))continue;for(const auto&u:battle->units){if(u.hp<=0&&spell!=Resurrection&&spell!=AnimateDead)continue;Point p{u.x,u.y};auto targets=spellTargets(spell,p);if(targets.empty())continue;double utility=0;
   for(int id:targets){auto at=std::find_if(battle->units.begin(),battle->units.end(),[&](auto&t){return t.id==id;});const auto&t=*at;const auto&d=Units[t.unit];bool own=t.team==spellTeam_;double score=0;
    if(Spells[spell].kind==SpellKind::Damage){int raw=casterPower()*Spells[spell].power+Spells[spell].values[spellMastery(spell)];score=std::min(t.hp,spellDamage(t,spell,raw))*worth(t)*(own?-1:1);}
    else if(spell==Resurrection||spell==AnimateDead)score=std::min(t.initialCount*d.hp-t.hp,casterPower()*Spells[spell].power+Spells[spell].values[spellMastery(spell)])*worth(t);
    else if(spell==Cure)score=std::min(t.count*d.hp-t.hp,casterPower()*Spells[spell].power+Spells[spell].values[spellMastery(spell)])*worth(t);
    else if(spell==Dispel){for(int e=0;e<SpellCount;++e)if(t.effects[e]>0)score+=threat(t)*((own==Spells[e].enemy)?1:-1);}
    else if(!t.effects[spell]){double hit=threat(t);if(spell==Slow)score=hit*(d.shots?.08:.9);else if(spell==Haste)score=hit*(d.shots?.1:.65);else if(spell==Bless)score=(d.maxDamage-(d.minDamage+d.maxDamage)/2.+Spells[spell].values[spellMastery(spell)])*t.count*2.;else if(spell==Curse)score=(d.maxDamage-d.minDamage)*t.count;else if(spell==Prayer)score=hit*.65;else if(spell==Shield||spell==StoneSkin)score=hit*.3;else if(spell==AirShield){bool shooters=std::any_of(battle->units.begin(),battle->units.end(),[&](auto&v){return v.hp>0&&v.team!=spellTeam_&&Units[v.unit].shots>0;});score=shooters?hit*.4:0;}}
    utility+=score;
   }
   utility/=1.+spellCost(spell)*.18;if(utility>best){best=utility;chosen=spell;aim=p;}
  }
 }
 return chosen>=0&&cast(chosen,aim).empty();
}
void Game::ai(){
 auto*u=current();if(!battle||!u)return;aiSpell();u=current();if(!battle||!u)return;
 int actor=u->id;double best=-1e100;int target=-1;for(const auto&t:battle->units)if(t.hp>0&&t.team!=u->team){auto plan=attackPlan(*u,t);if(!plan)continue;int dealt=std::min(t.hp,damage(*u,t,plan->ranged,plan->traveled));double score=dealt*worth(t);if(!plan->ranged&&!Units[u->unit].noRetaliation&&t.retaliations<Units[t.unit].retaliations+spellEffect(t,Counterstrike)){auto survivor=t;survivor.hp-=dealt;survivor.count=(survivor.hp+Units[t.unit].hp-1)/Units[t.unit].hp;if(survivor.count>0)score-=std::min(u->hp,damage(survivor,*u,false))*worth(*u)*.85;}
  if(t.hp<=dealt)score+=threat(t)*1.5;if(Units[t.unit].shots)score*=1.25;if(score>best){best=score;target=t.id;}}
 if(target>=0&&best>=0){auto at=std::find_if(battle->units.begin(),battle->units.end(),[&](auto&t){return t.id==target;});act("attack",{at->x,at->y});return;}
 if(battle->siege.fort>=2&&u->team!=battle->siege.attackerTeam&&std::any_of(battle->siege.hp.begin(),battle->siege.hp.begin()+3,[](int n){return n>0;})){
  bool insideEnemy=false;for(const auto&t:battle->units)if(t.hp>0&&t.team!=u->team){int x=battle->siege.attackerTeam?BattleW-1-t.x:t.x;insideEnemy|=x>=11;}
  if(!insideEnemy){act("defend");return;}
 }
 Point destination{u->x,u->y};double moveBest=-1e100;bool ranged=Units[u->unit].shots>0&&u->shots>0;
 for(int y=0;y<BattleH;++y)for(int x=0;x<BattleW;++x){auto steps=battlePath(*u,{x,y});if(!steps||!*steps||*steps>speed(*u))continue;auto candidate=*u;candidate.x=x;candidate.y=y;double danger=0,nearest=100,prospect=0;
  for(const auto&t:battle->units)if(t.hp>0&&t.team!=u->team){int d=distance(candidate,t);nearest=std::min(nearest,double(d));if(Units[t.unit].shots>0||d<=speed(t)+1)danger+=std::min(u->hp,damage(t,candidate,Units[t.unit].shots>0))*worth(*u);prospect=std::max(prospect,threat(t)/(d+1));}
  double score=ranged?std::min(nearest,10.)*8-danger*.6:prospect*2-nearest*20-danger*.12;if(inMoat(candidate,{x,y}))score-=100;score-=*steps*.01;if(score>moveBest){moveBest=score;destination={x,y};}
 }
 if(destination!=Point{u->x,u->y}){act("move",destination);return;}if(target>=0){for(const auto&t:battle->units)if(t.id==target){act("attack",{t.x,t.y});return;}}
 if(current()&&current()->id==actor)act("defend");
}
}
