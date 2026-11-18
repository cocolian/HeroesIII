#include "h3/game.hpp"
#include "h3/localization.hpp"
#include <algorithm>

namespace h3 {
namespace {
struct ArtifactRule {int id;std::array<int,4> primary;int movement;std::vector<int> immuneSpells;};
const ArtifactRule Rules[]={
#include "../generated/artifact_rules.inc"
};
struct ArtifactInfo {int id;const char*name;std::string rarity;std::vector<std::string> slots;};
const ArtifactInfo Catalog[]={
#include "../generated/artifact_catalog.inc"
};
const ArtifactRule*rule(int id){auto i=std::lower_bound(std::begin(Rules),std::end(Rules),id,[](const auto&r,int id){return r.id<id;});return i==std::end(Rules)||i->id!=id?nullptr:i;}
}
std::string artifactName(int id){return id>=0&&id<int(std::size(Catalog))?chineseMapText({},Catalog[id].name):"宝物 #"+std::to_string(id);}
int Game::rollArtifact(int sourceClass){std::vector<int>pool;const char*rarities[]={"","T","N","J","R"};int tier=std::clamp(sourceClass-65,0,4);for(const auto&a:Catalog)if(a.id>=7&&a.id<=127&&(tier==0?a.rarity!="S":a.rarity==rarities[tier]))pool.push_back(a.id);return pool.empty()?7:pool[int(random()*pool.size())];}
int heroPrimary(const Hero&hero,int stat){const int base[]={hero.attack,hero.defense,hero.power,hero.knowledge};if(stat<0||stat>3)return 0;int result=base[stat];for(int item:hero.wornArtifacts)if(auto*r=rule(item))result+=r->primary[stat];return std::clamp(result,0,1000);}
int Game::primarySkill(int stat)const{return heroPrimary(hero,stat);}
int Game::artifactLandMovement()const{if(hero.onBoat)return 0;int result=0;for(int item:hero.wornArtifacts)if(auto*r=rule(item))result+=r->movement;return result;}
bool Game::artifactSpellImmune(int spell)const{if(spell<0||spell>=SpellCount)return false;for(int item:hero.wornArtifacts)if(auto*r=rule(item);r&&std::find(r->immuneSpells.begin(),r->immuneSpells.end(),Spells[spell].originalId)!=r->immuneSpells.end())return true;return false;}
std::string Game::unequipArtifact(const std::string&slot){
 if(battle||report||neutralOffer||result)return "当前不能调整装备。";auto at=hero.equipmentSlots.find(slot);if(at==hero.equipmentSlots.end()||at->second<0)return "此栏位没有宝物。";
 int id=at->second;auto worn=std::find(hero.wornArtifacts.begin(),hero.wornArtifacts.end(),id);if(worn==hero.wornArtifacts.end())return "装备状态不一致。";hero.wornArtifacts.erase(worn);at->second=-1;hero.maxMovement=movementAllowance();hero.movement=std::min(hero.movement,hero.maxMovement);storeHero();return "已卸下"+artifactName(id)+"。";
}
std::string Game::equipArtifact(int id,const std::string&slot){
 if(battle||report||neutralOffer||result)return "当前不能调整装备。";if(id<0||id>=int(std::size(Catalog))||std::count(hero.artifactIds.begin(),hero.artifactIds.end(),id)<=std::count(hero.wornArtifacts.begin(),hero.wornArtifacts.end(),id))return "背包中没有这件宝物。";
 if(std::find(Catalog[id].slots.begin(),Catalog[id].slots.end(),slot)==Catalog[id].slots.end())return "这件宝物不能装备到此位置。";
 if(hero.equipmentSlots.contains(slot)&&hero.equipmentSlots[slot]>=0)unequipArtifact(slot);hero.equipmentSlots[slot]=id;hero.wornArtifacts.push_back(id);hero.maxMovement=movementAllowance();hero.movement=std::min(hero.movement,hero.maxMovement);storeHero();return "已装备"+artifactName(id)+"。";
}
std::string Game::transferHeroArtifact(int other,int id){
 if(battle||report||neutralOffer||result)return "当前不能交换宝物。";auto at=heroes.find(other);if(at==heroes.end()||at->second.owner!=currentOwner()||at->second.level!=mapLevel||std::max(std::abs(hero.pos.x-at->second.value.pos.x),std::abs(hero.pos.y-at->second.value.pos.y))>1)return "英雄需要相邻会面。";
 if(id<7||id>=int(std::size(Catalog)))return "战争机器、魔法书不能交换。";auto item=std::find(hero.artifactIds.begin(),hero.artifactIds.end(),id);if(item==hero.artifactIds.end())return "英雄没有这件宝物。";
 auto worn=std::find(hero.wornArtifacts.begin(),hero.wornArtifacts.end(),id);if(worn!=hero.wornArtifacts.end())for(const auto&[slot,value]:hero.equipmentSlots)if(value==id){unequipArtifact(slot);break;}
 hero.artifactIds.erase(item);auto name=std::find(hero.artifacts.begin(),hero.artifacts.end(),artifactName(id));if(name!=hero.artifacts.end())hero.artifacts.erase(name);at->second.value.artifactIds.push_back(id);at->second.value.artifacts.push_back(artifactName(id));hero.maxMovement=movementAllowance();hero.movement=std::min(hero.movement,hero.maxMovement);storeHero();return "已将"+artifactName(id)+"交给"+at->second.value.name+"。";
}
void Game::equipPickedArtifact(int id){
 // New pickups use a compatible empty slot; the hero window supports replacement.
 if(id<0||id>=int(std::size(Catalog))||std::count(hero.artifactIds.begin(),hero.artifactIds.end(),id)<=std::count(hero.wornArtifacts.begin(),hero.wornArtifacts.end(),id))return;std::string slot;
 for(const auto&candidate:Catalog[id].slots)if(!hero.equipmentSlots.contains(candidate)||hero.equipmentSlots[candidate]<0){slot=candidate;break;}
 if(slot.empty())return;
 hero.equipmentSlots[slot]=id;hero.wornArtifacts.push_back(id);
 hero.maxMovement=movementAllowance();hero.movement=std::min(hero.movement,hero.maxMovement);
}
}

namespace h3 {
void Game::optimizeEquipment(){
 if(battle||report||neutralOffer||result)return;
 for(const auto&[slot,id]:hero.equipmentSlots)if(id==125)unequipArtifact(slot);
 auto score=[&](int id){if(id<0)return -1;int n=0;if(auto*r=rule(id)){for(int i=0;i<4;++i)n+=r->primary[i]*(i<2?3:2);n+=r->movement/50+int(r->immuneSpells.size())*4;}if(id==54||id==55||id==56)n+=hero.skills[Necromancy]*(id==54?5:id==55?10:15);if(id==130)n+=20*std::max(1,hero.skills[Necromancy]);return n;};
 for(int pass=0;pass<2;++pass)for(int id:hero.artifactIds){if(id==125||id<7||id>=int(std::size(Catalog))||std::count(hero.artifactIds.begin(),hero.artifactIds.end(),id)<=std::count(hero.wornArtifacts.begin(),hero.wornArtifacts.end(),id))continue;for(const auto&slot:Catalog[id].slots){int old=hero.equipmentSlots.contains(slot)?hero.equipmentSlots.at(slot):-1;if(score(id)>score(old)){equipArtifact(id,slot);break;}}}
}
}
