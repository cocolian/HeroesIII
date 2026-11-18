#include "h3/battle_effect.hpp"
#include <array>

namespace h3 {
namespace {
const std::array<BattleEffectDef,4> Effects{{
#include "../generated/battle_effects.inc"
}};
const std::array<BattleEffectDef,SpellCount> SpellEffects{{
#include "../generated/spell_effects.inc"
}};
}
const BattleEffectDef* battleEffect(GameEventType type,int detail){
 int index=-1;
 if(type==GameEventType::CombatMorale&&detail!=0)index=detail>0?0:1;
 if(type==GameEventType::CombatLuck&&detail>0)index=2;
 if(type==GameEventType::CombatSpellEffect){
  if(detail==Mirth)index=0;
  if(detail==Sorrow)index=1;
  if(detail==Fortune)index=2;
  if(detail==Misfortune)index=3;
 }
 if(index<0&&type==GameEventType::CombatSpellEffect&&detail>=0&&detail<SpellCount&&SpellEffects[detail].animation)return &SpellEffects[detail];
 return index<0?nullptr:&Effects[size_t(index)];
}
}
