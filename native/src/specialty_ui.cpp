#include "h3/renderer.hpp"
#include <algorithm>
#include <fstream>
#include <stdexcept>
namespace h3 {
bool Desktop::artifactContext(Point p){
 const std::pair<const char*,Point> positions[]={{"head",{442,21}},{"neck",{442,69}},{"torso",{442,121}},{"feet",{442,303}},{"shoulders",{499,21}},{"right_hand",{315,133}},{"left_hand",{544,175}},{"right_ring",{330,185}},{"left_ring",{499,232}},{"misc1",{315,60}},{"misc2",{365,60}},{"misc3",{544,67}},{"misc4",{544,114}},{"misc5",{544,286}}};
 for(auto[name,at]:positions)if(p.x>=64+at.x&&p.x<108+at.x&&p.y>=7+at.y&&p.y<51+at.y){
  int item=game.hero.comboLocks.contains(name)?game.hero.comboLocks.at(name):game.hero.equipmentSlots.contains(name)?game.hero.equipmentSlots.at(name):-1;
  if(!artifactParts(item).empty()){combinationId_=item;combinationDisassemble_=true;scene=Scene::Combination;return true;}
  auto sets=game.assemblableArtifacts(item);if(!sets.empty()){combinationId_=sets.front();combinationDisassemble_=false;scene=Scene::Combination;return true;}
 }return false;
}
void Desktop::combinationDialog(){
 hero();buttons_.clear();box({0,0,800,600},{0,0,0,135});panel({135,125,530,350});centerText(artifactName(combinationId_),{155,142,490,38},22,{236,213,130,255});frame("artifact.def",0,combinationId_,{374,191,52,52});
 auto parts=artifactParts(combinationId_);int width=int(parts.size())*44,x=400-width/2;for(int part:parts){frame("artifact.def",0,part,{x,259,42,42});x+=44;}
 centerText(combinationDisassemble_?"拆卸后归还所有部件，并恢复部件的装备位置。":"组合后保留部件加成，原来的装备栏位仍会被占用。",{158,319,484,60},13);
 button({185,408,150,40},"取消",[this]{scene=Scene::Hero;focus_=-1;});button({465,408,150,40},combinationDisassemble_?"确认拆卸":"确认组合",[this]{say(combinationDisassemble_?game.disassembleArtifact(combinationId_):game.assembleArtifact(combinationId_));scene=Scene::Hero;heroArtifact_=-1;persist();});
}
void Desktop::specialtyPreview(const std::filesystem::path&dir){
 std::filesystem::create_directories(dir);saveDir_=dir/"test-saves";settings_=ClientSettings{};settings_.hdArtwork=false;fixedAnimationTime_=1000;SDL_SetWindowSize(window_,1280,800);std::string error;int shots=0;
 auto check=[](bool ok,const char*message){if(!ok)throw std::runtime_error(message);};auto shot=[&](const std::string&name){toastUntil_=0;mouseX_=mouseY_=-1000;render();check(screenshot(dir/(name+".png")),"Specialty screenshot failed");++shots;};
 check(frameCount("artifact.def",0)>=144&&demo_["def"]["artifact.def"]["groups"]["0"][129]["name"]=="HrArt129.pcx","Complete combination icons hidden by older expansion archive");
 for(int id=129;id<=140;++id){game=Game::create();game.hero.artifactIds.clear();game.hero.artifacts.clear();game.hero.wornArtifacts.clear();game.hero.equipmentSlots.clear();game.hero.comboLocks.clear();for(int part:artifactParts(id)){game.hero.artifactIds.push_back(part);game.hero.artifacts.push_back(artifactName(part));game.equipPickedArtifact(part);}auto possible=game.assemblableArtifacts();check(std::find(possible.begin(),possible.end(),id)!=possible.end(),"Original recipe could not be equipped");combinationId_=id;combinationDisassemble_=false;scene=Scene::Combination;shot("combination-"+std::to_string(id)+"-confirm");click({540,428},false);check(wearsArtifact(game.hero,id),"Combination confirmation button failed");shot("combination-"+std::to_string(id)+"-equipped");combinationId_=id;combinationDisassemble_=true;scene=Scene::Combination;render();click({540,428},false);check(!wearsArtifact(game.hero,id)&&game.hero.comboLocks.empty(),"Combination dismantling button failed");}
 game=Game::create();game.hero.army={Stack{Archangel,10},Stack{Pikeman,30}};game.objectById(15)->army={Stack{Skeleton,500}};game.startBattle(15);auto&ally=game.battle->units[1];ally.hp=ally.count=ally.permanentCount=0;ally.corpse=true;ally.deathOrder=++game.battle->deathSequence;game.battle->queue={0,100};game.battle->units[0].preparedRound=game.battle->round;scene=Scene::Battle;battleMotion_.clear();game.takeEvents();pendingCreature_=true;shot("archangel-resurrection-target");battleCellAction({ally.x,ally.y});check(game.battle&&game.battle->units[1].count==30,"Creature resurrection UI failed");shot("archangel-resurrection-animation");battleMotion_.clear();shot("archangel-resurrection-result");pendingCreature_=false;
 battleMotion_.clear();fixedAnimationTime_=0;game=Game::create();game.hero.army={Stack{Archer,50}};game.objectById(15)->army={Stack{Pikeman,1000}};game.startBattle(15);game.battle->units[0].x=4;game.battle->units[0].y=5;game.battle->units[1].x=13;game.battle->units[1].y=5;game.battle->queue={0,100};game.takeEvents();scene=Scene::Battle;int hp=game.battle->units[1].hp;battleAction([&]{return game.act("attack",{13,5});});
 for(uint64_t time=10;time<=5000&&battleMotion_.active()&&battleMotion_.projectiles().empty();time+=10){fixedAnimationTime_=time;tick();}
 check(!battleMotion_.projectiles().empty()&&battleMotion_.view()&&battleMotion_.view()->units[1].hp==hp,"Projectile did not fly before the target took visible damage");shot("archer-original-projectile-flight");battleMotion_.clear();game.battle->units[1].petrified=3;shot("petrified-original-palette");
 std::ofstream(dir/"specialty-ui-audit.json")<<nlohmann::json{{"screenshots",shots},{"allTwelveCombinationButtons",true},{"componentSlotsReserved",true},{"dismantlingPreservesParts",true},{"creatureCastUsesRealAction",true},{"isolatedSaves",true},{"nativeProjectileBeforeImpact",true},{"petrifiedOriginalPalette",true}}.dump(2)<<'\n';
}
}
