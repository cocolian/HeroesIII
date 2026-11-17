#include "h3/game.hpp"
#include <algorithm>

namespace h3 {
namespace {
constexpr std::array<int,5> GuildBuildings={MageGuild,MageGuild2,MageGuild3,MageGuild4,MageGuild5};
}

int Game::mageGuildLevel()const {
 for(int level=townDef().maxMageGuild;level>0;--level)
  if(town.built[GuildBuildings[level-1]])return level;
 return 0;
}

int Game::learnableSpellLevel()const {return 2+hero.skills[Wisdom];}

void Game::generateGuild() {
 // Guilds are fixed when the map is created, independent of construction order.
 // This local stream does not change the saved combat/adventure random stream.
 uint32_t state=seed^0x9e3779b9u^(uint32_t(town.faction+1)*0x85ebca6bu)
  ^(uint32_t(town.pos.x+town.pos.y*mapWidth)*0xc2b2ae35u);
 auto draw=[&](int total){state=state*1664525u+1013904223u;return int((uint64_t(state)*total)>>32);};
 town.guild={};
 for(int level=1;level<=townDef().maxMageGuild;level++){
  std::vector<int> pool;
  for(int spell=0;spell<SpellCount;spell++)if(Spells[spell].level==level&&!Spells[spell].special
    &&!bannedSpells[spell]&&Spells[spell].guildWeights[town.faction]>0)pool.push_back(spell);
  std::sort(pool.begin(),pool.end(),[](int a,int b){return Spells[a].originalId<Spells[b].originalId;});
  int slots=6-level+(town.faction==2?1:0); // Reserve the Tower library's extra slot.
  for(int slot=0;slot<slots&&!pool.empty();slot++){
   int total=0;for(int spell:pool)total+=Spells[spell].guildWeights[town.faction];
   int choice=draw(total);auto selected=pool.begin();
   for(;selected!=pool.end();++selected){choice-=Spells[*selected].guildWeights[town.faction];if(choice<0)break;}
   town.guild[level-1].push_back(*selected);pool.erase(selected);
  }
 }
}

std::vector<int> Game::guildSpells(int level)const {
 if(level<1||level>mageGuildLevel())return {};
 const auto&pool=town.guild[level-1];
 int slots=6-level+(town.faction==2&&town.built[Special3]?1:0);
 std::vector<int> spells(pool.begin(),pool.begin()+std::min<size_t>(pool.size(),slots));
 std::sort(spells.begin(),spells.end(),[](int a,int b){return Spells[a].originalId<Spells[b].originalId;});
 return spells;
}

void Game::learnTownSpells() {
 if(!visiting()||!hero.spellbook)return;
 for(int level=1;level<=std::min(mageGuildLevel(),learnableSpellLevel());level++)
  for(int spell:guildSpells(level))hero.spells[spell]=true;
}

std::string Game::buySpellbook() {
 if(battle||result||!hero.levelChoices.empty())return "请先完成当前战斗或升级";
 if(!visiting()||mageGuildLevel()==0)return "英雄必须进入建有魔法行会的城镇";
 if(hero.spellbook)return "英雄已经拥有魔法书";
 if(resources[Gold]<500)return "购买魔法书需要 500 金币";
 resources[Gold]-=500;hero.spellbook=true;learnTownSpells();
 message("花费 500 金币购买魔法书，学会行会中符合智慧术等级的法术。");
 return log.back();
}
}
