#include "h3/game.hpp"
#include "h3/masks.hpp"
#include "h3/audio_mixer.hpp"
#include "h3/battle_layout.hpp"
#include "h3/battle_motion.hpp"
#include "h3/pixel_scaler.hpp"
#include "h3/display_layout.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <set>
#include <limits>
#include <tuple>
#include <chrono>
using namespace h3;
#define CHECK(x) do{if(!(x))throw std::runtime_error(std::string("line ")+std::to_string(__LINE__)+": "+#x);}while(false)
struct TestFiles{
 std::filesystem::path root;
 TestFiles(){auto stamp=std::chrono::high_resolution_clock::now().time_since_epoch().count();for(int n=0;n<1000;n++){auto candidate=std::filesystem::temp_directory_path()/("heroesiii-tests-"+std::to_string(stamp)+"-"+std::to_string(n));if(std::filesystem::create_directory(candidate)){root=std::move(candidate);return;}}throw std::runtime_error("Unable to allocate isolated test directory");}
 ~TestFiles(){std::error_code error;std::filesystem::remove_all(root,error);}
};
std::filesystem::path testDirectory(){static TestFiles files;return files.root;}
Game spellGame(uint32_t seed=1999,int faction=0){auto g=Game::create(seed,faction);g.hero.spellbook=true;g.hero.spells[MagicArrow]=true;return g;}
int actionFrames(int,int group){return group>=11&&group<=16?10:group==3||group==4?4:group==5||group==6?6:group==7||group==8?2:group==0?4:group==2?8:0;}
Game meleeGame(int unit=Pikeman,int count=20,int enemy=Skeleton,int enemies=50){auto g=Game::create();g.hero.army={Stack{unit,count}};g.objectById(15)->army={Stack{enemy,enemies}};g.startBattle(15);g.battle->units[0].x=5;g.battle->units[0].y=5;g.battle->units[1].x=6;g.battle->units[1].y=5;g.battle->queue={0,100};g.takeEvents();return g;}

Game revivalGame(int unit=Pikeman,int count=20){auto g=spellGame();g.hero.skills={};g.hero.power=1;g.hero.knowledge=100;g.hero.mana=1000;g.hero.spells.fill(true);g.hero.army={Stack{Archangel,10},Stack{unit,count},Stack{Griffin,5}};g.objectById(15)->army={Stack{129,1000}};g.startBattle(15);g.battle->units[1].x=7;g.battle->units[1].y=5;g.battle->units[2].x=10;g.battle->units[2].y=5;g.battle->units[3].x=13;g.battle->units[3].y=9;return g;}
void makeCorpse(Game&g,int index,bool acted=false){auto&u=g.battle->units[index];u.hp=u.count=u.permanentCount=0;u.corpse=true;u.deathOrder=++g.battle->deathSequence;u.acted=acted;u.defending=false;u.defendingBonus=0;u.effects={};u.effectMastery={};std::erase(g.battle->queue,u.id);std::erase(g.battle->waiting,u.id);}
void winFixture(Game&g){g.hero.power=1000;g.battle->spellRound=0;auto enemy=g.battle->units[3];CHECK(g.cast(Implosion,{enemy.x,enemy.y}).empty());}

