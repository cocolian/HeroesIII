#include "h3/renderer.hpp"
#include "h3/battle_layout.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace h3 {
void Desktop::campaignFlowRenderTests(const std::filesystem::path& directory){
 auto check=[](bool ok,const std::string& message){if(!ok)throw std::runtime_error(message);};
 int oldW=0,oldH=0;SDL_GetWindowSize(window_,&oldW,&oldH);SDL_SetWindowSize(window_,1280,800);
 uint64_t clock=1000;int moves=0,spells=0,levels=0;auto advance=[&]{fixedAnimationTime_=clock+=100000;tick();};
 auto shot=[&](const std::string& name){toastUntil_=0;render();check(screenshot(directory/("campaign-"+name+".png")),"Campaign screenshot failed");};
 nlohmann::json encounters=nlohmann::json::array(),journey=nlohmann::json::array();
 auto choices=[&]{while(scene==Scene::Level){render();click({200,410},false);++levels;check(game.validate(),"Invalid level choice");}};
 auto fight=[&]{
  check(game.battle.has_value(),"Campaign encounter missing");int object=game.battle->targetId;int actions=0,rounds=1,cast=0;
  shot("battle-"+std::to_string(object));
  while(game.battle||battleMotion_.active()){
   check(++actions<600,"Campaign battle stuck");
   if(battleMotion_.active()||game.current()->team==1){advance();continue;}
   rounds=game.battle->round;
   if(game.hero.spells[MagicArrow]&&game.hero.mana>=game.spellCost(MagicArrow)&&game.battle->spellRound!=game.battle->round){
    auto target=std::find_if(game.battle->units.begin(),game.battle->units.end(),[](const auto& u){return u.team==1&&u.hp>0;});
    check(target!=game.battle->units.end(),"Campaign spell has no target");Point cell{target->x,target->y};
    key(SDLK_c,KMOD_NONE);render();click({200,135},false);check(pendingSpell_==MagicArrow,"Campaign spellbook did not select Magic Arrow");render();click(battleHexCenter(cell),false);++cast;++spells;
   }else battleAction([&]{game.ai();return std::string{};});
   advance();check(game.validate(),"Campaign battle created invalid state");
  }
  check(scene==Scene::Report&&game.report&&game.report->won,"Campaign encounter was not won");
  auto awaitingReport=game.toJson();check(!game.move({game.hero.pos.x+1,game.hero.pos.y}).empty()&&game.toJson()==awaitingReport,"Unacknowledged victory report allowed exploration");
  encounters.push_back({{"object",object},{"rounds",rounds},{"spells",cast},{"xp",game.report->xp},{"actions",actions}});shot("report-"+std::to_string(object));
  click({582,542},false);choices();
 };
 auto walk=[&](Point goal){
  int attempts=0;
  while(game.hero.pos!=goal){
   check(++attempts<180&&game.day<80,"Campaign navigation stuck at "+std::to_string(game.hero.pos.x)+","+std::to_string(game.hero.pos.y));
   check(scene==Scene::Adventure,"Campaign route has unresolved window");
   // Explore reachable visible frontiers; no fog reveal or hero teleport.
   Point waypoint=goal;auto path=game.path(goal);
   if(path.empty()){
    double best=std::numeric_limits<double>::max();bool found=false;
    for(int y=0;y<MapH;y++)for(int x=0;x<MapW;x++){
     Point p{x,y};if(p==game.hero.pos||!game.tile(p)->seen||!game.passable(p))continue;
     auto* object=game.objectAt(p);if(object&&(object->type==ObjectType::Monster||object->type==ObjectType::Town)&&p!=goal)continue;
     double score=std::hypot(x-goal.x,y-goal.y);if(score>=best)continue;auto candidate=game.path(p);if(candidate.empty())continue;
     best=score;waypoint=p;path=std::move(candidate);found=true;
    }
    check(found,"No reachable frontier in demo map");
   }
   if(game.hero.movement<path.front().cost){int day=game.day;key(SDLK_e,KMOD_NONE);check(game.day==day+1,"Campaign end-turn failed");continue;}
   camX_=game.hero.pos.x+.5f;camY_=game.hero.pos.y+.5f;
   auto map=adventureMapRect();while(path.size()>1){auto p=mapScreen({path.back().x,path.back().y});if(p.x>=map.x&&p.x<map.x+map.w&&p.y>=map.y&&p.y<map.y+map.h)break;path.pop_back();}
   waypoint={path.back().x,path.back().y};
   // A monster route terminates on its controlled approach tile. Click the
   // actual monster, rather than another object's silhouette on that tile.
   if(auto*object=game.objectAt(goal);object&&object->type==ObjectType::Monster){auto screen=mapScreen(goal);if(screen.x>=map.x&&screen.x<map.x+map.w&&screen.y>=map.y&&screen.y<map.y+map.h)waypoint=goal;}
   render();click(mapScreen(waypoint),false);key(SDLK_m,KMOD_NONE);
   auto origin=game.hero.pos;
   for(int steps=0;steps<100&&marching_&&scene==Scene::Adventure;steps++){auto before=game.hero.pos;advance();if(game.hero.pos!=before)++moves;}
   if(scene==Scene::Market){key(SDLK_ESCAPE,KMOD_NONE);marching_=false;}
   if(scene==Scene::Visit||scene==Scene::Puzzle){render();key(SDLK_RETURN,KMOD_NONE);check(scene==Scene::Adventure,"Campaign visit acknowledgement failed");}
   if(game.battle)fight();choices();
   check(game.validate(),"Campaign exploration created invalid state");
   if(game.hero.pos==origin&&scene==Scene::Adventure){marching_=false;key(SDLK_e,KMOD_NONE);}
   if(game.result)break;
  }
  journey.push_back({{"target",{goal.x,goal.y}},{"day",game.day},{"movement",game.hero.movement},{"gold",game.resources[Gold]}});
 };
 battleMotion_.clear();game=Game::create();scene=Scene::Menu;settings_=ClientSettings{};route_.clear();destination_.reset();marching_=false;autoCombat_=false;pendingSpell_=-1;mouseX_=mouseY_=-1;zoom_=1;fixedAnimationTime_=clock;
 render();click({600,80},false);check(scene==Scene::Scenario,"Campaign New Game failed");render();click({630,530},false);check(scene==Scene::Adventure,"Campaign scenario start failed");shot("new-game");
 walk({7,21});walk({4,20});render();click(mapScreen(game.hero.pos),false);check(scene==Scene::Town,"Campaign town entry failed");
 auto build=[&](int id){
  if(game.town.builtDay==game.day){scene=Scene::Adventure;key(SDLK_e,KMOD_NONE);scene=Scene::Town;}
  check(game.constructionState(id)==BuildState::Ready,"Campaign cannot afford/prerequisites building "+std::to_string(id));
  buildingPopup(id,Scene::Build);render();click({310,490},false);check(game.town.built[id],"Campaign building purchase failed");scene=Scene::Town;
 };
 build(MageGuild);townBuilding(MageGuild);render();click({470,530},false);check(game.hero.spellbook&&game.hero.spells[MagicArrow],"Campaign spellbook purchase failed");shot("guild");key(SDLK_ESCAPE,KMOD_NONE);
 build(UpArcherTower);build(UpGuardhouse);
 check(game.upgrade(false,0).find("升级为")!=std::string::npos&&game.upgrade(false,1).find("升级为")!=std::string::npos,"Campaign paid army upgrade failed");
 auto recruit=[&]{for(int unit:{Halberdier,Marksman}){openRecruit(unit,Scene::Town);render();key(SDLK_END,KMOD_NONE);if(recruitAmount_>0)key(SDLK_RETURN,KMOD_NONE);if(scene==Scene::Recruit)key(SDLK_ESCAPE,KMOD_NONE);}};
 recruit();scene=Scene::Adventure;while(game.day<15)key(SDLK_e,KMOD_NONE);scene=Scene::Town;recruit();shot("army-ready");key(SDLK_ESCAPE,KMOD_NONE);
 walk({8,18});walk({11,20});walk({14,19});walk({16,17});walk({21,16});check(game.hero.artifacts.size()==1&&game.hero.attack>=4,"Campaign sword pickup failed");shot("artifact");
 walk({12,16});walk({9,12});walk({17,12});walk({19,12});walk({22,10});walk({24,10});walk({24,8});walk({29,7});
 check(game.result==1&&!game.battle&&!game.report&&scene==Scene::Outcome&&game.hero.levelChoices.empty(),"Demo-map victory did not finish cleanly");
 auto* town=game.objectAt({29,7});check(town&&town->owner==0&&armyCount(town->army)==0,"Demo-map final town was not captured");
 check(encounters.size()==4&&spells>0&&levels>0&&moves>40,"Campaign did not exercise the whole exploration/combat flow");
 shot("map-complete");key(SDLK_ESCAPE,KMOD_NONE);check(scene==Scene::Outcome,"Map result dismissed with Escape");key(SDLK_RETURN,KMOD_NONE);check(scene==Scene::Menu,"Map result acknowledgement failed");scene=Scene::Adventure;shot("conquered-map-wide");scene=Scene::Menu;
 std::ofstream(directory/"campaign-flow-audit.json")<<nlohmann::json{{"seed",1999},{"map","Border practice / 36x28"},{"naturalProgression",true},{"noResourceOrArmyInjection",true},{"fogByExplorationOnly",true},{"completed",true},{"explicitScenarioResultAcknowledged",true},{"day",game.day},{"movementSteps",moves},{"spells",spells},{"levelChoices",levels},{"xp",game.hero.xp},{"resources",game.resources},{"encounters",encounters},{"journey",journey},{"finalState",game.toJson()}}.dump(2)<<'\n';
 render();click({600,80},false);render();click({630,530},false);check(game.result==0&&game.hero.pos==Point{4,20}&&game.hero.artifacts.empty()&&game.hero.xp==0&&toastUntil_==0&&toast_.empty()&&mouseX_<0&&mouseY_<0,"New game after victory retained previous campaign state");
 SDL_SetWindowSize(window_,oldW,oldH);game=Game::create();scene=Scene::Town;fixedAnimationTime_=0;route_.clear();destination_.reset();marching_=false;mouseX_=mouseY_=-1;
}
}
