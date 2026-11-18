#include "h3/renderer.hpp"
#include <algorithm>

namespace h3 {
namespace {constexpr SDL_Color GoldColor{236,213,130,255},Muted{189,171,135,255};}
void Desktop::hero(){
 adventure();buttons_.clear();
 if(!originalImage("heroscr4.pcx",{64,7,672,586}))art("references/hero-original.png",{0,0,800,600},{},false);
 const auto&h=game.hero;
 heroPortrait(game.hero,{82,27,58,64});
 centerText(h.name,{145,28,218,38},22,GoldColor);centerText(std::to_string(h.level)+" 级 英雄",{145,65,218,25},12);
 const char*labels[]={"攻击","防御","力量","知识"};const int values[]={h.attack,h.defense,h.power,h.knowledge};
 constexpr int primaryFrames[]={0,1,2,5};
 for(int i=0;i<4;i++){centerText(labels[i],{82+i*70,94,73,20},10,GoldColor);frame("pskil42.def",0,primaryFrames[i],{96+i*70,119,42,42});centerText(std::to_string(values[i]),{82+i*70,163,73,21},12);}
 frame("pskil42.def",0,4,{83,243,42,42});frame("pskil42.def",0,3,{239,243,42,42});
 text("经验 "+std::to_string(h.xp),131,243,11);text("下级 "+std::to_string(Game::experienceForLevel(h.level+1)),131,263,10);
 text("魔法值",287,243,11);text(std::to_string(h.mana)+" / "+std::to_string(game.maxMana()),287,263,11);
 int morale=game.armyMorale(),luck=game.armyLuck();frame("imrl42.def",0,3+morale,{96,190,42,38});frame("ilck42.def",0,3+luck,{240,190,42,38});
 centerText("士气 "+std::string(morale>0?"+":"")+std::to_string(morale),{142,194,76,25},11);centerText("幸运 "+std::to_string(luck),{285,194,76,25},11);
 // Classic secondary-skill indices are factual data from the pinned skill
 // catalog. Each has three mastery frames after three empty placeholders.
 const auto&originalSkill=OriginalSkillIcons;
 int slot=0;for(int skill=0;skill<SkillCount;skill++)if(h.skills[skill]){
  int x=82+(slot%2)*156,y=290+(slot/2)*48;
  frame("secskill.def",0,3+originalSkill[skill]*3+std::clamp(h.skills[skill]-1,0,2),{x,y,44,44});
  text(SkillLevels[h.skills[skill]],x+49,y+5,10,Muted);text(SkillNames[skill],x+49,y+24,12,GoldColor);slot++;
 }
 text("行动力 "+std::to_string(h.movement)+" / "+std::to_string(h.maxMovement),390,417,12,Muted);
 int y=366;for(const auto&artifact:h.artifacts){text(artifact,390,y,11,GoldColor);y+=17;}
 heroPortrait(game.hero,{675,94,48,32});
 for(int i=0;i<ArmySlots;i++){const auto&unit=h.army[i];if(unit.empty())continue;portrait(unit.unit,{80+i*65,492,57,63});text(std::to_string(unit.count),84+i*65,541,13);}
 button({380,435,122,38},"部队管理",[this]{sourceSlot_=-1;armyReturn_=Scene::Hero;scene=Scene::Army;});
 button({513,435,140,38},"魔法书",[this]{openSpellbook();});
 for(int mode=0;mode<2;mode++){
  SDL_Rect rect{545,491+mode*36,54,32};bool enabled=!game.battle&&!game.report&&!game.result&&game.hero.levelChoices.empty();bool selected=game.hero.tightFormation==bool(mode);
  bool hover=mouseX_>=rect.x&&mouseY_>=rect.y&&mouseX_<rect.x+rect.w&&mouseY_<rect.y+rect.h;
  frame(mode?"hsbtns7.def":"hsbtns6.def",0,!enabled?2:selected?1:hover?3:0,rect);
  buttons_.push_back({rect,[this,mode]{game.hero.tightFormation=mode;persist();},enabled,{},mode?"紧密阵型：部队集中在战场中央；下一场战斗生效":"松散阵型：部队沿我方边线展开；下一场战斗生效"});
 }
 originalButton({673,523,52,36},"hsbtns.def",0,[this]{scene=Scene::Adventure;focus_=-1;},true,-1,"关闭英雄窗口，返回冒险地图（Enter / Esc）");
}
}
