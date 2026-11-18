#pragma once
#include "h3/game.hpp"
#include "h3/battle_effect.hpp"
#include "h3/battle_layout.hpp"
#include <functional>
#include <map>
#include <span>

namespace h3 {
struct BattleProjectilePose {const BattleProjectileDef*definition=nullptr;double x=0,y=0;int frame=0;bool flip=false;};
struct BattlePose {double x=0,y=0;int group=0,frame=0;bool flip=false,dying=false;};
// A transient presentation of an already committed action. It owns no Game
// and cannot change its RNG, turn queue or saved result.
class BattleMotion {
public:
 using FrameCount=std::function<int(int unit,int group)>;
 void start(const Battle& before,std::span<const GameEvent>,const FrameCount&,uint64_t now);
 void tick(uint64_t now,bool playing=true);
 void clear();
 bool active()const{return index_<clips_.size();}
 const Battle* view()const{return active()?&view_:nullptr;}
 const Battle* retainedView()const{return view_.units.empty()?nullptr:&view_;}
 std::optional<BattlePose> pose(int stackId)const;
 std::optional<BattlePose> corpsePose(const CombatStack&)const;
 std::vector<GameEvent> takeEvents();
 const GameEvent* feedback()const;
 const std::string& damageMessage()const{return damageMessage_;}
 std::vector<BattleEffectPose> effects()const;std::vector<BattleProjectilePose> projectiles()const;
private:
 struct Track {int stackId=-1,group=0,frames=1;uint64_t delay=0,duration=0,frameMs=100;std::vector<Point> path;bool flip=false,loop=false,dying=false;};
 struct Moment {uint64_t at=0;GameEvent cue;int corpseGroup=-1;bool flip=false;};
 struct EffectTrack {const BattleEffectDef* definition=nullptr;Point pos{};int stackId=-1;uint64_t delay=0;};
 struct Missile {const BattleProjectileDef*definition=nullptr;Point from{},to{};uint64_t delay=0,duration=0;int frame=0;bool flip=false,arc=false;};
 struct Clip {uint64_t duration=0;std::vector<Track> tracks;std::vector<Moment> moments;std::optional<std::pair<int,bool>> facingAfter;std::vector<EffectTrack> effects{};std::vector<Missile>missiles{};};
 struct CorpseFrame {int unit=-1;BattlePose pose;};
 Battle view_;std::vector<Clip> clips_;std::vector<GameEvent> events_;std::map<int,bool> facing_;std::map<int,CorpseFrame> corpses_;size_t index_=0,moment_=0;uint64_t clock_=0,elapsed_=0;
 std::string damageMessage_;
 void advance(uint64_t delta);
 void apply(const Moment&);
};
}
