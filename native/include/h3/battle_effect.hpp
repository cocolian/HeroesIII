#pragma once
#include "h3/game.hpp"

namespace h3 {
struct BattleEffectDef {
 const char* animation;
 int frames=0,width=0,height=0;
 uint64_t frameMs=100;
};
// Original asset identities and canvases, compiled from pinned metadata.
// Playback speed and battlefield anchoring still require executable capture.
const BattleEffectDef* battleEffect(GameEventType,int detail);
struct BattleEffectPose {
 const BattleEffectDef* definition=nullptr;
 Point pos{};
 int stackId=-1,frame=0;
};
}
