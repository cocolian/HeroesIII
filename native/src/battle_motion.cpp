#include "h3/battle_motion.hpp"
#include "h3/battle_layout.hpp"
#include <algorithm>
#include <cmath>

namespace h3 {
namespace {
CombatStack* stack(Battle& battle,int id){auto it=std::find_if(battle.units.begin(),battle.units.end(),[id](const auto&u){return u.id==id;});return it==battle.units.end()?nullptr:&*it;}
bool damageEvent(const GameEvent&e){return e.type==GameEventType::CombatHit||e.type==GameEventType::CombatDeath;}
bool validState(const GameEvent&e,const CombatStack&unit){return e.state&&e.state->id==unit.id&&(e.state->unit==unit.unit||(e.type==GameEventType::CombatSpellEffect&&e.detail==Resurrection&&unit.hp==0&&unit.corpse&&e.state->unit==48&&e.state->hp>0&&e.state->team==unit.team));}
}
void BattleMotion::clear(){view_={};clips_.clear();events_.clear();facing_.clear();corpses_.clear();damageMessage_.clear();index_=moment_=0;clock_=elapsed_=0;}
const GameEvent*BattleMotion::feedback()const{if(!active()||clips_[index_].moments.empty())return nullptr;const auto&clip=clips_[index_];const auto&cue=clip.moments.front().cue;return cue.type==GameEventType::CombatMorale||cue.type==GameEventType::CombatLuck||(cue.type==GameEventType::SpellCast&&!clip.effects.empty())?&cue:nullptr;}
std::vector<BattleEffectPose>BattleMotion::effects()const{
 std::vector<BattleEffectPose> poses;if(!active())return poses;
 for(const auto&track:clips_[index_].effects){const auto&def=*track.definition;if(elapsed_>=track.delay&&elapsed_-track.delay<uint64_t(def.frames)*def.frameMs)poses.push_back({&def,track.pos,track.stackId,int((elapsed_-track.delay)/def.frameMs)});}
 return poses;
}
std::vector<BattleProjectilePose> BattleMotion::projectiles()const{
 std::vector<BattleProjectilePose> out;if(!active())return out;
 for(const auto&m:clips_[index_].missiles)if(elapsed_>=m.delay&&elapsed_-m.delay<m.duration){double p=double(elapsed_-m.delay)/m.duration;double x=m.from.x+(m.to.x-m.from.x)*p,y=m.from.y+(m.to.y-m.from.y)*p;if(m.arc)y-=320*p*(1-p);out.push_back({m.definition,x,y,m.frame,m.flip});}return out;
}
void BattleMotion::start(const Battle& before,std::span<const GameEvent> events,const FrameCount& count,uint64_t now){
 auto pending=takeEvents();auto oldFacing=std::move(facing_);auto oldCorpses=std::move(corpses_);bool sameBattle=before.targetId==view_.targetId;
 clear();events_=std::move(pending);view_=before;clock_=now;
 if(sameBattle){facing_=std::move(oldFacing);corpses_=std::move(oldCorpses);}
 Battle planned=before;planned.units.reserve(MaxBattleStacksPerSide*2);view_.units.reserve(MaxBattleStacksPerSide*2);std::map<int,bool> heading;
 for(const auto&unit:before.units){heading[unit.id]=facing_.contains(unit.id)?facing_[unit.id]:unit.team==1;if(unit.hp>0)corpses_.erase(unit.id);}
 auto frames=[&](int unit,int group){return std::max(0,count(unit,group));};
 auto track=[&](const CombatStack&unit,int group,uint64_t period,Point point,bool flip){int n=std::max(1,frames(unit.unit,group));return Track{unit.id,group,n,0,uint64_t(n)*period,period,{point},flip};};
 auto stationary=[&](const CombatStack&unit,int group,uint64_t period,Point point){if(!frames(unit.unit,group))return;auto pose=track(unit,group,period,point,heading[unit.id]);clips_.push_back({pose.duration,{pose},{},{}});};
 auto turn=[&](const CombatStack&unit,bool flip){
  if(heading[unit.id]==flip)return;
  stationary(unit,7,100,{unit.x,unit.y});heading[unit.id]=flip;
  Clip second;second.facingAfter=std::pair{unit.id,flip};
  if(frames(unit.unit,8)){auto pose=track(unit,8,100,{unit.x,unit.y},flip);second.duration=pose.duration;second.tracks.push_back(std::move(pose));}
  clips_.push_back(std::move(second));
 };
 auto lookAt=[](const CombatStack&from,const CombatStack&to,bool fallback){int dx=battleHexCenter({to.x,to.y}).x-battleHexCenter({from.x,from.y}).x;return dx?dx<0:fallback;};
 auto missile=[&](Clip&clip,const CombatStack&actor,Point destination,uint64_t release,int direction){
  const auto&d=BattleProjectiles[actor.unit];if(!d.animation||!d.frames)return uint64_t(0);auto center=battleHexCenter({actor.x,actor.y});bool flip=heading[actor.id];int at=2*direction;Point from{center.x+int(std::lround(d.offsets[at]))*(flip?-1:1),center.y+int(std::lround(d.offsets[at+1]))};Point to=battleHexCenter(destination);to.y-=30;
  double angle=std::atan2(double(from.y-to.y),std::abs(double(to.x-from.x)))*180./3.141592653589793;int frame=0;double error=1000;for(int i=0;i<std::min(d.frames,12);++i)if(std::abs(d.angles[i]-angle)<error){error=std::abs(d.angles[i]-angle);frame=i;}
  uint64_t flight=std::max<uint64_t>(60,uint64_t(std::lround(std::hypot(double(to.x-from.x),double(to.y-from.y)))));clip.missiles.push_back({&d,from,to,release,flight,frame,to.x<from.x,actor.unit==CatapultUnit});clip.duration=std::max(clip.duration,release+flight);return flight;
 };
 auto addEffect=[&](Clip&clip,const GameEvent&event,uint64_t delay=0){
  clip.moments.push_back({delay,event});auto*unit=stack(planned,event.stackId);
  if(!unit&&event.type==GameEventType::CombatSpellEffect&&event.state&&event.state->id==event.stackId&&event.state->unit==event.unit&&(event.state->origin==StackOrigin::Clone||event.state->origin==StackOrigin::Summoned)&&planned.units.size()<MaxBattleStacksPerSide*2){auto placeholder=*event.state;placeholder.hp=placeholder.count=placeholder.permanentCount=0;planned.units.push_back(placeholder);view_.units.push_back(placeholder);unit=&planned.units.back();heading[unit->id]=unit->team==1;}
  const auto*definition=battleEffect(event.type,event.detail);
  if(!definition||!unit||!validState(event,*unit)||(event.state->hp<=0&&event.type!=GameEventType::CombatSpellEffect)||event.pos!=Point{event.state->x,event.state->y})return;
  clip.effects.push_back({definition,event.pos,event.stackId,delay});clip.duration=std::max(clip.duration,delay+uint64_t(definition->frames)*definition->frameMs);
  if(event.type==GameEventType::CombatMorale&&event.detail<0&&frames(unit->unit,3)){
   auto reaction=track(*unit,3,100,event.pos,heading[unit->id]);clip.duration=std::max(clip.duration,reaction.duration);clip.tracks.push_back(std::move(reaction));
  }
  *unit=*event.state;
 };
 for(size_t i=0;i<events.size();i++){
  const auto&event=events[i];auto*unit=stack(planned,event.stackId);
  bool valid=unit&&unit->unit==event.unit&&event.unit>=0&&event.unit<UnitCount;
  if(event.type==GameEventType::SpellCast){
   Clip cast;cast.moments.push_back({0,event});uint64_t release=0;
   if(unit&&event.unit>=0&&validState(event,*unit)){
    int direction=event.pos.y<unit->y?0:event.pos.y>unit->y?2:1,group=17+direction;if(!frames(unit->unit,group))group=18;
    if(frames(unit->unit,group)){auto period=uint64_t(std::lround(100*std::clamp(CreatureAnimationTimings[unit->unit].attackTime,.1,10.)));auto pose=track(*unit,group,period,{unit->x,unit->y},heading[unit->id]);release=uint64_t(pose.frames/2)*period;cast.duration=pose.duration;cast.tracks.push_back(std::move(pose));}
   }
   while(i+1<events.size()&&(damageEvent(events[i+1])||(events[i+1].type==GameEventType::CombatSpellEffect&&events[i+1].detail==event.detail))){
    const auto&effect=events[++i];if(effect.type==GameEventType::CombatSpellEffect){addEffect(cast,effect,release);continue;}auto*victim=stack(planned,effect.stackId);if(!victim||!validState(effect,*victim)){cast.moments.push_back({release,effect});continue;}
    bool dead=effect.state->hp<=0,vanished=dead&&!effect.state->corpse;int response=dead?(frames(victim->unit,6)?6:5):3;cast.moments.push_back({release,effect,dead&&!vanished?response:-1,heading[victim->id]});if(!vanished&&frames(victim->unit,response)){auto pose=track(*victim,response,100,{victim->x,victim->y},heading[victim->id]);pose.delay=release;pose.dying=dead;cast.duration=std::max(cast.duration,release+pose.duration);cast.tracks.push_back(std::move(pose));}*victim=*effect.state;
   }
   std::stable_sort(cast.moments.begin(),cast.moments.end(),[](auto&a,auto&b){return a.at<b.at;});clips_.push_back(std::move(cast));continue;
  }
  bool moving=event.type==GameEventType::CombatMove&&valid&&event.path.size()>=2&&event.path.front()==Point{unit->x,unit->y}&&event.path.back()==event.pos;
  if(moving)for(auto p:event.path)if(p.x<0||p.x>=BattleW||p.y<0||p.y>=BattleH)moving=false;
  if(moving){
   auto first=battleHexCenter(event.path.front()),last=battleHexCenter(event.path.back());turn(*unit,last.x==first.x?heading[unit->id]:last.x<first.x);
   // Raw CRANIM walk factors are preserved; these base milliseconds are
   // provisional until original-executable frame capture is available.
   auto period=uint64_t(std::lround(100*std::clamp(CreatureAnimationTimings[event.unit].walkTime,.1,10.)));
   stationary(*unit,20,period,event.path.front());
   int cells=Units[event.unit].flying?Game::hexDistance(event.path.front(),event.path.back()):int(event.path.size())-1;
   uint64_t duration=std::max<uint64_t>(1,uint64_t(std::max(1,cells))*period*12/5);
   Track walking{unit->id,0,std::max(1,frames(unit->unit,0)),0,duration,period,event.path,heading[unit->id],true};
   clips_.push_back({duration,{std::move(walking)},{{0,event}}, {}});unit->x=event.pos.x;unit->y=event.pos.y;
   stationary(*unit,21,period,event.path.back());continue;
  }
  if(event.type==GameEventType::CombatAttack&&valid&&event.detail==2){
   int group=unit->unit==CatapultUnit?15:12+(event.pos.y<unit->y?0:event.pos.y>unit->y?2:1);auto period=uint64_t(std::lround(100*std::clamp(CreatureAnimationTimings[unit->unit].attackTime,.1,10.)));auto pose=track(*unit,group,period,{unit->x,unit->y},heading[unit->id]);uint64_t release=uint64_t(std::clamp(CreatureAnimationTimings[unit->unit].attackClimax,0,pose.frames-1))*period;Clip shot{pose.duration,{pose},{{0,event}}, {}};uint64_t impact=release+missile(shot,*unit,event.pos,release,1);if(i+1<events.size()&&events[i+1].type==GameEventType::SiegeHit)shot.moments.push_back({impact,events[++i]});clips_.push_back(std::move(shot));continue;
  }
  auto*target=stack(planned,event.targetId);
  if(event.type==GameEventType::CombatAttack&&valid&&target&&target->hp>0&&unit->hp>0){
   if(validState(event,*unit))*unit=*event.state;
   turn(*unit,lookAt(*unit,*target,heading[unit->id]));turn(*target,lookAt(*target,*unit,heading[target->id]));
   bool ranged=event.detail!=0;int direction=target->y<unit->y?0:target->y>unit->y?2:1;
   int group=(ranged?14:11)+direction;if(!frames(unit->unit,group))group=ranged?15:12;
   uint64_t period=uint64_t(std::lround(100*std::clamp(CreatureAnimationTimings[unit->unit].attackTime,.1,10.)));
   auto attack=track(*unit,group,period,{unit->x,unit->y},heading[unit->id]);
   // Melee uses the middle frame provisionally. Ranged uses its original
   // release frame; projectile flight and original melee timing remain open.
   int impactFrame=ranged?std::clamp(CreatureAnimationTimings[unit->unit].attackClimax,0,attack.frames-1):attack.frames/2;
   uint64_t impact=uint64_t(impactFrame)*period;
   Clip clip{attack.duration,{attack},{{0,event}}, {}};if(ranged)impact+=missile(clip,*unit,{target->x,target->y},impact,direction);
   while(i+1<events.size()&&damageEvent(events[i+1])){
    const auto&hit=events[++i];auto*victim=stack(planned,hit.stackId);
    if(!victim||victim->unit!=hit.unit||!validState(hit,*victim)){clip.moments.push_back({impact,hit});continue;}
    bool dead=hit.state->hp<=0,vanished=dead&&!hit.state->corpse;
    int response=dead?(ranged&&frames(victim->unit,6)?6:5):!ranged&&victim->defending&&frames(victim->unit,4)?4:3;
    bool flip=heading[victim->id];
    clip.moments.push_back({impact,hit,dead&&!vanished?response:-1,flip});
    if(!vanished){auto reaction=track(*victim,response,100,{victim->x,victim->y},flip);reaction.delay=impact;reaction.dying=dead;clip.duration=std::max(clip.duration,impact+reaction.duration);clip.tracks.push_back(std::move(reaction));}
    *victim=*hit.state;
   }
   clips_.push_back(std::move(clip));continue;
  }
  Clip cue;if(battleEffect(event.type,event.detail))addEffect(cue,event);else cue.moments.push_back({0,event});clips_.push_back(std::move(cue));
  if(valid&&validState(event,*unit))*unit=*event.state;
 }
 // Return surviving stacks to their side's resting direction after the
 // whole exchange, rather than interrupting retaliation or double strikes.
 for(auto&unit:planned.units)if(unit.hp>0)turn(unit,unit.team==1);
 advance(0);
}
void BattleMotion::tick(uint64_t now,bool playing){uint64_t delta=now>=clock_?now-clock_:0;clock_=now;if(playing)advance(delta);}
void BattleMotion::apply(const Moment&moment){
 auto cue=moment.cue;cue.scheduled=clips_[index_].duration>0;events_.push_back(std::move(cue));if(moment.cue.type==GameEventType::SiegeHit&&moment.cue.targetId>=0&&moment.cue.targetId<8){view_.siege.hp[moment.cue.targetId]=moment.cue.detail;return;}auto*unit=stack(view_,moment.cue.stackId);if(!unit||!validState(moment.cue,*unit))return;
 if(damageEvent(moment.cue)&&moment.cue.state->hp<unit->hp){
  damageMessage_=Units[unit->unit].name+"受到 "+std::to_string(unit->hp-moment.cue.state->hp)+" 点伤害，损失 "+std::to_string(unit->count-moment.cue.state->count)+" 个。";
 }
 *unit=*moment.cue.state;
 if(unit->hp>0)corpses_.erase(unit->id);
 if(unit->hp<=0){
  if(unit->corpse&&moment.corpseGroup>=0){auto center=battleHexCenter({unit->x,unit->y});corpses_[unit->id]={unit->unit,{double(center.x),double(center.y),moment.corpseGroup,-1,moment.flip,false}};}
  else corpses_.erase(unit->id);
  for(auto&copy:view_.units)if(copy.origin==StackOrigin::Clone&&copy.cloneOf==unit->id){copy.hp=copy.count=copy.permanentCount=0;copy.corpse=false;corpses_.erase(copy.id);}
 }
}
void BattleMotion::advance(uint64_t delta){
 while(active()){
  const auto&clip=clips_[index_];uint64_t step=std::min(delta,clip.duration-elapsed_),end=elapsed_+step;
  while(moment_<clip.moments.size()&&clip.moments[moment_].at<=end)apply(clip.moments[moment_++]);
  elapsed_=end;delta-=step;if(elapsed_<clip.duration)return;
  for(const auto&track:clip.tracks)if(!track.path.empty())if(auto*unit=stack(view_,track.stackId)){unit->x=track.path.back().x;unit->y=track.path.back().y;}
  if(clip.facingAfter)facing_[clip.facingAfter->first]=clip.facingAfter->second;
  index_++;elapsed_=0;moment_=0;
 }
}
std::optional<BattlePose> BattleMotion::pose(int id)const{
 if(!active())return {};const auto&clip=clips_[index_];
 for(auto it=clip.tracks.rbegin();it!=clip.tracks.rend();++it){
  const auto&track=*it;if(track.stackId!=id||track.path.empty()||elapsed_<track.delay)continue;
  uint64_t time=elapsed_-track.delay;if(time>=track.duration&&!track.dying)continue;
  double progress=track.duration?std::min(1.,double(time)/track.duration):1,segment=progress*double(track.path.size()-1);
  size_t i=std::min(size_t(segment),track.path.size()-1),j=std::min(i+1,track.path.size()-1);
  auto from=battleHexCenter(track.path[i]),to=battleHexCenter(track.path[j]);double part=segment-double(i);
  int frame=track.loop?int(time/track.frameMs)%track.frames:int(std::min<uint64_t>(time/track.frameMs,uint64_t(track.frames-1)));
  return BattlePose{from.x+(to.x-from.x)*part,from.y+(to.y-from.y)*part,track.group,frame,track.flip,track.dying&&time<track.duration};
 }
 auto unit=std::find_if(view_.units.begin(),view_.units.end(),[id](const auto&u){return u.id==id;});
 if(unit!=view_.units.end()){
  if(auto corpse=corpsePose(*unit))return corpse;
  if(unit->hp>0&&facing_.contains(id)){auto center=battleHexCenter({unit->x,unit->y});return BattlePose{double(center.x),double(center.y),2,0,facing_.at(id),false};}
 }return {};
}
std::optional<BattlePose> BattleMotion::corpsePose(const CombatStack&unit)const{
 auto it=corpses_.find(unit.id);return unit.hp<=0&&unit.corpse&&it!=corpses_.end()&&it->second.unit==unit.unit?std::optional<BattlePose>(it->second.pose):std::nullopt;
}
std::vector<GameEvent> BattleMotion::takeEvents(){std::vector<GameEvent> result;result.swap(events_);return result;}
}
