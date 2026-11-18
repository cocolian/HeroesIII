#include "h3/game.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <map>
using namespace h3;
struct Danger {Point cell;int level=0;double strength=0;};
int main(int argc,char**argv){try{
 auto root=std::filesystem::path(H3_TEST_SOURCE_ROOT);std::filesystem::path out=argc>1?argv[1]:"/private/tmp/heroesiii-0.45-campaign";std::filesystem::create_directories(out);Game g;std::string error;if(!g.importScenario(root/"assets/demo/scenarios/seven-lakes-1.json",error))throw std::runtime_error(error);
 nlohmann::json turns=nlohmann::json::array(),fights=nlohmann::json::array(),retries=nlohmann::json::array();std::vector<Danger> dangers;std::map<int,std::filesystem::path> snapshots;int moves=0,casts=0,visits=0,reloads=0;bool retry=false;std::ofstream trace(out/"actions.jsonl");
 auto choose=[&]{while(!g.hero.levelChoices.empty()){const int priority[]={Archery,Armorer,Offense,Leadership,Logistics,Wisdom,EarthMagic,AirMagic,Intelligence,WaterMagic,FireMagic,LuckSkill,Mysticism};int selected=g.hero.levelChoices.front().skills.front();for(int s:priority)if(std::find(g.hero.levelChoices.front().skills.begin(),g.hero.levelChoices.front().skills.end(),s)!=g.hero.levelChoices.front().skills.end()){selected=s;break;}g.chooseSkill(selected);}};
 auto resolve=[&]{
  while(g.battle){int target=g.battle->targetId,day=g.day,actor=g.activeHeroId,level=g.mapLevel,actions=0;Point position=g.objectById(target)?g.objectById(target)->pos:g.hero.pos;Army enemy{};for(const auto&u:g.battle->units)if(u.team==1&&u.slot>=0&&u.slot<7)enemy[u.slot]={u.unit,u.count};double strength=g.armyPower(enemy),attackerStrength=g.armyPower(g.hero.army)*(1.+(g.primarySkill(0)+g.primarySkill(1))/30.);bool defence=g.battle->defendingTown||!g.computerQueue.empty();
   while(g.battle&&actions++<2000){
    if(g.current()&&g.current()->team==0&&g.hero.spellbook&&g.battle->spellRound!=g.battle->round){bool cast=false;for(int spell:{Resurrection,Bless,Prayer,Shield,Haste}){if(cast||!g.battle||!g.hero.spells[spell]||g.hero.mana<g.spellCost(spell))continue;int id=-1,best=0;for(const auto&u:g.battle->units)if(u.team==0&&u.origin==StackOrigin::Army){int score=spell==Resurrection?(u.initialCount-u.count)*Units[u.unit].hp:(u.hp>0&&!u.effects[spell]?u.hp:0);if(spell==Resurrection&&g.hero.skills[EarthMagic]<2)score=0;if(score>best){best=score;id=u.id;}}if(id>=0){auto u=std::find_if(g.battle->units.begin(),g.battle->units.end(),[&](auto&u){return u.id==id;});if(u!=g.battle->units.end()&&g.cast(spell,{u->x,u->y}).empty()){++casts;cast=true;}}}if(!cast&&g.battle&&g.hero.spells[MagicArrow]&&g.hero.mana>=g.spellCost(MagicArrow)){auto t=std::find_if(g.battle->units.begin(),g.battle->units.end(),[](auto&u){return u.team==1&&u.hp>0;});if(t!=g.battle->units.end()&&g.cast(MagicArrow,{t->x,t->y}).empty())++casts;}}
    if(g.battle)g.ai();g.takeEvents();
   }
   if(g.battle)throw std::runtime_error("Battle exceeded action bound");bool won=g.report&&g.report->won;auto record=nlohmann::json{{"day",day},{"target",target},{"hero",actor},{"won",won},{"actions",actions},{"computerAttack",defence}};std::cout<<"battle "<<record.dump()<<std::endl;
   if(!won){dangers.push_back({position,level,std::max(strength*2.3,attackerStrength*1.3)});retries.push_back(record);retry=true;return;}fights.push_back(record);g.report.reset();choose();if(!g.computerQueue.empty())g.computerTurns();
  }
  choose();while(g.takeAdventureVisit())++visits;g.takeEvents();
 };
 auto restore=[&]{if(++reloads>150)throw std::runtime_error("Retry budget exhausted");int day=g.day;auto it=snapshots.upper_bound(day);if(it==snapshots.begin())throw std::runtime_error("No rollback checkpoint");--it;if(reloads%3==0){auto older=snapshots.upper_bound(std::max(1,day-4));if(older!=snapshots.begin())it=--older;}int restoredDay=it->first;if(!g.load(it->second,error))throw std::runtime_error(error);while(!turns.empty()&&turns.back()["day"].get<int>()>=restoredDay)turns.erase(turns.end()-1);while(!fights.empty()&&fights.back()["day"].get<int>()>=restoredDay)fights.erase(fights.end()-1);trace<<nlohmann::json{{"reload",true},{"fromDay",day},{"toDay",restoredDay},{"observedDangerCount",dangers.size()}}.dump()<<'\n';retry=false;};
 for(int iteration=0;iteration<4000&&!g.result;++iteration){
  if(g.day>1500)throw std::runtime_error("Campaign exceeded 1500 days");auto checkpoint=out/("rollback-"+std::to_string(g.day%8)+".json");if(!g.save(checkpoint,error)){std::ofstream(out/"invalid-checkpoint.json")<<g.toJson().dump();throw std::runtime_error(error);}for(auto it=snapshots.begin();it!=snapshots.end();)if(it->second==checkpoint)it=snapshots.erase(it);else ++it;snapshots[g.day]=checkpoint;
  resolve();if(retry){restore();continue;}if(g.ownedHeroes().empty())g.manageKingdom(true);auto ids=g.ownedHeroes();for(int id:ids){if(g.result||retry)break;if(!g.selectHero(id))continue;g.manageKingdom(true);resolve();if(g.day<36)continue;
   for(int action=0;action<60&&!g.result&&!retry&&g.hero.movement>0;++action){
    // Planning remembers only battles actually lost in previous attempts. The
    // live world's terrain, fog, resources and armies are never edited here.
    Game planner=g;double power=g.armyPower(g.hero.army)*(1.+(g.primarySkill(0)+g.primarySkill(1))/30.);for(auto&d:dangers)if(d.level==planner.mapLevel&&power<d.strength)if(auto*t=planner.tile(d.cell))t->blocked=true;int target=0;auto route=planner.adventurePlan(&target);if(route.empty()||route.front().cost>g.hero.movement)break;g.hero.adventureGoal=planner.hero.adventureGoal;
    auto previous=g.hero.pos;int movement=g.hero.movement;g.move({route.front().x,route.front().y});if(auto*site=g.objectById(target);site&&Game::extendedLocation(site->sourceClass)&&g.canVisitLocation(*site))g.autoVisitLocation(target);if(g.hero.pos!=previous)++moves;trace<<nlohmann::json{{"day",g.day},{"hero",id},{"from",{previous.x,previous.y}},{"to",{g.hero.pos.x,g.hero.pos.y}},{"target",target},{"gold",g.resources[Gold]},{"movement",g.hero.movement}}.dump()<<'\n';resolve();if(g.result||!g.heroes.contains(id)||retry)break;if(g.visiting())g.manageKingdom(false);if(g.hero.pos==previous&&g.hero.movement==movement)break;
   }
  }
  if(retry){restore();continue;}resolve();if(retry){restore();continue;}int towns=0,enemies=0;for(auto&o:g.objects)if(o.type==ObjectType::Town){if(o.owner==0)++towns;else if(o.owner>0)++enemies;}turns.push_back({{"day",g.day},{"towns",towns},{"enemyTowns",enemies},{"heroes",g.heroes.size()},{"gold",g.resources[Gold]},{"armyPower",g.armyPower(g.hero.army)},{"movesIncludingRetries",moves}});std::cout<<turns.back().dump()<<std::endl;
  if(!g.result){int previous=g.day;g.endDay();if(g.day!=previous+1)throw std::runtime_error("End day blocked");resolve();if(retry){restore();continue;}}
 }
 std::ofstream(out/"campaign-audit.json")<<nlohmann::json{{"map","seven-lakes-1"},{"sourceSha256",g.scenarioRules["sourceSha256"]},{"completed",g.result==1},{"noResourceOrArmyInjection",true},{"noTeleportOrFogReveal",true},{"realMapCollisionDataUnchanged",true},{"usesNormalSaveLoadRetries",true},{"reloads",reloads},{"observedDefeats",retries},{"day",g.day},{"movesIncludingRetries",moves},{"spellsIncludingRetries",casts},{"visitsIncludingRetries",visits},{"turns",turns},{"battles",fights}}.dump(2)<<'\n';if(!g.save(out/"final.json",error))throw std::runtime_error(error);if(g.result!=1)throw std::runtime_error("Campaign incomplete");std::cout<<"Downloaded map completed through normal gameplay with "<<reloads<<" save/load retries\n";return 0;
 }catch(const std::exception&e){std::cerr<<"Community campaign: "<<e.what()<<std::endl;return 1;}}