#include "adventure_cases.hpp"
int main(){int passed=0,failed=0;auto run=[&](const char*name,auto test){try{test();passed++;std::cout<<"PASS "<<name<<'\n';}catch(const std::exception&e){failed++;std::cerr<<"FAIL "<<name<<": "<<e.what()<<'\n';}};
adventureCases(run);
run("expanded map uses window points and Retina pixel density",[]{auto d=displayLayout(1512,982,3024,1964,true);CHECK(d.width==1512&&d.height==982);CHECK(d.pointScale==1&&d.pixelScale==2);CHECK(d.pixelWidth==3024&&d.pixelHeight==1964);});
run("wide map expands view instead of scaling classic canvas",[]{auto a=displayLayout(800,600,1600,1200,true),b=displayLayout(1920,1080,3840,2160,true);CHECK(a.pointScale==b.pointScale);CHECK(b.width-208==1712);CHECK(b.height-56==1024);CHECK(b.pixelScale==2);});
run("small window map retains readable minimum layout",[]{auto d=displayLayout(640,480,1280,960,true);CHECK(d.width==800&&d.height==600);CHECK(d.pointScale==.8&&d.pixelScale==1.6);});
run("classic dialogs keep aspect ratio on wide displays",[]{auto d=displayLayout(1920,1080,3840,2160,false);CHECK(d.width==800&&d.height==600);CHECK(d.pointScale==1.8&&d.pixelScale==3.6);CHECK(std::abs(d.pointX-240)<1e-7&&d.pixelX==480);});
run("seeded maps and valid initial native state",[]{auto a=Game::create(),b=Game::create();CHECK(a.toJson()==b.toJson());CHECK(a.validate());CHECK(a.visiting());CHECK(a.hero.skills[Offense]==1);CHECK(a.hero.maxMovement==a.movementAllowance());});
run("pathfinding, movement, mine ownership and cost",[]{auto g=Game::create();auto route=g.path({8,18});CHECK(!route.empty());int total=0;for(auto step:route){total+=step.cost;g.move({step.x,step.y});}CHECK(g.hero.movement==1560-total);CHECK(g.objectAt({8,18})->owner==0);CHECK(g.validate());});
run("diagonal corner cutting and hidden movement are rejected",[]{auto g=Game::create();g.tile({5,20})->decor=Decor::Tree;auto p=g.hero.pos;CHECK(!g.move({5,21}).empty());CHECK(g.hero.pos==p);CHECK(g.path({29,7}).empty());CHECK(g.hero.pos==p);});
run("daily mine production, town income and weekly stock",[]{auto g=Game::create();g.objectAt({8,18})->owner=0;g.objectAt({22,10})->owner=0;g.endDay();CHECK(g.resources[Gold]==11000);CHECK(g.resources[Wood]==22);CHECK(g.resources[Crystal]==6);while(g.day<8)g.endDay();CHECK(g.town.pool[0]==28);CHECK(g.hero.movement==g.movementAllowance());});
run("ore mine capture starts next morning and continues after the hero leaves",[]{
 auto g=Game::create();int id=g.objectAt({4,14})->id;g.hero.pos={4,15};g.tile({4,14})->seen=true;auto before=g.resources;
 g.move({4,14});CHECK(g.objectById(id)->owner==0&&g.resources==before);CHECK(g.log.back().find("从明天起每日产出 2 矿石")!=std::string::npos);
 g.visit(id);CHECK(g.resources==before);g.move({4,15});
 for(int day=2;day<=9;day++){g.endDay();CHECK(g.day==day&&g.resources[Ore]==before[Ore]+2*(day-1));CHECK(g.log.back().find("矿山 +2 矿石")!=std::string::npos);}
 CHECK(g.mineIncome()[Ore]==2&&g.dailyIncome()[Ore]==2);g.objectById(id)->owner=1;int ore=g.resources[Ore];g.endDay();CHECK(g.resources[Ore]==ore);
});
run("all seven mine yields stack by ownership with resource silos",[]{
 auto g=Game::create();g.objects.clear();g.town.built[ResourceSilo]=true;
 for(int resource=0;resource<ResourceCount;resource++)for(int owner:{-1,0,0,1}){Object o;o.id=int(g.objects.size())+1;o.type=ObjectType::Mine;o.resource=resource;o.owner=owner;g.objects.push_back(o);}
 Resources expected{2000,4,4,2,2,2,2};CHECK(g.mineIncome()==expected);auto before=g.resources;auto rng=g.randomState;
 for(int day=2;day<=4;day++){g.endDay();for(int r=0;r<ResourceCount;r++)CHECK(g.resources[r]==before[r]+(day-1)*(expected[r]+g.townDef().silo[r]+(r==Gold?1000:0)));}CHECK(g.randomState==rng);
});
run("pending battle report blocks daily production and resources respect the inventory cap",[]{
 auto g=Game::create();g.hero.pos={4,14};g.visit(g.objectAt({4,14})->id);g.report=BattleReport{};auto before=g.toJson();g.endDay();CHECK(g.toJson()==before);
 g.report.reset();g.resources.fill(999999999);g.endDay();CHECK(g.resources[Ore]==1000000000&&g.resources[Gold]==1000000000&&g.resources[Wood]==999999999);
});
run("one building per day and classic castle dependencies enforced",[]{auto g=Game::create();CHECK(g.build(Portal)=="尚未满足前置建筑要求");CHECK(g.build(GriffinTower)=="尚未满足前置建筑要求");g.build(Blacksmith);CHECK(g.town.built[Blacksmith]);CHECK(g.resources[Gold]==9000);CHECK(g.build(Barracks)=="城镇每天只能建设一次");g.endDay();g.build(Barracks);CHECK(g.town.built[Barracks]);g.endDay();g.build(GriffinTower);CHECK(g.town.built[GriffinTower]);CHECK(g.town.pool[2]==7);});
run("citadel and castle growth multipliers",[]{auto g=Game::create();g.build(Citadel);CHECK(g.weeklyGrowth(0)==21);g.endDay();g.build(Castle);CHECK(g.weeklyGrowth(0)==28);CHECK(g.weeklyGrowth(1)==18);});
run("recruit exact quantity and resource accounting",[]{auto g=Game::create();g.recruit(Pikeman,3);CHECK(g.hero.army[0].count==31);CHECK(g.town.pool[0]==11);CHECK(g.resources[Gold]==9820);auto before=g.toJson();CHECK(!g.recruit(Pikeman,-1).empty());CHECK(g.toJson()==before);});
run("remote recruitment uses garrison and hero requires town visit",[]{auto g=Game::create();g.hero.pos={8,18};CHECK(g.recruit(Pikeman,2)=="英雄必须进入城镇才能接收部队");g.recruit(Pikeman,2,true);CHECK(g.town.garrison[0].count==2);CHECK(g.hero.army[0].count==28);});
run("upgraded dwelling and stack upgrade costs",[]{auto g=Game::create();g.build(UpGuardhouse);g.upgrade(false,0);CHECK(g.hero.army[0].unit==Halberdier);CHECK(g.resources[Gold]==10000-1000-28*15);g.recruit(Halberdier,2);CHECK(g.hero.army[0].count==30);CHECK(g.town.pool[0]==12);});
run("angel gem costs cannot be bypassed",[]{auto g=Game::create();g.town.built[Portal]=true;g.town.pool[6]=4;g.resources[Gold]=100000;g.resources[Gems]=0;CHECK(g.recruitMax(Angel)==0);g.resources[Gems]=2;CHECK(g.recruitMax(Angel)==2);g.recruit(Angel,2);CHECK(g.resources[Gems]==0);CHECK(g.resources[Gold]==94000);});
run("split, merge, swap, seven slots and army conservation",[]{auto g=Game::create();int total=armyCount(g.hero.army);g.transfer(false,0,false,3,10);CHECK(g.hero.army[0].count==18);CHECK(g.hero.army[3].count==10);CHECK(armyCount(g.hero.army)==total);g.transfer(false,3,true,4,5);CHECK(g.town.garrison[4].count==5);g.transfer(false,1,true,4,16);CHECK(g.hero.army[1].unit==Pikeman);CHECK(g.town.garrison[4].unit==Archer);CHECK(armyCount(g.hero.army)+armyCount(g.town.garrison)==total);CHECK(g.validate());});
run("last army unit cannot be merged or moved away",[]{auto g=Game::create();g.hero.army={Stack{Pikeman,1}};g.town.garrison={Stack{Pikeman,5}};auto before=g.toJson();CHECK(g.transfer(false,0,true,0,1)=="英雄至少需要保留一个单位");CHECK(g.toJson()==before);CHECK(g.dismiss(false,0)=="英雄必须保留一支部队");});
run("hero spell replenishment only in town",[]{auto g=Game::create();g.town.built[MageGuild]=true;g.hero.mana=0;g.hero.pos={8,18};g.endDay();CHECK(g.hero.mana==1);g.hero.pos=g.town.pos;g.endDay();CHECK(g.hero.mana==g.maxMana());});
run("experience thresholds and player skill choice",[]{auto g=Game::create();g.gainXP(2000);CHECK(g.hero.level==3);CHECK(g.hero.xp==2000);CHECK(g.hero.levelChoices.size()==2);auto before=g.hero.skills;int id=g.hero.levelChoices.front().skills.front();g.chooseSkill(id);CHECK(g.hero.skills[id]==before[id]+1);CHECK(g.hero.levelChoices.size()==1);CHECK(g.validate());});
run("ranged penalty begins beyond ten hexes",[]{auto g=Game::create();g.hero.attack=0;g.hero.skills={};CombatStack a,t;a.unit=Archer;a.team=0;a.count=10;a.x=1;a.y=5;t.unit=Archer;t.team=1;t.count=10;t.x=11;t.y=5;int near=g.damage(a,t,true,0,.5);t.x=12;int far=g.damage(a,t,true,0,.5);CHECK(far==near/2);});
run("shooters melee penalty and zealot exemption",[]{auto g=Game::create();g.hero.attack=g.hero.defense=0;g.hero.skills={};CombatStack a,t;a.unit=Monk;a.count=10;a.team=0;t.unit=Pikeman;t.team=1;t.x=1;int ranged=g.damage(a,t,true);int melee=g.damage(a,t,false);CHECK(melee==ranged/2);a.unit=Zealot;CHECK(g.damage(a,t,true)==g.damage(a,t,false));});
run("large creatures occupy two cells and cannot overlap",[]{auto g=Game::create();g.startBattle(15);CHECK(g.battle.has_value());auto&u=g.battle->units[2];CHECK(u.unit==Griffin);CHECK(g.footprint(u,{u.x,u.y}).size()==2);CHECK(g.combatAt({u.x-1,u.y})->id==u.id);CHECK(!g.battlePath(u,{0,5}).has_value());CHECK(g.validate());});
run("waiting creates a separate slow-to-fast phase",[]{auto g=Game::create();g.startBattle(15);CHECK(g.current()->unit==Griffin);int fastId=g.current()->id;g.act("wait");CHECK(g.current()->id!=fastId);int ticks=0;while(!g.battle->waitPhase&&ticks++<20)g.act("wait");CHECK(g.battle->waitPhase);CHECK(g.speed(*g.current())==4);CHECK(g.battle->queue.back()==fastId);CHECK(g.act("wait")=="本轮已经等待过");});
run("spell limits, damage and survivor restoration",[]{auto g=spellGame();g.objectById(15)->army={Stack{Skeleton,20}};g.startBattle(15);auto t=g.battle->units.back();g.cast(MagicArrow,{t.x,t.y});CHECK(g.hero.mana==15);CHECK(g.battle->units.back().hp==100);CHECK(g.cast(MagicArrow,{t.x,t.y})=="英雄每轮只能施法一次");CHECK(g.validate());});
run("combat runs to completion and creates casualty report",[]{auto g=Game::create();g.startBattle(15);int steps=0;while(g.battle&&steps++<500){auto*u=g.current();CHECK(u);if(u->team==1){g.ai();continue;}auto target=std::find_if(g.battle->units.begin(),g.battle->units.end(),[](auto&t){return t.team==1&&t.hp>0;});CHECK(target!=g.battle->units.end());Point goal{target->x,target->y};if(g.act("attack",goal).empty())continue;int best=999;Point dest;for(int y=0;y<BattleH;y++)for(int x=0;x<BattleW;x++){auto d=g.battlePath(*u,{x,y});if(d&&*d>0&&*d<=g.speed(*u)){auto candidate=*u;candidate.x=x;candidate.y=y;int n=g.distance(candidate,*target);if(n<best){best=n;dest={x,y};}}}if(best<999)g.act("move",dest);else g.act("defend");}CHECK(steps<500);CHECK(g.report.has_value());CHECK(g.report->won);CHECK(g.hero.xp==180);CHECK(g.validate());});
run("native save round trip including mid-battle state",[]{auto g=Game::create();g.startBattle(15);g.act("wait");auto p=testDirectory()/"heroesiii-native-test-save.json";std::string error;CHECK(g.save(p,error));auto loaded=Game::create(42);CHECK(loaded.load(p,error));CHECK(loaded.toJson()==g.toJson());std::filesystem::remove(p);});
run("corrupt saves are rejected without modifying live game",[]{auto g=Game::create();auto before=g.toJson();auto j=before;j["hero"]["skills"][Intelligence]=99;auto p=testDirectory()/"heroesiii-native-test-bad.json";std::ofstream(p)<<j.dump();std::string error;CHECK(!g.load(p,error));CHECK(g.toJson()==before);j=before;j["hero"]["army"][0]["unit"]=1000;std::ofstream(p)<<j.dump();CHECK(!g.load(p,error));CHECK(g.toJson()==before);std::filesystem::remove(p);});
run("multiple level choices never offer a maxed skill",[]{auto g=Game::create();g.gainXP(10000);int n=0;while(!g.hero.levelChoices.empty()&&n++<30){int id=g.hero.levelChoices.front().skills.front();CHECK(g.hero.skills[id]<3);g.chooseSkill(id);}CHECK(g.hero.levelChoices.empty());CHECK(g.validate());});
run("native unit and building artwork paths exist",[]{std::ifstream f(std::filesystem::path(H3_TEST_SOURCE_ROOT)/"assets/manifest.json");auto m=nlohmann::json::parse(f);auto lower=[](std::string name){for(auto&c:name)c=char(std::tolower(static_cast<unsigned char>(c)));return name;};for(const auto&u:Units)if(u.originalId<145)CHECK(m.contains(lower(u.asset)));std::ifstream demoFile(std::filesystem::path(H3_TEST_SOURCE_ROOT)/"assets/demo/manifest.json");auto demo=nlohmann::json::parse(demoFile);for(const auto&t:TownDefs)for(const auto&layer:t.layers)CHECK((!layer.art.empty()&&m.contains(lower(layer.art)))||demo["def"].contains(lower(layer.animation)));});
run("original atlas frames keep bounded crops, canvases and source pages",[]{
 auto root=std::filesystem::path(H3_TEST_SOURCE_ROOT);std::ifstream file(root/"assets/demo/manifest.json");auto demo=nlohmann::json::parse(file);
 CHECK(demo["pcx"].size()==1224&&demo["def"].size()==2552);int frames=0,available=0;
 for(const auto&unit:Units){CHECK(!unit.animation.empty());if(demo["def"].contains(unit.animation))available++;CHECK(unit.portraitFrame>=0&&unit.portraitFrame<int(demo["def"]["twcrport.def"]["groups"]["0"].size()));const auto&portrait=demo["def"]["twcrport.def"]["groups"]["0"][unit.portraitFrame];auto name=portrait["name"].get<std::string>();CHECK(name.starts_with("CPrL")&&std::stoi(name.substr(4,3))==unit.originalId);}
 CHECK(available==132);CHECK(Units[Pikeman].portraitFrame==2&&Units[44].portraitFrame==48&&Units[46].portraitFrame==46);CHECK(demo["def"].contains(Units[74].animation)&&demo["def"].contains(Units[75].animation));
 for(const auto&animation:demo["def"]){
  const auto&pages=animation["pages"];for(const auto&page:pages)CHECK(std::filesystem::exists(root/page["file"].get<std::string>()));
  for(const auto&sequence:animation["groups"])for(const auto&frame:sequence){
   int page=frame["page"];CHECK(page>=0&&page<int(pages.size()));int x=frame["rect"][0],y=frame["rect"][1],w=frame["rect"][2],h=frame["rect"][3];
   CHECK(x>=0&&y>=0&&w>0&&h>0&&x+w<=pages[page]["width"].get<int>()&&y+h<=pages[page]["height"].get<int>());
   CHECK(frame["offset"][0].get<int>()+w<=frame["canvas"][0].get<int>()&&frame["offset"][1].get<int>()+h<=frame["canvas"][1].get<int>());frames++;
  }
 }
 CHECK(frames==32820);
});
run("verified RoE supplements complete all eight original armies without replacing Complete sprites",[]{
 std::ifstream file(std::filesystem::path(H3_TEST_SOURCE_ROOT)/"assets/demo/manifest.json");auto demo=nlohmann::json::parse(file);
 for(int i=0;i<112;i++){CHECK(demo["def"].contains(Units[i].animation));const auto&groups=demo["def"][Units[i].animation]["groups"];CHECK(!groups["2"].empty()&&!groups["5"].empty());}
 CHECK(demo["def"][Units[Pikeman].animation]["source"]["edition"]=="mac-complete-demo");
 CHECK(demo["def"][Units[19].animation]["source"]["edition"]=="windows-roe-demo");
 CHECK(demo["def"][Units[126].animation]["source"]["edition"]=="windows-roe-demo");
 CHECK(!demo["def"].contains(Units[125].animation));
 int windows=0;for(const char*category:{"pcx","def"})for(const auto&asset:demo[category])if(asset["source"]["edition"]=="windows-roe-demo")windows++;CHECK(windows==150);
});
run("classic cell centers and original pixel hit mask agree",[]{
 CHECK((battleHexCenter({0,0})==Point{58,112}));CHECK((battleHexCenter({0,1})==Point{36,154}));CHECK((battleHexCenter({14,10})==Point{674,532}));
 CHECK(!battleCellContains({0,0})&&!battleCellContains({44,51})&&battleCellContains({22,26}));
 for(int y=0;y<BattleH;y++)for(int x=0;x<BattleW;x++)CHECK((battleHexAt(battleHexCenter({x,y}))==Point{x,y}));
 CHECK(!battleHexAt({36,86})&&!battleHexAt({0,0})&&!battleHexAt({799,599}));
});
run("hex distance matches the original staggered pixel neighbors",[]{
 for(int y=0;y<BattleH;y++)for(int x=0;x<BattleW;x++)for(int yy=0;yy<BattleH;yy++)for(int xx=0;xx<BattleW;xx++){
  auto a=battleHexCenter({x,y}),b=battleHexCenter({xx,yy});int dx=std::abs(a.x-b.x),dy=std::abs(a.y-b.y);bool neighbor=(dy==0&&dx==44)||(dy==42&&dx==22);
  CHECK((Game::hexDistance({x,y},{xx,yy})==1)==neighbor);CHECK(Game::hexDistance({x,y},{xx,yy})==Game::hexDistance({xx,yy},{x,y}));
 }
 CHECK(Game::hexDistance({5,4},{6,5})==1&&Game::hexDistance({5,5},{4,6})==1&&Game::hexDistance({5,4},{4,5})==2);
});
run("ground paths agree with hex distances on an empty classic field",[]{
 auto g=Game::create();g.hero.army={Stack{Pikeman,10}};g.objectById(15)->army={Stack{Pikeman,10}};g.startBattle(15);auto&unit=g.battle->units[0];unit.x=5;unit.y=5;g.battle->units[1].x=14;g.battle->units[1].y=10;
 for(int y=0;y<BattleH;y++)for(int x=0;x<BattleW;x++){if(x==14&&y==10)continue;auto path=g.battlePath(unit,{x,y});CHECK(path&&*path==Game::hexDistance({5,5},{x,y}));}
});
run("formations compact occupied slots and keep wide creatures inside both edges",[]{
 const int loose[7][7]={{5},{2,8},{2,5,8},{0,4,6,10},{0,2,5,8,10},{0,2,4,6,8,10},{0,2,4,5,6,8,10}};
 const int tight[7][7]={{5},{4,6},{4,5,6},{2,4,6,8},{2,4,5,6,8},{0,2,4,6,8,10},{0,2,4,5,6,8,10}};
 for(int count=1;count<=7;count++)for(int rank=0;rank<count;rank++)for(int team=0;team<2;team++)for(int size=1;size<=2;size++)for(bool packed:{false,true}){
  auto p=battleDeployment(team,size,count,rank,packed);CHECK(p.y==(packed?tight:loose)[count-1][rank]);CHECK(p.x==(team?BattleW-size:size-1));
 }
 for(bool packed:{false,true}){auto g=Game::create();g.hero.tightFormation=packed;g.hero.army={Stack{Pikeman,10},{},{},Stack{Griffin,10},{},{},Stack{Archer,10}};g.startBattle(15);for(int rank=0;rank<3;rank++){const auto&unit=g.battle->units[rank];CHECK(unit.slot==rank*3&&unit.y==(packed?tight:loose)[2][rank]);for(auto p:g.footprint(unit,{unit.x,unit.y}))CHECK(p.x>=0&&p.x<BattleW);}CHECK(g.validate());}
});
run("formation saves migrate legacy games without altering deployed stacks",[]{
 auto g=Game::create();g.hero.tightFormation=true;g.startBattle(15);auto path=testDirectory()/"formation-v9.json";std::string error;CHECK(g.save(path,error));Game restored;CHECK(restored.load(path,error)&&restored.toJson()==g.toJson()&&restored.hero.tightFormation);
 auto old=g.toJson();old["version"]=8;while(old["hero"]["skills"].size()>12)old["hero"]["skills"].erase(old["hero"]["skills"].end()-1);old["hero"].erase("tightFormation");std::ofstream(path)<<old.dump();CHECK(restored.load(path,error)&&!restored.hero.tightFormation);CHECK(restored.toJson()["battle"]==g.toJson()["battle"]&&restored.randomState==g.randomState);
 auto before=restored.toJson();old=g.toJson();old["hero"]["tightFormation"]=1;std::ofstream(path)<<old.dump();CHECK(!restored.load(path,error)&&restored.toJson()==before);std::filesystem::remove(path);
});
run("automatic combat can operate either side and creates a valid report",[]{
 auto g=Game::create();g.startBattle(15);int steps=0;while(g.battle&&steps++<500){g.ai();CHECK(g.validate());}CHECK(steps<500&&g.report&&g.validate());
});
run("original CRANIM records cover the classic roster with preserved timing facts",[]{
 CHECK(CreatureAnimationTimings[Pikeman].walkTime==1.15&&CreatureAnimationTimings[Halberdier].walkTime==.9&&CreatureAnimationTimings[Archer].attackClimax==7);
 for(const auto&timing:CreatureAnimationTimings)CHECK(timing.walkTime>=0&&timing.walkTime<=20&&timing.attackTime>=0&&timing.attackTime<=20);
});
run("complete classic creature roster is explicit and internally consistent",[]{CHECK(CreatureUnitCount==141&&UnitCount==145);std::set<int>original;std::array<int,10>counts{};for(int i=0;i<CreatureUnitCount;i++){const auto&u=Units[i];CHECK(original.insert(u.originalId).second);CHECK(u.hp>0&&u.attack>=0&&u.defense>=0&&u.minDamage>0&&u.maxDamage>=u.minDamage&&u.speed>0&&u.cost>0);CHECK(u.size==1||u.size==2);counts[u.faction<0?9:u.faction]++;if(u.faction>=0){CHECK(u.tier==(i%14)/2);CHECK(baseUnit(i)==i-(u.upgraded?1:0));}}for(int i=0;i<9;i++)CHECK(counts[i]==14);CHECK(Units[124].growth==2&&Units[124].cost==1500);CHECK(Units[125].growth==2&&Units[125].cost==2000);});
run("non-gold creature costs preserve resource types",[]{CHECK(unitCost(125)[Mercury]==1);CHECK(unitCost(26)[Crystal]==1);CHECK(unitCost(83)[Sulfur]==2);CHECK(unitCost(140)[Mercury]==20);CHECK(unitCost(13,2)[Gems]==6);});
run("no-retaliation creatures prevent counterattacks",[]{auto g=Game::create();g.hero.army={Stack{73,10}};g.objectById(15)->army={Stack{Pikeman,40}};g.startBattle(15);CHECK(g.current()->team==0);auto&attacker=g.battle->units[0];auto&target=g.battle->units[1];attacker.x=3;attacker.y=4;target.x=4;target.y=4;int hp=attacker.hp;CHECK(g.act("attack",{4,4}).empty());CHECK(g.battle->units[0].hp==hp);CHECK(g.battle->units[1].retaliations==0);});
run("spell immunity rejects invalid target before consuming mana",[]{auto g=spellGame();g.objectById(15)->army={Stack{83,2}};g.startBattle(15);while(g.current()->team==1)g.act("defend");auto target=g.battle->units.back();int mana=g.hero.mana;CHECK(g.cast(MagicArrow,{target.x,target.y})=="该单位免疫这一等级的魔法");CHECK(g.hero.mana==mana);CHECK(g.battle->spellRound==0);});
run("golems reduce magical damage but still accept spells",[]{auto g=spellGame();g.objectById(15)->army={Stack{32,10}};g.startBattle(15);while(g.current()->team==1)g.act("defend");auto target=g.battle->units.back();int hp=target.hp;CHECK(g.cast(MagicArrow,{target.x,target.y}).empty());CHECK(g.battle->units.back().hp==hp-10);CHECK(g.hero.mana==15);});
run("version-one saves migrate old neutral identifiers without losing armies",[]{auto g=spellGame();auto old=g.toJson();old["version"]=1;while(old["hero"]["spells"].size()>9)old["hero"]["spells"].erase(old["hero"]["spells"].end()-1);while(old["hero"]["skills"].size()>8)old["hero"]["skills"].erase(old["hero"]["skills"].end()-1);while(old["town"]["built"].size()>27)old["town"]["built"].erase(old["town"]["built"].end()-1);old["town"].erase("faction");old["town"].erase("stableWeek");old["town"].erase("lessons");for(auto&o:old["objects"])for(auto&s:o["army"]){if(s["unit"]==Skeleton)s["unit"]=14;else if(s["unit"]==Wolf)s["unit"]=15;else if(s["unit"]==Orc)s["unit"]=16;}auto p=testDirectory()/"heroesiii-v1-migration.json";std::ofstream(p)<<old.dump();Game restored;std::string error;CHECK(restored.load(p,error));CHECK(restored.toJson()==g.toJson());std::filesystem::remove(p);});
run("large stack costs saturate without signed overflow",[]{auto c=unitCost(140,1000000);CHECK(c[Gold]==std::numeric_limits<int>::max());CHECK(c[Mercury]==20000000);});
run("regeneration heals only the top creature once when a new round begins",[]{auto g=Game::create();g.hero.army={Stack{60,3}};g.objectById(15)->army={Stack{Pikeman,10}};g.startBattle(15);CHECK(g.current()->unit==60);auto&u=g.battle->units[0];u.hp=50;u.count=3;g.act("wait");CHECK(g.battle->units[0].hp==50);g.act("defend");CHECK(g.battle->units[0].hp==50);g.act("defend");CHECK(g.battle->round==2);CHECK(g.battle->units[0].hp==54);CHECK(g.battle->units[0].count==3);CHECK(g.validate());});
run("wraith drains hero mana on its first turn",[]{auto g=Game::create();g.objectById(15)->army={Stack{61,3}};g.tile(g.objectById(15)->pos)->terrain=Terrain::Dirt;g.startBattle(15);CHECK(g.current()->unit==61);CHECK(g.hero.mana==20-Units[61].manaDrain);CHECK(Units[61].manaDrain==2);});
run("all original creatures can enter a legal battle without overlap",[]{for(int unit=0;unit<CreatureUnitCount;unit++){auto g=Game::create();g.hero.army={Stack{unit,10}};g.objectById(15)->army={Stack{Skeleton,100}};g.startBattle(15);CHECK(g.validate());CHECK(g.current());for(int i=0;i<4&&g.battle;i++){if(g.current()->team==1)g.ai();else g.act("defend");CHECK(g.validate());}}});
run("all nine faction backgrounds and map sprites are available",[]{std::ifstream f(std::filesystem::path(H3_TEST_SOURCE_ROOT)/"assets/manifest.json");auto m=nlohmann::json::parse(f);auto lower=[](std::string value){for(auto&c:value)c=char(std::tolower(static_cast<unsigned char>(c)));return value;};for(const auto&faction:Factions){CHECK(m.contains(lower(faction.background)));CHECK(m.contains(lower(faction.mapSprite)));}});
run("corrupted casualty report is rejected before UI rendering",[]{auto g=spellGame();g.objectById(15)->army={Stack{Skeleton,1}};g.startBattle(15);auto t=g.battle->units.back();CHECK(g.cast(MagicArrow,{t.x,t.y}).empty());CHECK(g.report.has_value());auto before=g.toJson(),bad=before;bad["report"]["before"][0]["unit"]=10000;auto file=testDirectory()/"heroesiii-bad-report.json";std::ofstream(file)<<bad.dump();std::string error;CHECK(!g.load(file,error));CHECK(g.toJson()==before);std::filesystem::remove(file);});
run("nine faction starts recruit their own base and upgraded creatures",[]{for(int f=0;f<FactionCount;f++){auto g=Game::create(1999,f);CHECK(g.validate());CHECK(g.hero.army[0].unit==f*14);CHECK(g.recruitableUnits()[0]==f*14);int gold=g.resources[Gold],pool=g.town.pool[0],count=g.hero.army[0].count;g.recruit(f*14,1);CHECK(g.hero.army[0].count==count+1);CHECK(g.town.pool[0]==pool-1);CHECK(g.resources[Gold]==gold-Units[f*14].cost);g.build(UpGuardhouse);CHECK(g.town.built[UpGuardhouse]);CHECK(g.recruitableUnits()[0]==f*14+1);g.upgrade(false,0);CHECK(g.hero.army[0].unit==f*14+1);CHECK(g.validate());}});
run("recruitment and upgrades reject units from another faction",[]{auto g=Game::create(1999,1);g.town.built[UpGuardhouse]=true;CHECK(g.recruitMax(Pikeman)==0);g.hero.army[0]={Pikeman,10};auto before=g.toJson();CHECK(g.upgrade(false,0)=="该部队无法在城镇升级");CHECK(g.toJson()==before);});
run("all 126 dwelling costs and prerequisites are retained per faction",[]{for(int f=0;f<FactionCount;f++){const auto&t=TownDefs[f];for(int tier=0;tier<7;tier++){CHECK(t.buildings[Guardhouse+tier].unit==f*14+tier*2);CHECK(t.buildings[Guardhouse+tier].cost[Gold]>0);CHECK(t.buildings[UpGuardhouse+tier].cost[Gold]>0);CHECK(!t.buildings[UpGuardhouse+tier].requirements.empty());for(const auto&requirements:t.buildings[UpGuardhouse+tier].requirements)CHECK(std::find(requirements.begin(),requirements.end(),Guardhouse+tier)!=requirements.end());}CHECK(!t.buildings[MageGuild5].available||(t.maxMageGuild==5));}auto g=Game::create(1999,7);g.town.built[Monastery]=true;CHECK(!g.buildingRequirementsMet(UpMonastery));g.town.built[ResourceSilo]=true;CHECK(g.buildingRequirementsMet(UpMonastery));});
run("all faction recruits consume every required resource",[]{for(int f=0;f<FactionCount;f++){auto g=Game::create(1999,f);g.resources.fill(1000000);g.town.built.fill(true);g.town.garrison={};for(int tier=0;tier<7;tier++){g.town.pool[tier]=5;int unit=f*14+tier*2+1;auto before=g.resources,cost=unitCost(unit,2);g.recruit(unit,2,true);CHECK(g.town.garrison[tier].unit==unit&&g.town.garrison[tier].count==2);for(int r=0;r<ResourceCount;r++)CHECK(g.resources[r]==before[r]-cost[r]);}CHECK(g.validate());}});
run("horde growth is added after fortification multipliers",[]{auto g=Game::create();g.town.built[Castle]=g.town.built[GriffinBastion]=true;CHECK(g.weeklyGrowth(2)==17);CHECK(Buildings[GriffinBastion].cost[Gold]==1000);CHECK(Buildings[GriffinBastion].cost[Ore]==0);g=Game::create(1999,1);g.town.built[Castle]=g.town.built[GriffinBastion]=g.town.built[Horde2]=true;CHECK(g.weeklyGrowth(1)==20);CHECK(g.weeklyGrowth(4)==8);});
run("all nine resource silos produce the correct resource",[]{for(int f=0;f<FactionCount;f++){auto g=Game::create(1999,f);g.town.built[ResourceSilo]=true;auto before=g.resources;g.endDay();for(int r=0;r<ResourceCount;r++)CHECK(g.resources[r]==before[r]+g.townDef().silo[r]+(r==Gold?g.income():0));CHECK(g.validate());}});
run("stables grants 400 movement once per week and expires away from town",[]{auto g=Game::create();int base=g.movementAllowance();g.town.built[Special2]=true;g.visitTownBuildings();CHECK(g.hero.movement==base+400);CHECK(g.movementAllowance()==base+400);g.visitTownBuildings();CHECK(g.hero.movement==base+400);g.hero.pos={8,18};while(g.day<8)g.endDay();CHECK(g.movementAllowance()==base);g.hero.pos=g.town.pos;g.visitTownBuildings();CHECK(g.hero.movement==base+400);CHECK(g.validate());});
run("town permanent lessons are applied once and survive save load",[]{for(int f:{2,3,5,6,7}){auto g=Game::create(1999,f);int slot=f==7?Special1:Special4;g.town.built[slot]=true;auto before=g.hero;g.visitTownBuildings();CHECK((f==2&&g.hero.knowledge==before.knowledge+1)||(f==3&&g.hero.power==before.power+1)||(f==5&&g.hero.xp==before.xp+1000)||(f==6&&g.hero.attack==before.attack+1)||(f==7&&g.hero.defense==before.defense+1));auto state=g.toJson();g.visitTownBuildings();CHECK(g.toJson()==state);auto p=testDirectory()/"heroesiii-town-lesson.json";std::string error;CHECK(g.save(p,error));Game loaded;CHECK(loaded.load(p,error));CHECK(loaded.toJson()==state);std::filesystem::remove(p);}});
run("faction saves preserve town rules and reject invalid faction indices",[]{for(int f=0;f<FactionCount;f++){auto g=Game::create(1999,f);auto p=testDirectory()/"heroesiii-faction.json";std::string error;CHECK(g.save(p,error));Game restored;CHECK(restored.load(p,error));CHECK(restored.toJson()==g.toJson());auto invalid=g.toJson();invalid["town"]["faction"]=9;std::ofstream(p)<<invalid.dump();CHECK(!restored.load(p,error));CHECK(restored.toJson()==g.toJson());std::filesystem::remove(p);}});
run("behemoths ignore 40 or 80 percent of total defense for their attack",[]{auto g=Game::create();g.hero.attack=0;g.hero.skills={};CombatStack a,t;a.team=0;a.count=1;t.team=1;t.unit=Archangel;t.count=1;a.unit=96;CHECK(Units[a.unit].enemyDefenseReduction==40);CHECK(g.damage(a,t,false,0,.5)==39);a.unit=97;CHECK(Units[a.unit].enemyDefenseReduction==80);CHECK(g.damage(a,t,false,0,.5)==66);});
run("hatred adds to the classic attack multiplier",[]{auto g=Game::create();g.hero.attack=0;g.hero.skills={};CombatStack a,t;a.unit=Archangel;a.team=0;a.count=1;t.unit=55;t.team=1;t.count=1;CHECK(g.damage(a,t,false)==80);CHECK(Units[114].hates.size()==2);});
run("sharpshooters ignore distance while normal archers retain the penalty",[]{auto g=Game::create();g.hero.attack=0;g.hero.skills={};CombatStack a,t;a.unit=136;a.team=0;a.count=10;a.x=1;a.y=5;t.unit=Pikeman;t.team=1;t.x=5;t.y=5;int near=g.damage(a,t,true);t.x=13;CHECK(g.damage(a,t,true)==near);a.unit=Archer;int far=g.damage(a,t,true);t.x=5;CHECK(far==g.damage(a,t,true)/2);});
run("spell cost auras use original army sources even after casualties and never stack",[]{auto g=Game::create();g.hero.army={Stack{34,2},Stack{35,2}};g.objectById(15)->army={Stack{20,2},Stack{21,2}};g.startBattle(15);CHECK(g.spellCost(MagicArrow)==5);for(auto&u:g.battle->units)if(u.team==1)u.hp=u.count=0;CHECK(g.spellCost(MagicArrow)==5);for(auto&u:g.battle->units)if(u.team==0)u.hp=u.count=0;CHECK(g.spellCost(MagicArrow)==5);});
run("mage discount is applied when checking and consuming mana",[]{auto g=spellGame();g.hero.army={Stack{34,2}};g.objectById(15)->army={Stack{Skeleton,100}};g.startBattle(15);g.hero.mana=3;auto target=g.battle->units.back();CHECK(g.cast(MagicArrow,{target.x,target.y}).empty());CHECK(g.hero.mana==0);CHECK(g.validate());});
run("elemental spell immunities and vulnerabilities affect actual casting",[]{auto g=spellGame();g.hero.spells[LightningBolt]=true;g.objectById(15)->army={Stack{114,10}};g.startBattle(15);while(g.current()->team==1)g.act("defend");auto t=g.battle->units.back();int hp=t.hp;CHECK(g.cast(LightningBolt,{t.x,t.y}).empty());CHECK(g.battle->units.back().hp==hp-70);g=spellGame();g.hero.spells[LightningBolt]=true;g.objectById(15)->army={Stack{120,10}};g.startBattle(15);while(g.current()->team==1)g.act("defend");t=g.battle->units.back();CHECK(g.spellImmune(t,LightningBolt));CHECK(!g.cast(LightningBolt,{t.x,t.y}).empty());CHECK(g.hero.mana==20);CHECK(g.battle->spellRound==0);});
run("fire immunity includes curse and magic arrow",[]{auto g=Game::create();CombatStack t;t.unit=52;CHECK(g.spellImmune(t,Curse));CHECK(g.spellImmune(t,MagicArrow));t.unit=118;CHECK(g.spellImmune(t,Curse));CHECK(g.spellImmune(t,MagicArrow));});
run("undead reject bless and curse while retaining cure and haste",[]{auto g=Game::create();for(int unit=0;unit<UnitCount;unit++)if(Units[unit].undead){CombatStack t;t.unit=unit;CHECK(g.spellImmune(t,Bless));CHECK(g.spellImmune(t,Curse));CHECK(!g.spellImmune(t,Cure));CHECK(!g.spellImmune(t,Haste));}});
run("classic damage samples at most ten individual integer rolls",[]{for(auto[count,expected]:std::array<std::pair<int,int>,3>{{{1,1},{10,17},{100,175}}}){auto g=Game::create();g.hero.attack=0;g.hero.skills={};g.hero.army={Stack{Pikeman,count}};g.objectById(15)->army={Stack{Pikeman,100}};g.startBattle(15);auto&a=g.battle->units[0];auto&t=g.battle->units[1];a.effects[Sorrow]=10;a.x=2;a.y=5;t.x=3;t.y=5;t.retaliations=1;int hp=t.hp;g.randomState=1999;CHECK(g.act("attack",{3,5}).empty());CHECK(g.battle->units[1].hp==hp-expected);if(count>=10)CHECK(g.randomState==1809624993u);CHECK(g.validate());}});
run("experience accumulation saturates within the save domain",[]{auto g=Game::create();g.hero.xp=999999990;g.gainXP(1000000000);CHECK(g.hero.xp==1000000000);CHECK(g.validate());});

run("school mastery selects the best matching school and lowers only relevant spell costs",[]{auto g=Game::create();CHECK(g.spellCost(MagicArrow)==5);g.hero.skills[FireMagic]=2;CHECK(g.spellMastery(MagicArrow)==2);CHECK(g.spellCost(MagicArrow)==4);CHECK(g.spellCost(LightningBolt)==10);g.hero.skills[AirMagic]=3;CHECK(g.spellMastery(MagicArrow)==3);CHECK(g.spellMastery(LightningBolt)==3);CHECK(g.spellCost(LightningBolt)==8);CHECK(!g.spellMass(MagicArrow));CHECK(g.spellMass(Haste));});
run("direct magic damage uses each classic proficiency constant",[]{for(int spell:{MagicArrow,LightningBolt})for(int mastery=0;mastery<4;mastery++){auto g=spellGame();g.hero.skills[AirMagic]=mastery;g.hero.spells[spell]=true;g.objectById(15)->army={Stack{Skeleton,100}};g.startBattle(15);auto target=g.battle->units.back();int hp=target.hp;CHECK(g.cast(spell,{target.x,target.y}).empty());int expected=spell==MagicArrow?std::array<int,4>{20,20,30,40}[mastery]:std::array<int,4>{35,35,45,75}[mastery];CHECK(g.battle->units.back().hp==hp-expected);CHECK(g.hero.mana==20-(mastery?Spells[spell].skilledCost:Spells[spell].cost));CHECK(g.validate());}});
run("haste records cast mastery instead of reading subsequent hero skill changes",[]{auto g=spellGame();g.hero.skills[AirMagic]=2;g.hero.spells[Haste]=true;g.hero.power=2;g.startBattle(15);auto t=g.battle->units[0];CHECK(g.cast(Haste,{t.x,t.y}).empty());CHECK(g.speed(g.battle->units[0])==Units[t.unit].speed+g.nativeTerrainBonus(g.battle->units[0])+5);CHECK(g.battle->units[0].effectMastery[Haste]==2);g.hero.skills[AirMagic]=0;CHECK(g.speed(g.battle->units[0])==Units[t.unit].speed+g.nativeTerrainBonus(g.battle->units[0])+5);CHECK(g.validate());});
run("advanced slow uses classic fifty percent and rounds odd speed down",[]{auto g=spellGame();g.hero.army={Stack{Archangel,1}};g.objectById(15)->army={Stack{RoyalGriffin,100}};g.hero.skills[EarthMagic]=2;g.hero.spells[Slow]=true;g.startBattle(15);auto t=g.battle->units.back();CHECK(g.cast(Slow,{t.x,t.y}).empty());CHECK(g.speed(g.battle->units.back())==5);CHECK(g.hero.mana==15);});
run("expert slow selects all eligible enemy stacks without requiring a clicked target",[]{auto g=spellGame();g.hero.army={Stack{Archangel,1}};g.objectById(15)->army={Stack{Pikeman,100},Stack{RoyalGriffin,100},Stack{83,10}};g.hero.skills[EarthMagic]=3;g.hero.spells[Slow]=true;g.startBattle(15);CHECK(g.spellTargets(Slow).size()==2);CHECK(g.cast(Slow).empty());CHECK(g.battle->units[0].effects[Slow]==0);CHECK(g.battle->units[1].effects[Slow]>0);CHECK(g.battle->units[2].effects[Slow]>0);CHECK(g.battle->units[3].effects[Slow]==0);CHECK(g.hero.mana==15);CHECK(g.battle->spellRound==1);CHECK(g.validate());});
run("expert curse excludes undead and fire immune creatures",[]{auto g=spellGame();g.hero.army={Stack{Archangel,1}};g.objectById(15)->army={Stack{Pikeman,100},Stack{Skeleton,100},Stack{52,10},Stack{83,10}};g.hero.skills[FireMagic]=3;g.hero.spells[Curse]=true;g.startBattle(15);CHECK(g.spellTargets(Curse).size()==1);CHECK(g.cast(Curse).empty());CHECK(g.battle->units[1].effectMastery[Curse]==3);for(int i=2;i<5;i++)CHECK(g.battle->units[i].effects[Curse]==0);CHECK(g.hero.mana==15);CHECK(g.validate());});
run("mass spells with no eligible targets do not spend mana or consume a cast",[]{auto g=spellGame();g.hero.army={Stack{Archangel,1}};g.objectById(15)->army={Stack{83,1}};g.hero.skills[EarthMagic]=3;g.hero.spells[Slow]=true;g.startBattle(15);auto before=g.toJson();CHECK(!g.cast(Slow).empty());CHECK(g.toJson()==before);});
run("hostile mass spell resistance is rolled separately for every stack",[]{auto g=spellGame();g.hero.army={Stack{Archangel,1}};g.objectById(15)->army={Stack{16,10},Stack{16,10}};g.hero.skills[EarthMagic]=3;g.hero.spells[Slow]=true;g.startBattle(15);g.randomState=1999;CHECK(g.cast(Slow).empty());CHECK(g.battle->units[1].effects[Slow]==0);CHECK(g.battle->units[2].effects[Slow]>0);CHECK(g.hero.mana==15);CHECK(g.validate());});
run("advanced shield and stone skin affect damage at their captured strengths",[]{auto g=spellGame();g.hero.army={Stack{Pikeman,10}};g.objectById(15)->army={Stack{Pikeman,100}};g.hero.defense=0;g.hero.skills={};g.hero.skills[EarthMagic]=2;g.hero.spells[Shield]=true;g.startBattle(15);auto target=g.battle->units[0],attacker=g.battle->units[1];attacker.count=10;CHECK(g.damage(attacker,target,false)==19);CHECK(g.cast(Shield,{target.x,target.y}).empty());target=g.battle->units[0];CHECK(g.damage(attacker,target,false)==13);target.effects[Shield]=target.effectMastery[Shield]=0;target.effects[StoneSkin]=2;target.effectMastery[StoneSkin]=2;CHECK(g.spellEffect(target,StoneSkin)==6);CHECK(g.damage(attacker,target,false)==16);});
run("advanced bless increases actual ranged base damage by one per creature",[]{auto g=spellGame();g.hero.army={Stack{Archer,10}};g.objectById(15)->army={Stack{Pikeman,100}};g.hero.attack=0;g.hero.skills={};g.hero.skills[WaterMagic]=2;g.hero.spells[Bless]=true;g.startBattle(15);auto&a=g.battle->units[0];auto&t=g.battle->units[1];a.x=1;a.y=5;t.x=5;t.y=5;CHECK(g.cast(Bless,{1,5}).empty());CHECK(g.act("attack",{5,5}).empty());CHECK(g.battle->units[1].hp==1000-42);CHECK(g.validate());});
run("advanced curse uses minimum minus one without an extra twenty percent penalty",[]{auto g=spellGame();g.hero.army={Stack{Archangel,1}};g.objectById(15)->army={Stack{Swordsman,100}};g.hero.defense=0;g.hero.skills={};g.hero.skills[FireMagic]=2;g.hero.spells[Curse]=true;g.startBattle(15);auto t=g.battle->units.back();CHECK(g.cast(Curse,{t.x,t.y}).empty());auto attacker=g.battle->units.back();attacker.count=10;CombatStack target;target.unit=Pikeman;target.team=0;CHECK(g.damage(attacker,target,false)==62);attacker.unit=Pikeman;CHECK(g.damage(attacker,target,false)>=1);});
run("expert cure heals only surviving creatures and clears negative mastery",[]{auto g=spellGame();g.hero.army={Stack{Pikeman,10},Stack{Archer,10},Stack{83,1}};g.hero.skills[WaterMagic]=3;g.hero.spells[Cure]=true;g.startBattle(15);auto&a=g.battle->units[0];a.count=a.permanentCount=8;a.hp=75;a.effects[Slow]=3;a.effectMastery[Slow]=2;auto&b=g.battle->units[1];b.count=b.permanentCount=5;b.hp=46;b.effects[Curse]=3;b.effectMastery[Curse]=2;CHECK(g.spellTargets(Cure).size()==2);CHECK(g.cast(Cure).empty());CHECK(a.count==8&&a.hp==80);CHECK(b.count==5&&b.hp==50);CHECK(a.effects[Slow]==0&&a.effectMastery[Slow]==0);CHECK(b.effects[Curse]==0&&b.effectMastery[Curse]==0);CHECK(g.hero.mana==15);CHECK(g.validate());});
run("cure without wounds or negative effects is rejected before payment",[]{auto g=spellGame();g.hero.spells[Cure]=true;g.startBattle(15);auto t=g.battle->units[0];auto before=g.toJson();CHECK(!g.cast(Cure,{t.x,t.y}).empty());CHECK(g.toJson()==before);});
run("counter spells clear both duration and captured mastery",[]{auto g=spellGame();g.hero.skills[AirMagic]=3;g.hero.spells[Haste]=true;g.startBattle(15);auto&t=g.battle->units[0];t.effects[Slow]=4;t.effectMastery[Slow]=2;CHECK(g.cast(Haste).empty());CHECK(t.effects[Slow]==0&&t.effectMastery[Slow]==0);CHECK(t.effectMastery[Haste]==3);CHECK(g.validate());});
run("a speed spell reorders remaining troops while keeping the current turn",[]{auto g=spellGame();g.hero.army={Stack{Monk,10},Stack{Pikeman,10}};g.objectById(15)->army={Stack{Pikeman,100}};g.hero.skills[AirMagic]=2;g.hero.spells[Haste]=true;g.startBattle(15);int active=g.current()->id;auto t=g.battle->units[1];CHECK(g.cast(Haste,{t.x,t.y}).empty());CHECK(g.current()->id==active);g.act("defend");CHECK(g.current()->id==t.id);CHECK(g.validate());});
run("timed spell mastery expires at round transition and survives midbattle save load",[]{auto g=spellGame();g.hero.army={Stack{Pikeman,10}};g.objectById(15)->army={Stack{Pikeman,100}};g.hero.skills[AirMagic]=2;g.hero.spells[Haste]=true;g.startBattle(15);auto t=g.battle->units[0];CHECK(g.cast(Haste,{t.x,t.y}).empty());auto p=testDirectory()/"heroesiii-spell-mastery.json";std::string error;CHECK(g.save(p,error));Game loaded;CHECK(loaded.load(p,error));CHECK(loaded.toJson()==g.toJson());loaded.act("defend");loaded.act("defend");CHECK(loaded.battle->round==2);CHECK(loaded.battle->units[0].effects[Haste]==0);CHECK(loaded.battle->units[0].effectMastery[Haste]==0);CHECK(loaded.speed(loaded.battle->units[0])==Units[Pikeman].speed+loaded.nativeTerrainBonus(loaded.battle->units[0]));CHECK(loaded.validate());std::filesystem::remove(p);});
run("version three saves migrate eight skills and legacy effect strengths",[]{auto g=spellGame();g.hero.spells[Haste]=true;g.startBattle(15);auto t=g.battle->units[0];CHECK(g.cast(Haste,{t.x,t.y}).empty());auto old=g.toJson();old["version"]=3;while(old["hero"]["spells"].size()>9)old["hero"]["spells"].erase(old["hero"]["spells"].end()-1);for(auto&unit:old["battle"]["units"]){while(unit["effects"].size()>9)unit["effects"].erase(unit["effects"].end()-1);unit.erase("defenseReduction");unit.erase("defendingBonus");}while(old["hero"]["skills"].size()>8)old["hero"]["skills"].erase(old["hero"]["skills"].end()-1);for(auto&unit:old["battle"]["units"])unit.erase("effectMastery");auto p=testDirectory()/"heroesiii-v3-spells.json";std::ofstream(p)<<old.dump();Game loaded;std::string error;CHECK(loaded.load(p,error));CHECK(loaded.toJson()==g.toJson());CHECK(loaded.speed(loaded.battle->units[0])==Units[Pikeman].speed+loaded.nativeTerrainBonus(loaded.battle->units[0])+3);std::filesystem::remove(p);});
run("corrupted captured mastery is rejected without modifying live game",[]{auto g=Game::create();g.startBattle(15);auto before=g.toJson(),bad=before;bad["battle"]["units"][0]["effectMastery"][Haste]=4;auto p=testDirectory()/"heroesiii-bad-mastery.json";std::ofstream(p)<<bad.dump();std::string error;CHECK(!g.load(p,error));CHECK(g.toJson()==before);bad=before;bad["battle"]["units"][0]["effectMastery"].push_back(0);std::ofstream(p)<<bad.dump();CHECK(!g.load(p,error));CHECK(g.toJson()==before);bad=before;bad["battle"]["spellRound"]=2;std::ofstream(p)<<bad.dump();CHECK(!g.load(p,error));CHECK(g.toJson()==before);std::filesystem::remove(p);});
run("magic university teaches each basic school for classic gold-only cost",[]{auto g=Game::create(1999,8);g.build(MageGuild);g.endDay();g.build(Special2);CHECK(g.town.built[Special2]);g.resources[Gold]=10000;auto resources=g.resources;for(int skill=AirMagic;skill<=EarthMagic;skill++){g.learnMagic(skill);CHECK(g.hero.skills[skill]==1);}CHECK(g.resources[Gold]==2000);for(int r=Wood;r<ResourceCount;r++)CHECK(g.resources[r]==resources[r]);auto before=g.toJson();g.learnMagic(AirMagic);CHECK(g.toJson()==before);CHECK(g.validate());});
run("university enforces eight skills, town visit and sufficient gold",[]{auto g=Game::create(1999,8);g.town.built[Special2]=true;for(int i=0;i<8;i++)g.hero.skills[i]=1;auto before=g.toJson();CHECK(g.learnMagic(AirMagic)=="英雄最多掌握八项第二技能");CHECK(g.toJson()==before);g.hero.skills[Wisdom]=0;g.resources[Gold]=1999;before=g.toJson();g.learnMagic(AirMagic);CHECK(g.toJson()==before);g.resources[Gold]=10000;g.hero.pos={8,18};before=g.toJson();g.learnMagic(AirMagic);CHECK(g.toJson()==before);});
run("level ups never offer a ninth secondary skill",[]{auto g=Game::create();for(int i=0;i<8;i++)g.hero.skills[i]=3;g.gainXP(10000);CHECK(g.hero.levelChoices.empty());CHECK(g.validate());});

run("all thirteen direct damage spells use their classic expert coefficients",[]{const std::pair<int,int>cases[]={{MagicArrow,50},{LightningBolt,100},{IceBolt,90},{Implosion,450},{ChainLightning,180},{FrostRing,80},{Fireball,80},{Inferno,100},{MeteorShower,150},{DeathRipple,40},{DestroyUndead,70},{Armageddon,220},{TitanBolt,600}};for(auto[spell,expected]:cases){auto g=spellGame();g.hero.army={Stack{Archangel,1}};g.hero.power=2;g.hero.knowledge=100;g.hero.mana=1000;for(int skill=AirMagic;skill<=EarthMagic;skill++)g.hero.skills[skill]=3;g.hero.spells[spell]=true;g.objectById(15)->army={Stack{spell==DestroyUndead?Skeleton:129,1000000}};g.startBattle(15);auto target=g.battle->units.back();Point aim{target.x,target.y};if(spell==FrostRing)aim.x--;CHECK(g.cast(spell,aim).empty());CHECK(g.battle.has_value());CHECK(g.battle->units.back().hp==target.hp-expected);CHECK(g.validate());}});
run("hex spell templates contain seven, nineteen or six cells and clip to battle bounds",[]{auto g=Game::create();CHECK(g.spellArea(Fireball,{7,5}).size()==7);CHECK(g.spellArea(Inferno,{7,5}).size()==19);CHECK(g.spellArea(FrostRing,{7,5}).size()==6);auto ring=g.spellArea(FrostRing,{7,5});CHECK(std::find(ring.begin(),ring.end(),Point{7,5})==ring.end());for(auto p:g.spellArea(Inferno,{0,0}))CHECK(p.x>=0&&p.x<BattleW&&p.y>=0&&p.y<BattleH);CHECK(g.spellArea(Fireball,{-1,0}).empty());});
run("area spells affect friends and hit double-wide stacks only once",[]{auto g=spellGame();g.hero.army={Stack{Archangel,10},Stack{Pikeman,100}};g.objectById(15)->army={Stack{Griffin,100},Stack{Pikeman,100}};g.hero.spells[Fireball]=true;g.startBattle(15);g.battle->units[0].x=1;g.battle->units[0].y=0;g.battle->units[1].x=7;g.battle->units[1].y=4;g.battle->units[2].x=7;g.battle->units[2].y=5;g.battle->units[3].x=13;g.battle->units[3].y=9;CHECK(g.validate());auto targets=g.spellTargets(Fireball,{7,5});CHECK(targets.size()==2);int ally=g.battle->units[1].hp,enemy=g.battle->units[2].hp;CHECK(g.cast(Fireball,{7,5}).empty());CHECK(g.battle->units[1].hp==ally-25);CHECK(g.battle->units[2].hp==enemy-25);CHECK(g.battle->units[3].hp==1000);CHECK(g.validate());});
run("frost ring excludes the center hex but includes the other half of a wide stack",[]{auto g=spellGame();g.hero.army={Stack{Archangel,10},Stack{Pikeman,100}};g.objectById(15)->army={Stack{Griffin,100},Stack{Pikeman,100}};g.hero.spells[FrostRing]=true;g.startBattle(15);g.battle->units[1].x=7;g.battle->units[1].y=5;g.battle->units[2].x=7;g.battle->units[2].y=4;g.battle->units[3].x=13;g.battle->units[3].y=9;int ally=g.battle->units[1].hp,enemy=g.battle->units[2].hp;CHECK(g.cast(FrostRing,{7,5}).empty());CHECK(g.battle->units[1].hp==ally);CHECK(g.battle->units[2].hp==enemy-25);CHECK(g.validate());});
run("chain lightning halves each jump, includes friends and skips naturally immune stacks",[]{auto g=spellGame();g.hero.army={Stack{Archangel,10},Stack{Pikeman,100},Stack{Pikeman,100}};g.objectById(15)->army={Stack{Pikeman,100},Stack{83,10}};g.hero.spells[ChainLightning]=true;g.hero.mana=30;g.hero.knowledge=3;g.startBattle(15);auto&u=g.battle->units;u[0].x=1;u[0].y=0;u[1].x=7;u[1].y=5;u[2].x=9;u[2].y=5;u[3].x=5;u[3].y=5;u[4].x=6;u[4].y=4;std::array<int,5>hp;for(int i=0;i<5;i++)hp[i]=u[i].hp;CHECK(g.validate());CHECK(g.cast(ChainLightning,{5,5}).empty());CHECK(u[3].hp==hp[3]-65);CHECK(u[1].hp==hp[1]-32);CHECK(u[2].hp==hp[2]-16);CHECK(u[0].hp==hp[0]-8);CHECK(u[4].hp==hp[4]);CHECK(g.hero.mana==6);CHECK(g.validate());});
run("chain lightning continues after a later target resists",[]{auto g=spellGame();g.hero.army={Stack{Archangel,10}};g.objectById(15)->army={Stack{Pikeman,100},Stack{16,100},Stack{Pikeman,100}};g.hero.spells[ChainLightning]=true;g.hero.knowledge=3;g.hero.mana=30;g.startBattle(15);auto&u=g.battle->units;u[1].x=5;u[1].y=5;u[2].x=7;u[2].y=5;u[3].x=9;u[3].y=5;g.randomState=1999;int hp=u[2].hp,next=u[3].hp;CHECK(g.cast(ChainLightning,{5,5}).empty());CHECK(u[2].hp==hp);CHECK(u[3].hp==next-16);CHECK(g.validate());});
run("death ripple includes non-living golems but excludes undead",[]{auto g=spellGame();g.hero.army={Stack{Archangel,10},Stack{Skeleton,100}};g.objectById(15)->army={Stack{32,100},Stack{Skeleton,100},Stack{Pikeman,100}};g.hero.spells[DeathRipple]=true;g.startBattle(15);auto&u=g.battle->units;std::array<int,5>hp;for(int i=0;i<5;i++)hp[i]=u[i].hp;CHECK(g.cast(DeathRipple).empty());CHECK(u[0].hp==hp[0]-15);CHECK(u[1].hp==hp[1]);CHECK(u[2].hp==hp[2]-7);CHECK(u[3].hp==hp[3]);CHECK(u[4].hp==hp[4]-15);CHECK(g.validate());});
run("school protection uses the strongest applicable shield for magic arrow",[]{auto g=Game::create();CombatStack t;t.unit=Pikeman;t.effects[ProtectAir]=t.effects[ProtectFire]=2;t.effectMastery[ProtectAir]=1;t.effectMastery[ProtectFire]=2;CHECK(g.spellDamage(t,MagicArrow,100)==50);CHECK(g.spellDamage(t,LightningBolt,100)==70);CHECK(g.spellDamage(t,IceBolt,100)==100);});
run("anti magic cleanses negative effects and dispel bypasses its immunity",[]{auto g=spellGame();g.hero.skills[EarthMagic]=2;g.hero.skills[WaterMagic]=2;g.hero.spells[AntiMagic]=g.hero.spells[Dispel]=true;g.startBattle(15);auto&t=g.battle->units[0];t.effects[Slow]=3;t.effectMastery[Slow]=2;t.effects[Bless]=3;t.effectMastery[Bless]=2;CHECK(g.cast(AntiMagic,{t.x,t.y}).empty());CHECK(t.effects[Slow]==0&&t.effects[Bless]>0);CHECK(g.spellImmune(t,LightningBolt));CHECK(!g.spellImmune(t,Implosion));CHECK(!g.spellImmune(t,Dispel));g.battle->spellRound=0;CHECK(g.cast(Dispel,{t.x,t.y}).empty());CHECK(t.effects[AntiMagic]==0&&t.effects[Bless]==0);CHECK(!g.spellImmune(t,LightningBolt));CHECK(g.validate());});
run("basic advanced and expert dispel use their original allegiance and mass rules",[]{auto g=spellGame();g.hero.army={Stack{Archangel,10}};g.objectById(15)->army={Stack{83,10}};g.hero.spells[Dispel]=true;g.startBattle(15);auto&ally=g.battle->units[0];auto&enemy=g.battle->units[1];ally.effects[Haste]=3;enemy.effects[Slow]=3;CHECK(g.spellTargets(Dispel,{enemy.x,enemy.y}).empty());g.hero.skills[WaterMagic]=2;CHECK(g.spellTargets(Dispel,{enemy.x,enemy.y}).size()==1);g.hero.skills[WaterMagic]=3;CHECK(g.spellTargets(Dispel).size()==2);CHECK(g.cast(Dispel).empty());CHECK(ally.effects[Haste]==0&&enemy.effects[Slow]==0);CHECK(g.validate());});
run("disrupting ray is cumulative, persistent and survives dispel",[]{auto g=spellGame();g.hero.spells[DisruptingRay]=g.hero.spells[Dispel]=true;g.hero.knowledge=10;g.hero.mana=100;g.hero.skills[AirMagic]=3;g.hero.skills[WaterMagic]=2;g.startBattle(15);auto&t=g.battle->units.back();t.unit=Swordsman;t.hp=t.initialCount*Units[Swordsman].hp;t.count=t.initialCount;CHECK(g.cast(DisruptingRay,{t.x,t.y}).empty());CHECK(t.defenseReduction==5);g.battle->spellRound=0;CHECK(g.cast(DisruptingRay,{t.x,t.y}).empty());CHECK(t.defenseReduction==10);t.effects[Haste]=3;g.battle->spellRound=0;CHECK(g.cast(Dispel,{t.x,t.y}).empty());CHECK(t.effects[Haste]==0&&t.defenseReduction==10);CHECK(g.validate());});
run("prayer affects attack defense and speed while precision and bloodlust stay directional",[]{auto g=Game::create();g.hero.attack=g.hero.defense=0;g.hero.skills={};CombatStack a,t;a.unit=Archer;a.team=0;a.count=10;a.x=1;a.y=5;t.unit=Pikeman;t.team=1;t.x=6;t.y=5;CHECK(g.damage(a,t,true)==26);a.effects[Precision]=2;a.effectMastery[Precision]=2;CHECK(g.damage(a,t,true)==33);CHECK(g.damage(a,t,false)==13);a.effects[Bloodlust]=2;a.effectMastery[Bloodlust]=2;CHECK(g.damage(a,t,false)==16);a.effects[Prayer]=2;a.effectMastery[Prayer]=2;CHECK(g.speed(a)==8);CHECK(g.defense(a)==7);CHECK(g.damage(a,t,true)==38);});
run("classic advanced forgetfulness is mass and reduces ranged and melee damage",[]{auto g=spellGame();g.hero.attack=0;g.hero.skills={};g.hero.skills[WaterMagic]=2;g.hero.army={Stack{Archangel,10}};g.objectById(15)->army={Stack{Archer,100},Stack{Marksman,100},Stack{Pikeman,100}};g.hero.spells[Forgetfulness]=true;g.startBattle(15);CHECK(g.spellMass(Forgetfulness));CHECK(g.spellTargets(Forgetfulness).size()==2);CHECK(g.cast(Forgetfulness).empty());CHECK(!g.canShoot(g.battle->units[1]));CHECK(!g.canShoot(g.battle->units[2]));CombatStack a,t;a.unit=Archer;a.team=0;a.count=10;a.x=1;a.y=5;t.unit=Pikeman;t.team=1;t.x=6;t.y=5;a.effects[Forgetfulness]=2;CHECK(g.damage(a,t,true)==13);CHECK(g.damage(a,t,false)==6);a.effectMastery[Forgetfulness]=2;CHECK(g.damage(a,t,false)==6);CHECK(g.validate());});
run("counterstrike grants extra retaliations without granting unlimited counters",[]{auto g=spellGame();g.hero.attack=g.hero.defense=0;g.hero.skills={};g.hero.skills[AirMagic]=2;g.hero.spells[Counterstrike]=true;g.hero.army={Stack{Pikeman,200}};g.objectById(15)->army={Stack{Pikeman,100},Stack{Pikeman,100},Stack{Pikeman,100},Stack{Pikeman,100}};g.hero.knowledge=3;g.hero.mana=30;g.startBattle(15);auto&u=g.battle->units;u[0].x=5;u[0].y=5;u[1].x=6;u[1].y=5;u[2].x=7;u[2].y=5;u[3].x=7;u[3].y=6;u[4].x=7;u[4].y=4;CHECK(g.cast(Counterstrike,{5,5}).empty());g.act("defend");for(int i=0;i<3;i++)CHECK(g.act("attack",{5,5}).empty());CHECK(u[0].retaliations==3);int fourth=u[4].hp;CHECK(g.act("attack",{5,5}).empty());CHECK(u[4].hp==fourth);CHECK(g.validate());});
run("fire shield reflects melee damage even against general magic immunity",[]{auto g=Game::create();g.hero.attack=0;g.hero.skills={};g.hero.army={Stack{83,10}};g.objectById(15)->army={Stack{Pikeman,1},Stack{Swordsman,100}};g.startBattle(15);auto&u=g.battle->units;u[0].x=4;u[0].y=5;u[1].x=5;u[1].y=5;u[1].effects[FireShield]=3;u[1].effectMastery[FireShield]=3;u[1].retaliations=1;int hp=u[0].hp;CHECK(g.act("attack",{5,5}).empty());CHECK(u[0].hp==hp-3);CHECK(g.validate());});
run("efreet sultan natural fire shield uses the stronger spell without adding both",[]{for(bool stronger:{false,true}){auto g=Game::create();g.hero.attack=0;g.hero.skills={};g.hero.army={Stack{Archangel,1}};g.objectById(15)->army={Stack{53,20}};g.startBattle(15);auto&u=g.battle->units;u[0].x=4;u[0].y=5;u[1].x=5;u[1].y=5;u[1].retaliations=1;if(stronger){u[1].effects[FireShield]=3;u[1].effectMastery[FireShield]=3;}int hp=u[0].hp;CHECK(g.act("attack",{5,5}).empty());CHECK(u[0].hp==hp-(stronger?27:18));CHECK(g.validate());}});
run("fire immune attackers ignore fire shield and golems reduce its reflected damage",[]{auto g=Game::create();g.hero.attack=0;g.hero.skills={};g.hero.army={Stack{52,10}};g.objectById(15)->army={Stack{Pikeman,100}};g.startBattle(15);auto&u=g.battle->units;u[0].x=4;u[0].y=5;u[1].x=5;u[1].y=5;u[1].effects[FireShield]=3;u[1].effectMastery[FireShield]=3;u[1].retaliations=1;int hp=u[0].hp;CHECK(g.act("attack",{5,5}).empty());CHECK(u[0].hp==hp);g=Game::create();g.hero.attack=0;g.hero.skills={};g.hero.army={Stack{32,10}};g.objectById(15)->army={Stack{129,100}};g.startBattle(15);g.battle->units[0].x=4;g.battle->units[0].y=5;g.battle->units[0].effects[Bless]=2;g.battle->units[0].effectMastery[Bless]=2;g.battle->units[1].x=5;g.battle->units[1].y=5;g.battle->units[1].effects[FireShield]=2;g.battle->units[1].effectMastery[FireShield]=3;g.battle->units[1].retaliations=1;hp=g.battle->units[0].hp;CHECK(g.act("attack",{5,5}).empty());CHECK(g.battle->units[0].hp==hp-12);CHECK(g.validate());});
run("defending snapshots its bonus when commanded rather than after later buffs",[]{auto g=Game::create();g.hero.defense=0;g.hero.army={Stack{Pikeman,10}};g.objectById(15)->army={Stack{Pikeman,100}};g.startBattle(15);g.act("defend");auto&t=g.battle->units[0];CHECK(t.defendingBonus==1);t.effects[StoneSkin]=3;t.effectMastery[StoneSkin]=2;CHECK(g.defense(t)==13);CHECK(g.validate());});
run("new spell state and permanent defense loss survive midbattle save load",[]{auto g=spellGame();g.hero.army={Stack{Archangel,10}};g.objectById(15)->army={Stack{Swordsman,100}};g.hero.spells[DisruptingRay]=true;g.startBattle(15);auto target=g.battle->units.back();CHECK(g.cast(DisruptingRay,{target.x,target.y}).empty());g.battle->units[0].effects[Prayer]=3;g.battle->units[0].effectMastery[Prayer]=3;auto p=testDirectory()/"heroesiii-expanded-spells.json";std::string error;CHECK(g.save(p,error));Game loaded;CHECK(loaded.load(p,error));CHECK(loaded.toJson()==g.toJson());CHECK(loaded.battle->units.back().defenseReduction==3);std::filesystem::remove(p);});
run("version four saves migrate nine spells and restore a legacy defending bonus",[]{auto g=spellGame();g.hero.army={Stack{Pikeman,10}};g.objectById(15)->army={Stack{Pikeman,100}};g.startBattle(15);g.act("defend");auto old=g.toJson();old["version"]=4;while(old["hero"]["skills"].size()>12)old["hero"]["skills"].erase(old["hero"]["skills"].end()-1);while(old["hero"]["spells"].size()>9)old["hero"]["spells"].erase(old["hero"]["spells"].end()-1);for(auto&u:old["battle"]["units"]){for(auto name:{"effects","effectMastery"})while(u[name].size()>9)u[name].erase(u[name].end()-1);u.erase("defenseReduction");u.erase("defendingBonus");}auto p=testDirectory()/"heroesiii-v4-spells.json";std::ofstream(p)<<old.dump();Game loaded;std::string error;CHECK(loaded.load(p,error));CHECK(loaded.toJson()==g.toJson());std::filesystem::remove(p);});

run("global damage removes a dead active stack and prepares the next living stack",[]{auto g=spellGame();g.hero.army={Stack{Archangel,1},Stack{118,10}};g.objectById(15)->army={Stack{32,100}};g.hero.power=5;g.hero.knowledge=3;g.hero.mana=30;g.hero.spells[Armageddon]=true;g.startBattle(15);CHECK(g.current()->id==0);CHECK(g.cast(Armageddon).empty());CHECK(g.battle.has_value());CHECK(g.battle->units[0].hp==0);CHECK(g.current()->id==1);CHECK(g.current()->preparedRound==1);CHECK(g.validate());});
run("killing the last queued stack enters the living waiting phase without skipping a turn",[]{auto g=spellGame();g.hero.army={Stack{Archangel,1},Stack{118,10}};g.objectById(15)->army={Stack{32,100}};g.hero.power=5;g.hero.knowledge=3;g.hero.mana=30;g.hero.spells[Armageddon]=true;g.startBattle(15);g.battle->queue={0};g.battle->waiting={1,100};g.battle->units[1].waited=g.battle->units[2].waited=true;CHECK(g.validate());CHECK(g.cast(Armageddon).empty());CHECK(g.battle.has_value());CHECK(g.battle->round==1&&g.battle->waitPhase);CHECK(g.current()->id==100);CHECK(g.battle->queue.size()==2&&g.battle->waiting.empty());CHECK(g.validate());});
run("mutual destruction from armageddon ends in defeat with no experience",[]{auto g=spellGame();g.hero.army={Stack{Pikeman,1}};g.objectById(15)->army={Stack{Pikeman,1}};g.hero.knowledge=3;g.hero.mana=30;g.hero.spells[Armageddon]=true;g.startBattle(15);CHECK(g.cast(Armageddon).empty());CHECK(!g.battle&&g.report.has_value());CHECK(!g.report->won&&g.report->xp==0&&g.result==-1);CHECK(armyCount(g.hero.army)==0);CHECK(g.validate());});

run("the full classic spell catalogue has seventy distinct original identities",[]{std::set<int>ids;int connected=0,adventure=0;for(const auto&spell:Spells){CHECK(ids.insert(spell.originalId).second);CHECK(spell.originalId>=0&&spell.originalId<70);CHECK(spell.level>=1&&spell.level<=5);CHECK(spell.school>0&&spell.school<=15);CHECK(spell.cost>=spell.skilledCost);connected+=spell.implemented;adventure+=spell.adventure;for(int weight:spell.guildWeights)CHECK(weight>=0);}CHECK(ids.size()==70&&connected==70&&adventure==10);for(int weight:Spells[MagicArrow].guildWeights)CHECK(weight==30);CHECK(Spells[Cure].guildWeights[4]==0);CHECK(Spells[Fireball].guildWeights[3]==50);CHECK(Spells[Armageddon].guildWeights[0]==0);CHECK(Spells[Resurrection].guildWeights[4]>0);});
run("the knight starts without a spell book or learned spells",[]{auto g=Game::create();CHECK(!g.hero.spellbook);CHECK(std::none_of(g.hero.spells.begin(),g.hero.spells.end(),[](bool known){return known;}));CHECK(g.validate());});
run("all faction guild pools use eligible non-repeating spells and their classic level caps",[]{for(int faction=0;faction<9;faction++)for(int seed=0;seed<20;seed++){auto g=Game::create(seed,faction);for(int level=1;level<=5;level++){const auto&pool=g.town.guild[level-1];CHECK(pool.size()==size_t(level>g.townDef().maxMageGuild?0:6-level+(faction==2?1:0)));std::set<int>unique;for(int spell:pool){CHECK(unique.insert(spell).second);CHECK(Spells[spell].level==level);CHECK(Spells[spell].guildWeights[faction]>0);CHECK(!Spells[spell].special);CHECK(spell!=TitanBolt);}CHECK(g.guildSpells(level).empty());}CHECK(g.validate());}});
run("guilds are fixed at map creation and never consume the battle random stream",[]{auto a=Game::create(1999,2),b=Game::create(1999,2);CHECK(a.town.guild==b.town.guild);auto original=a.town.guild;for(int i=0;i<50;i++)a.random();auto state=a.randomState;a.generateGuild();CHECK(a.randomState==state&&a.town.guild==original);a.resources.fill(10000);a.build(MageGuild);a.endDay();a.build(Special3);a.visit(1);CHECK(a.town.guild==original);});
run("map spell bans reduce guild slots instead of filling them with forbidden spells",[]{auto g=Game::create(1999,2);g.bannedSpells.fill(true);g.bannedSpells[IceBolt]=g.bannedSpells[Fireball]=false;auto state=g.randomState;g.generateGuild();CHECK(g.randomState==state);CHECK(g.town.guild[0].empty());CHECK(g.town.guild[1]==std::vector<int>{IceBolt});CHECK(g.town.guild[2]==std::vector<int>{Fireball});CHECK(g.town.guild[3].empty()&&g.town.guild[4].empty());CHECK(g.validate());});
run("buying a spell book costs only five hundred gold and teaches the guild spells",[]{auto g=Game::create();g.build(MageGuild);auto before=g.resources;auto offered=g.guildSpells(1);CHECK(offered.size()==5);g.buySpellbook();CHECK(g.hero.spellbook);CHECK(g.resources[Gold]==before[Gold]-500);for(int r=1;r<ResourceCount;r++)CHECK(g.resources[r]==before[r]);CHECK(std::count(g.hero.spells.begin(),g.hero.spells.end(),true)==5);for(int spell:offered)CHECK(g.hero.spells[spell]);before=g.resources;g.buySpellbook();CHECK(g.resources==before);CHECK(g.validate());});
run("spell book purchase rejects absent guild remote hero insufficient gold and battle",[]{for(int reason=0;reason<4;reason++){auto g=Game::create();if(reason!=0)g.build(MageGuild);if(reason==1)g.hero.pos={5,20};if(reason==2)g.resources[Gold]=499;if(reason==3)g.startBattle(15);auto before=g.toJson();CHECK(!g.buySpellbook().empty());CHECK(g.toJson()==before);}});
run("wisdom progressively permits learning levels three four and five",[]{auto g=Game::create(1999,2);g.hero.spellbook=true;for(int building:{MageGuild,MageGuild2,MageGuild3,MageGuild4,MageGuild5})g.town.built[building]=true;const int expected[]={9,12,14,15};for(int mastery=0;mastery<4;mastery++){g.hero.skills[Wisdom]=mastery;g.learnTownSpells();CHECK(g.learnableSpellLevel()==2+mastery);CHECK(std::count(g.hero.spells.begin(),g.hero.spells.end(),true)==expected[mastery]);for(int spell=0;spell<SpellCount;spell++)if(g.hero.spells[spell])CHECK(Spells[spell].level<=2+mastery);}CHECK(g.validate());});
run("guild teaching requires a book and actual town visit",[]{auto g=Game::create();g.town.built[MageGuild]=g.town.built[MageGuild2]=true;g.learnTownSpells();CHECK(std::count(g.hero.spells.begin(),g.hero.spells.end(),true)==0);g.hero.spellbook=true;g.hero.pos={5,20};g.learnTownSpells();CHECK(std::count(g.hero.spells.begin(),g.hero.spells.end(),true)==0);g.hero.pos=g.town.pos;g.visit(1);CHECK(std::count(g.hero.spells.begin(),g.hero.spells.end(),true)==9);auto before=g.hero.spells;g.town.built[MageGuild]=g.town.built[MageGuild2]=false;g.learnTownSpells();CHECK(g.hero.spells==before);});
run("learning wisdom while visiting teaches newly eligible guild spells",[]{auto g=Game::create();g.hero.spellbook=true;g.town.built[MageGuild]=g.town.built[MageGuild2]=g.town.built[MageGuild3]=true;g.learnTownSpells();CHECK(std::count(g.hero.spells.begin(),g.hero.spells.end(),true)==9);g.hero.level=2;g.hero.xp=1000;g.hero.levelChoices={LevelChoice{2,0,{Wisdom}}};g.chooseSkill(Wisdom);CHECK(g.hero.skills[Wisdom]==1);CHECK(std::count(g.hero.spells.begin(),g.hero.spells.end(),true)==12);CHECK(g.validate());});
run("a tower library adds one fixed extra spell to every constructed guild level",[]{auto g=Game::create(1999,2);g.resources.fill(10000);g.hero.spellbook=true;g.hero.skills[Wisdom]=3;for(int building:{MageGuild,MageGuild2,MageGuild3,MageGuild4,MageGuild5})g.town.built[building]=true;g.learnTownSpells();auto regular=g.hero.spells;for(int level=1;level<=5;level++)CHECK(g.guildSpells(level).size()==size_t(6-level));g.build(Special3);CHECK(g.town.built[Special3]);for(int level=1;level<=5;level++){auto offered=g.guildSpells(level);CHECK(offered.size()==size_t(7-level));for(int spell:offered)CHECK(g.hero.spells[spell]);}CHECK(std::count(g.hero.spells.begin(),g.hero.spells.end(),true)==20);for(int spell=0;spell<SpellCount;spell++)if(regular[spell])CHECK(g.hero.spells[spell]);CHECK(g.validate());});
run("tower upgraded mages can be built through the now available library prerequisite",[]{auto g=Game::create(1999,2);g.resources.fill(10000);for(int building:{MageGuild,GriffinTower,Barracks}){g.build(building);CHECK(g.town.built[building]);g.endDay();}g.build(UpBarracks);CHECK(!g.town.built[UpBarracks]);g.build(Special3);CHECK(g.town.built[Special3]);g.endDay();g.build(UpBarracks);CHECK(g.town.built[UpBarracks]);CHECK(g.validate());});
run("unsupported catalogue spells and a missing book cannot consume a battle cast",[]{auto g=spellGame();g.hero.spells[Blind]=g.hero.spells[SummonBoat]=true;g.startBattle(15);auto before=g.toJson();CHECK(!g.cast(Blind,{13,0}).empty());CHECK(g.toJson()==before);CHECK(!g.cast(SummonBoat).empty());CHECK(g.toJson()==before);g.hero.spellbook=false;before=g.toJson();CHECK(!g.cast(MagicArrow,{g.battle->units.back().x,g.battle->units.back().y}).empty());CHECK(g.toJson()==before);});
run("already learned higher level spells do not require wisdom to cast",[]{auto g=spellGame();g.hero.army={Stack{Archangel,10}};g.hero.spells[Implosion]=true;g.hero.knowledge=3;g.hero.mana=30;g.hero.skills[Wisdom]=0;g.objectById(15)->army={Stack{Pikeman,100}};g.startBattle(15);CHECK(g.cast(Implosion,{g.battle->units.back().x,g.battle->units.back().y}).empty());CHECK(g.hero.mana==0);CHECK(g.battle->units.back().hp==825);CHECK(g.validate());});
run("version five saves gain persistent guilds and expand thirty five spell arrays",[]{auto g=spellGame();g.hero.spells[Fireball]=true;g.startBattle(15);g.randomState=98765;auto old=g.toJson();old["version"]=5;while(old["hero"]["skills"].size()>12)old["hero"]["skills"].erase(old["hero"]["skills"].end()-1);old.erase("bannedSpells");old["town"].erase("guild");old["hero"].erase("spellbook");while(old["hero"]["spells"].size()>35)old["hero"]["spells"].erase(old["hero"]["spells"].end()-1);for(auto&unit:old["battle"]["units"])for(auto key:{"effects","effectMastery"})while(unit[key].size()>35)unit[key].erase(unit[key].end()-1);auto p=testDirectory()/"heroesiii-v5-guild.json";std::ofstream(p)<<old.dump();Game loaded;std::string error;CHECK(loaded.load(p,error));CHECK(loaded.toJson()==g.toJson());CHECK(loaded.randomState==98765);std::filesystem::remove(p);});
run("invalid guild identities duplicates book types and arrays do not overwrite a live game",[]{auto g=Game::create();g.build(MageGuild);const auto original=g.toJson();auto p=testDirectory()/"heroesiii-invalid-guild.json";for(int problem=0;problem<8;problem++){auto bad=original;switch(problem){case 0:bad["town"]["guild"][0][0]=-1;break;case 1:bad["town"]["guild"][0][1]=bad["town"]["guild"][0][0];break;case 2:bad["town"]["guild"][0][0]=TitanBolt;break;case 3:bad["town"]["guild"][0][0]=IceBolt;break;case 4:bad["hero"]["spellbook"]=1;break;case 5:bad["bannedSpells"].erase(bad["bannedSpells"].end()-1);break;case 6:bad["town"]["guild"].erase(bad["town"]["guild"].end()-1);break;case 7:bad["bannedSpells"][g.town.guild[0][0]]=true;break;}std::ofstream(p)<<bad.dump();std::string error;CHECK(!g.load(p,error));CHECK(g.toJson()==original);}std::filesystem::remove(p);});

run("resurrection grades restore classic health amounts and capture permanence",[]{for(int mastery=0;mastery<4;mastery++){auto g=revivalGame();g.hero.power=2;g.hero.skills[EarthMagic]=mastery;makeCorpse(g,1);CHECK(g.validate());CHECK(g.cast(Resurrection,{7,5}).empty());const auto&u=g.battle->units[1];const int health[]={140,140,180,200};CHECK(u.hp==health[mastery]&&u.count==health[mastery]/10);CHECK(u.permanentCount==(mastery>=2?u.count:0));CHECK(!u.corpse&&u.deathOrder==0);CHECK(g.hero.mana==1000-(mastery?16:20));CHECK(g.validate());}});
run("animate dead is permanent at every earth mastery and uses its own coefficients",[]{for(int mastery=0;mastery<4;mastery++){auto g=revivalGame(Skeleton,40);g.hero.power=2;g.hero.skills[EarthMagic]=mastery;makeCorpse(g,1);CHECK(g.cast(AnimateDead,{7,5}).empty());const int health[]={130,130,160,240};const auto&u=g.battle->units[1];CHECK(u.hp==health[mastery]);CHECK(u.permanentCount==u.count);CHECK(g.validate());}});
run("resurrection rejects undead golems gargoyles elementals and magic immune dragons",[]{for(int unit:std::array<int,7>{Skeleton,30,32,118,27,83,123}){auto g=revivalGame(unit,2);makeCorpse(g,1);auto before=g.toJson();CHECK(!g.cast(Resurrection,{7,5}).empty());CHECK(g.toJson()==before);}auto g=revivalGame();makeCorpse(g,1);auto before=g.toJson();CHECK(!g.cast(AnimateDead,{7,5}).empty());CHECK(g.toJson()==before);});
run("a full creature health unit is required to resurrect an empty stack",[]{auto g=revivalGame(140,1);makeCorpse(g,1);auto before=g.toJson();CHECK(!g.cast(Resurrection,{7,5}).empty());CHECK(g.toJson()==before);g.hero.power=20;g.hero.skills[EarthMagic]=3;CHECK(g.cast(Resurrection,{7,5}).empty());CHECK(g.battle->units[1].count==1);CHECK(g.validate());});
run("basic resurrection retains the classic minimum count through repeated casualties",[]{auto g=revivalGame();CHECK(g.cast(Fireball,{7,5}).empty());CHECK(g.battle->units[1].count==18&&g.battle->units[1].permanentCount==18);g.battle->spellRound=0;CHECK(g.cast(Resurrection,{7,5}).empty());CHECK(g.battle->units[1].count==20&&g.battle->units[1].permanentCount==18);g.battle->spellRound=0;CHECK(g.cast(Fireball,{7,5}).empty());CHECK(g.battle->units[1].count==18&&g.battle->units[1].permanentCount==18);winFixture(g);CHECK(g.report->won&&g.hero.army[1].count==18);CHECK(g.validate());});
run("temporary revival is removed after victory and permanent revival is retained",[]{for(bool permanent:{false,true}){auto g=revivalGame();makeCorpse(g,1);if(permanent)g.hero.skills[EarthMagic]=2;CHECK(g.cast(Resurrection,{7,5}).empty());int alive=g.battle->units[1].count;winFixture(g);CHECK(g.report->won);CHECK(permanent?g.hero.army[1].count==alive:g.hero.army[1].empty());CHECK(g.validate());}});
run("permanent revival adds only newly returned creatures after an earlier temporary revival",[]{auto g=revivalGame();makeCorpse(g,1);CHECK(g.cast(Resurrection,{7,5}).empty());CHECK(g.battle->units[1].count==9&&g.battle->units[1].permanentCount==0);g.battle->spellRound=0;g.hero.skills[EarthMagic]=2;CHECK(g.cast(Resurrection,{7,5}).empty());CHECK(g.battle->units[1].count==20&&g.battle->units[1].permanentCount==11);winFixture(g);CHECK(g.hero.army[1].count==11);});
run("a living unit covering either half of a corpse prevents revival before payment",[]{auto g=revivalGame(Griffin,5);makeCorpse(g,1);g.battle->units[0].x=6;g.battle->units[0].y=5;CHECK(g.validate());auto before=g.toJson();CHECK(!g.cast(Resurrection,{7,5}).empty());CHECK(g.toJson()==before);g.battle->units[0].x=1;g.battle->units[0].y=0;CHECK(g.cast(Resurrection,{7,5}).empty());CHECK(g.validate());});
run("a newer small corpse on the hind hex blocks a classic wide corpse",[]{auto g=revivalGame(Griffin,5);auto&small=g.battle->units[2];small.unit=Archer;small.initialCount=5;small.hp=50;small.x=6;small.y=5;makeCorpse(g,1);makeCorpse(g,2);CHECK(g.validate());CHECK(g.spellTarget(Resurrection,{6,5})->id==2);auto before=g.toJson();CHECK(!g.cast(Resurrection,{7,5}).empty());CHECK(g.toJson()==before);});
run("reviving an unacted or waited stack preserves the correct action phase",[]{for(int state=0;state<3;state++){auto g=revivalGame();makeCorpse(g,1,state==1);g.battle->units[1].waited=state==2;CHECK(g.cast(Resurrection,{7,5}).empty());bool queued=std::find(g.battle->queue.begin(),g.battle->queue.end(),1)!=g.battle->queue.end();bool waiting=std::find(g.battle->waiting.begin(),g.battle->waiting.end(),1)!=g.battle->waiting.end();CHECK(queued==(state==0));CHECK(waiting==(state==2));CHECK(g.current()->id==0);CHECK(g.validate());}});
run("resurrection cleanses negatives while permanent disrupting ray remains",[]{auto g=revivalGame();auto&u=g.battle->units[1];u.count=u.permanentCount=10;u.hp=96;u.effects[Slow]=u.effects[Bless]=3;u.effectMastery[Slow]=u.effectMastery[Bless]=2;u.defenseReduction=3;g.hero.skills[EarthMagic]=2;CHECK(g.cast(Resurrection,{7,5}).empty());CHECK(u.count==20&&u.permanentCount==20);CHECK(u.effects[Slow]==0&&u.effects[Bless]>0&&u.defenseReduction==3);CHECK(g.validate());});
run("death clears ordinary effects but leaves the permanent defense loss on the corpse",[]{auto g=revivalGame(Pikeman,1);auto&u=g.battle->units[1];u.effects[Bless]=u.effects[Slow]=3;u.defenseReduction=3;CHECK(g.cast(Fireball,{7,5}).empty());CHECK(u.corpse&&u.deathOrder==1);CHECK(u.effects[Bless]==0&&u.effects[Slow]==0&&u.defenseReduction==3);CHECK(g.validate());});
run("clone mastery controls creature level including classic expert neutral dragons",[]{for(auto[mastery,unit,allowed]:{std::tuple{0,int(Cavalier),false},std::tuple{2,int(Cavalier),true},std::tuple{2,int(Archangel),false},std::tuple{3,int(Archangel),true},std::tuple{3,140,true}}){auto g=revivalGame(unit,1);g.hero.skills[WaterMagic]=mastery;auto before=g.toJson();auto error=g.cast(Clone,{7,5});CHECK(error.empty()==allowed);if(allowed){CHECK(g.battle->units.back().origin==StackOrigin::Clone);CHECK(g.battle->units.back().unit==unit);CHECK(g.validate());}else CHECK(g.toJson()==before);}});
run("only one living clone per source is allowed and clones cannot be cloned",[]{auto g=revivalGame();CHECK(g.cast(Clone,{7,5}).empty());int copy=g.battle->units.back().id;Point pos{g.battle->units.back().x,g.battle->units.back().y};g.battle->spellRound=0;auto before=g.toJson();CHECK(!g.cast(Clone,{7,5}).empty());CHECK(g.toJson()==before);CHECK(!g.cast(Clone,pos).empty());CHECK(g.toJson()==before);CHECK(copy>=7);});
run("a clone disappears on any damage without losing the original",[]{auto g=revivalGame();CHECK(g.cast(Clone,{7,5}).empty());auto copy=g.battle->units.back();int original=g.battle->units[1].hp;g.battle->spellRound=0;CHECK(g.cast(Fireball,{copy.x,copy.y}).empty());CHECK(g.battle->units.back().hp==0&&!g.battle->units.back().corpse);CHECK(g.battle->units[1].hp>0&&g.battle->units[1].hp<=original);g.battle->spellRound=0;CHECK(g.cast(Clone,{7,5}).empty());CHECK(g.battle->units.back().id!=copy.id);CHECK(g.validate());});
run("killing the original removes its clone and clears both from the pending queue",[]{auto g=revivalGame();CHECK(g.cast(Clone,{7,5}).empty());int id=g.battle->units.back().id;g.battle->spellRound=0;g.hero.power=100;CHECK(g.cast(Fireball,{7,5}).empty());CHECK(g.battle->units[1].corpse&&g.battle->units.back().hp==0);CHECK(std::find(g.battle->queue.begin(),g.battle->queue.end(),id)==g.battle->queue.end());CHECK(g.validate());});
run("a clone expires after the captured spell power duration",[]{auto g=revivalGame();CHECK(g.cast(Clone,{7,5}).empty());CHECK(g.battle->units.back().cloneTurns==1);g.hero.power=20;for(int i=0;i<10&&g.battle->round==1;i++)g.act("defend");CHECK(g.battle->round==2);CHECK(g.battle->units.back().hp==0&&g.battle->units.back().cloneTurns==0);CHECK(g.validate());});
run("fire shield destroys an attacking clone before the enemy retaliates",[]{auto g=revivalGame();CHECK(g.cast(Clone,{7,5}).empty());int id=g.battle->units.back().id;g.battle->units.back().x=5;g.battle->units.back().y=5;auto&enemy=g.battle->units[3];enemy.x=6;enemy.y=5;enemy.effects[FireShield]=3;enemy.effectMastery[FireShield]=3;std::erase(g.battle->queue,id);g.battle->queue.insert(g.battle->queue.begin(),id);CHECK(g.act("attack",{6,5}).empty());CHECK(g.battle->units.back().hp==0);CHECK(enemy.retaliations==0);CHECK(g.validate());});
run("dispel removes a clone buff without deleting the clone itself",[]{auto g=revivalGame();g.hero.skills[WaterMagic]=2;CHECK(g.cast(Clone,{7,5}).empty());Point p{g.battle->units.back().x,g.battle->units.back().y};g.battle->units.back().effects[Bless]=3;g.battle->spellRound=0;CHECK(g.cast(Dispel,p).empty());CHECK(g.battle->units.back().hp>0&&g.battle->units.back().effects[Bless]==0);CHECK(g.validate());});
run("all four elemental summon spells use two three and four times spell power",[]{const std::pair<int,int>spells[]={{SummonFireElemental,118},{SummonEarthElemental,120},{SummonWaterElemental,116},{SummonAirElemental,114}};for(auto[spell,unit]:spells)for(int mastery=0;mastery<4;mastery++){auto g=revivalGame();g.hero.power=3;for(int s=AirMagic;s<=EarthMagic;s++)g.hero.skills[s]=mastery;CHECK(g.cast(spell).empty());const auto&u=g.battle->units.back();CHECK(u.origin==StackOrigin::Summoned&&u.unit==unit);CHECK(u.count==3*(mastery<2?2:mastery==2?3:4));CHECK(u.permanentCount==0&&g.battle->summonedType[0]==unit);CHECK(g.current()->id==0);CHECK(g.validate());}});
run("classic summoning type persists after its elemental stack is killed",[]{auto g=revivalGame();CHECK(g.cast(SummonEarthElemental).empty());Point p{g.battle->units.back().x,g.battle->units.back().y};g.battle->spellRound=0;g.hero.power=100;CHECK(g.cast(Fireball,p).empty());CHECK(g.battle->units.back().hp==0);g.hero.power=1;g.battle->spellRound=0;auto before=g.toJson();CHECK(!g.cast(SummonFireElemental).empty());CHECK(g.toJson()==before);CHECK(g.cast(SummonEarthElemental).empty());CHECK(g.validate());});
run("summons do not enter the casualty report or the surviving adventure army",[]{auto g=revivalGame();CHECK(g.cast(SummonEarthElemental).empty());winFixture(g);CHECK(g.report->won&&armyStacks(g.hero.army)==3);CHECK(g.report->xp==1000);CHECK(g.hero.army[1].unit==Pikeman);CHECK(g.validate());});
run("a battle can continue on summons but cannot retain them as the only survivors",[]{auto g=revivalGame();CHECK(g.cast(SummonEarthElemental).empty());makeCorpse(g,0);makeCorpse(g,1);makeCorpse(g,2);CHECK(g.battle.has_value()&&g.validate());winFixture(g);CHECK(!g.report->won&&g.result==-1&&armyCount(g.hero.army)==0);CHECK(g.validate());});
run("twenty allocated stacks remain the classic cap even after summoned stacks die",[]{auto g=revivalGame();for(int i=0;i<17;i++){g.battle->spellRound=0;CHECK(g.cast(SummonEarthElemental).empty());}CHECK(g.battle->units.size()==21);CHECK(g.validate());g.battle->spellRound=0;auto before=g.toJson();CHECK(!g.cast(SummonEarthElemental).empty());CHECK(g.toJson()==before);auto pos=Point{g.battle->units.back().x,g.battle->units.back().y};g.hero.power=100;CHECK(g.cast(Fireball,pos).empty());g.battle->spellRound=0;before=g.toJson();CHECK(!g.cast(SummonEarthElemental).empty());CHECK(g.toJson()==before);CHECK(g.validate());});
run("sacrifice requires two valid living targets and leaves no usable victim corpse",[]{auto g=revivalGame();makeCorpse(g,1);auto before=g.toJson();CHECK(!g.cast(Sacrifice,{7,5}).empty());CHECK(g.toJson()==before);auto victim=g.battle->units[2];CHECK(g.cast(Sacrifice,{7,5},{victim.x,victim.y}).empty());CHECK(g.battle->units[2].hp==0&&!g.battle->units[2].corpse);CHECK(g.battle->units[1].count==15&&g.battle->units[1].permanentCount==15);CHECK(g.validate());});
run("sacrifice uses each fire mastery flat constant and is always permanent",[]{const int expected[]={145,145,160,180};for(int mastery=0;mastery<4;mastery++){auto g=revivalGame();g.hero.skills[FireMagic]=mastery;makeCorpse(g,1);auto victim=g.battle->units[2];CHECK(g.cast(Sacrifice,{7,5},{victim.x,victim.y}).empty());CHECK(g.battle->units[1].hp==expected[mastery]);CHECK(g.battle->units[1].permanentCount==g.battle->units[1].count);}});
run("sacrifice respects fire immunity and cannot consume clones or raise a sacrificed body",[]{auto g=revivalGame(52,5);makeCorpse(g,2);CHECK(g.sacrificeVictims(2)==std::vector<int>{0});g.battle->units[0].effects[AntiMagic]=3;g.battle->units[0].effectMastery[AntiMagic]=3;CHECK(g.sacrificeVictims(2).empty());auto before=g.toJson();CHECK(!g.cast(Sacrifice,{10,5},{7,5}).empty());CHECK(g.toJson()==before);g=revivalGame();CHECK(g.cast(Clone,{7,5}).empty());makeCorpse(g,2);g.battle->spellRound=0;auto copy=g.battle->units.back();before=g.toJson();CHECK(!g.cast(Sacrifice,{10,5},{copy.x,copy.y}).empty());CHECK(g.toJson()==before);});
run("a midbattle save retains corpses temporary counts clones and summon restrictions",[]{auto g=revivalGame();makeCorpse(g,1);CHECK(g.cast(Resurrection,{7,5}).empty());g.battle->spellRound=0;CHECK(g.cast(Clone,{10,5}).empty());g.battle->spellRound=0;CHECK(g.cast(SummonEarthElemental).empty());auto p=testDirectory()/"heroesiii-revival-state.json";std::string error;CHECK(g.save(p,error));Game loaded;CHECK(loaded.load(p,error));CHECK(loaded.toJson()==g.toJson());std::filesystem::remove(p);});
run("version six battles infer original survivors without losing random or guild state",[]{auto g=revivalGame();CHECK(g.cast(Fireball,{7,5}).empty());auto old=g.toJson();old["version"]=6;while(old["hero"]["skills"].size()>12)old["hero"]["skills"].erase(old["hero"]["skills"].end()-1);old["battle"].erase("summonedType");old["battle"].erase("deathSequence");for(auto&u:old["battle"]["units"])for(auto key:{"origin","permanentCount","cloneOf","cloneTurns","deathOrder","corpse","acted"})u.erase(key);auto p=testDirectory()/"heroesiii-v6-revival.json";std::ofstream(p)<<old.dump();Game loaded;std::string error;CHECK(loaded.load(p,error));CHECK(loaded.toJson()==g.toJson());std::filesystem::remove(p);});
run("invalid clone links survivor counts corpse orders and summon types cannot overwrite live state",[]{auto g=revivalGame();CHECK(g.cast(Clone,{7,5}).empty());auto original=g.toJson();auto p=testDirectory()/"heroesiii-invalid-revival.json";for(int issue=0;issue<7;issue++){auto bad=original;switch(issue){case 0:bad["battle"]["units"].back()["cloneOf"]=999;break;case 1:bad["battle"]["units"][0]["permanentCount"]=1000001;break;case 2:bad["battle"]["units"][0]["corpse"]=true;break;case 3:bad["battle"]["summonedType"][0]=Pikeman;break;case 4:bad["battle"]["units"].back()["cloneTurns"]=0;break;case 5:bad["battle"]["units"][0].erase("permanentCount");break;case 6:bad["battle"]["summonedType"].push_back(-1);break;}std::ofstream(p)<<bad.dump();std::string error;CHECK(!g.load(p,error));CHECK(g.toJson()==original);}std::filesystem::remove(p);});

run("large fire shield reflection uses wide arithmetic without overflowing",[]{auto g=spellGame();g.hero.attack=g.hero.defense=0;g.hero.skills={};g.hero.army={Stack{140,1000000}};g.objectById(15)->army={Stack{Archangel,1000000}};g.startBattle(15);auto&blue=g.battle->units[0];auto&red=g.battle->units[1];blue.x=4;blue.y=5;red.x=5;red.y=5;red.effects[FireShield]=3;red.effectMastery[FireShield]=3;red.retaliations=1;int blueHP=blue.hp,redHP=red.hp;CHECK(g.validate());CHECK(g.act("attack",{5,5}).empty());int dealt=redHP-red.hp;CHECK(int64_t(dealt)*30>std::numeric_limits<int>::max());CHECK(blueHP-blue.hp==int(int64_t(dealt)*30/100));CHECK(g.validate());});
run("classic market lots match all nine observed rate rows",[]{
 const int expected[9][8][2]={
  {{2500,1},{5000,1},{1,25},{10,1},{20,1},{1,50},{5,1},{10,1}},
  {{1667,1},{3333,1},{1,37},{7,1},{13,1},{1,75},{3,1},{7,1}},
  {{1250,1},{2500,1},{1,50},{5,1},{10,1},{1,100},{3,1},{5,1}},
  {{1000,1},{2000,1},{1,62},{4,1},{8,1},{1,125},{2,1},{4,1}},
  {{833,1},{1667,1},{1,75},{3,1},{7,1},{1,150},{2,1},{3,1}},
  {{714,1},{1429,1},{1,88},{3,1},{6,1},{1,175},{1,1},{3,1}},
  {{625,1},{1250,1},{1,100},{3,1},{5,1},{1,200},{1,1},{3,1}},
  {{556,1},{1111,1},{1,112},{2,1},{4,1},{1,225},{1,1},{2,1}},
  {{500,1},{1000,1},{1,125},{2,1},{4,1},{1,250},{1,1},{2,1}}};
 const std::pair<int,int> pairs[]={{Gold,Wood},{Gold,Gems},{Wood,Gold},{Wood,Ore},{Wood,Gems},{Gems,Gold},{Gems,Ore},{Gems,Mercury}};
 for(int count=1;count<=9;count++)for(int category=0;category<8;category++){
  auto rate=marketRate(pairs[category].first,pairs[category].second,count);CHECK(rate);CHECK(rate->give==expected[count-1][category][0]&&rate->receive==expected[count-1][category][1]);
 }
});
run("market quotes cover all seven resources and reject invalid pairs",[]{
 for(int count=1;count<=9;count++)for(int sold=0;sold<ResourceCount;sold++)for(int bought=0;bought<ResourceCount;bought++){
  auto rate=marketRate(sold,bought,count);if(sold==bought){CHECK(!rate);continue;}CHECK(rate&&rate->give>0&&rate->receive>0);
  if(bought==Mercury||bought==Sulfur||bought==Crystal||bought==Gems){
   if(sold==Gold)CHECK(rate==marketRate(Gold,Gems,count));
   else if(sold==Wood||sold==Ore)CHECK(rate==marketRate(Wood,Gems,count));
   else CHECK(rate==marketRate(Gems,Mercury,count));
  }
 }
 CHECK(!marketRate(-1,Gold,1)&&!marketRate(Gold,7,1)&&!marketRate(Gold,Wood,0)&&!marketRate(Gold,Wood,-1));
 CHECK(marketRate(Gold,Wood,10)==marketRate(Gold,Wood,9));CHECK(marketRate(Gems,Gold,std::numeric_limits<int>::max())==marketRate(Gems,Gold,9));
});
run("a town market requires construction at its classic resource cost",[]{
 auto g=Game::create();auto before=g.toJson();CHECK(g.marketplaces()==0&&g.tradeLimit(Gold,Wood)==0);g.trade(Gold,Wood,1);CHECK(g.toJson()==before);
 g.build(Marketplace);CHECK(g.town.built[Marketplace]&&g.resources[Gold]==9500&&g.resources[Wood]==15&&g.marketplaces()==1);CHECK(g.validate());
});
run("a town market trades remotely without a visiting hero",[]{
 auto g=Game::create();g.build(Marketplace);g.hero.pos={8,18};CHECK(!g.visiting());g.trade(Gold,Mercury,1);
 CHECK(g.resources[Gold]==4500&&g.resources[Mercury]==6);CHECK(g.validate());
});
run("whole trade lots preserve resource remainders",[]{
 auto g=Game::create();g.build(Marketplace);CHECK(g.tradeLimit(Wood,Ore)==1);g.trade(Wood,Ore,1);CHECK(g.resources[Wood]==5&&g.resources[Ore]==21);
 g.trade(Crystal,Wood,1);CHECK(g.resources[Crystal]==0&&g.resources[Wood]==6);
 g.trade(Mercury,Gold,3);CHECK(g.resources[Mercury]==2&&g.resources[Gold]==9650);CHECK(g.validate());
});
run("invalid quantities and exchanges leave the entire game unchanged",[]{
 auto g=Game::create();g.build(Marketplace);auto before=g.toJson();
 for(auto [sold,bought,lots]:std::vector<std::tuple<int,int,int>>{{Gold,Gold,1},{Gold,Wood,0},{Gold,Wood,-1},{-1,Wood,1},{Gold,7,1},{Gold,Mercury,2},{Gold,Wood,std::numeric_limits<int>::max()}}){g.trade(sold,bought,lots);CHECK(g.toJson()==before);}
});
run("a stale market amount cannot overspend after a previous transaction",[]{
 auto g=Game::create();g.build(Marketplace);int limit=g.tradeLimit(Gold,Wood);CHECK(limit==3);g.trade(Gold,Wood,limit);CHECK(g.resources[Gold]==2000&&g.resources[Wood]==18&&g.tradeLimit(Gold,Wood)==0);
 auto before=g.toJson();g.trade(Gold,Wood,limit);CHECK(g.toJson()==before);CHECK(g.validate());
});
run("market limits guard the destination balance and large integer quantities",[]{
 auto g=Game::create();g.build(Marketplace);g.resources[Gold]=1000000000;g.resources[Wood]=999999999;
 CHECK(g.tradeLimit(Gold,Wood)==1);g.trade(Gold,Wood,1);CHECK(g.resources[Wood]==1000000000&&g.resources[Gold]==999997500);CHECK(g.validate());
 g.resources[Wood]=1000000000;g.resources[Gold]=999999990;CHECK(g.tradeLimit(Wood,Gold)==0);auto before=g.toJson();g.trade(Wood,Gold,std::numeric_limits<int>::max());CHECK(g.toJson()==before);CHECK(g.validate());
});
run("market exchanges preserve all unrelated resources and turn state",[]{
 for(int sold=0;sold<ResourceCount;sold++)for(int bought=0;bought<ResourceCount;bought++)if(sold!=bought){
  auto g=Game::create();g.build(Marketplace);g.resources.fill(100000);auto before=g.toJson();auto resources=g.resources;auto rate=marketRate(sold,bought,1);CHECK(rate);
  auto message=g.trade(sold,bought,1);resources[sold]-=rate->give;resources[bought]+=rate->receive;CHECK(g.resources==resources);
  auto after=g.toJson();before["resources"]=resources;before["log"].push_back(message);CHECK(after==before);CHECK(g.validate());
 }
});
run("battle result and pending level choices lock market transactions",[]{
 for(int phase=0;phase<4;phase++){
  auto g=Game::create();g.build(Marketplace);
  if(phase==0)g.startBattle(15);else if(phase==1)g.result=1;else if(phase==2)g.gainXP(1000);else g.report=BattleReport{};
  auto before=g.toJson();CHECK(g.tradeLimit(Gold,Wood)==0);g.trade(Gold,Wood,1);CHECK(g.toJson()==before);
 }
});
run("a pending battle report prevents exploration and resource pickup",[]{
 auto g=Game::create();Point destination{g.hero.pos.x,g.hero.pos.y+1};auto*t=g.tile(destination);t->terrain=Terrain::Grass;t->decor=Decor::None;t->seen=true;
 Object resource;resource.id=999;resource.type=ObjectType::Resource;resource.pos=destination;resource.resource=Ore;resource.amount=7;g.objects.push_back(resource);g.report=BattleReport{};
 auto before=g.toJson();CHECK(!g.move(destination).empty());CHECK(g.toJson()==before);
 g.report.reset();int ore=g.resources[Ore];CHECK(g.move(destination).find("7")!=std::string::npos);CHECK(g.hero.pos==destination&&g.resources[Ore]==ore+7&&!g.objectById(999));CHECK(g.validate());
});
run("Trading Post access requires the actual hero and a genuine post object",[]{
 auto g=Game::create();auto post=std::find_if(g.objects.begin(),g.objects.end(),[](const auto&o){return o.type==ObjectType::TradingPost;});CHECK(post!=g.objects.end());int id=post->id;Point position=post->pos;
 auto before=g.toJson();CHECK(g.marketplaces(id)==0);g.trade(Gold,Wood,1,id);CHECK(g.toJson()==before);
 g.hero.pos=position;CHECK(g.marketplaces(id)==5&&g.marketplaces()==0&&g.marketplaces(-1)==0&&g.marketplaces(999)==0&&g.marketplaces(1)==0);
 g.trade(Gold,Wood,1,id);CHECK(g.resources[Gold]==9167&&g.resources[Wood]==21&&!g.town.built[Marketplace]);CHECK(g.validate());
});
run("Trading Post prices are fixed at five markets without stacking",[]{
 auto g=Game::create();g.build(Marketplace);auto post=std::find_if(g.objects.begin(),g.objects.end(),[](const auto&o){return o.type==ObjectType::TradingPost;});CHECK(post!=g.objects.end());int id=post->id;g.hero.pos=post->pos;
 CHECK(g.marketplaces()==1&&g.marketplaces(id)==5);CHECK(g.tradeLimit(Gold,Wood,id)==11&&g.tradeLimit(Gold,Wood)==3);
 int gold=g.resources[Gold];g.trade(Gems,Gold,2,id);CHECK(g.resources[Gold]==gold+300&&g.resources[Gems]==3);CHECK(g.marketplaces()==1&&g.marketplaces(id)==5);
});
run("a normal route visits and revisits the persistent Trading Post",[]{
 auto g=Game::create();CHECK(g.move({4,21}).empty());CHECK(g.move({5,21}).empty());auto post=std::find_if(g.objects.begin(),g.objects.end(),[](const auto&o){return o.type==ObjectType::TradingPost;});CHECK(post!=g.objects.end());int id=post->id;Point position=post->pos;
 auto route=g.path(position);CHECK(!route.empty());for(auto step:route)g.move({step.x,step.y});CHECK(g.hero.pos==position&&g.marketplaces(id)==5&&g.objectById(id));
 auto resources=g.resources;g.visit(id);CHECK(g.resources==resources&&g.marketplaces(id)==5);CHECK(g.validate());
});
run("leaving a Trading Post immediately revokes its trading access",[]{
 auto g=Game::create();auto post=std::find_if(g.objects.begin(),g.objects.end(),[](const auto&o){return o.type==ObjectType::TradingPost;});CHECK(post!=g.objects.end());int id=post->id;g.hero.pos=post->pos;CHECK(g.marketplaces(id)==5);
 g.hero.pos={4,20};auto before=g.toJson();CHECK(g.marketplaces(id)==0&&g.tradeLimit(Gold,Wood,id)==0);g.trade(Gold,Wood,1,id);CHECK(g.toJson()==before);
});
run("market resources and visited post state survive a version eight save",[]{
 auto g=Game::create();auto post=std::find_if(g.objects.begin(),g.objects.end(),[](const auto&o){return o.type==ObjectType::TradingPost;});CHECK(post!=g.objects.end());int id=post->id;g.hero.pos=post->pos;g.trade(Gold,Wood,2,id);
 auto path=testDirectory()/"market-v8.json";std::string error;CHECK(g.save(path,error));Game loaded;CHECK(loaded.load(path,error));CHECK(loaded.toJson()==g.toJson()&&loaded.toJson()["version"]==19&&loaded.marketplaces(id)==5);std::filesystem::remove(path);
});
run("version seven saves retain resources and their original map objects",[]{
 auto g=Game::create();g.build(Marketplace);std::erase_if(g.objects,[](const auto&o){return o.type==ObjectType::TradingPost;});auto old=g.toJson();old["version"]=7;while(old["hero"]["skills"].size()>12)old["hero"]["skills"].erase(old["hero"]["skills"].end()-1);
 auto path=testDirectory()/"market-v7.json";std::ofstream(path)<<old.dump();Game loaded;std::string error;CHECK(loaded.load(path,error));CHECK(loaded.toJson()==g.toJson()&&loaded.marketplaces()==1);
 CHECK(std::none_of(loaded.objects.begin(),loaded.objects.end(),[](const auto&o){return o.type==ObjectType::TradingPost;}));std::filesystem::remove(path);
});
run("unknown market map object types cannot overwrite a valid save",[]{
 auto g=Game::create();auto before=g.toJson();auto bad=before;bad["objects"][0]["type"]=int(ObjectType::WaterWheel)+1;
 auto path=testDirectory()/"market-bad-object.json";std::ofstream(path)<<bad.dump();std::string error;CHECK(!g.load(path,error));CHECK(g.toJson()==before);std::filesystem::remove(path);
});
run("classic exchange spreads cannot create resources through a return trade",[]{
 for(int markets=1;markets<=9;markets++)for(int a=0;a<ResourceCount;a++)for(int b=0;b<ResourceCount;b++)if(a!=b){auto ab=marketRate(a,b,markets),ba=marketRate(b,a,markets);CHECK(ab&&ba);CHECK(int64_t(ab->give)*ba->give>=int64_t(ab->receive)*ba->receive);}
});
run("normal town income and market purchases can fund a fifth-level guild",[]{
 auto g=Game::create(1999,2);g.build(Marketplace);CHECK(g.town.built[Marketplace]);
 for(int building:{MageGuild,MageGuild2,MageGuild3,MageGuild4,MageGuild5}){
  if(g.town.builtDay==g.day)g.endDay();const auto cost=g.townDef().buildings[building].cost;
  for(int resource=1;resource<ResourceCount;resource++)if(cost[resource]>g.resources[resource]){
   int amount=cost[resource]-g.resources[resource];auto rate=marketRate(Gold,resource,g.marketplaces());CHECK(rate&&rate->receive==1);
   while(g.resources[Gold]<int64_t(rate->give)*amount+cost[Gold]){g.endDay();CHECK(g.day<1000);}
   g.trade(Gold,resource,amount);CHECK(g.resources[resource]>=cost[resource]);
  }
  while(!g.canAfford(cost)){g.endDay();CHECK(g.day<1000);}g.build(building);CHECK(g.town.built[building]&&g.validate());
 }
 CHECK(g.mageGuildLevel()==5&&g.day>100&&g.resources[Mercury]>=0&&g.resources[Sulfur]>=0&&g.resources[Crystal]>=0&&g.resources[Gems]>=0);
});


run("town masks preserve nonrectangular pixels and bounds",[]{
 auto path=testDirectory()/"known.mask";const unsigned char data[]={'H','3','M','K',3,0,0,0,2,0,0,0,0,1,0,1,0,1};
 std::ofstream(path,std::ios::binary).write(reinterpret_cast<const char*>(data),sizeof(data));std::string error;auto mask=readPixelMask(path,error);
 CHECK(mask&&mask->width==3&&mask->height==2);CHECK(!mask->contains(0,0)&&mask->contains(1,0)&&mask->contains(0,1)&&mask->contains(2,1));
 CHECK(!mask->contains(-1,0)&&!mask->contains(0,-1)&&!mask->contains(3,1)&&!mask->contains(2,2));
});
run("malformed or unbounded town masks are rejected",[]{
 auto path=testDirectory()/"corrupt.mask";std::vector<unsigned char> valid={'H','3','M','K',1,0,0,0,1,0,0,0,1};std::string error;
 auto rejected=[&](std::vector<unsigned char> bytes){std::ofstream(path,std::ios::binary).write(reinterpret_cast<const char*>(bytes.data()),bytes.size());CHECK(!readPixelMask(path,error));};
 auto bad=valid;bad[0]='X';rejected(bad);bad=valid;bad[4]=0;rejected(bad);bad=valid;bad[5]=32;rejected(bad);
 bad=valid;bad.pop_back();rejected(bad);bad=valid;bad.push_back(0);rejected(bad);bad=valid;bad[12]=2;rejected(bad);rejected({});
});
run("all active town layers have loadable masks and tracked provenance",[]{
 auto root=std::filesystem::path(H3_TEST_SOURCE_ROOT);std::ifstream input(root/"assets/demo/town-ui.json");auto metadata=nlohmann::json::parse(input);int original=0,derived=0;
 CHECK(metadata["missingHallBackgrounds"].size()==4&&metadata["derivedMasks"].size()==237);
 for(const auto&town:TownDefs)for(const auto&layer:town.layers){std::string error;auto mask=readPixelMask(root/layer.mask,error);CHECK(mask);CHECK(std::filesystem::exists(root/layer.outline));CHECK(std::any_of(mask->pixels.begin(),mask->pixels.end(),[](int p){return p==1;}));if(layer.originalMask)original++;else derived++;}
 CHECK(original>90&&derived>180);
});
run("hall cards use bounded original icons in all nine factions",[]{
 auto root=std::filesystem::path(H3_TEST_SOURCE_ROOT);std::ifstream input(root/"assets/demo/manifest.json");auto assets=nlohmann::json::parse(input);
 for(int faction=0;faction<FactionCount;faction++){auto g=Game::create(1999,faction);const auto&t=g.townDef();CHECK(t.hallSlots.size()==5);CHECK(assets["def"].contains(t.hallIcons));int frames=assets["def"][t.hallIcons]["groups"]["0"].size();CHECK(frames==44);
  std::set<int> listed;for(int row=0;row<5;row++){CHECK(t.hallSlots[row].size()>=2&&t.hallSlots[row].size()<=4);for(int col=0;col<int(t.hallSlots[row].size());col++){
   CHECK(g.hallBuilding(row,col)>=0);for(int id:t.hallSlots[row][col]){CHECK(id>=0&&id<BuildingCount);CHECK(listed.insert(id).second);CHECK(t.buildings[id].icon>=0&&t.buildings[id].icon<frames);CHECK(g.hallIcon(id)>=0&&g.hallIcon(id)<frames);}
  }}
  for(int id=0;id<BuildingCount;id++)if(t.buildings[id].available)CHECK(listed.contains(id));
 }
});
run("eight original factions use dedicated fort previews and recovered tower halls",[]{
 auto root=std::filesystem::path(H3_TEST_SOURCE_ROOT);std::ifstream file(root/"assets/demo/manifest.json");auto assets=nlohmann::json::parse(file);
 std::ifstream metadataFile(root/"data/native/town-interface.json");auto metadata=nlohmann::json::parse(metadataFile);
 for(int faction=0;faction<8;faction++){const auto&preview=metadata["fortPreviewBackgrounds"][faction];CHECK(preview["dedicatedFortAsset"].get<bool>());const auto&image=assets["pcx"][preview["file"].get<std::string>()];CHECK(image["width"]==100&&image["height"]==120);}
 CHECK(!metadata["fortPreviewBackgrounds"][8]["dedicatedFortAsset"].get<bool>());
 for(int faction:{2,6}){const auto&image=assets["pcx"][TownDefs[faction].hallBackground];CHECK(image["width"]==800&&image["height"]==600);CHECK(image["source"]["edition"]=="windows-roe-demo");}
});
run("hall cards advance to the next unbuilt upgrade and then stay owned",[]{
 for(int faction=0;faction<FactionCount;faction++){auto g=Game::create(1999,faction);g.town.built.fill(false);
  for(int row=0;row<int(g.townDef().hallSlots.size());row++)for(int col=0;col<int(g.townDef().hallSlots[row].size());col++){
   std::vector<int> chain;for(int id:g.townDef().hallSlots[row][col])if(g.townDef().buildings[id].available)chain.push_back(id);CHECK(!chain.empty());
   for(int id:chain){CHECK(g.hallBuilding(row,col)==id);g.town.built[id]=true;}CHECK(g.hallBuilding(row,col)==chain.back());CHECK(g.constructionState(chain.back())==BuildState::Owned);
  }
  CHECK(g.hallBuilding(-1,0)==-1&&g.hallBuilding(5,0)==-1&&g.hallBuilding(0,99)==-1&&g.hallIcon(-1)==-1&&g.hallIcon(BuildingCount)==-1);
 }
});
run("horde hall illustrations follow the matching upgraded dwelling",[]{
 for(int faction=0;faction<FactionCount;faction++){auto g=Game::create(1999,faction);for(int id:{GriffinBastion,Horde2}){
  const auto&definition=g.townDef().buildings[id];if(!definition.available||definition.upIcon<0)continue;CHECK(g.hallIcon(id)==definition.icon);int tier=g.townDef().hordeTier[id==Horde2];CHECK(tier>=0&&tier<7);
  g.town.built[UpGuardhouse+tier]=true;CHECK(g.hallIcon(id)==definition.upIcon);
 }}
});
run("construction status distinguishes costs dependencies daily limit and ownership",[]{
 auto g=Game::create();CHECK(g.constructionState(Marketplace)==BuildState::Ready);CHECK(g.constructionState(Capitol)==BuildState::MissingRequirements);g.resources[Gold]=0;CHECK(g.constructionState(Marketplace)==BuildState::NoResources);
 g.resources[Gold]=10000;g.build(Marketplace);CHECK(g.constructionState(Marketplace)==BuildState::Owned);CHECK(g.constructionState(ResourceSilo)==BuildState::AlreadyBuiltToday);g.endDay();CHECK(g.constructionState(ResourceSilo)==BuildState::Ready);
 CHECK(g.constructionState(Shipyard)==BuildState::Unavailable);CHECK(g.constructionState(-1)==BuildState::Invalid&&g.constructionState(BuildingCount)==BuildState::Invalid);
 g=Game::create(1999,2);CHECK(g.constructionState(MageGuild5)==BuildState::MissingRequirements);CHECK(g.constructionState(Shipyard)==BuildState::Unavailable);
});
run("construction cannot mutate state during battle report level choice or result",[]{
 for(int phase=0;phase<4;phase++){auto g=Game::create();if(phase==0)g.startBattle(15);if(phase==1){g.report=BattleReport{};}if(phase==2)g.gainXP(1000);if(phase==3)g.result=1;
  auto before=g.toJson();CHECK(g.constructionState(Marketplace)==BuildState::Busy);CHECK(g.build(Marketplace)=="当前无法建设");CHECK(g.toJson()==before);
 }
});


run("town army operations reject unresolved phases without side effects",[]{
 for(int phase=0;phase<4;phase++){auto g=Game::create();g.town.built[UpGuardhouse]=true;if(phase==0)g.startBattle(15);if(phase==1)g.report=BattleReport{};if(phase==2)g.gainXP(1000);if(phase==3)g.result=1;auto before=g.toJson();
  CHECK(g.recruit(Pikeman,1)=="当前无法招募");CHECK(g.upgrade(false,0)=="当前无法升级");CHECK(g.transfer(false,0,true,0,1)=="当前无法调整部队");CHECK(g.dismiss(false,0)=="无效操作");CHECK(g.toJson()==before);
 }
});
run("recruitment and merging cannot exceed the current native save stack limit",[]{
 auto g=Game::create();g.hero.army[0].count=MaxArmyStack-1;auto before=g.toJson();CHECK(g.recruit(Pikeman,2)=="超过当前部队数量上限");CHECK(g.toJson()==before);
 g.recruit(Pikeman,1);CHECK(g.hero.army[0].count==MaxArmyStack&&g.validate());g.town.garrison[0]={Pikeman,2};before=g.toJson();CHECK(g.transfer(true,0,false,0,1)=="超过当前部队数量上限");CHECK(g.toJson()==before);
 g.town.garrison[1]={Archer,1};CHECK(g.transfer(false,0,true,1,MaxArmyStack)=="部队交换完成。");CHECK(g.town.garrison[1].count==MaxArmyStack&&g.hero.army[0].unit==Archer&&g.validate());
});


run("stereo audio preserves channels and clips mixed peaks",[]{
 AudioMixer mixer;mixer.volumes(9,9);auto clip=std::make_shared<AudioBuffer>();clip->samples={.75f,-.75f};CHECK(mixer.play(clip)&&mixer.play(clip));std::array<float,2> output;mixer.render(output);CHECK(output[0]==1&&output[1]==-1&&mixer.activeVoices()==0);
});
run("music and effects volumes are independent",[]{
 AudioMixer mixer;auto clip=std::make_shared<AudioBuffer>();clip->samples={.9f,.9f};mixer.music(clip,true,0);mixer.volumes(3,6);mixer.play(clip);std::array<float,2> output;mixer.render(output);CHECK(std::abs(output[0]-.9f)<.0001f);mixer.volumes(3,0);mixer.play(clip);mixer.render(output);CHECK(std::abs(output[0]-.3f)<.0001f);
});
run("audio loop crosses callback boundaries and clears incomplete frames",[]{
 AudioMixer mixer;mixer.volumes(9,9);auto clip=std::make_shared<AudioBuffer>();clip->samples={.1f,.2f,.3f,.4f};mixer.play(clip,AudioBus::Effects,true);std::array<float,5> output;mixer.render(output);CHECK(output[0]==.1f&&output[1]==.2f&&output[2]==.3f&&output[3]==.4f&&output[4]==0);mixer.render(output);CHECK(output[0]==.1f&&mixer.activeVoices()==1);
});
run("battle routes retain every legal walking step around occupied cells",[]{
 auto g=Game::create();g.hero.army={Stack{Pikeman,20},Stack{Pikeman,5}};g.startBattle(15);auto&u=g.battle->units[0];u.x=3;u.y=5;g.battle->units[1].x=4;g.battle->units[1].y=5;
 auto path=g.battleRoute(u,{6,5});CHECK((path.front()==Point{3,5}&&path.back()==Point{6,5}));CHECK(g.battlePath(u,{6,5})==int(path.size())-1);
 for(size_t i=1;i<path.size();i++){CHECK(Game::hexDistance(path[i-1],path[i])==1);CHECK(!g.combatAt(path[i]));}
 CHECK(g.battleRoute(u,{4,5}).empty()&&g.battleRoute(u,{-1,5}).empty());CHECK(g.battleRoute(u,{3,5}).size()==1);
 u.unit=Champion;auto wide=g.battleRoute(u,{6,6});CHECK(!wide.empty());for(size_t i=1;i<wide.size();i++)for(auto p:g.footprint(u,wide[i])){CHECK(p.x>=0&&p.x<BattleW);CHECK(!g.combatAt(p)||g.combatAt(p)->id==u.id);}
});
run("flying routes keep a direct flight while validating the landing footprint",[]{
 auto g=Game::create();g.startBattle(15);auto&u=g.battle->units[2];Point start{u.x,u.y},end{6,7};auto route=g.battleRoute(u,end);CHECK((route==std::vector<Point>{start,end}));CHECK(g.battlePath(u,end)==Game::hexDistance(start,end));
 auto other=g.battle->units.back();CHECK(g.battleRoute(u,{other.x,other.y}).empty());u.unit=Archangel;CHECK(g.battleRoute(u,{0,6}).empty());
});
run("accepted combat movement emits the exact route and stable stack identity",[]{
 auto g=Game::create();g.hero.army={Stack{Pikeman,20},Stack{},Stack{Pikeman,5}};g.objectById(15)->army={Stack{Skeleton,30}};g.startBattle(15);g.takeEvents();auto*active=g.current();int id=active->id;Point start{active->x,active->y},end{3,active->y};auto route=g.battleRoute(*active,end);CHECK(g.act("move",end).empty());auto events=g.takeEvents();CHECK(events.size()==1&&events[0].type==GameEventType::CombatMove&&events[0].stackId==id&&events[0].path==route);CHECK((events[0].path.front()==start&&events[0].pos==end));
 g.act("move",{-1,-1});CHECK(g.takeEvents().empty());
});
run("combat attack and damage events identify both duplicate creature stacks",[]{
 auto g=Game::create();g.hero.army={Stack{Archer,50},Stack{},Stack{Archer,1}};g.objectById(15)->army={Stack{Pikeman,1}};g.startBattle(15);g.takeEvents();CHECK(g.act("attack",{g.battle->units.back().x,g.battle->units.back().y}).empty());auto events=g.takeEvents();CHECK(events[0].type==GameEventType::CombatAttack&&events[0].stackId==0&&events[0].targetId==100);CHECK(events[1].type==GameEventType::CombatDeath&&events[1].stackId==100);
});
run("movement presentation plays original start walk and end groups in sequence",[]{
 auto g=Game::create();g.hero.army={Stack{Pikeman,20}};g.startBattle(15);Battle before=*g.battle;auto&u=before.units[0];u.x=3;u.y=5;Point from{3,5},to{4,5};GameEvent move{GameEventType::CombatMove,Pikeman,0,to,u.id,-1,{from,to}};
 std::vector<GameEvent> events{{GameEventType::NewDay,-1,2},move,{GameEventType::CombatAttack,Pikeman,0,to,u.id,100}};
 BattleMotion motion;motion.start(before,events,[](int,int group){return group==20?2:group==21?3:4;},1000);auto cue=motion.takeEvents();CHECK(cue.size()==1&&cue[0].type==GameEventType::NewDay);CHECK(motion.pose(u.id)->group==20&&motion.pose(u.id)->frame==0);
 motion.tick(1230);CHECK(motion.pose(u.id)->group==0);cue=motion.takeEvents();CHECK(cue.size()==1&&cue[0].type==GameEventType::CombatMove);
 motion.tick(1368);auto pose=motion.pose(u.id);auto a=battleHexCenter(from),b=battleHexCenter(to);CHECK(pose&&std::abs(pose->x-(a.x+b.x)/2.)<.001&&pose->group==0&&pose->frame==1);CHECK((motion.view()->units[0].x==from.x));
 motion.tick(1506);pose=motion.pose(u.id);CHECK(pose&&pose->group==21&&pose->x==b.x&&motion.takeEvents().empty());motion.tick(1851);CHECK(motion.active()&&motion.pose(u.id)->group==12);cue=motion.takeEvents();CHECK(cue.size()==1&&cue[0].type==GameEventType::CombatAttack);motion.tick(10000);CHECK(!motion.active()&&!motion.view());
});
run("paused movement preserves progress and repeated reads do not advance it",[]{
 auto g=Game::create();g.startBattle(15);auto before=*g.battle;auto u=before.units[0];Point end{u.x+1,u.y};GameEvent event{GameEventType::CombatMove,u.unit,0,end,u.id,-1,{{u.x,u.y},end}};
 BattleMotion motion;motion.start(before,std::span(&event,1),[](int,int group){return group==0?8:0;},1000);auto start=motion.pose(u.id);motion.tick(5000,false);CHECK(motion.pose(u.id)->x==start->x);motion.tick(5100,true);auto moved=motion.pose(u.id);CHECK(moved->x>start->x&&motion.pose(u.id)->x==moved->x);motion.tick(5200,false);CHECK(motion.pose(u.id)->x==moved->x);motion.tick(5150,true);CHECK(motion.pose(u.id)->x==moved->x);
 motion.tick(10000);CHECK(!motion.active());motion.clear();CHECK(motion.takeEvents().empty());
});
run("movement presentation distinguishes equal unit types and retains queued cues",[]{
 auto g=Game::create();g.hero.army={Stack{Pikeman,20},Stack{Pikeman,5}};g.startBattle(15);auto before=*g.battle;auto u=before.units[1];Point end{u.x+1,u.y};GameEvent event{GameEventType::CombatMove,u.unit,0,end,u.id,-1,{{u.x,u.y},end}};
 BattleMotion motion;auto count=[](int,int group){return group==0?8:0;};motion.start(before,std::span(&event,1),count,0);CHECK(motion.pose(u.id)&&!motion.pose(before.units[0].id));motion.tick(10000);GameEvent cue{GameEventType::NewDay,-1,2};motion.start(before,std::span(&cue,1),count,10000);auto cues=motion.takeEvents();CHECK(cues.size()==2&&cues[0].type==GameEventType::CombatMove&&cues[1].type==GameEventType::NewDay&&!motion.active());
});
run("missing movement sprites and stale routes cannot stall the presentation",[]{
 auto g=Game::create();g.startBattle(15);auto before=*g.battle;auto u=before.units[0];Point end{u.x+1,u.y};GameEvent event{GameEventType::CombatMove,u.unit,0,end,u.id,-1,{{u.x,u.y},end}};BattleMotion motion;auto missing=[](int,int){return 0;};
 motion.start(before,std::span(&event,1),missing,0);CHECK(motion.active()&&motion.pose(u.id)->frame==0);motion.tick(10000);CHECK(!motion.active());motion.takeEvents();event.stackId=999;motion.start(before,std::span(&event,1),missing,10000);CHECK(!motion.active()&&motion.takeEvents().size()==1);event.stackId=u.id;event.path.front()={-1,0};motion.start(before,std::span(&event,1),missing,10000);CHECK(!motion.active());
});
run("movement playback leaves the committed save and random stream untouched",[]{
 auto g=Game::create();g.startBattle(15);g.takeEvents();Battle before=*g.battle;auto id=g.current()->id;CHECK(g.act("move",{4,7}).empty());auto events=g.takeEvents();auto saved=g.toJson();BattleMotion motion;motion.start(before,events,[](int,int group){return group==0?8:0;},0);CHECK(motion.active()&&motion.pose(id));for(int t=0;t<=10000;t+=37){motion.tick(t);motion.takeEvents();CHECK(g.toJson()==saved);}CHECK(!motion.active());
});
run("combat events retain intermediate snapshots for retaliation and double strikes",[]{
 auto g=meleeGame(Crusader,100,Skeleton,1000);auto before=g.toJson();CHECK(g.act("attack",{6,5}).empty());auto events=g.takeEvents();std::vector<GameEvent> attacks;for(const auto&e:events)if(e.type==GameEventType::CombatAttack)attacks.push_back(e);
 CHECK(attacks.size()==3&&attacks[0].stackId==0&&attacks[1].stackId==100&&attacks[2].stackId==0);CHECK(attacks[0].state&&attacks[2].state&&attacks[0].state->count>attacks[2].state->count);CHECK(events[1].state&&events[1].state->hp<before["battle"]["units"][1]["hp"].get<int>());auto saved=g.toJson();g.takeEvents();CHECK(g.toJson()==saved);
});
run("melee impact updates counts only at the scheduled hit and then retaliates",[]{
 auto g=meleeGame();auto before=*g.battle;CHECK(g.act("attack",{6,5}).empty());auto events=g.takeEvents();CHECK(events.size()==4);auto saved=g.toJson();BattleMotion motion;motion.start(before,events,actionFrames,0);auto cues=motion.takeEvents();CHECK(cues.size()==1&&cues[0].stackId==0&&motion.pose(0)->group==12);
 motion.tick(499);CHECK(motion.view()->units[1].hp==before.units[1].hp&&!motion.pose(100));motion.tick(500);CHECK(motion.view()->units[1].hp==events[1].state->hp&&motion.pose(100)->group==3&&motion.pose(100)->frame==0);cues=motion.takeEvents();CHECK(cues.size()==1&&cues[0].stackId==100);
 motion.tick(1000);CHECK(motion.pose(100)->group==12&&motion.view()->units[0].hp==before.units[0].hp);motion.tick(1500);CHECK(motion.view()->units[0].hp==events[3].state->hp&&motion.pose(0)->group==3);motion.tick(2000);CHECK(!motion.active()&&g.toJson()==saved);
});
run("a partial hit reduces HP and reports damage even without a casualty",[]{
 auto g=meleeGame(Pikeman,10,Cavalier,2);auto before=*g.battle;CHECK(g.act("attack",{6,5}).empty());auto saved=g.toJson();auto events=g.takeEvents();
 CHECK(events[1].type==GameEventType::CombatHit&&events[1].state->count==2&&events[1].state->hp<200);
 BattleMotion motion;motion.start(before,events,actionFrames,0);CHECK(motion.damageMessage().empty());motion.tick(499);CHECK(motion.view()->units[1].hp==200&&motion.damageMessage().empty());
 motion.tick(500);CHECK(motion.view()->units[1].hp==events[1].state->hp&&motion.view()->units[1].count==2);CHECK(motion.damageMessage().find(std::to_string(200-events[1].state->hp))!=std::string::npos);
 auto message=motion.damageMessage();motion.takeEvents();CHECK(motion.damageMessage()==message);motion.tick(100000);CHECK(g.toJson()==saved);motion.clear();CHECK(motion.damageMessage().empty());
});
run("lethal feedback reports actual remaining HP instead of overkill or clone metadata",[]{
 auto g=meleeGame(Pikeman,100,Skeleton,1);auto before=*g.battle;CHECK(g.act("attack",{6,5}).empty());auto events=g.takeEvents();
 BattleMotion motion;motion.start(before,events,actionFrames,0);motion.tick(500);CHECK(motion.damageMessage().find("6 ")!=std::string::npos);CHECK(motion.view()->units[1].hp==0&&motion.view()->units[1].count==0);
});
run("defending victims use the original defending hit group without looping",[]{
 auto g=meleeGame();g.battle->units[1].defending=true;auto before=*g.battle;g.act("attack",{6,5});auto events=g.takeEvents();BattleMotion motion;motion.start(before,events,actionFrames,0);motion.tick(700);CHECK(motion.pose(100)->group==4&&motion.pose(100)->frame==2);motion.tick(950);CHECK(!motion.pose(100));motion.tick(1000);CHECK(motion.pose(100)->group==12);
});
run("death transitions play once before preserving the final corpse frame",[]{
 auto g=meleeGame(Pikeman,100,Skeleton,1);auto before=*g.battle;g.act("attack",{6,5});CHECK(!g.battle&&g.report);auto events=g.takeEvents();BattleMotion motion;motion.start(before,events,actionFrames,0);motion.tick(499);CHECK(motion.view()->units[1].hp>0);motion.tick(500);CHECK(motion.pose(100)->dying&&motion.pose(100)->group==5&&motion.view()->units[1].count==0);motion.tick(1099);CHECK(motion.pose(100)->frame==5&&motion.pose(100)->dying);auto corpse=motion.view()->units[1];motion.tick(1100);CHECK(!motion.active());auto pose=motion.corpsePose(corpse);CHECK(pose&&pose->group==5&&pose->frame==-1&&!pose->dying);corpse.hp=10;corpse.corpse=false;CHECK(!motion.corpsePose(corpse));
});
run("ranged attacks select direction and use the original release frame",[]{
 auto g=meleeGame(Archer,50,Pikeman,1000);g.battle->units[1].x=10;g.battle->units[1].y=3;g.battle->units[1].defending=true;auto before=*g.battle;CHECK(g.act("attack",{10,3}).empty());auto events=g.takeEvents();CHECK(events[0].state&&events[0].state->shots==before.units[0].shots-1);BattleMotion motion;motion.start(before,events,actionFrames,0);CHECK(motion.pose(0)->group==14);motion.tick(699);CHECK(motion.view()->units[1].hp==before.units[1].hp);motion.tick(700);CHECK(motion.projectiles().size()==1&&motion.view()->units[1].hp==before.units[1].hp);for(int t=701;t<3000&&motion.view()->units[1].hp==before.units[1].hp;++t)motion.tick(t);CHECK(motion.projectiles().empty()&&motion.pose(100)->group==3&&motion.view()->units[1].hp<before.units[1].hp);motion.tick(10000);CHECK(!motion.active());
});
run("ranged death frames remain distinct from melee corpses across actions",[]{
 auto g=meleeGame(Archer,50,Pikeman,1);g.battle->units[1].x=10;auto before=*g.battle;g.act("attack",{10,5});auto events=g.takeEvents();BattleMotion motion;motion.start(before,events,actionFrames,0);motion.tick(700);CHECK(!motion.projectiles().empty()&&motion.view()->units[1].hp>0);for(int t=701;t<3000&&motion.view()->units[1].hp>0;++t)motion.tick(t);CHECK(motion.pose(100)->group==6&&motion.pose(100)->dying);auto corpse=motion.view()->units[1];motion.tick(10000);CHECK(!motion.active()&&motion.corpsePose(corpse)->group==6);before.units[1]=corpse;GameEvent cue{GameEventType::NewDay,-1,2};motion.start(before,std::span(&cue,1),actionFrames,12000);CHECK(motion.corpsePose(corpse)->group==6);motion.clear();CHECK(!motion.corpsePose(corpse));
});
run("backward attacks turn both living stacks and restore their resting side",[]{
 auto g=meleeGame();g.battle->units[0].x=7;auto before=*g.battle;g.act("attack",{6,5});auto events=g.takeEvents();BattleMotion motion;motion.start(before,events,actionFrames,0);CHECK(motion.pose(0)->group==7&&!motion.pose(0)->flip);motion.tick(200);CHECK(motion.pose(0)->group==8&&motion.pose(0)->flip);motion.tick(400);CHECK(motion.pose(0)->group==2&&motion.pose(0)->flip&&motion.pose(100)->group==7);motion.tick(600);CHECK(motion.pose(100)->group==8&&!motion.pose(100)->flip);motion.tick(800);CHECK(motion.pose(0)->group==12&&motion.pose(0)->flip);motion.tick(2800);CHECK(motion.pose(0)->group==7&&motion.pose(0)->flip);motion.tick(3000);CHECK(motion.pose(0)->group==8&&!motion.pose(0)->flip);motion.tick(3400);CHECK(motion.pose(100)->group==8&&motion.pose(100)->flip);motion.tick(3600);CHECK(!motion.active());
});
run("double strikes preserve attack retaliation and second attack order",[]{
 auto g=meleeGame(Crusader,100,Skeleton,1000);auto before=*g.battle;g.act("attack",{6,5});auto events=g.takeEvents();BattleMotion motion;motion.start(before,events,actionFrames,0);std::vector<int> attackers;for(int t=0;t<=10000;t+=37){motion.tick(t);for(const auto&e:motion.takeEvents())if(e.type==GameEventType::CombatAttack)attackers.push_back(e.stackId);}CHECK((attackers==std::vector<int>{0,100,0}));CHECK(!motion.active()&&motion.takeEvents().empty());
});
run("fire shield reactions share the impact without delaying the reflected loss",[]{
 auto g=meleeGame(Pikeman,100,53,1);auto before=*g.battle;g.act("attack",{6,5});auto events=g.takeEvents();CHECK(events.size()==4&&events[1].type==GameEventType::CombatDeath&&events[2].type==GameEventType::CombatHit);BattleMotion motion;motion.start(before,events,actionFrames,0);motion.tick(500);CHECK(motion.pose(100)->dying&&motion.pose(0)->group==3);CHECK(motion.view()->units[0].hp==events[2].state->hp);motion.tick(1100);CHECK(!motion.active());
});
run("killing an original removes its mirror without animating a second corpse",[]{
 auto g=spellGame();g.hero.spells[Clone]=true;g.hero.skills[WaterMagic]=3;g.hero.army={Stack{Pikeman,20}};g.objectById(15)->army={Stack{Archer,1000}};g.startBattle(15);auto u=g.battle->units[0];CHECK(g.cast(Clone,{u.x,u.y}).empty());int mirror=g.battle->units.back().id;g.battle->queue={100,0,mirror};g.takeEvents();auto before=*g.battle;CHECK(g.act("attack",{u.x,u.y}).empty());auto events=g.takeEvents();BattleMotion motion;motion.start(before,events,actionFrames,0);for(int t=0;t<5000&&motion.view()->units[0].hp>0;++t)motion.tick(t);CHECK(motion.view()->units[0].hp==0&&motion.view()->units.back().hp==0&&!motion.view()->units.back().corpse&&!motion.pose(mirror));motion.tick(100000);CHECK(!motion.active());
});
run("missing action groups and mismatched damage snapshots cannot stall or corrupt playback",[]{
 auto g=meleeGame();auto before=*g.battle;g.act("attack",{6,5});auto events=g.takeEvents();events[1].state->id=999;BattleMotion motion;motion.start(before,events,[](int,int){return 0;},0);CHECK(motion.active()&&motion.view()->units[1].hp==before.units[1].hp);motion.tick(100000);CHECK(!motion.active());CHECK(motion.takeEvents().size()==events.size());
});
run("pausing and replaying combat frames never changes committed game results",[]{
 auto g=meleeGame();auto before=*g.battle;g.act("attack",{6,5});auto saved=g.toJson();auto events=g.takeEvents();BattleMotion motion;motion.start(before,events,actionFrames,0);motion.tick(499);motion.tick(10000,false);CHECK(motion.view()->units[1].hp==before.units[1].hp&&g.toJson()==saved);motion.tick(10001);CHECK(motion.view()->units[1].hp==events[1].state->hp);for(int t=10001;t<=20000;t+=37){motion.tick(t);motion.takeEvents();CHECK(g.toJson()==saved);}CHECK(!motion.active());
});
run("only animated cues bypass the provisional audio sequence delays",[]{
 auto g=meleeGame();auto before=*g.battle;g.act("attack",{6,5});auto events=g.takeEvents();CHECK(!events[0].scheduled);BattleMotion motion;motion.start(before,events,actionFrames,0);CHECK(motion.takeEvents().front().scheduled);motion.tick(500);CHECK(motion.takeEvents().front().scheduled);motion.tick(100000);motion.takeEvents();std::array spell{GameEvent{GameEventType::SpellCast,-1,MagicArrow},events[1]};motion.start(before,spell,actionFrames,100000);auto cues=motion.takeEvents();CHECK(motion.active()&&cues.size()==2&&cues[0].scheduled&&cues[1].scheduled);motion.tick(200000);CHECK(!motion.active());motion.start(before,spell,[](int,int){return 0;},200000);cues=motion.takeEvents();CHECK(!motion.active()&&cues.size()==2&&!cues[0].scheduled&&!cues[1].scheduled);
});
run("HD artwork preserves uniform color and source pixels at both supported scales",[]{
 Rgba color{21,81,144,127};std::vector<Rgba> source(6,color),before=source;for(int factor:{2,4}){auto output=scaleArtwork(source,3,2,factor);CHECK(output.size()==size_t(6*factor*factor)&&std::all_of(output.begin(),output.end(),[&](auto p){return p==color;}));}CHECK(source==before);
});
run("HD corner reconstruction smooths diagonal steps without inventing opaque colors",[]{
 Rgba dark{20,10,5,255},light{220,180,110,255};std::vector<Rgba> source{dark,light,light,light,dark,dark,light,dark,dark};auto output=scaleArtwork(source,3,3,2);CHECK(output[2*6+2]==light&&output[2*6+3]==dark);for(auto pixel:output)CHECK(pixel==dark||pixel==light);
});
run("HD transparent borders inherit adjacent edge color without adding coverage",[]{
 Rgba white{240,225,190,255};std::vector<Rgba> source(9);source[4]=white;auto output=scaleArtwork(source,3,3,4);bool extended=false;for(auto pixel:output){CHECK(pixel.a==0||pixel.a==255);if(!pixel.a&&pixel.r){CHECK(pixel.r==white.r&&pixel.g==white.g&&pixel.b==white.b);extended=true;}}CHECK(extended&&source[0]==Rgba{});
});
run("HD fully transparent images discard hidden palette colors",[]{
 std::vector<Rgba> source{{255,0,255,0},{0,255,255,0}};auto output=scaleArtwork(source,2,1);CHECK(std::all_of(output.begin(),output.end(),[](auto p){return p==Rgba{};}));
});
run("HD scaling handles one pixel and narrow crops without out of bounds reads",[]{
 Rgba a{10,20,30,255},b{30,20,10,255};CHECK(scaleArtwork(std::array{a},1,1).size()==16);for(auto dims:{Point{1,3},Point{3,1}}){auto output=scaleArtwork(std::array{a,b,a},dims.x,dims.y);CHECK(output.size()==48);for(auto p:output)CHECK(p==a||p==b);}
});
run("HD scaling rejects mismatched invalid or excessive dimensions before allocation",[]{
 for(auto spec:{std::array<int,3>{0,1,4},{2,2,4},{1,1,3},{1000000000,1000000000,4}}){bool rejected=false;try{scaleArtwork(std::array{Rgba{1,2,3,255}},spec[0],spec[1],spec[2]);}catch(const std::invalid_argument&){rejected=true;}CHECK(rejected);}
});
run("artwork seams select the matching column without changing either input",[]{
 std::vector<Rgba> a(20,Rgba{20,30,40,255}),b(20,Rgba{200,100,80,255});for(int y=0;y<5;y++)b[y*4+2]=a[y*4+2];auto before=b;auto seam=minimumArtworkSeam(a,b,4,5);CHECK(seam==std::vector<int>(5,2)&&b==before);
});
run("artwork seams follow a connected low contrast path around mismatches",[]{
 std::vector<Rgba> a(25,Rgba{0,0,0,255}),b(25,Rgba{255,255,255,255});std::vector<int> path{1,2,3,2,1};for(int y=0;y<5;y++)b[y*5+path[y]]=a[y*5+path[y]];CHECK(minimumArtworkSeam(a,b,5,5)==path);
});
run("artwork seams handle uniform single row and single column overlaps",[]{
 std::vector<Rgba> a(6,Rgba{20,30,40,255});CHECK(minimumArtworkSeam(a,a,3,2)==std::vector<int>({0,0}));CHECK(minimumArtworkSeam(a,a,6,1)==std::vector<int>({0}));CHECK(minimumArtworkSeam(a,a,1,6)==std::vector<int>(6,0));
});
run("artwork seams reject mismatched and excessive dimensions before allocation",[]{
 std::array a{Rgba{1,2,3,255}};for(auto dims:{Point{0,1},Point{-1,1},Point{2,1},Point{1000000000,1000000000}}){bool rejected=false;try{minimumArtworkSeam(a,a,dims.x,dims.y);}catch(const std::invalid_argument&){rejected=true;}CHECK(rejected);}bool rejected=false;try{minimumArtworkSeam(a,std::span<const Rgba>{},1,1);}catch(const std::invalid_argument&){rejected=true;}CHECK(rejected);
});
run("low alpha sprite colors inherit nearby stone without changing coverage or inputs",[]{
 std::vector<Rgba> source(7,Rgba{255,0,0,1});source[3]={80,90,100,253};auto before=source;auto output=extendArtworkEdgeColors(source,7,1);CHECK(source==before);for(int i=0;i<7;i++){CHECK(output[i].r==80&&output[i].g==90&&output[i].b==100&&output[i].a==source[i].a);}CHECK(extendArtworkEdgeColors(output,7,1)==output);
});
run("edge color repair preserves covered red gold and every original alpha value",[]{
 std::array source{Rgba{255,0,0,255},Rgba{255,220,40,253},Rgba{20,40,80,17},Rgba{255,255,0,16}};auto output=extendArtworkEdgeColors(source,4,1);for(int i=0;i<3;i++)CHECK(output[i]==source[i]);CHECK(output[3]==(Rgba{20,40,80,16}));
});
run("edge color repair has a bounded radius and cannot color isolated empty space",[]{
 std::vector<Rgba> source(13,Rgba{255,0,0,0});source[0]={90,80,70,255};auto output=extendArtworkEdgeColors(source,13,1);CHECK(output[4]==(Rgba{90,80,70,0})&&output[5]==source[5]&&output[12]==source[12]);std::array empty{Rgba{255,0,0,0},Rgba{0,255,0,1}};CHECK(extendArtworkEdgeColors(empty,1,2)==std::vector<Rgba>(empty.begin(),empty.end()));
});
run("edge color repair reaches diagonal corners and handles single pixel images",[]{
 std::vector<Rgba> source(25,Rgba{255,0,0,1});source[0]={90,80,70,255};auto output=extendArtworkEdgeColors(source,5,5);CHECK(output.back()==(Rgba{90,80,70,1}));std::array one{Rgba{21,31,41,255}};CHECK(extendArtworkEdgeColors(one,1,1).front()==one.front());
});
run("edge color repair rejects malformed dimensions before allocating",[]{
 std::array source{Rgba{1,2,3,255}};for(auto dims:{Point{0,1},Point{-1,1},Point{2,1},Point{1000000000,1000000000}}){bool rejected=false;try{extendArtworkEdgeColors(source,dims.x,dims.y);}catch(const std::invalid_argument&){rejected=true;}CHECK(rejected);}
});
run("all town fortification layers hide every upgraded ancestor",[]{
 for(const auto&town:TownDefs){auto layer=[&](int id)->const TownLayer&{auto it=std::find_if(town.layers.begin(),town.layers.end(),[&](const auto&l){return l.needs==std::vector<int>{id};});CHECK(it!=town.layers.end());return *it;};auto contains=[](const auto&l,int id){return std::find(l.excludes.begin(),l.excludes.end(),id)!=l.excludes.end();};CHECK(contains(layer(Fort),Citadel)&&contains(layer(Fort),Castle));CHECK(contains(layer(Citadel),Castle)&&!contains(layer(Citadel),Fort));CHECK(!contains(layer(Castle),Fort)&&!contains(layer(Castle),Citadel));}
});
run("all town civic layers replace lower halls without hiding unrelated buildings",[]{
 constexpr int chain[]={VillageHall,TownHall,CityHall,Capitol};for(const auto&town:TownDefs)for(int stage=0;stage<4;stage++){auto it=std::find_if(town.layers.begin(),town.layers.end(),[&](const auto&l){return l.needs==std::vector<int>{chain[stage]};});CHECK(it!=town.layers.end());for(int higher=stage+1;higher<4;higher++)CHECK(std::find(it->excludes.begin(),it->excludes.end(),chain[higher])!=it->excludes.end());for(int unrelated:{Fort,Tavern,Marketplace})CHECK(std::find(it->excludes.begin(),it->excludes.end(),unrelated)==it->excludes.end());}
});
run("available guild upgrades replace each lower visual layer across all factions",[]{
 constexpr int chain[]={MageGuild,MageGuild2,MageGuild3,MageGuild4,MageGuild5};for(const auto&town:TownDefs)for(int stage=0;stage<town.maxMageGuild;stage++){auto it=std::find_if(town.layers.begin(),town.layers.end(),[&](const auto&l){return l.needs==std::vector<int>{chain[stage]};});CHECK(it!=town.layers.end());for(int higher=stage+1;higher<town.maxMageGuild;higher++)CHECK(std::find(it->excludes.begin(),it->excludes.end(),chain[higher])!=it->excludes.end());CHECK(std::find(it->excludes.begin(),it->excludes.end(),Fort)==it->excludes.end());}
});
run("all dwelling upgrade exclusions remain specific to their own dwelling",[]{
 for(const auto&town:TownDefs)for(int tier=0;tier<7;tier++){auto it=std::find_if(town.layers.begin(),town.layers.end(),[&](const auto&l){return l.needs==std::vector<int>{Guardhouse+tier};});CHECK(it!=town.layers.end());CHECK(std::find(it->excludes.begin(),it->excludes.end(),UpGuardhouse+tier)!=it->excludes.end());for(int other=0;other<7;other++)if(other!=tier)CHECK(std::find(it->excludes.begin(),it->excludes.end(),UpGuardhouse+other)==it->excludes.end());}
});
#include "morale_tests.inc"
#include "battle_effect_tests.inc"
run("audio one-shots finish exactly without repeating",[]{
 AudioMixer mixer;mixer.volumes(9,9);auto clip=std::make_shared<AudioBuffer>();clip->samples={.1f,.2f};mixer.play(clip);std::array<float,6> output;mixer.render(output);CHECK(output[0]==.1f&&output[1]==.2f&&output[2]==0&&output[5]==0&&mixer.activeVoices()==0);
});
run("music crossfade uses both advancing tracks",[]{
 AudioMixer mixer;mixer.volumes(9,9);auto a=std::make_shared<AudioBuffer>(),b=std::make_shared<AudioBuffer>();a->samples={1,1};b->samples={-1,-1};mixer.music(a,true,0);mixer.music(b,true,4);std::array<float,10> output;mixer.render(output);CHECK(output[0]==1&&output[2]==.5f&&output[4]==0&&output[6]==-.5f&&output[8]==-1);
});
run("reselecting music retains its playback position",[]{
 AudioMixer mixer;auto clip=std::make_shared<AudioBuffer>();clip->samples={.1f,.2f,.3f,.4f,.5f,.6f};mixer.music(clip,true,0);std::array<float,4> output;mixer.render(output);CHECK(mixer.musicCursor()==2);mixer.music(clip,true,10);CHECK(mixer.musicCursor()==2);
});
run("delayed sound cues begin on their scheduled frame",[]{
 AudioMixer mixer;mixer.volumes(9,9);auto clip=std::make_shared<AudioBuffer>();clip->samples={.25f,.5f};mixer.play(clip,AudioBus::Effects,false,1,0,2);std::array<float,8> output;mixer.render(output);CHECK(output[0]==0&&output[2]==0&&output[4]==.25f&&output[5]==.5f&&output[6]==0&&mixer.activeVoices()==0);
});
run("ambient updates preserve cursors and apply stereo pan",[]{
 AudioMixer mixer;mixer.volumes(9,9);auto clip=std::make_shared<AudioBuffer>();clip->samples={.1f,.2f,.3f,.4f};mixer.play(clip,AudioBus::Ambient,true,1,0,0,17);std::array<float,2> output;mixer.render(output);CHECK(mixer.update(17,AudioBus::Ambient,.5f,-1));mixer.render(output);CHECK(output[0]==.15f&&output[1]==0);mixer.stop(17,AudioBus::Ambient);CHECK(!mixer.update(17,AudioBus::Ambient,1,0)&&mixer.activeVoices()==0);
});
run("bounded audio voices retain separate effects and ambient capacity",[]{
 AudioMixer mixer;mixer.volumes(9,9);auto clip=std::make_shared<AudioBuffer>();clip->samples={.01f,.01f};for(int i=0;i<30;i++)mixer.play(clip,AudioBus::Effects,true);for(int i=0;i<12;i++)mixer.play(clip,AudioBus::Ambient,true);CHECK(mixer.activeVoices()==32);std::array<float,2> output;mixer.render(output);CHECK(std::abs(output[0]-.32f)<.0001f);mixer.clear();CHECK(mixer.activeVoices()==0);
});
run("sound tags replace matching voices without affecting other buses",[]{
 AudioMixer mixer;auto clip=std::make_shared<AudioBuffer>();clip->samples={.1f,.1f};mixer.play(clip,AudioBus::Effects,true,1,0,0,7);mixer.play(clip,AudioBus::Effects,true,1,0,0,7);mixer.play(clip,AudioBus::Ambient,true,1,0,0,7);CHECK(mixer.activeVoices()==2);mixer.stop(7,AudioBus::Effects);CHECK(mixer.activeVoices()==1);
});
run("muting advances audio without leaking samples",[]{
 AudioMixer mixer;auto clip=std::make_shared<AudioBuffer>();clip->samples={.1f,.1f,.2f,.2f};mixer.music(clip,true,0);mixer.volumes(9,9,true);std::array<float,2> output;mixer.render(output);CHECK(output[0]==0&&output[1]==0&&mixer.musicCursor()==1);mixer.volumes(9,9);mixer.render(output);CHECK(output[0]==.2f&&output[1]==.2f);
});
run("invalid audio buffers and nonfinite parameters cannot corrupt output",[]{
 AudioMixer mixer;auto clip=std::make_shared<AudioBuffer>();CHECK(!mixer.play(clip));clip->samples={1};CHECK(!mixer.play(clip));clip->samples={1,1};CHECK(!mixer.play(clip,AudioBus::Effects,false,std::numeric_limits<float>::infinity()));CHECK(!mixer.play(clip,AudioBus::Music));clip->samples={std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()};mixer.play(clip);std::array<float,2> output;mixer.render(output);CHECK(output[0]==0&&output[1]==0);
});
run("accepted town actions emit cues while rejected actions stay silent",[]{
 auto g=Game::create();CHECK(g.takeEvents().empty());g.build(Portal);CHECK(g.takeEvents().empty());g.build(Marketplace);auto events=g.takeEvents();CHECK(events.size()==1&&events[0].type==GameEventType::Build&&events[0].detail==Marketplace);g.build(Blacksmith);CHECK(g.takeEvents().empty());g.recruit(Pikeman,2);events=g.takeEvents();CHECK(events.size()==1&&events[0].type==GameEventType::Recruit&&events[0].unit==Pikeman);g.endDay();events=g.takeEvents();CHECK(events.size()==1&&events[0].type==GameEventType::NewDay&&events[0].detail==2);
});
run("presentation events do not alter game saves or random state",[]{
 auto g=Game::create();g.move({4,21});auto before=g.toJson();auto events=g.takeEvents();CHECK(!events.empty()&&events[0].type==GameEventType::HeroMove);CHECK(g.toJson()==before&&g.takeEvents().empty());for(int i=0;i<300;i++)g.emit({GameEventType::NewDay,-1,i});events=g.takeEvents();CHECK(events.size()==256&&events.front().detail==44&&events.back().detail==299&&g.toJson()==before);
});
run("combat attack hit and result cues preserve accepted event order",[]{
 auto g=Game::create();g.hero.army={Stack{Archer,50}};g.objectById(15)->army={Stack{Pikeman,1}};g.startBattle(15);CHECK(g.act("attack",{g.battle->units.back().x,g.battle->units.back().y}).empty());auto events=g.takeEvents();CHECK(events.size()==4);CHECK(events[0].type==GameEventType::CombatStart);CHECK(events[1].type==GameEventType::CombatAttack&&events[1].unit==Archer&&events[1].detail==1);CHECK(events[2].type==GameEventType::CombatDeath&&events[2].unit==Pikeman);CHECK(events[3].type==GameEventType::CombatEnd&&events[3].detail==1);
});
run("spell cues occur only after valid targets and payment",[]{
 auto g=spellGame();g.hero.army={Stack{Archangel,5}};g.objectById(15)->army={Stack{Pikeman,1000}};g.startBattle(15);g.takeEvents();CHECK(!g.cast(MagicArrow,{-1,-1}).empty()&&g.takeEvents().empty());CHECK(g.cast(MagicArrow,{g.battle->units.back().x,g.battle->units.back().y}).empty());auto events=g.takeEvents();CHECK(events.size()==3&&events[0].type==GameEventType::SpellCast&&events[0].detail==MagicArrow&&events[1].type==GameEventType::CombatHit&&events[2].type==GameEventType::CombatSpellEffect&&events[2].detail==MagicArrow);CHECK(!g.cast(MagicArrow,{g.battle->units.back().x,g.battle->units.back().y}).empty()&&g.takeEvents().empty());
});
run("loading a game discards transient presentation cues",[]{
 auto g=Game::create();g.build(Marketplace);auto path=testDirectory()/"presentation-save.json";std::string error;CHECK(g.save(path,error));auto loaded=Game::create();loaded.emit({GameEventType::CombatStart});CHECK(loaded.load(path,error));CHECK(loaded.takeEvents().empty()&&loaded.toJson()==g.toJson());
});

run("windmill gives a non-gold non-wood resource only once per week",[]{
 auto g=Game::create();auto at=std::find_if(g.objects.begin(),g.objects.end(),[](auto&o){return o.type==ObjectType::Windmill;});CHECK(at!=g.objects.end());int id=at->id;g.hero.pos=at->pos;auto before=g.resources;g.visit(id);int gain=0;for(int r=0;r<ResourceCount;++r){int delta=g.resources[r]-before[r];if(r==Gold||r==Wood)CHECK(delta==0);else gain+=delta;}CHECK(gain>=3&&gain<=6);auto state=g.toJson();g.visit(id);CHECK(g.randomState==state["randomState"].get<uint32_t>());CHECK(g.resources==state["resources"].get<Resources>());
});
run("water wheel changes from first-week to later-week reward",[]{
 auto g=Game::create();auto at=std::find_if(g.objects.begin(),g.objects.end(),[](auto&o){return o.type==ObjectType::WaterWheel;});CHECK(at!=g.objects.end());int id=at->id;g.hero.pos=at->pos;int gold=g.resources[Gold];g.visit(id);CHECK(g.resources[Gold]==gold+500);g.visit(id);CHECK(g.resources[Gold]==gold+500);for(int i=0;i<7;++i)g.endDay();CHECK(!g.objectById(id)->visited);gold=g.resources[Gold];g.visit(id);CHECK(g.resources[Gold]==gold+1000);
});
run("river and weekly object state survive saving",[]{
 auto g=Game::create();auto at=std::find_if(g.objects.begin(),g.objects.end(),[](auto&o){return o.type==ObjectType::Windmill;});g.visit(at->id);g.hero.pos=at->pos;auto path=testDirectory()/"map-objects-save.json";std::string error;CHECK(g.save(path,error));Game loaded;CHECK(loaded.load(path,error));CHECK(g.toJson()==loaded.toJson());CHECK(loaded.tile({22,23})->river);
});
run("legacy tile records without a river field still load",[]{
 auto g=Game::create();for(auto&t:g.tiles)t.river=false;auto state=g.toJson();for(auto&t:state["tiles"])t.erase("river");auto path=testDirectory()/"legacy-map-save.json";{std::ofstream f(path);f<<state;}Game loaded;std::string error;CHECK(loaded.load(path,error));CHECK(loaded.toJson()==g.toJson());
});
run("attack preview and executed approach share the same reachable hex",[]{
 auto g=Game::create();g.hero.army={Stack{Pikeman,20}};g.objectById(15)->army={Stack{Skeleton,100}};g.startBattle(15);
 auto&a=g.battle->units[0];auto&t=g.battle->units[1];a.x=5;a.y=5;t.x=8;t.y=5;g.battle->queue={0,100};auto state=g.toJson();
 auto plan=g.attackPlan(a,t);CHECK(plan&&!plan->ranged&&plan->traveled>0&&plan->traveled<=g.speed(a));CHECK(g.toJson()==state);
 Point from=plan->from;int hp=t.hp;CHECK(g.act("attack",{t.x,t.y}).empty());CHECK(a.x==from.x&&a.y==from.y&&t.hp<hp);
});
run("attack preview rejects distant melee and invalid stack identities",[]{
 auto g=Game::create();g.hero.army={Stack{Pikeman,20}};g.objectById(15)->army={Stack{Skeleton,100}};g.startBattle(15);
 auto&a=g.battle->units[0];auto&t=g.battle->units[1];a.x=0;a.y=0;t.x=14;t.y=10;
 CHECK(!g.attackPlan(a,t));CHECK(!g.attackPlan(a,a));t.hp=0;CHECK(!g.attackPlan(a,t));
});
run("ranged preview switches to melee when an enemy blocks shooting",[]{
 auto g=Game::create();g.hero.army={Stack{Archer,20}};g.objectById(15)->army={Stack{Skeleton,100}};g.startBattle(15);
 auto&a=g.battle->units[0];auto&t=g.battle->units[1];a.x=5;a.y=5;t.x=9;t.y=5;auto plan=g.attackPlan(a,t);CHECK(plan&&plan->ranged&&plan->traveled==0);
 t.x=6;plan=g.attackPlan(a,t);CHECK(plan&&!plan->ranged&&plan->traveled==0);t.x=14;a.shots=0;CHECK(!g.attackPlan(a,t));
});
run("two hex attack preview never lands on occupied or out of board cells",[]{
 auto g=Game::create();g.hero.army={Stack{Cavalier,20},Stack{Pikeman,10}};g.objectById(15)->army={Stack{Cavalier,100}};g.startBattle(15);
 auto&a=g.battle->units[0];auto&t=g.battle->units[2];a.x=4;a.y=5;t.x=8;t.y=5;auto plan=g.attackPlan(a,t);CHECK(plan);
 auto route=g.battleRoute(a,plan->from);CHECK(!route.empty());for(auto p:g.footprint(a,plan->from)){CHECK(p.x>=0&&p.x<BattleW&&p.y>=0&&p.y<BattleH);auto*u=g.combatAt(p);CHECK(!u||u->id==a.id);}
 auto next=a;next.x=plan->from.x;next.y=plan->from.y;CHECK(g.distance(next,t)==1);
});
std::cout<<passed<<" passed, "<<failed<<" failed\n";return failed?1:0;}
