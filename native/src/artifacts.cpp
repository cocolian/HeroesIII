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
int Game::primarySkill(int stat)const{const int base[]={hero.attack,hero.defense,hero.power,hero.knowledge};if(stat<0||stat>3)return 0;int result=base[stat];for(int item:hero.wornArtifacts)if(auto*r=rule(item))result+=r->primary[stat];return std::clamp(result,0,1000);}
int Game::artifactLandMovement()const{if(hero.onBoat)return 0;int result=0;for(int item:hero.wornArtifacts)if(auto*r=rule(item))result+=r->movement;return result;}
bool Game::artifactSpellImmune(int spell)const{if(spell<0||spell>=SpellCount)return false;for(int item:hero.wornArtifacts)if(auto*r=rule(item);r&&std::find(r->immuneSpells.begin(),r->immuneSpells.end(),Spells[spell].originalId)!=r->immuneSpells.end())return true;return false;}
void Game::equipPickedArtifact(int id){
 // Automatically occupy an empty basic equipment slot. Replacing occupied
 // slots and combination artifacts still require the future equipment UI.
 if(id<0||id>=int(std::size(Catalog)))return;std::string slot;
 for(const auto&candidate:Catalog[id].slots)if(!hero.equipmentSlots.contains(candidate)||hero.equipmentSlots[candidate]<0){slot=candidate;break;}
 if(slot.empty())return;
 hero.equipmentSlots[slot]=id;hero.wornArtifacts.push_back(id);
 hero.maxMovement=movementAllowance();hero.movement=std::min(hero.movement,hero.maxMovement);
}
}
