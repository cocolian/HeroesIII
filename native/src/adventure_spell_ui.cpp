#include "h3/renderer.hpp"
#include <algorithm>
#include <cmath>
namespace h3 {
void Desktop::adventureSpellPanel(){
 adventure();buttons_.clear();mapDialog({128,70,544,460});centerText("时空之门 · 选择城镇",{150,85,500,40},22,{236,213,130,255});auto towns=game.portalTowns();int pages=std::max(1,(int(towns.size())+5)/6);locationPage_=std::clamp(locationPage_,0,pages-1);
 originalButton({370,475,64,32},"icancel.def",0,[this]{scene=Scene::Adventure;},true,-1,"取消时空之门，不消耗魔法值");
 for(int i=0;i<6&&locationPage_*6+i<int(towns.size());++i){int id=towns[locationPage_*6+i];auto*o=game.objectById(id);const auto&t=game.settlements.at(id);bool occupied=std::any_of(game.heroes.begin(),game.heroes.end(),[&](auto&h){return h.first!=game.activeHeroId&&h.second.level==o->level&&h.second.value.pos==o->pos;});button({162,143+i*48,476,38},o->name+" · "+Factions[t.faction].name+(occupied?"（城门已有英雄）":""),[this,id]{auto error=game.castAdventure(TownPortal,{-1,-1},id);say(error);if(error.empty()){scene=Scene::Adventure;camX_=game.hero.pos.x+.5f;camY_=game.hero.pos.y+.5f;route_.clear();persist();settle();}},!occupied);}
 if(pages>1){button({162,475,90,30},"上一页",[this]{--locationPage_;},locationPage_>0);button({548,475,90,30},"下一页",[this]{++locationPage_;},locationPage_+1<pages);}
}
void Desktop::worldSurvey(){
 adventure();buttons_.clear();mapDialog({55,45,690,510});centerText(Spells[worldSurveySpell_].name,{75,58,650,35},23,{236,213,130,255});int mastery=game.hero.adventureSpellMastery[worldSurveySpell_];bool terrain=worldSurveySpell_==ViewEarth&&mastery==3;float size=std::min(620.f/game.mapWidth,365.f/game.mapHeight);int ox=int(400-game.mapWidth*size/2),oy=110;
 const SDL_Color colors[]={{49,79,25,255},{92,70,46,255},{167,139,83,255},{37,65,106,255},{190,199,209,255},{50,65,42,255},{116,86,54,255},{73,65,60,255},{91,37,22,255},{17,15,14,255}};
 for(int y=0;y<game.mapHeight;++y)for(int x=0;x<game.mapWidth;++x){auto*t=game.tile({x,y});box({ox+int(x*size),oy+int(y*size),int(std::ceil(size)),int(std::ceil(size))},terrain||t->seen?colors[int(t->terrain)]:SDL_Color{0,0,0,255});}
 for(const auto&o:game.objects)if(o.level==game.mapLevel){bool show=worldSurveySpell_==ViewEarth?(o.type==ObjectType::Resource||(mastery>=2&&o.type==ObjectType::Mine)):(o.type==ObjectType::Artifact||(mastery>=2&&o.type==ObjectType::EnemyHero)||(mastery==3&&o.type==ObjectType::Town));if(show)box({ox+int(o.pos.x*size)-1,oy+int(o.pos.y*size)-1,std::max(3,int(size)),std::max(3,int(size))},o.type==ObjectType::Mine?SDL_Color{220,220,220,255}:o.type==ObjectType::EnemyHero?SDL_Color{230,70,60,255}:SDL_Color{245,218,80,255});}
 centerText(worldSurveySpell_==ViewEarth?"金色：资源  白色：矿山":"金色：宝物与城镇  红色：英雄",{80,480,640,24},12,{236,213,130,255});originalButton({370,510,64,32},"iokay.def",0,[this]{scene=Scene::Adventure;},true,-1,"关闭透视地图；探索迷雾保持原有状态");
}
}
