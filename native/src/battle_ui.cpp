#include "h3/renderer.hpp"
#include "h3/battle_layout.hpp"
#include <algorithm>
#include <cmath>

namespace h3 {
namespace {constexpr SDL_Color GoldColor{236,213,130,255};}
void Desktop::battleCount(const CombatStack&stack,Point p){
 auto&plate=images_["#classic-purple-count"];
 if(!plate.texture){
  auto*surface=readSurface("assets/demo/pcx/cmnumwin.png",false,false);
  if(surface){
   for(int y=1;y<surface->h-1;++y)for(int x=1;x<surface->w-1;++x){
    auto*pixel=static_cast<Uint8*>(surface->pixels)+y*surface->pitch+x*4;
    pixel[0]=Uint8(int(pixel[0])*160/255);pixel[1]=Uint8(int(pixel[1])*60/255);pixel[2]=Uint8(int(pixel[2])*213/255);
   }
   plate={SDL_CreateTextureFromSurface(renderer_,surface),surface->w,surface->h};SDL_FreeSurface(surface);
  }
 }
 SDL_Rect rect{p.x,p.y,30,11};if(plate.texture)SDL_RenderCopy(renderer_,plate.texture,nullptr,&rect);
 centerText(std::to_string(stack.count),rect,9,{255,255,255,255});
 // Keep health separate from quantity, with a fixed battle-start capacity:
 // casualties must not reset the surviving stack to a full health bar.
 const int64_t maximum=int64_t(stack.initialCount)*Units[stack.unit].hp;
 const int64_t remaining=std::clamp<int64_t>(stack.hp,0,maximum);
 box({p.x,p.y+12,30,5},{32,22,17,255});
 int width=maximum>0?int(28*remaining/maximum):0;
 if(remaining>0)width=std::max(1,width);
 SDL_Color health=remaining*4<=maximum?SDL_Color{210,55,44,255}:remaining*2<=maximum?SDL_Color{225,171,45,255}:SDL_Color{63,190,79,255};
 if(width)box({p.x+1,p.y+13,width,3},health);
}
void Desktop::battleAction(const std::function<std::string()>&action){
 if(battleMotion_.active()||!game.battle)return;Battle before=*game.battle;auto message=action();
 if(message.empty()){auto events=game.takeEvents();battleMotion_.start(before,events,[this](int unit,int group){return frameCount(Units[unit].animation,group);},fixedAnimationTime_.value_or(SDL_GetTicks64()));persist();settle();}
 say(message);
}
void Desktop::battle(){
 const Battle*view=scene==Scene::Report?battleMotion_.retainedView():battleMotion_.view();if(!view&&game.battle)view=&*game.battle;if(!view){settle();return;}
 const auto&b=*view;bool busy=battleMotion_.active();if(!siegeArtwork(b)&&!redrawnBattle())originalImage("cmbkgrtr.pcx",{0,0,800,556});
 // Hero class sprites and flags follow the heroes participating in this battle.
 int heroClass=game.hero.originalId>=0&&game.hero.originalId<144?game.hero.originalId/8:0;auto number=std::to_string(heroClass);if(heroClass==10||(heroClass>=12&&heroClass<=15))number="0"+number;auto heroAsset="ch"+std::string(number.size()<2?1:0,'0')+number+".def";if(!frame(heroAsset,0,0,{-25,-20,150,175}))frame("ch00.def",0,0,{-25,-20,150,175});
 if(game.heroes.contains(b.targetId)){int enemy=game.heroes.at(b.targetId).value.originalId;int enemyClass=enemy>=0&&enemy<144?enemy/8:0;auto n=std::to_string(enemyClass);if(enemyClass==10||(enemyClass>=12&&enemyClass<=15))n="0"+n;frame("ch"+std::string(n.size()<2?1:0,'0')+n+".def",0,0,{675,-20,150,175},true);frame("cmflagr.def",0,int(animationTime_/100),{735,15,17,15},false,false,false,game.heroes.at(b.targetId).owner);}
 frame("cmflagl.def",0,int(animationTime_/100),{45,15,17,15},false,false,false,0);
 auto*active=busy?nullptr:game.current();const auto*hovered=busy?nullptr:battleTargetAt({mouseX_,mouseY_});auto aim=hovered?std::optional<Point>(Point{hovered->x,hovered->y}):battleHexAt({mouseX_,mouseY_});
 std::vector<Point> area;if(pendingSpell_>=0&&aim)area=game.spellArea(pendingSpell_,*aim);
 for(int y=0;y<BattleH;y++)for(int x=0;x<BattleW;x++){
  Point cell{x,y},top=battleHexTopLeft(cell);SDL_Rect rect{top.x,top.y,45,52};
  bool affected=std::find(area.begin(),area.end(),cell)!=area.end();
  bool reachable=false;if(!autoCombat_&&pendingSpell_<0&&active&&active->team==0&&!game.combatAt(cell)){auto distance=game.battlePath(*active,cell);reachable=distance&&*distance<=game.speed(*active);}
  if(reachable||affected)art("assets/demo/battle-ui/cell-shadow.png",rect,{},false);
  if(settings_.battleGrid)art("assets/demo/battle-ui/cell-grid.png",rect,{},false);
  buttons_.push_back({rect,[this,cell]{battleCellAction(cell);},active&&active->team==0&&!autoCombat_,[cell](Point p){return battleHexAt(p)==cell;},pendingSpell_>=0?"选择"+Spells[pendingSpell_].name+"的施法目标；右键取消":"战场格：点击空地移动，点击敌军攻击；灰色区域无法到达"});
 }
 std::vector<const CombatStack*> corpses,living;
 for(const auto&unit:b.units){auto pose=battleMotion_.pose(unit.id);bool dying=pose&&pose->dying;if(unit.corpse&&!dying)corpses.push_back(&unit);if(unit.hp>0||dying)living.push_back(&unit);}
 std::sort(corpses.begin(),corpses.end(),[](const auto*a,const auto*c){return a->deathOrder<c->deathOrder;});
 SDL_Rect field{0,0,800,556};SDL_RenderSetClipRect(renderer_,&field);
 for(const auto*body:corpses){if(combatImage(*body,5,true))continue;auto center=battleHexCenter({body->x,body->y});portrait(body->unit,{center.x-17,center.y-7,30,33});}
 std::stable_sort(living.begin(),living.end(),[this](const auto*a,const auto*c){auto pa=battleMotion_.pose(a->id),pc=battleMotion_.pose(c->id);auto ca=battleHexCenter({a->x,a->y}),cc=battleHexCenter({c->x,c->y});double ay=pa?pa->y:ca.y,cy=pc?pc->y:cc.y,ax=pa?pa->x:ca.x,cx=pc?pc->x:cc.x;return ay!=cy?ay<cy:ax<cx;});
 for(const auto*stack:living){
  const auto&unit=*stack;auto center=battleHexCenter({unit.x,unit.y});if(auto pose=battleMotion_.pose(unit.id))center={int(std::lround(pose->x)),int(std::lround(pose->y))};const auto&definition=Units[unit.unit];bool clone=unit.origin==StackOrigin::Clone;
  if(!combatImage(unit,2,false,active&&active->id==unit.id,hovered&&hovered->id==unit.id&&unit.team==1&&active&&active->team==0)){
   if(definition.portraitOnly)portrait(unit.unit,{center.x-25,center.y-48,50,55},clone);
   else{auto&sheet=image("original/"+definition.asset);SDL_Rect source{0,0,std::min(sheet.w,definition.size==2?150:80),std::min(sheet.h,130)};art("original/"+definition.asset,{center.x-37,center.y-74,74,85},source,true,unit.team==1,clone);}
  }
  if(clone)text("镜像",center.x-15,center.y-85,9,{135,195,255,255});
  int countX=center.x+(unit.team?-52:22),countY=center.y+2;
  battleCount(unit,{countX,countY});
  if(unit.hp>0)if(auto visual=combatVisual(unit)){
   SDL_Rect bounds{};if(SDL_IntersectRect(&visual->canvas,&field,&bounds)){
    int id=unit.id;
    buttons_.push_back({bounds,[this,id]{if(!game.battle)return;for(const auto&u:game.battle->units)if(u.id==id&&u.hp>0){battleCellAction({u.x,u.y});return;}},active&&active->team==0&&!autoCombat_,[this,id](Point p){auto*target=battleTargetAt(p);return target&&target->id==id;},Units[unit.unit].name+" × "+std::to_string(unit.count)+(unit.team==0?"：我方部队":"：点击攻击敌军，或作为法术目标")});
   }
  }
 }
 for(const auto&effect:battleMotion_.effects()){
  auto point=battleHexCenter(effect.pos);const auto&def=*effect.definition;auto target=std::find_if(b.units.begin(),b.units.end(),[&](const auto&unit){return unit.id==effect.stackId;});
  if(target!=b.units.end()&&Units[target->unit].size==2)point.x+=target->team?22:-22;
  frame(def.animation,0,effect.frame,{point.x-def.width/2,point.y-def.height,def.width,def.height});
 }
 SDL_RenderSetClipRect(renderer_,nullptr);originalImage("cbar.pcx",{0,556,800,44});
 std::string message=battleMotion_.damageMessage().empty()?(b.log.empty()?"":b.log.back()):battleMotion_.damageMessage();
 if(pendingSpell_>=0)message=(pendingSpell_==Sacrifice?(sacrificeTarget_<0?"选择复生目标：":"选择牺牲部队："):"选择目标：")+Spells[pendingSpell_].name;
 else if(toastUntil_>SDL_GetTicks64()&&!toast_.empty())message=toast_;
 if(busy){if(battleMotion_.damageMessage().empty())message="正在播放战斗动作。";if(const auto*cue=battleMotion_.feedback())message=cue->type==GameEventType::SpellCast?"凯瑟琳施放"+Spells[cue->detail].name+"。":cue->type==GameEventType::CombatLuck?"幸运攻击！":cue->detail<0?"士气低落，失去行动。":"士气高涨，可以再次行动！";}else if(autoCombat_)message="自动战斗；再次点击交叉双剑或按 Esc 取消。";
 else if(b.moralePhase)message="士气额外行动等待阶段。";
 if(!busy&&pendingSpell_<0&&hovered){
  const auto&unit=*hovered;message=Units[unit.unit].name+" × "+std::to_string(unit.count)+"  总生命 "+std::to_string(unit.hp)+"/"+std::to_string(int64_t(unit.initialCount)*Units[unit.unit].hp);
 }
 text(message,216,569,10,GoldColor,394);
 bool own=active&&active->team==0&&!autoCombat_;
 originalButton({3,561,48,36},"icm003.def",0,[this]{openOptions();});
 originalButton({54,561,48,36},"icm001.def",0,[]{},false,2);
 originalButton({105,561,48,36},"icm002.def",0,[]{},false,2);
 originalButton({156,561,48,36},"icm004.def",0,[this]{autoCombat_=!autoCombat_;pendingSpell_=sacrificeTarget_=-1;nextAI_=0;});
 if(autoCombat_)frame("icm004.def",0,1,{156,561,48,36});
 originalButton({645,561,48,36},"icm005.def",0,[this]{openSpellbook();},own&&game.hero.spellbook,2);
 originalButton({696,561,48,36},"icm006.def",0,[this]{battleAction([this]{return game.act("wait");});},own&&!active->waited&&!b.waitPhase&&!b.moralePhase,2);
 originalButton({747,561,48,36},"icm007.def",0,[this]{battleAction([this]{return game.act("defend");});},own,2);
}
void Desktop::battleRenderTests(const std::filesystem::path&directory){
 auto check=[](bool ok,const char*message){if(!ok)throw std::runtime_error(message);};
 auto shot=[&](const char*name){toastUntil_=0;fixedAnimationTime_=0;render();check(screenshot(directory/(std::string(name)+".png")),"Battle layout screenshot failed");};
 game=Game::create();scene=Scene::Hero;auto before=game.toJson();render();click({570,543},false);before["hero"]["tightFormation"]=true;
 check(game.hero.tightFormation&&game.toJson()==before,"Hero formation button changed unrelated state");shot("hero-formation-tight");
 std::string error;Game loaded;check(loaded.load(saveDir_/"autosave.json",error)&&loaded.hero.tightFormation,"Hero formation did not persist");
 game.startBattle(15);scene=Scene::Battle;autoCombat_=false;pendingSpell_=sacrificeTarget_=-1;shot("battle-formation-tight");
 game=Game::create();game.startBattle(15);scene=Scene::Battle;shot("battle-formation-loose");
 auto state=game.toJson();click({80,86},false);check(game.toJson()==state,"Transparent hex corner accepted an action");
 key(SDLK_F6,KMOD_NONE);check(!settings_.battleGrid&&game.toJson()==state,"Grid toggle changed combat state");shot("battle-grid-off");key(SDLK_F6,KMOD_NONE);
 render();click({27,578},false);check(scene==Scene::Options&&optionsReturn_==Scene::Battle,"Original combat computer button did not open settings");key(SDLK_ESCAPE,KMOD_NONE);
 render();click({180,578},false);check(autoCombat_&&game.toJson()==state,"Original automatic combat button changed a stack before ticking");shot("battle-auto-enabled");
 Point previous{game.battle->units[2].x,game.battle->units[2].y};nextAI_=0;tick();
 check(game.battle&&Point{game.battle->units[2].x,game.battle->units[2].y}!=previous,"Automatic combat did not advance the player through the desktop timer");state=game.toJson();
 key(SDLK_ESCAPE,KMOD_NONE);check(!autoCombat_&&scene==Scene::Battle&&game.toJson()==state,"Escape did not stop automatic combat safely");
 battleMotion_.clear();game=Game::create();game.startBattle(15);scene=Scene::Battle;render();Point destination{4,7};int moving=game.current()->id;click(battleHexCenter(destination),false);
 auto unit=std::find_if(game.battle->units.begin(),game.battle->units.end(),[moving](const auto&stack){return stack.id==moving;});
 check(unit!=game.battle->units.end()&&unit->x==destination.x&&unit->y==destination.y,"Original hex center selected the wrong destination");
 battleMotion_.clear();game=Game::create();scene=Scene::Town;autoCombat_=false;
 battleMotionRenderTests(directory);
 battleActionRenderTests(directory);
 battleHealthRenderTests(directory);
 battleTargetRenderTests(directory);
}
void Desktop::battleMotionRenderTests(const std::filesystem::path&directory){
 auto check=[](bool ok,const char*message){if(!ok)throw std::runtime_error(message);};
 auto shot=[&](const char*name){toastUntil_=0;render();check(screenshot(directory/(std::string(name)+".png")),"Movement screenshot failed");};
 auto reset=[&]{battleMotion_.clear();game=Game::create();autoCombat_=false;pendingSpell_=sacrificeTarget_=-1;fixedAnimationTime_=0;nextAI_=0;};
 auto advance=[&](uint64_t time){fixedAnimationTime_=time;tick();};
 reset();game.startBattle(15);scene=Scene::Battle;render();int id=game.current()->id;Point end{4,7};click(battleHexCenter(end),false);auto committed=game.toJson();
 check(battleMotion_.active()&&battleMotion_.pose(id)->group==20,"Flying stack did not start its original takeoff group");shot("battle-flight-start");
 key(SDLK_d,KMOD_NONE);render();click(battleHexCenter({5,6}),false);check(game.toJson()==committed,"A second input changed combat during movement");
 advance(900);check(battleMotion_.active()&&battleMotion_.pose(id)->group==0,"Flying stack did not enter its original movement group");shot("battle-flight-midpoint");
 auto pose=*battleMotion_.pose(id);openOptions();advance(9000);check(battleMotion_.pose(id)->x==pose.x&&game.toJson()==committed,"Options did not pause movement");key(SDLK_ESCAPE,KMOD_NONE);advance(9050);check(battleMotion_.active(),"Closing options skipped the rest of the flight");
 advance(100000);check(!battleMotion_.active()&&game.toJson()==committed&&scene==Scene::Battle,"Movement advanced the next actor or changed the committed result");shot("battle-flight-landed");
 // A ground route must avoid an allied occupied cell instead of drawing a
 // straight line through it. The count plate shares the interpolated anchor.
 reset();game.hero.army={Stack{Pikeman,20},Stack{Pikeman,5}};game.startBattle(15);game.battle->units[0].x=3;game.battle->units[0].y=5;game.battle->units[1].x=4;game.battle->units[1].y=5;scene=Scene::Battle;render();
 click(battleHexCenter({6,5}),false);check(battleMotion_.active(),"Walking input did not queue movement");committed=game.toJson();advance(500);shot("battle-walk-around-ally");advance(100000);check(!battleMotion_.active()&&game.toJson()==committed,"Walking playback changed the turn queue");
 // The rules can finish immediately, but the report waits for movement.
 reset();game.hero.army={Stack{Champion,40}};game.objectById(15)->army={Stack{Skeleton,1}};game.startBattle(15);game.battle->units[0].x=2;game.battle->units[0].y=5;game.battle->units[1].x=7;game.battle->units[1].y=5;scene=Scene::Battle;render();click(battleHexCenter({7,5}),false);
 check(!game.battle&&game.report&&game.report->won&&battleMotion_.active()&&scene==Scene::Battle&&audioTheme()=="combat.1","Victory interrupted the queued approach movement");committed=game.toJson();shot("battle-move-before-victory");advance(100000);
 check(!battleMotion_.active()&&scene==Scene::Report&&game.toJson()==committed,"Movement completion did not show the committed report");shot("battle-after-move-victory");
 reset();game=Game::create();scene=Scene::Town;
}
void Desktop::battleActionRenderTests(const std::filesystem::path&directory){
 auto check=[](bool ok,const char*message){if(!ok)throw std::runtime_error(message);};
 auto shot=[&](const char*name){toastUntil_=0;render();check(screenshot(directory/(std::string(name)+".png")),"Combat action screenshot failed");};
 auto reset=[&](int unit,int amount,int enemy,int enemies){battleMotion_.clear();game=Game::create();game.hero.army={Stack{unit,amount}};game.objectById(15)->army={Stack{enemy,enemies}};game.startBattle(15);game.battle->units[0].x=5;game.battle->units[0].y=5;game.battle->units[1].x=6;game.battle->units[1].y=5;game.battle->queue={0,100};game.takeEvents();scene=Scene::Battle;fixedAnimationTime_=0;nextAI_=0;autoCombat_=false;pendingSpell_=sacrificeTarget_=-1;};
 auto advance=[&](uint64_t time){fixedAnimationTime_=time;tick();};
 reset(Pikeman,20,Skeleton,50);auto hp=game.battle->units[1].hp;render();click(battleHexCenter({6,5}),false);auto committed=game.toJson();check(battleMotion_.active()&&battleMotion_.pose(0)->group==12&&battleMotion_.view()->units[1].hp==hp,"Melee impact was shown before its frame");shot("battle-melee-attack");
 key(SDLK_d,KMOD_NONE);render();click(battleHexCenter({7,5}),false);check(game.toJson()==committed,"A second input interrupted melee playback");advance(500);check(battleMotion_.view()->units[1].hp<hp&&battleMotion_.pose(100)->group==3,"Melee impact did not update the visible victim");shot("battle-melee-hit");
 auto pose=*battleMotion_.pose(100);openOptions();advance(10000);check(battleMotion_.pose(100)->frame==pose.frame,"Combat options did not pause a hit animation");key(SDLK_ESCAPE,KMOD_NONE);advance(10600);check(battleMotion_.pose(100)->group==12,"Retaliation was not queued after the first impact");shot("battle-melee-retaliation");advance(100000);check(!battleMotion_.active()&&game.toJson()==committed,"Combat playback changed the committed result");
 reset(Pikeman,20,Skeleton,50);game.battle->units[1].defending=true;render();click(battleHexCenter({6,5}),false);advance(700);check(battleMotion_.pose(100)->group==4,"Defending victim did not use its original defending group");shot("battle-defending-hit");
 reset(Pikeman,100,Skeleton,1);CombatStack reserve=game.battle->units[1];reserve.id=101;reserve.slot=1;reserve.x=13;reserve.y=9;reserve.count=reserve.initialCount=reserve.permanentCount=1000;reserve.hp=6000;game.objectById(15)->army[1]={Skeleton,1000};game.battle->units.push_back(reserve);game.battle->queue.push_back(101);render();click(battleHexCenter({6,5}),false);advance(700);check(battleMotion_.pose(100)->dying&&battleMotion_.view()->units[1].count==0,"Dying stack was replaced before its original death frames");shot("battle-melee-death");advance(100000);check(!battleMotion_.active()&&game.battle->units[1].corpse,"Melee corpse did not remain after playback");shot("battle-melee-corpse");
 reset(Archer,50,Pikeman,1);game.battle->units[1].x=10;render();click(battleHexCenter({10,5}),false);check(!game.battle&&game.report&&scene==Scene::Battle&&battleMotion_.pose(0)->group==15,"Ranged victory skipped the shoot animation");committed=game.toJson();shot("battle-ranged-attack");advance(900);check(scene==Scene::Battle&&battleMotion_.pose(100)->dying&&audioTheme()=="combat.1","Victory interrupted the dying target or combat music");shot("battle-ranged-death");advance(100000);check(scene==Scene::Report&&game.toJson()==committed,"Death completion did not show the committed report");shot("battle-after-action-victory");
 reset(Archer,50,75,1);game.battle->units[1].x=10;render();click(battleHexCenter({10,5}),false);advance(900);check(battleMotion_.pose(100)->group==6&&battleMotion_.pose(100)->dying,"Original Evil Eye alternate ranged death was not used");shot("battle-ranged-alternate-death");
 reset(Pikeman,20,Skeleton,50);game.battle->units[0].x=7;render();click(battleHexCenter({6,5}),false);check(battleMotion_.pose(0)->group==7&&!battleMotion_.pose(0)->flip,"Backward attack did not begin the turn");shot("battle-turn-first-half");advance(200);check(battleMotion_.pose(0)->group==8&&battleMotion_.pose(0)->flip,"Backward attack did not flip between turn halves");shot("battle-turn-second-half");
 reset(Pikeman,100,53,1);render();click(battleHexCenter({6,5}),false);advance(600);check(battleMotion_.pose(0)->group==3&&battleMotion_.pose(100)->dying,"Fire shield did not show reflected damage with the primary impact");shot("battle-fire-shield-impact");
 reset(Pikeman,20,Archer,1000);game.hero.spellbook=true;game.hero.spells[Clone]=true;game.hero.skills[WaterMagic]=3;auto original=game.battle->units[0];check(game.cast(Clone,{original.x,original.y}).empty(),"Mirror fixture could not cast Clone");int mirror=game.battle->units.back().id;game.battle->units[1].x=12;game.battle->queue={100,0,mirror};game.takeEvents();battleAction([&]{return game.act("attack",{original.x,original.y});});advance(700);check(battleMotion_.view()->units[0].hp==0&&battleMotion_.view()->units.back().hp==0&&!battleMotion_.view()->units.back().corpse,"The dead original left a living mirror in the presentation");shot("battle-mirror-disappeared");
 battleMotion_.clear();game=Game::create();scene=Scene::Town;fixedAnimationTime_=0;autoCombat_=false;
}
void Desktop::moraleRenderTests(const std::filesystem::path&directory){
 auto check=[](bool ok,const char*message){if(!ok)throw std::runtime_error(message);};int width=0,height=0;SDL_GetWindowSize(window_,&width,&height);SDL_SetWindowSize(window_,2560,1920);settings_.hdArtwork=true;fixedAnimationTime_=0;mouseX_=mouseY_=-1000;autoCombat_=false;pendingSpell_=-1;toastUntil_=0;battleMotion_.clear();
 game=Game::create();game.hero.skills={};game.hero.army={Stack{0,1},Stack{14,1},Stack{28,1},Stack{42,1},Stack{70,1},Stack{84,1},Stack{98,1}};scene=Scene::Hero;check(game.armyMorale()==-3,"Mixed army morale fixture failed");render();check(screenshot(directory/"hero-morale-low.png"),"Low morale hero capture failed");
 game=Game::create();game.hero.skills[LuckSkill]=3;check(game.armyLuck()==3,"Expert Luck hero fixture failed");render();check(screenshot(directory/"hero-luck-high.png"),"Expert Luck hero capture failed");
 auto reset=[&](int unit=Pikeman,int stacks=1){battleMotion_.clear();game=Game::create();game.hero.skills={};game.hero.army={};for(int i=0;i<stacks;i++)game.hero.army[i]={unit,20};game.hero.spellbook=true;game.hero.spells[Haste]=true;game.objectById(15)->army={Stack{Skeleton,1000}};game.startBattle(15);game.takeEvents();scene=Scene::Battle;fixedAnimationTime_=0;toastUntil_=0;};
 auto capture=[&](GameEventType kind,int detail,const char*name){
  auto state=game.toJson();bool found=false;uint64_t begin=0;for(uint64_t time=0;time<=20000;time+=10){battleMotion_.tick(time);const auto*cue=battleMotion_.feedback();if(cue&&cue->type==kind&&cue->detail==detail){begin=time;found=true;break;}}
  check(found&&battleMotion_.effects().size()==1,"Original morale or luck effect was not presented");const auto*def=battleMotion_.effects().front().definition;check(frameCount(def->animation)==def->frames,"Original effect lost frames");std::string stem=std::filesystem::path(name).stem().string();
  fixedAnimationTime_=begin;render();check(screenshot(directory/(stem+"-start.png")),"Original effect first frame capture failed");
  auto middle=begin+uint64_t(def->frames/2)*def->frameMs;battleMotion_.tick(middle);fixedAnimationTime_=middle;check(battleMotion_.effects().front().frame==def->frames/2,"Original effect middle frame failed");render();check(screenshot(directory/name),"Morale or luck cue capture failed");battleAction([&]{return game.act("defend");});check(game.toJson()==state,"Input or presentation changed committed state during morale feedback");
  auto end=begin+uint64_t(def->frames-1)*def->frameMs;battleMotion_.tick(end);fixedAnimationTime_=end;check(battleMotion_.effects().front().frame==def->frames-1,"Original effect final frame failed");render();check(screenshot(directory/(stem+"-end.png")),"Original effect final frame capture failed");
 };
 reset();auto p=*game.current();game.randomState=634785765u;battleAction([&]{return game.act("move",{p.x+1,p.y});});check(game.current()->moraleUsed,"Good morale fixture did not gain a bonus action");capture(GameEventType::CombatMorale,1,"battle-morale-high.png");
 reset();p=*game.current();auto&low=game.battle->units[0];low.effects[Sorrow]=10;low.effectMastery[Sorrow]=3;low.preparedRound=0;game.randomState=634785765u;battleAction([&]{return game.cast(Haste,{p.x,p.y});});capture(GameEventType::CombatMorale,-1,"battle-morale-low.png");
 reset(Archer);game.hero.skills[LuckSkill]=3;auto enemy=game.battle->units.back();game.battle->units[0].effects[Bless]=4;game.randomState=634785765u;battleAction([&]{return game.act("attack",{enemy.x,enemy.y});});capture(GameEventType::CombatLuck,1,"battle-lucky-attack.png");
 reset(Pikeman,3);game.act("wait");for(int id:{1,2}){check(game.current()->id==id,"Morale FIFO fixture order failed");p=*game.current();game.randomState=634785765u;game.act("move",{p.x+1,p.y});game.act("wait");}game.act("defend");game.takeEvents();check(game.battle->moralePhase&&!game.battle->waitPhase&&game.battle->queue==std::vector<int>({1,2})&&game.validate(),"Morale wait phase fixture failed");render();check(screenshot(directory/"battle-morale-wait.png"),"Morale wait phase capture failed");
 for(int spell:{int(Mirth),int(Sorrow),int(Fortune),int(Misfortune)}){
  battleMotion_.clear();game=Game::create();game.hero.skills={};for(int skill:{AirMagic,FireMagic,WaterMagic,EarthMagic})game.hero.skills[skill]=3;game.hero.spellbook=true;game.hero.spells[spell]=true;game.hero.army={Stack{Archangel,5},Stack{Pikeman,20},Stack{30,10},Stack{32,10}};game.objectById(15)->army={Stack{Pikeman,1000},Stack{30,1000},Stack{32,1000}};game.startBattle(15);game.takeEvents();scene=Scene::Battle;fixedAnimationTime_=0;toastUntil_=0;
  battleAction([&]{return game.cast(spell);});auto state=game.toJson();int expected=spell==Mirth?3:spell==Sorrow?2:spell==Fortune?4:3;check(int(battleMotion_.effects().size())==expected,"Mass spell effect target count failed");battleMotion_.tick(700);fixedAnimationTime_=700;for(const auto&e:battleMotion_.effects())check(e.frame==7,"Mass spell effect clocks diverged");render();
  const char*name=spell==Mirth?"mirth":spell==Sorrow?"sorrow":spell==Fortune?"fortune":"misfortune";check(screenshot(directory/(std::string("battle-spell-")+name+".png")),"Mass morale spell capture failed");battleAction([&]{return game.act("defend");});check(game.toJson()==state,"Mass spell presentation accepted a combat action");
 }
 battleMotion_.clear();SDL_SetWindowSize(window_,width,height);game=Game::create();scene=Scene::Town;fixedAnimationTime_=0;toastUntil_=0;
}
}
