#include "h3/renderer.hpp"
#include "h3/battle_layout.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>

namespace h3 {
bool Desktop::combatBodyContains(const CombatStack&unit,Point point){
 if(unit.hp<=0||point.x<0||point.y<0||point.x>=800||point.y>=556)return false;
 auto visual=combatVisual(unit);if(!visual)return false;
 auto name=visual->animation;for(auto&c:name)c=char(std::tolower(static_cast<unsigned char>(c)));
 const auto&animation=demo_["def"][name];const auto&frames=animation["groups"][std::to_string(visual->group)];
 const auto&f=frames[visual->index<0?int(frames.size())-1:visual->index%frames.size()];
 int x=point.x-visual->canvas.x,y=point.y-visual->canvas.y;
 if(visual->flip)x=visual->canvas.w-1-x;
 x-=f["offset"][0].get<int>();y-=f["offset"][1].get<int>();
 int w=f["rect"][2],h=f["rect"][3];if(x<0||y<0||x>=w||y>=h)return false;
 auto file=animation["pages"][f["page"].get<int>()]["file"].get<std::string>();
 int sx=f["rect"][0],sy=f["rect"][1];auto key="#combat-hit#"+file+":"+std::to_string(sx)+":"+std::to_string(sy)+":"+std::to_string(w)+":"+std::to_string(h);
 auto it=masks_.find(key);if(it==masks_.end()){
  auto*surface=readSurface(file,false,false);if(!surface)return false;
  PixelMask mask{w,h,std::vector<uint8_t>(size_t(w)*h)};
  for(int row=0;row<h;row++)for(int col=0;col<w;col++)mask.pixels[size_t(row)*w+col]=static_cast<Uint8*>(surface->pixels)[(sy+row)*surface->pitch+(sx+col)*4+3]>=192;
  SDL_FreeSurface(surface);it=masks_.emplace(key,std::move(mask)).first;
 }
 return it->second.contains(x,y);
}
const CombatStack* Desktop::battleTargetAt(Point point){
 if(!game.battle||battleMotion_.active()||point.x<0||point.y<0||point.x>=800||point.y>=556)return nullptr;
 std::vector<const CombatStack*> order;for(const auto&u:game.battle->units)if(u.hp>0)order.push_back(&u);
 std::stable_sort(order.begin(),order.end(),[](const auto*a,const auto*b){return a->y!=b->y?a->y<b->y:a->x<b->x;});
 // Match the painter order: a foreground creature owns its visible body.
 for(auto it=order.rbegin();it!=order.rend();++it)if(combatBodyContains(**it,point))return *it;
 if(auto cell=battleHexAt(point))return game.combatAt(*cell);return nullptr;
}
void Desktop::battleCellAction(Point cell){
 if(battleMotion_.active()||autoCombat_||!game.current()||game.current()->team!=0)return;
 std::string message;
 if(pendingSpell_==Sacrifice&&sacrificeTarget_<0){
  auto targets=game.spellTargets(Sacrifice,cell);if(targets.empty())message="请选择可复生的友军";else{sacrificeTarget_=targets.front();message="请选择另一支活物友军作为牺牲。";}
 }else if(pendingSpell_==Sacrifice){
  auto target=std::find_if(game.battle->units.begin(),game.battle->units.end(),[this](const auto&unit){return unit.id==sacrificeTarget_;});
  if(target!=game.battle->units.end())battleAction([&]{return message=game.cast(Sacrifice,{target->x,target->y},cell);});else message="复生目标已不存在";
  if(message.empty())pendingSpell_=sacrificeTarget_=-1;
 }else if(pendingSpell_>=0){battleAction([&]{return message=game.cast(pendingSpell_,cell);});if(message.empty())pendingSpell_=-1;}
 else battleAction([&]{auto*target=game.combatAt(cell);return message=game.act(target&&target->team==1?"attack":"move",cell);});
 say(message);persist();settle();
}
void Desktop::battleTargetRenderTests(const std::filesystem::path&directory){
 auto check=[](bool ok,const char*message){if(!ok)throw std::runtime_error(message);};
 int oldW=0,oldH=0;SDL_GetWindowSize(window_,&oldW,&oldH);SDL_SetWindowSize(window_,800,600);auto previous=settings_;
 auto reset=[&](int enemy=Skeleton,int enemies=50){battleMotion_.clear();game=Game::create();game.hero.army={Stack{Pikeman,20}};game.objectById(15)->army={Stack{enemy,enemies}};game.startBattle(15);game.battle->units[0].x=5;game.battle->units[0].y=5;game.battle->units[1].x=6;game.battle->units[1].y=5;game.battle->queue={0,100};game.takeEvents();scene=Scene::Battle;fixedAnimationTime_=0;nextAI_=0;autoCombat_=false;pendingSpell_=sacrificeTarget_=-1;focus_=-1;toastUntil_=0;mouseX_=mouseY_=-1000;};
 auto body=[&](int id){const auto&u=*std::find_if(game.battle->units.begin(),game.battle->units.end(),[=](const auto&s){return s.id==id;});auto v=combatVisual(u);check(bool(v),"Body fixture missing original DEF");auto cells=game.footprint(u,{u.x,u.y});
  for(int y=std::max(0,v->canvas.y);y<std::min(556,v->canvas.y+v->canvas.h);y++){
   for(int x=std::max(0,v->canvas.x);x<std::min(800,v->canvas.x+v->canvas.w);x++){
    Point p{x,y};auto tile=battleHexAt(p);if(tile&&std::find(cells.begin(),cells.end(),*tile)!=cells.end())continue;auto*hit=battleTargetAt(p);if(combatBodyContains(u,p)&&hit&&hit->id==id)return p;
   }
  }
  throw std::runtime_error("Fixture has no visible torso outside its hex");};
 auto shot=[&](const std::string&name){toastUntil_=0;render();check(screenshot(directory/(name+".png")),"Battle target screenshot failed");};nlohmann::json modes=nlohmann::json::array();
 for(bool hd:{false,true}){settings_.hdArtwork=hd;reset();render();auto p=body(100);mouseX_=p.x;mouseY_=p.y;auto cursor=combatCursor();check(cursor&&cursor->asset=="crcombat.def"&&cursor->frame>=7&&cursor->frame<=14,"Enemy torso hover did not show a melee sword");auto state=game.toJson();shot(hd?"target-hd-hover":"target-classic-hover");check(game.toJson()==state,"Hover mutated the battle");
  int hp=game.battle->units[1].hp;click(p,false);check(game.battle->units[1].hp<hp&&battleMotion_.active(),"Clicking the enemy torso moved instead of attacking");fixedAnimationTime_=500;tick();shot(hd?"target-hd-hit":"target-classic-hit");modes.push_back({{"hd",hd},{"bodyPoint",{p.x,p.y}},{"enemyHPBefore",hp},{"enemyHPAfter",game.battle->units[1].hp}});
 }
 reset(Cavalier,2);render();auto p=body(100);check(Units[game.battle->units[1].unit].size==2,"Wide target fixture failed");click(p,false);check(game.battle&&game.battle->units[1].hp<200,"Wide mirrored creature body missed the attack");fixedAnimationTime_=500;tick();shot("target-wide-hit");
 reset();game.hero.spellbook=true;game.hero.spells[MagicArrow]=true;pendingSpell_=MagicArrow;render();p=body(100);int hp=game.battle->units[1].hp;click(p,false);check(game.battle->units[1].hp==hp-20&&pendingSpell_==-1,"Spell clicked on a torso used the wrong hex");shot("target-body-spell");
 reset();game.battle->units[1].unit=Pikeman;game.battle->units[1].hp=500;render();p=body(100);check(battleTargetAt(p)->id==100,"Identical creature types selected the wrong stack");
 auto v=combatVisual(game.battle->units[1]);check(!combatBodyContains(game.battle->units[1],{v->canvas.x,v->canvas.y}),"Transparent canvas padding is clickable");
 game.battle->units[1].hp=game.battle->units[1].count=0;check(!combatBodyContains(game.battle->units[1],p),"Dead creature body intercepted a click");
 reset();render();p=body(100);SDL_Event e{};e.type=SDL_MOUSEBUTTONDOWN;e.button.button=SDL_BUTTON_LEFT;e.button.x=p.x;e.button.y=p.y;handleNativeEvent(e,true);e.type=SDL_MOUSEBUTTONUP;handleNativeEvent(e,true);check(game.battle->units[1].hp<300,"Native torso press/release failed");
 std::ofstream(directory/"battle-target-audit.json")<<nlohmann::json{{"modes",modes},{"wideMirroredBody",true},{"bodySpellTarget",true},{"duplicateCreatureIdentity",true},{"transparentPaddingExcluded",true},{"deadBodiesExcluded",true},{"nativePressRelease",true},{"renderUsesSameFrameAndAnchorAsHitTest",true}}.dump(2)<<'\n';
 battleMotion_.clear();game=Game::create();scene=Scene::Town;fixedAnimationTime_=0;settings_=previous;mouseX_=mouseY_=-1;nextAI_=0;SDL_SetWindowSize(window_,oldW,oldH);
}
}
