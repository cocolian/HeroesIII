#include "h3/renderer.hpp"
#include <algorithm>

namespace h3 {
namespace {constexpr SDL_Color GoldColor{236,213,130,255},Muted{189,171,135,255};}
void Desktop::hero(){
 adventure();buttons_.clear();
 if(!originalImage("heroscr4.pcx",{64,7,672,586}))art("references/hero-original.png",{0,0,800,600},{},false);
 const auto&h=game.hero;
 heroPortrait(game.hero,{82,27,58,64});button({82,27,58,64},"",[]{},true,true,heroSpecialtyDescription(game.hero));
 centerText(h.name,{145,28,218,38},22,GoldColor);centerText(std::to_string(h.level)+" 级 英雄",{145,65,218,25},12);
 const char*labels[]={"攻击","防御","力量","知识"};const int values[]={game.primarySkill(0),game.primarySkill(1),game.primarySkill(2),game.primarySkill(3)};
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
  text(SkillLevels[h.skills[skill]],x+49,y+5,10,Muted);text(SkillNames[skill],x+49,y+24,12,GoldColor);button({x,y,150,44},"",[]{},true,true,skillDescription(skill,h.skills[skill]));slot++;
 }
 text("行动力 "+std::to_string(h.movement)+" / "+std::to_string(h.maxMovement),390,417,12,Muted);
 const std::pair<const char*,Point> equipment[]={
  {"head",{442,21}},{"neck",{442,69}},{"torso",{442,121}},{"feet",{442,303}},
  {"shoulders",{499,21}},{"right_hand",{315,133}},{"left_hand",{544,175}},
  {"right_ring",{330,185}},{"left_ring",{499,232}},
  {"misc1",{315,60}},{"misc2",{365,60}},{"misc3",{544,67}},{"misc4",{544,114}},{"misc5",{544,286}}
 };
 for(const auto&[name,p]:equipment){SDL_Rect rect{64+p.x,7+p.y,44,44};auto at=h.equipmentSlots.find(name);int id=at==h.equipmentSlots.end()?-1:at->second;int lock=h.comboLocks.contains(name)?h.comboLocks.at(name):-1;if(lock>=0){frame("artifact.def",0,145,rect);}else if(id>=0)frame("artifact.def",0,id,rect);button(rect,"",[this,slot=std::string(name)]{say(heroArtifact_>=0?game.equipArtifact(heroArtifact_,slot):game.unequipArtifact(slot));heroArtifact_=-1;persist();},lock<0,true,lock>=0?artifactName(lock)+"占用此栏位；右键点击可拆卸":id>=0?artifactName(id)+"：点击卸下；右键组合或拆卸；选中背包宝物后点击此处替换":"空装备栏：先选背包中的宝物，再点击合适栏位装备");}
 auto backpack=h.artifactIds;for(int worn:h.wornArtifacts){auto at=std::find(backpack.begin(),backpack.end(),worn);if(at!=backpack.end())backpack.erase(at);}std::erase_if(backpack,[](int id){return id<7&&id!=2;});int pages=std::max(1,(int(backpack.size())+4)/5);heroArtifactPage_=std::clamp(heroArtifactPage_,0,pages-1);
 for(int i=0;i<5;++i){int index=heroArtifactPage_*5+i;if(index>=int(backpack.size()))break;int id=backpack[index];SDL_Rect rect{400+i*46,363,44,44};frame("artifact.def",0,id,rect);if(heroArtifact_==id)outline(rect,GoldColor);button(rect,"",[this,id]{if(id==2)say("携带圣杯进入己方城镇，即可建造圣杯建筑。");else heroArtifact_=id;},true,true,artifactName(id)+(id==2?"：携带进入己方城镇后建造圣杯建筑":"：选择后点击装备栏"));}
 if(pages>1){button({379,367,18,32},"◀",[this]{--heroArtifactPage_;},heroArtifactPage_>0);button({634,367,18,32},"▶",[this]{++heroArtifactPage_;},heroArtifactPage_+1<pages);}


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
