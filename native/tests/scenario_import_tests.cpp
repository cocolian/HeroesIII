#include "h3/game.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace h3;
namespace {
int checks=0;
void check(bool condition,const std::string&message){if(!condition)throw std::runtime_error(message);++checks;}
}
int main(){try{
 auto root=std::filesystem::path(H3_TEST_SOURCE_ROOT);nlohmann::json library;std::ifstream(root/"assets/demo/map-library.json")>>library;
 int maps=0;std::string error;
 for(const auto&entry:library["entries"]){if(!entry.contains("scenarioFile"))continue;Game game;
  check(game.importScenario(root/entry["scenarioFile"].get<std::string>(),error),entry["id"].get<std::string>()+": "+error);
  check(game.validate()&&game.imported()&&game.mapWidth==entry["width"]&&game.mapHeight==entry["height"]&&game.mapLevels==entry["levels"],"Original map bounds were replaced");
  check(game.scenarioId==entry["id"].get<std::string>()&&game.scenarioRules["sourceSha256"]==entry["sha256"]&&game.decorations.size()>(entry["width"].get<int>()==36?300:1000)&&!game.objects.empty(),"Original scenario identity or objects missing");
  check(std::all_of(game.objects.begin(),game.objects.end(),[](const auto&o){return o.sourceClass!=76||o.sprite.empty();}),"Random resource kept its map-editor placeholder sprite");
  check(std::all_of(game.objects.begin(),game.objects.end(),[](const auto&o){return o.sourceClass<65||o.sourceClass>69||o.sourceSubtype>=7;}),"Random artifact became a spellbook instead of a rolled artifact");
  auto mask=game.movementMask();check(mask.size()==size_t(game.mapWidth*game.mapHeight)&&game.tile(game.hero.pos)->seen,"Start tile or movement grid invalid");
  if(game.scenarioId=="wayfarer-1"){check(game.artifactLandMovement()==600,"Original equipped Boots of Speed lost their movement bonus");}
  if(game.scenarioId=="jedi-story-1"){CombatStack own;own.unit=Crusader;own.team=0;check(game.spellImmune(own,DeathRipple),"Starting Pendant of Life failed to protect the original army");own.team=1;check(!game.spellImmune(own,DeathRipple),"Hero pendant incorrectly protected enemy troops");}
  if(game.scenarioId=="wayfarer-1")check(game.hero.name=="约克"&&game.hero.pos==Point{3,39}&&game.hero.xp==655&&armyCount(game.hero.army)==1&&!game.hasTown(),"Wayfarer original start was replaced by demo state");
  bool moved=false;for(int dy=-1;dy<=1&&!moved;++dy)for(int dx=-1;dx<=1&&!moved;++dx){Point target{game.hero.pos.x+dx,game.hero.pos.y+dy};if(game.canStep(game.hero.pos,target)&&!game.guardingMonster(target)&&(!game.objectAt(target)||(game.objectAt(target)->type!=ObjectType::Monster&&game.objectAt(target)->type!=ObjectType::EnemyHero&&game.objectAt(target)->type!=ObjectType::Gate&&game.objectAt(target)->type!=ObjectType::Portal&&!armyCount(game.objectAt(target)->army)))){auto route=game.path(target);if(!route.empty()){auto previous=game.hero.pos;auto reply=game.move(target);check(game.hero.pos==target&&game.hero.pos!=previous,"Original-map step failed: "+game.scenarioId+" "+reply+" result="+std::to_string(game.result));moved=true;}}}
  check(moved,"Imported hero has no working initial exploration step");
  auto income=game.dailyIncome(),before=game.resources;game.endDay();check(game.day==2,"Imported map cannot end a day");
  if(game.scenarioId=="seven-lakes-1")check(game.resources[Gold]>=before[Gold]+income[Gold]&&game.hasTown(),"Original town income missing");
  if(game.scenarioId=="seven-lakes-1"||game.scenarioId=="wayfarer-1"){
   auto file=std::filesystem::temp_directory_path()/("heroesiii-import-"+game.scenarioId+".json");check(game.save(file,error),"Imported save failed: "+error);Game restored;check(restored.load(file,error)&&restored.toJson()==game.toJson(),"Imported dimensions/layers/objects did not round-trip: "+error);std::filesystem::remove(file);
  }
  std::cout<<game.scenarioId<<": "<<game.mapWidth<<" x "<<game.mapHeight<<", layers "<<game.mapLevels<<", objects "<<game.objects.size()<<", legalStep "<<moved<<'\n';++maps;
 }
 check(maps==15,"Not all downloaded scenarios were checked");
 Game wide;wide.scenarioId="bounds-fixture";wide.mapWidth=wide.mapHeight=72;wide.mapLevels=2;wide.tiles.resize(72*72*2);wide.hero.pos={69,69};wide.hero.army={Stack{Pikeman,10}};for(auto&t:wide.tiles)t.seen=true;
 auto route=wide.path({71,69});check(route.size()==2,"Pathfinding still uses practice-map dimensions");wide.mapLevel=1;wide.tile({70,69})->blocked=true;auto detour=wide.path({71,69});check(!detour.empty()&&std::none_of(detour.begin(),detour.end(),[](const auto&s){return s.x==70&&s.y==69;}),"Underground collision grid ignored");wide.mapLevel=0;check(wide.path({71,69}).size()==2,"Underground blocking leaked to surface");
 Game wayfarer;check(wayfarer.importScenario(root/"assets/demo/scenarios/wayfarer-1.json",error),error);
 // Isolate gate/quest rules from the original guards for these focused fixtures.
 std::erase_if(wayfarer.objects,[](const auto&o){return o.type==ObjectType::Monster;});
 auto portal=std::find_if(wayfarer.objects.begin(),wayfarer.objects.end(),[](const auto&o){return o.type==ObjectType::Portal&&o.sourceClass==103&&o.properties.contains("destination");});check(portal!=wayfarer.objects.end(),"Original underground gates missing");
 auto target=portal->properties["destination"];wayfarer.mapLevel=portal->level;wayfarer.hero.pos=portal->pos;wayfarer.visit(portal->id);check(wayfarer.mapLevel==target["level"]&&wayfarer.hero.pos==Point{target["x"],target["y"]},"Original underground gate did not transfer hero");
 auto gate=std::find_if(wayfarer.objects.begin(),wayfarer.objects.end(),[](const auto&o){return o.type==ObjectType::Gate&&o.properties.contains("quest")&&o.properties["quest"]["type"]==5;});check(gate!=wayfarer.objects.end(),"Original artifact quests missing");int id=gate->id;auto quest=gate->properties["quest"];wayfarer.mapLevel=gate->level;wayfarer.hero.pos=gate->pos;wayfarer.visit(id);check(wayfarer.objectById(id)&&!wayfarer.objectById(id)->visited,"Unmet quest was consumed");for(int artifact:quest["details"]["artifacts"])wayfarer.hero.artifactIds.push_back(artifact);wayfarer.visit(id);check(!wayfarer.objectById(id),"Satisfied artifact quest did not open");
 auto equipment=wayfarer;equipment.hero.wornArtifacts.clear();equipment.hero.equipmentSlots.clear();equipment.hero.artifactIds={7};check(equipment.primarySkill(0)==equipment.hero.attack,"Backpack-only artifact granted equipped stats");equipment.equipPickedArtifact(7);check(equipment.primarySkill(0)==equipment.hero.attack+2,"Equipped basic sword did not grant attack");equipment.hero.artifactIds.push_back(8);equipment.equipPickedArtifact(8);check(equipment.hero.wornArtifacts==std::vector<int>{7},"Occupied equipment slot applied a second sword bonus");
 equipment.hero.artifactIds.push_back(103);equipment.equipPickedArtifact(103);check(equipment.artifactSpellImmune(DeathRipple),"Picked Pendant of Life did not occupy its empty neck slot");
 // Focused economy/combat fixtures retain original map objects and ownership.
 // Troop counts and hero position are controlled; this is not a map walkthrough.
 Game lakes;check(lakes.importScenario(root/"assets/demo/scenarios/seven-lakes-1.json",error),error);std::erase_if(lakes.objects,[](const auto&o){return o.type==ObjectType::Monster;});
 auto mine=std::find_if(lakes.objects.begin(),lakes.objects.end(),[](const auto&o){return o.type==ObjectType::Mine&&o.resource==Ore;});check(mine!=lakes.objects.end(),"Original ore mines missing");int mineId=mine->id;lakes.mapLevel=mine->level;lakes.hero.pos=mine->pos;mine->army={};lakes.visit(mineId);check(lakes.objectById(mineId)->owner==0,"Original ore mine cannot be claimed");auto beforeOre=lakes.resources[Ore];lakes.endDay();check(lakes.resources[Ore]==beforeOre+2,"Imported ore mine failed to produce on the following day");
 auto enemyTown=std::find_if(lakes.objects.begin(),lakes.objects.end(),[](const auto&o){return o.type==ObjectType::Town&&o.owner>0;});check(enemyTown!=lakes.objects.end(),"Original enemy towns missing");int townId=enemyTown->id;enemyTown->army={Stack{Pikeman,20}};lakes.hero.army={Stack{Marksman,100}};lakes.hero.spellbook=true;lakes.hero.spells[MagicArrow]=true;lakes.hero.mana=100;lakes.hero.knowledge=10;lakes.mapLevel=enemyTown->level;lakes.hero.pos=enemyTown->pos;lakes.visit(townId);check(lakes.battle&&lakes.battle->targetId==townId,"Visiting an original guarded town did not start its battle");auto defender=lakes.battle->units.back();check(lakes.cast(MagicArrow,{defender.x,defender.y}).empty()&&lakes.battle->units.back().hp<defender.hp,"Imported combat spell did not reduce defender health");int actions=0;while(lakes.battle&&actions++<200)lakes.ai();check(!lakes.battle&&lakes.report&&lakes.report->won&&lakes.objectById(townId)->owner==0,"Imported town battle failed to finish and capture");check(lakes.result==0,"First imported town capture incorrectly completed the entire map");lakes.report.reset();while(!lakes.hero.levelChoices.empty())lakes.chooseSkill(lakes.hero.levelChoices.front().skills.front());check(lakes.validate(),"State invalid after imported spell, battle and capture");auto earned=lakes.dailyIncome(),beforeDay=lakes.resources;lakes.endDay();check(lakes.day==3&&lakes.resources[Gold]==beforeDay[Gold]+earned[Gold]&&lakes.resources[Ore]==beforeDay[Ore]+earned[Ore],"Multiple imported towns and mines failed to pay next-day income");
 std::cout<<checks<<" scenario checks passed across "<<maps<<" downloaded maps\n";return 0;
 }catch(const std::exception&e){std::cerr<<"Scenario import: "<<e.what()<<'\n';return 1;}}
