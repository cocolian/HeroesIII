#include "h3/renderer.hpp"
#include "h3/battle_layout.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>

namespace h3 {
void Desktop::battleHealthRenderTests(const std::filesystem::path&directory){
 auto check=[](bool ok,const char*message){if(!ok)throw std::runtime_error(message);};
 int oldW=0,oldH=0;SDL_GetWindowSize(window_,&oldW,&oldH);SDL_SetWindowSize(window_,800,600);
 const auto previousSettings=settings_;nlohmann::json audit;
 auto reset=[&](int ally,int allies,int enemy,int enemies){
  battleMotion_.clear();game=Game::create();game.hero.army={Stack{ally,allies}};game.objectById(15)->army={Stack{enemy,enemies}};
  game.startBattle(15);game.battle->units[0].x=5;game.battle->units[0].y=5;game.battle->units[1].x=6;game.battle->units[1].y=5;
  game.battle->queue={0,100};game.takeEvents();scene=Scene::Battle;fixedAnimationTime_=0;nextAI_=0;
  autoCombat_=false;pendingSpell_=sacrificeTarget_=-1;toastUntil_=0;mouseX_=mouseY_=-1000;focus_=-1;
 };
 auto shot=[&](const std::string&name){toastUntil_=0;render();check(screenshot(directory/(name+".png")),"Health regression screenshot failed");};
 // Inspect the real rendered gauge independently of text, sprites or logs.
 auto gauge=[&](int id){
  const Battle*view=battleMotion_.view();if(!view&&game.battle)view=&*game.battle;
  check(view!=nullptr,"Health test lost the visible battle");
  const auto&u=*std::find_if(view->units.begin(),view->units.end(),[=](const auto&s){return s.id==id;});
  auto center=battleHexCenter({u.x,u.y});if(auto pose=battleMotion_.pose(id))center={int(std::lround(pose->x)),int(std::lround(pose->y))};
  int left=center.x+(u.team?-52:22);SDL_Rect area{int(std::floor((left+1)*pixelScale_)),int(std::floor((center.y+16)*pixelScale_)),int(std::floor(28*pixelScale_)),1};
  std::vector<Uint8>pixels(size_t(area.w)*4);check(SDL_SetRenderTarget(renderer_,canvas_.texture)==0,"Health canvas unavailable");
  bool read=SDL_RenderReadPixels(renderer_,&area,SDL_PIXELFORMAT_RGBA32,pixels.data(),area.w*4)==0;SDL_SetRenderTarget(renderer_,nullptr);check(read,"Health pixel readback failed");
  int filled=0;for(int x=0;x<area.w;x++){const auto*p=&pixels[size_t(x)*4];if(p[0]>50||p[1]>50||p[2]>50)filled++;}
  return filled;
 };
 for(bool hd:{false,true}){
  settings_.hdArtwork=hd;reset(Pikeman,10,Cavalier,2);render();int full=gauge(100);check(full>0,"Full enemy gauge is empty");
  std::string mode=hd?"hd":"classic";shot("health-"+mode+"-before");
  std::string error;check(game.save(directory/("health-"+mode+"-before.json"),error),"Could not save isolated health fixture");
  int initialHP=game.battle->units[1].hp;click(battleHexCenter({6,5}),false);auto committed=game.toJson();
  render();check(gauge(100)==full,"Health changed before the melee hit frame");
  fixedAnimationTime_=500;tick();render();const auto&hit=battleMotion_.view()->units[1];int hitWidth=gauge(100);
  check(hit.hp<initialHP&&hit.count==2&&hitWidth<full&&hitWidth>0,"Nonlethal attack did not visibly reduce health without a casualty");
  check(!battleMotion_.damageMessage().empty(),"The impact did not report its damage");shot("health-"+mode+"-partial-hit");
  int damage=initialHP-hit.hp;auto beforeCounter=battleMotion_.view()->units[0].hp;
  for(uint64_t time=600;time<=10000&&battleMotion_.retainedView()->units[0].hp==beforeCounter;time+=100){fixedAnimationTime_=time;tick();}
  render();check(battleMotion_.retainedView()->units[0].hp<beforeCounter&&gauge(0)<full,"Retaliation did not reduce friendly health");shot("health-"+mode+"-retaliation");
  fixedAnimationTime_=100000;tick();check(!battleMotion_.active()&&game.toJson()==committed,"Health playback changed committed combat rules");
  mouseX_=battleHexCenter({6,5}).x;mouseY_=battleHexCenter({6,5}).y;shot("health-"+mode+"-hover");
  audit[mode]={{"beforeHP",initialHP},{"afterHP",initialHP-damage},{"countBefore",2},{"countAfter",2},{"fullPixels",full},{"damagedPixels",hitWidth},{"retaliationUpdatesAlly",true},{"playbackStateUnchanged",true}};
 }
 settings_.hdArtwork=false;reset(Pikeman,20,Skeleton,50);render();int full=gauge(100);click(battleHexCenter({6,5}),false);fixedAnimationTime_=500;tick();render();
 check(battleMotion_.view()->units[1].count<50&&gauge(100)<full,"Casualties reset the bar to full");shot("health-casualties");
 reset(Archer,50,Cavalier,10);game.battle->units[1].x=10;render();full=gauge(100);click(battleHexCenter({10,5}),false);fixedAnimationTime_=900;tick();render();
 check(battleMotion_.view()->units[1].hp<1000&&gauge(100)<full,"Ranged damage did not reduce the visible bar");shot("health-ranged-hit");
 reset(Pikeman,10,Cavalier,2);game.hero.spellbook=true;game.hero.spells[MagicArrow]=true;render();full=gauge(100);
 battleAction([&]{return game.cast(MagicArrow,{6,5});});render();check(game.battle->units[1].hp==180&&gauge(100)<full,"Magic Arrow did not reduce health");shot("health-magic-arrow");
 reset(Pikeman,100,Skeleton,1);render();click(battleHexCenter({6,5}),false);fixedAnimationTime_=700;tick();render();
 check(battleMotion_.pose(100)->dying&&gauge(100)==0,"Killed enemy retained a nonempty health gauge");shot("health-lethal-hit");fixedAnimationTime_=100000;tick();check(scene==Scene::Report,"Health rendering delayed the victory report");
 // A wounded survivor's capacity is the original stack, including after
 // cure/resurrection; never use the current count as the denominator.
 reset(Pikeman,10,Cavalier,2);render();full=gauge(100);game.battle->units[1].count=game.battle->units[1].permanentCount=1;game.battle->units[1].hp=100;render();
 int half=gauge(100);check(std::abs(half*2-full)<=1,"A half-strength surviving stack rendered a full bar");shot("health-half-stack");
 game.battle->units[1].hp=1;render();check(gauge(100)>0&&gauge(100)<full/4,"A living stack at one HP lost its health indicator");
 reset(Cavalier,2,Skeleton,200);game.hero.spellbook=true;game.hero.spells[Cure]=true;game.battle->units[0].hp=150;render();int wounded=gauge(0);
 battleAction([&]{return game.cast(Cure,{5,5});});fixedAnimationTime_=10000;tick();render();check(game.battle->units[0].hp>150&&gauge(0)>wounded,"Cure did not restore the visible health gauge");shot("health-cure");
 audit["casualtiesKeepInitialCapacity"]=true;audit["rangedDamage"]=true;audit["magicDamage"]=true;audit["deathGaugeEmpty"]=true;audit["oneHPSurvivorVisible"]=true;audit["cureRestoresGauge"]=true;
 std::ofstream(directory/"battle-health-audit.json")<<audit.dump(2)<<'\n';
 battleMotion_.clear();game=Game::create();scene=Scene::Town;fixedAnimationTime_=0;settings_=previousSettings;mouseX_=mouseY_=-1;nextAI_=0;SDL_SetWindowSize(window_,oldW,oldH);
}
}
