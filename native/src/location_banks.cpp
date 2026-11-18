#include "h3/locations.hpp"
#include <algorithm>

namespace h3::locations {
void initializeBank(Game&g,Object&o){auto&p=o.properties;if(p.contains("locationBankReady"))return;p["locationBankReady"]=true;
 if(o.visited&&armyCount(o.army)==0){p["locationCleared"]=true;return;}
 if(o.sourceClass==63){std::vector<int>pool;for(int spell=0;spell<SpellCount;++spell)if(Spells[spell].level==5&&!g.bannedSpells[spell]&&!Spells[spell].special)pool.push_back(spell);p["locationSpell"]=pool.empty()?-1:pool[int(g.random()*pool.size())];o.army={Stack{originalUnit(116),40},Stack{originalUnit(117),10},Stack{originalUnit(117),10}};return;}
 auto key=std::to_string(o.sourceClass)+":"+std::to_string(std::max(0,o.sourceSubtype));
 if(!facts()["banks"].contains(key)){p["locationCleared"]=true;return;}
 int roll=int(g.random()*100);const auto&rows=facts()["banks"][key];const nlohmann::json*selected=&rows.back();for(const auto&r:rows)if(roll>=r["min"].get<int>()&&roll<r["max"].get<int>()){selected=&r;break;}
 p["locationLoot"]=*selected;o.army={};int slot=0;for(const auto&s:(*selected)["guards"]){if(slot>=ArmySlots)break;int unit=s["unit"];if(s.value("upgradeChance",0)>int(g.random()*100)&&unit+1<UnitCount&&Units[unit+1].upgraded)++unit;o.army[slot++]={unit,s["count"]};}
 std::vector<int>artifacts;for(const std::string r:(*selected)["artifacts"])artifacts.push_back(g.rollArtifact(r=="TREASURE"?66:r=="MINOR"?67:r=="MAJOR"?68:69));p["locationArtifacts"]=artifacts;
}
std::string bank(Game&g,int id){auto&o=*g.objectById(id);auto&p=o.properties;
 if(p.value("locationCleared",false)){
  if(o.sourceClass==24||o.sourceClass==84||o.sourceClass==85)bonus(g,o,-1,0);
  if(o.sourceClass==63)bonus(g,o,0,-2);
  return o.sourceClass==63?"金字塔已经被搜索过。空荡的墓室使部队幸运 -2，持续到下一场战斗。":o.sourceClass==24||o.sourceClass==84||o.sourceClass==85?"这里已经被洗劫一空。再次搜索使士气 -1，持续到下一场战斗。":"宝屋已经被攻克，没有剩余的战利品。";
 }
 if(armyCount(o.army)>0&&!p.value("locationGuardsDefeated",false))return "此地有强大的守卫。是否发动进攻？击败守卫后才可领取战利品。";
 if(o.sourceClass==63){int spell=p.value("locationSpell",-1);p["locationCleared"]=true;o.visited=true;
  if(spellLearnable(g,spell)&&g.hero.skills[Wisdom]==3){g.hero.spells[spell]=true;return "你在金字塔中学会了"+Spells[spell].name+"。";}
  return "守卫已被击败，但研读金字塔中的五级法术需要魔法书和高级智慧术。墓中的知识已经随探索结束而消失。";
 }
 if(!p.contains("locationLoot"))return "这座宝屋没有剩余的战利品。";
 const auto&loot=p["locationLoot"];Army trial=g.hero.army;
 for(const auto&s:loot["creatures"])if(!merge(trial,s["unit"],s["count"]))return "守卫已被击败，奖励部队仍在等待。请先整理部队栏位，再来领取全部战利品。";
 const char*keys[]={"gold","wood","ore","mercury","sulfur","crystal","gems"};Resources reward{};
 for(int r=0;r<ResourceCount;++r){reward[r]=loot["resources"].value(keys[r],0);g.resources[r]=int(std::min<int64_t>(1000000000,int64_t(g.resources[r])+reward[r]));}
 g.hero.army=trial;std::string text="战利品："+resourcesText(reward);
 for(int artifact:p.value("locationArtifacts",std::vector<int>{})){giveArtifact(g,artifact);text+="\n"+artifactName(artifact);}
 for(const auto&s:loot["creatures"])text+="\n"+Units[s["unit"].get<int>()].name+" × "+std::to_string(s["count"].get<int>());
 p["locationCleared"]=true;o.visited=true;return text+"。";
}
}
namespace h3 {
void Game::setupLocationBattle(int id){if(!battle)return;auto*o=objectById(id);if(!o||!bankLocation(o->sourceClass)||o->sourceClass==63)return;
 auto key=std::to_string(o->sourceClass)+":"+std::to_string(std::max(0,o->sourceSubtype));const auto&data=locations::facts();
 if(!data["bankLayouts"].contains(key))return;auto layout=data["bankLayouts"][key].get<std::string>();if(!data["battleLayouts"].contains(layout))return;
 const auto&positions=data["battleLayouts"][layout];std::array<int,2>rank{};
 for(auto&unit:battle->units){int raw=positions[unit.team?"defenderUnits":"attackerUnits"][rank[unit.team]++];unit.x=std::clamp(raw%17-1,unit.team?0:Units[unit.unit].size-1,unit.team?BattleW-Units[unit.unit].size:BattleW-1);unit.y=raw/17;}
 battle->log.push_back("宝屋战场：我方在中央布阵，守卫从四周迎战。");
}
}
