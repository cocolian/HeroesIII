#include "h3/game.hpp"
#include <algorithm>
#include <functional>
#include <set>

namespace h3 {
namespace {
struct Recipe{int id;std::vector<int> parts;};
const Recipe Recipes[]={
#include "../generated/artifact_combinations.inc"
};
const Recipe* recipe(int id){for(const auto&r:Recipes)if(r.id==id)return &r;return nullptr;}
void eraseOne(std::vector<int>&v,int id){auto i=std::find(v.begin(),v.end(),id);if(i!=v.end())v.erase(i);}
void removeName(Hero&h,int id){auto i=std::find(h.artifacts.begin(),h.artifacts.end(),artifactName(id));if(i!=h.artifacts.end())h.artifacts.erase(i);}
std::map<std::string,int> reservation(const Hero&h,int id,const std::string&primary){
 auto*r=recipe(id);if(!r)return {};std::vector<int> parts=r->parts;
 std::stable_sort(parts.begin(),parts.end(),[](int a,int b){return artifactSlots(a).size()<artifactSlots(b).size();});
 std::map<std::string,int> slots;
 std::function<bool(size_t)> assign=[&](size_t i){
  if(i==parts.size())return slots.contains(primary);
  auto candidates=artifactSlots(parts[i]);auto at=std::find(candidates.begin(),candidates.end(),primary);if(at!=candidates.end())std::rotate(candidates.begin(),at,at+1);
  for(const auto&slot:candidates){
   if(slots.contains(slot)||h.comboLocks.contains(slot)||(slot!=primary&&h.equipmentSlots.contains(slot)&&h.equipmentSlots.at(slot)>=0))continue;
   slots[slot]=parts[i];if(assign(i+1))return true;slots.erase(slot);
  }return false;
 };
 return assign(0)?slots:std::map<std::string,int>{};
}
}
std::vector<int> artifactParts(int id){auto*r=recipe(id);return r?r->parts:std::vector<int>{};}
std::vector<int> equippedArtifacts(const Hero&h){auto ids=h.wornArtifacts;for(int id:h.wornArtifacts)if(auto*r=recipe(id))ids.insert(ids.end(),r->parts.begin(),r->parts.end());return ids;}
bool wearsArtifact(const Hero&h,int id){auto ids=equippedArtifacts(h);return std::find(ids.begin(),ids.end(),id)!=ids.end();}
bool Game::reserveArtifactSlots(Hero&h,int id,const std::string&primary){if(!recipe(id))return true;auto plan=reservation(h,id,primary);if(plan.empty())return false;for(auto[slot,part]:plan)if(slot!=primary)h.comboLocks[slot]=id;return true;}
bool Game::artifactSlotLocked(const std::string&slot)const{return hero.comboLocks.contains(slot);}
std::vector<int> Game::assemblableArtifacts(int component)const{
 std::vector<int> out;if(battle||report||neutralOffer||result)return out;
 for(const auto&r:Recipes){if(component>=0&&std::find(r.parts.begin(),r.parts.end(),component)==r.parts.end())continue;auto available=hero.wornArtifacts;bool all=true;
  for(int part:r.parts){auto i=std::find(available.begin(),available.end(),part);if(i==available.end()){all=false;break;}available.erase(i);}if(all)out.push_back(r.id);
 }return out;
}
std::string Game::assembleArtifact(int id){
 auto possible=assemblableArtifacts();if(std::find(possible.begin(),possible.end(),id)==possible.end())return "请先装备这套组合宝物的全部部件。";
 const auto&r=*recipe(id);Hero next=hero;std::map<std::string,int> slots;
 for(int part:r.parts){auto at=std::find_if(next.equipmentSlots.begin(),next.equipmentSlots.end(),[&](auto&entry){return entry.second==part;});if(at==next.equipmentSlots.end())return "部件装备位置不完整。";slots[at->first]=part;at->second=-1;eraseOne(next.wornArtifacts,part);eraseOne(next.artifactIds,part);removeName(next,part);}
 std::string primary;for(const auto&slot:artifactSlots(id))if(slots.contains(slot)){primary=slot;break;}if(primary.empty())return "无法放置组合宝物。";
 next.artifactIds.push_back(id);next.artifacts.push_back(artifactName(id));next.wornArtifacts.push_back(id);next.equipmentSlots[primary]=id;
 for(const auto&[slot,part]:slots)if(slot!=primary)next.comboLocks[slot]=id;
 hero=std::move(next);equipmentSpellbook();hero.maxMovement=movementAllowance();hero.movement=std::min(hero.movement,hero.maxMovement);storeHero();return "已组合："+artifactName(id)+"。";
}
std::string Game::disassembleArtifact(int id){
 if(battle||report||neutralOffer||result)return "当前不能拆卸组合宝物。";auto*r=recipe(id);if(!r||std::find(hero.artifactIds.begin(),hero.artifactIds.end(),id)==hero.artifactIds.end())return "没有这件组合宝物。";
 auto worn=std::find(hero.wornArtifacts.begin(),hero.wornArtifacts.end(),id);bool equipped=worn!=hero.wornArtifacts.end();std::string primary;
 if(equipped)for(auto&[slot,item]:hero.equipmentSlots)if(item==id){primary=slot;item=-1;break;}
 std::set<std::string> oldSlots;for(auto i=hero.comboLocks.begin();i!=hero.comboLocks.end();)if(i->second==id){oldSlots.insert(i->first);i=hero.comboLocks.erase(i);}else ++i;
 if(equipped)eraseOne(hero.wornArtifacts,id);eraseOne(hero.artifactIds,id);removeName(hero,id);
 for(int part:r->parts){hero.artifactIds.push_back(part);hero.artifacts.push_back(artifactName(part));}
 if(equipped&&!primary.empty()){
  // Reserve the original combination footprint, then restore each real part.
  Hero footprint=hero;for(auto&[slot,item]:footprint.equipmentSlots)if(slot!=primary&&!oldSlots.contains(slot)&&item<0)item=0;
  auto plan=reservation(footprint,id,primary);for(auto[slot,part]:plan){hero.equipmentSlots[slot]=part;hero.wornArtifacts.push_back(part);}
 }
 equipmentSpellbook();hero.maxMovement=movementAllowance();hero.movement=std::min(hero.movement,hero.maxMovement);storeHero();return "已拆卸："+artifactName(id)+"，所有部件均已归还。";
}
void Game::repairArtifactLocks(Hero&h){
 h.comboLocks.clear();auto worn=h.wornArtifacts;for(int id:worn)if(recipe(id)){
  auto at=std::find_if(h.equipmentSlots.begin(),h.equipmentSlots.end(),[&](auto&v){return v.second==id;});
  auto plan=at==h.equipmentSlots.end()?std::map<std::string,int>{}:reservation(h,id,at->first);
  if(plan.empty()){eraseOne(h.wornArtifacts,id);if(at!=h.equipmentSlots.end())at->second=-1;continue;}
  for(auto[slot,part]:plan)if(slot!=at->first)h.comboLocks[slot]=id;
 }
 if(wearsArtifact(h,135))h.spellbook=true;
}
bool validArtifactLocks(const Hero&h){
 if(h.comboLocks.size()>14)return false;
 for(const auto&[slot,id]:h.comboLocks)if(!recipe(id)||std::find(h.wornArtifacts.begin(),h.wornArtifacts.end(),id)==h.wornArtifacts.end()||(h.equipmentSlots.contains(slot)&&h.equipmentSlots.at(slot)>=0))return false;
 for(int id:h.wornArtifacts)if(const auto*r=recipe(id)){
  auto at=std::find_if(h.equipmentSlots.begin(),h.equipmentSlots.end(),[&](auto&v){return v.second==id;});if(at==h.equipmentSlots.end())return false;
  Hero footprint=h;size_t count=0;
  for(auto i=footprint.comboLocks.begin();i!=footprint.comboLocks.end();)if(i->second==id){++count;i=footprint.comboLocks.erase(i);}else ++i;
  if(count+1!=r->parts.size())return false;
  for(auto&[slot,item]:footprint.equipmentSlots)if(slot!=at->first&&!h.comboLocks.contains(slot)&&item<0)item=0;
  // A saved lock must form exactly one legal slot for every physical part.
  auto plan=reservation(footprint,id,at->first);if(plan.size()!=r->parts.size())return false;
  for(auto[slot,part]:plan)if(slot!=at->first&&(!h.comboLocks.contains(slot)||h.comboLocks.at(slot)!=id))return false;
 }
 return true;
}
bool heroKnowsSpell(const Hero&h,int spell){return spell>=0&&spell<SpellCount&&(h.spells[spell]||(wearsArtifact(h,135)&&spell==TitanBolt)||(wearsArtifact(h,123)&&(spell==SummonBoat||spell==ScuttleBoat)));}
std::array<bool,SpellCount> heroKnownSpells(const Hero&h){auto known=h.spells;if(wearsArtifact(h,135))known[TitanBolt]=true;if(wearsArtifact(h,123))known[SummonBoat]=known[ScuttleBoat]=true;return known;}
void Game::equipmentSpellbook(){if(wearsArtifact(hero,135))hero.spellbook=true;}

int Game::artifactDuration(const Hero&h)const{int n=0;for(int id:equippedArtifacts(h))if(id>=76&&id<=78)n+=id-75;else if(id==139)n+=50;return n;}
int Game::artifactHealth(const Hero&h,int unit)const{int n=Units[unit].hp;for(int id:equippedArtifacts(h))if(id==94||id==95)++n;else if(id==96)n+=2;if(wearsArtifact(h,131)&&Units[unit].living)n+=Units[unit].hp/4;return n;}
Resources Game::artifactIncome(const Hero&h)const{Resources n{};for(int id:equippedArtifacts(h))switch(id){case 109:++n[Crystal];break;case 110:++n[Gems];break;case 111:++n[Mercury];break;case 112:++n[Ore];break;case 113:++n[Sulfur];break;case 114:++n[Wood];break;case 115:n[Gold]+=1000;break;case 116:n[Gold]+=750;break;case 117:n[Gold]+=500;break;case 140:for(int r=Mercury;r<ResourceCount;++r)n[r]+=4;break;}return n;}
int Game::artifactGrowth(int owner)const{for(const auto&[id,h]:heroes)if(h.owner==owner&&wearsArtifact(id==activeHeroId?hero:h.value,133))return 50;return !imported()&&wearsArtifact(hero,133)?50:0;}
int Game::manaRecovery(const Hero&h)const{if(wearsArtifact(h,138))return 1000000;int n=1+int(secondaryEffect(h,Mysticism,h.skills[Mysticism]));for(int id:equippedArtifacts(h))if(id>=73&&id<=75)n+=id-72;return n;}
}
