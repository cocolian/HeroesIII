#include "h3/game.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace h3;
namespace {
Game fixture(){Game g;std::string error;if(!g.importScenario(std::filesystem::path(H3_TEST_SOURCE_ROOT)/"assets/demo/scenarios/dead-and-buried-1.json",error))throw std::runtime_error(error);g.objects.clear();g.settlements.clear();g.activeTownId=0;g.heroes.clear();g.heroes[g.activeHeroId]={g.hero,0,g.mapLevel};g.scenarioRules["hadEnemy"]=false;g.hero.onBoat=true;g.hero.army={Stack{Pikeman,10}};return g;}
int add(Game&g,int cls){Object o;o.id=90000+int(g.objects.size());o.type=ObjectType::Site;o.sourceClass=cls;o.pos=o.anchor=g.hero.pos;o.level=g.mapLevel;o.name=adventureSiteName(cls);g.objects.push_back(o);return o.id;}
}
int main(int argc,char**argv){int checks=0;auto check=[&](bool ok,const std::string&text){if(!ok)throw std::runtime_error(text);++checks;};try{
 for(int n:{1,2,3,4,9,10,11,1000000}){
  auto g=fixture();int id=add(g,92);g.hero.army={Stack{Pikeman,n}};int before=g.hero.xp,lost=n==1?0:(3*n+9)/10;
  auto reply=g.visit(id);check(g.hero.army[0].count==n-lost,"Sirens rounding/last creature protection failed");
  check(g.hero.xp==before+lost*Units[Pikeman].hp,"Sirens health-to-experience reward failed");
  check(reply.find("尚未实现")==std::string::npos&&g.hasAdventureBonus(g.hero,92),"Sirens missing visit marker");
 }
 auto g=fixture();int siren=add(g,92),second=add(g,92);g.hero.army={Stack{Pikeman,10},Stack{Skeleton,2},Stack{Archangel,1}};int xp=g.hero.xp;g.visit(siren);
 check(g.hero.army[0].count==7&&g.hero.army[1].count==1&&g.hero.army[2].count==1,"Sirens did not apply to all stacks, undead or singleton correctly");
 check(g.hero.xp==xp+3*Units[Pikeman].hp+Units[Skeleton].hp,"Mixed Sirens reward wrong");
 auto state=g.toJson()["hero"];auto rng=g.randomState;g.visit(second);check(g.toJson()["hero"]==state&&g.randomState==rng,"Different Sirens charged a protected hero again");
 Hero other=g.hero;other.adventureBonuses.clear();g.heroes[99000]={other,0,g.mapLevel};int first=g.activeHeroId;
 check(g.selectHero(99000),"Second hero could not activate");g.visit(siren);check(g.hero.army[0].count==4,"Sirens protection leaked between heroes");g.selectHero(first);
 std::string error;auto path=std::filesystem::temp_directory_path()/("heroesiii-services-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".json");
 check(g.save(path,error),error);Game loaded;check(loaded.load(path,error)&&loaded.toJson()==g.toJson(),"Siren visit marker failed save/load");std::filesystem::remove(path);
 g.hero.army={Stack{Marksman,100}};Object enemy;enemy.id=99999;enemy.type=ObjectType::Monster;enemy.pos=g.hero.pos;enemy.level=g.mapLevel;enemy.army={Stack{Skeleton,1}};g.objects.push_back(enemy);g.startBattle(enemy.id);for(int i=0;g.battle&&i<100;++i)g.ai();
 check(g.report&&g.report->won&&!g.hasAdventureBonus(g.hero,92),"Original SoD post-battle Sirens reset failed");g.report.reset();g.hero.levelChoices.clear();int count=g.hero.army[0].count;g.visit(second);check(g.hero.army[0].count==count-(3*count+9)/10,"Sirens could not be revisited after battle");
 g=fixture();int lighthouse=add(g,42);int movement=g.hero.movement,maximum=g.hero.maxMovement;g.visit(lighthouse);
 check(g.objectById(lighthouse)->owner==0&&g.movementAllowance()==2000,"Lighthouse capture or 500 sea bonus failed");
 check(g.hero.movement==movement&&g.hero.maxMovement==maximum,"Lighthouse added movement on capture day");g.visit(lighthouse);check(g.movementAllowance()==2000,"Repeat capture duplicated sea bonus");
 int another=add(g,42);g.visit(another);check(g.movementAllowance()==2500,"Multiple lighthouses did not stack");
 g.hero.onBoat=false;int land=g.movementAllowance();g.objects.clear();check(g.movementAllowance()==land,"Sea bonus affected land movement");
 g.hero.onBoat=true;lighthouse=add(g,42);g.visit(lighthouse);Hero boat=g.hero;g.heroes[99000]={boat,0,g.mapLevel};g.heroes[99001]={boat,1,g.mapLevel};g.hero.pos={5,5};g.hero.movement=42;g.storeHero();
 int enemyLight=add(g,42);g.objectById(enemyLight)->owner=1;
 check(g.movementAllowance(0)==2000&&g.movementAllowance(1)==2000&&g.movementAllowance(2)==1500,"Sea bonuses leaked between players");
 g.day++;g.mapNewDay();check(g.heroes[99000].value.maxMovement==2000&&g.heroes[99000].value.movement==2000,"Inactive friendly hero missed new-day sea bonus");
 check(!g.heroes.contains(99001)||g.heroes[99001].value.maxMovement==2000,"Computer hero received human sea bonus");
 g=fixture();lighthouse=add(g,42);g.visit(lighthouse);g.objectById(lighthouse)->owner=1;
 check(g.movementAllowance(0)==1500&&g.movementAllowance(1)==2000,"Lighthouse capture did not transfer its bonus");
 Object castle;castle.id=99002;castle.type=ObjectType::Town;castle.owner=0;castle.pos=castle.anchor=g.hero.pos;castle.level=g.mapLevel;g.objects.push_back(castle);
 g.activeTownId=castle.id;g.town.faction=0;g.town.objectId=castle.id;g.town.built[Special1]=true;g.settlements[castle.id]=g.town;
 check(g.townDef().buildings[Special1].implemented&&g.movementAllowance(0)==2000&&g.movementAllowance(1)==2000,"Castle lighthouse missing or leaked its bonus to another player");
 g.objectById(lighthouse)->owner=0;check(g.movementAllowance()==2500,"Town and adventure lighthouse bonuses did not combine");
 g=fixture();lighthouse=add(g,42);g.scenarioRules["additional"]["victory_condition"]={{"type",9},{"details",nlohmann::json::object()}};g.checkScenarioVictory();check(!g.result,"Flag-all-mines victory ignored a neutral lighthouse");g.visit(lighthouse);check(g.result==1,"Capturing final lighthouse did not fulfill flag-all-mines victory");
 g=fixture();int den=add(g,97);g.hero.army={Stack{Archangel,3}};g.hero.artifactIds={7,8};g.resources={100,20,30,1,2,3,4};g.playerResources[1]={200,2,3,4,5,6,7};Hero opponent;opponent.army={Stack{Pikeman,3}};g.heroes[99001]={opponent,1,g.mapLevel};
 auto before=g.toJson();auto stats=g.thievesGuild();check(g.toJson()==before,"Reading thieves guild mutated state or RNG");check(stats.size()==2,"Guild omitted active players");
 auto human=std::find_if(stats.begin(),stats.end(),[](auto&p){return p.owner==0;});check(human!=stats.end()&&human->totals[1]==1&&human->totals[2]==100&&human->totals[3]==50&&human->totals[4]==10&&human->totals[6]==2,"Guild used stale active hero/resources");
 check(human->bestHero&&human->bestHero->army[0].unit==Archangel&&human->bestCreature==Archangel,"Guild strongest hero/creature missing");
 g.visit(den);auto notice=g.takeAdventureVisit();check(notice&&notice->sourceClass==97,"Den did not open complete intelligence window");g.visit(den);check(g.takeAdventureVisit().has_value(),"Den could not be revisited");
 if(argc>1){check(loaded.load(argv[1],error),error);int count=0;for(int cls:{97,92,42}){auto copy=loaded;std::erase_if(copy.objects,[](auto&o){return o.type==ObjectType::Monster;});auto it=std::find_if(copy.objects.begin(),copy.objects.end(),[&](auto&o){return o.sourceClass==cls;});check(it!=copy.objects.end(),"Reported map object missing");int id=it->id;copy.hero.pos=it->pos;copy.mapLevel=it->level;copy.hero.levelChoices.clear();copy.hero.onBoat=cls==92;auto response=copy.visit(id);check(response.find("尚未实现")==std::string::npos&&response.find("完整规则")==std::string::npos,"Reported object still unsupported");check(copy.objectById(id)->name==adventureSiteName(cls),"Legacy generic object name not upgraded");++count;}std::cout<<count<<" reported Wayfarer objects verified using a private copy\n";}
 std::cout<<checks<<" map service checks passed\n";return 0;
 }catch(const std::exception&e){std::cerr<<"Map services after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
