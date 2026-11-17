#pragma once

Game openAdventure(){
 auto g=Game::create();g.objects.clear();
 for(auto&t:g.tiles){t.seen=true;t.decor=Decor::None;t.road=false;t.terrain=Terrain::Grass;}
 g.hero.pos={10,10};return g;
}
Object guardObject(int id,Point p){Object o;o.id=id;o.pos=p;o.type=ObjectType::Monster;o.owner=-1;o.army={Stack{Skeleton,1}};o.name="测试守军";return o;}
template<class Run> void adventureCases(Run run){
 run("rendered mountain blocks all four original footprint cells",[]{
  auto g=openAdventure();g.tile({12,12})->decor=Decor::Mountain;
  for(Point p:{Point{12,12},Point{11,12},Point{11,11},Point{10,11}})CHECK(!g.passable(p));
  CHECK(g.passable({12,11})&&g.passable({10,12}));auto grid=g.movementMask();
  for(int y=0;y<MapH;++y)for(int x=0;x<MapW;++x)CHECK(grid[y*MapW+x]==g.passable({x,y}));
  g.hero.pos={9,12};auto route=g.path({14,12});CHECK(!route.empty());Point from=g.hero.pos;
  for(auto step:route){Point p{step.x,step.y};CHECK(g.passable(p)&&g.canStep(from,p));CHECK(g.move(p).empty());from=p;}CHECK((g.hero.pos==Point{14,12}));
 });
 run("compiled collision masks match all verified original object templates",[]{
  std::ifstream file(std::filesystem::path(H3_TEST_SOURCE_ROOT)/"assets/demo/adventure.json");auto data=nlohmann::json::parse(file);
  for(const auto&[name,item]:data["templates"].items()){
   const auto&t=adventureTemplate(name);uint64_t blocked=0,visits=0;auto b=item["blockedMask"].get<std::string>(),v=item["visitMask"].get<std::string>();
   for(int i=0;i<48;++i){if(b[i]=='0')blocked|=uint64_t(1)<<i;if(v[i]=='1')visits|=uint64_t(1)<<i;}
   CHECK(t.blocked==blocked&&t.visitable==visits&&t.visitOffset.x==item["visitOffset"][0]&&t.visitOffset.y==item["visitOffset"][1]);
  }
 });
 run("mine and water wheel body tiles block movement but their entrance remains open",[]{
  auto g=openAdventure();Object o;o.id=10;o.type=ObjectType::Mine;o.resource=Gold;o.pos={12,12};g.objects.push_back(o);
  CHECK(g.passable(o.pos)&&!g.passable({11,12})&&!g.passable({13,12}));g.objects[0].type=ObjectType::WaterWheel;
  CHECK(g.passable(o.pos)&&!g.passable({11,12})&&!g.passable({10,11})&&!g.passable({12,11}));
 });
 run("new demo map keeps every visit tile and the hero outside obstacle footprints",[]{
  for(int faction=0;faction<FactionCount;++faction){auto g=Game::create(1999,faction);CHECK(g.passable(g.hero.pos)&&g.passable(g.grailPos));for(const auto&o:g.objects)CHECK(g.passable(o.pos));}
 });
 run("remote visits cannot grant resources, spells or mine ownership",[]{
  auto g=openAdventure();Object o;o.id=10;o.pos={15,15};o.type=ObjectType::Windmill;g.objects.push_back(o);auto state=g.toJson();CHECK(!g.visit(10).empty()&&g.toJson()==state&&!g.takeAdventureVisit());
 });
 run("magic wells share a single daily use per hero and full mana does not spend it",[]{
  auto g=openAdventure();Object o;o.id=10;o.type=ObjectType::Well;o.pos=g.hero.pos;g.objects.push_back(o);g.visit(10);CHECK(g.hero.wellDay==0);
  g.hero.mana=2;g.visit(10);CHECK(g.hero.mana==g.maxMana()&&g.hero.wellDay==g.day);g.hero.mana=1;g.visit(10);CHECK(g.hero.mana==1);
  g.objects[0].id=11;g.visit(11);CHECK(g.hero.mana==1);g.endDay();g.visit(11);CHECK(g.hero.mana==g.maxMana()&&g.hero.wellDay==g.day);
 });
 run("first level shrine teaches a spell only with a book and grants no experience",[]{
  auto g=openAdventure();Object o;o.id=10;o.type=ObjectType::Shrine;o.pos=g.hero.pos;g.objects.push_back(o);int xp=g.hero.xp;g.visit(10);CHECK(!g.hero.spells[MagicArrow]&&!g.objects[0].visited&&g.hero.xp==xp);
  g.hero.spellbook=true;g.visit(10);CHECK(g.hero.spells[MagicArrow]&&g.objects[0].visited&&g.hero.xp==xp);auto state=g.toJson();g.visit(10);CHECK(g.hero.spells[MagicArrow]&&g.hero.xp==xp&&g.randomState==state["randomState"]);
 });
 run("obelisk records puzzle progress once without opening the adventure fog",[]{
  auto g=openAdventure();Object o;o.id=10;o.type=ObjectType::Obelisk;o.pos=g.hero.pos;g.objects.push_back(o);g.tile({29,7})->seen=false;
  auto before=g.tiles;g.visit(10);CHECK(g.obelisksVisited()==1&&g.obelisksTotal()==1&&!g.tile({29,7})->seen);g.visit(10);CHECK(g.obelisksVisited()==1);
  for(int i=0;i<MapW*MapH;++i)CHECK(g.tiles[i].seen==before[i].seen);auto notice=g.takeAdventureVisit();CHECK(notice&&notice->type==ObjectType::Obelisk&&!g.takeAdventureVisit());
 });
 run("windmills pay separately once per week and retain animation identity after collection",[]{
  auto g=openAdventure();for(int id:{10,11}){Object o;o.id=id;o.type=ObjectType::Windmill;o.pos={id,10};g.objects.push_back(o);}auto before=g.resources;
  for(int id:{10,11}){g.hero.pos=g.objectById(id)->pos;g.visit(id);auto notice=g.takeAdventureVisit();CHECK(notice&&notice->reward[Gold]==0&&notice->reward[Wood]==0);int gain=0;for(int n:notice->reward)gain+=n;CHECK(gain>=3&&gain<=6);CHECK(g.mapObjectAsset(*g.objectById(id))=="avmwndd0.def");}
  auto reward=g.resources;g.visit(11);CHECK(g.resources==reward);for(int i=0;i<7;++i)g.endDay();CHECK(!g.objects[0].visited&&!g.objects[1].visited);g.visit(11);CHECK(g.resources!=reward&&g.resources[Gold]>=before[Gold]);
 });
 run("digging consumes the full day and can recover the grail only once",[]{
  auto g=openAdventure();g.grailPos={11,10};g.hero.movement--;auto before=g.toJson();CHECK(!g.dig().empty()&&g.toJson()==before);g.endDay();g.dig();CHECK(!g.grailFound&&g.hero.movement==0);
  g.hero.pos=g.grailPos;g.endDay();g.dig();CHECK(g.grailFound&&g.hero.movement==0&&g.hero.artifacts==std::vector<std::string>{"圣杯"});g.endDay();g.dig();CHECK(g.hero.artifacts.size()==1);
 });
 run("daily well use and puzzle progress roundtrip with safe legacy defaults",[]{
  auto g=Game::create();g.hero.wellDay=1;g.objectById(21)->visited=true;auto path=testDirectory()/"visits-v11.json";std::string error;CHECK(g.save(path,error));Game loaded;CHECK(loaded.load(path,error)&&loaded.toJson()==g.toJson());
  auto old=g.toJson();old["version"]=10;old["hero"].erase("wellDay");old.erase("grailPos");old.erase("grailFound");std::ofstream(path)<<old;CHECK(loaded.load(path,error)&&loaded.hero.wellDay==0&&!loaded.grailFound);
 });
 run("dirt roads require both endpoints and use original diagonal costs",[]{
  auto g=openAdventure();CHECK(g.moveCost({10,10},{11,10})==100);CHECK(g.moveCost({10,10},{11,11})==141);
  g.tile({11,10})->road=true;CHECK(g.moveCost({10,10},{11,10})==100);g.tile({10,10})->road=true;CHECK(g.moveCost({10,10},{11,10})==75);
  CHECK(g.moveCost({11,10},{12,10})==100);g.tile({11,11})->road=true;CHECK(g.moveCost({10,10},{11,11})==106);
  g.tile({12,10})->terrain=Terrain::Sand;g.tile({12,11})->terrain=Terrain::Sand;CHECK(g.moveCost({11,10},{12,10})==150&&g.moveCost({11,10},{12,11})==212);
 });
 run("shortest route uses cheaper roads without changing gameplay RNG",[]{
  auto g=openAdventure();g.hero.pos={10,10};for(int x=10;x<=16;++x)g.tile({x,9})->road=true;
  auto before=g.toJson();auto route=g.path({16,10});int cost=0;bool road=false;for(const auto&s:route){cost+=s.cost;road|=s.y==9;}
  CHECK(road&&cost==582&&g.toJson()==before);for(const auto&s:route)g.move({s.x,s.y});CHECK((g.hero.pos==Point{16,10})&&g.hero.movement==1560-cost);
 });
 run("all eight adjacent guard squares intercept a moving hero",[]{
  for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){if(!dx&&!dy)continue;auto g=openAdventure();Point p{12+dx,12+dy};g.objects.push_back(guardObject(100,{12,12}));g.hero.pos={p.x-1,p.y};g.move(p);CHECK(g.battle&&g.battle->targetId==100&&g.hero.pos==p&&g.battle->deferredVisitId==0&&g.validate());}
 });
 run("guard does not intercept from two tiles away or across a shoreline",[]{
  auto g=openAdventure();g.objects.push_back(guardObject(100,{12,12}));CHECK(!g.guardingMonster({10,12}));g.tile({12,12})->terrain=Terrain::Water;CHECK(!g.guardingMonster({11,12}));g.move({11,10});CHECK(!g.battle);
 });
 run("overlapping guards choose columns in fixed order and direct target wins",[]{
  auto g=openAdventure();g.objects={guardObject(102,{11,11}),guardObject(101,{9,11}),guardObject(100,{10,9})};
  CHECK(g.guardingMonster({10,10})->id==101);CHECK(g.guardingMonster({11,11})->id==102);std::reverse(g.objects.begin(),g.objects.end());CHECK(g.guardingMonster({10,10})->id==101);
  g.move({11,11});CHECK(g.battle&&g.battle->targetId==102&&(g.hero.pos==Point{10,10}));
 });
 run("route detours visible guard zones and cannot pass a guarded choke",[]{
  auto g=openAdventure();g.hero.pos={8,10};g.objects.push_back(guardObject(100,{12,10}));auto rng=g.randomState;auto route=g.path({16,10});CHECK(!route.empty());for(auto s:route)CHECK(!g.guardingMonster({s.x,s.y},true));CHECK(g.randomState==rng);
  for(auto&t:g.tiles)t.decor=Decor::Tree;for(int x=8;x<=16;++x)g.tile({x,10})->decor=Decor::None;
  CHECK(g.path({16,10}).empty());auto attack=g.path({12,10});CHECK(!attack.empty()&&attack.back().x==11&&attack.back().y==10);
  for(auto s:attack)g.move({s.x,s.y});CHECK(g.battle&&g.battle->targetId==100&&(g.hero.pos==Point{11,10}));
 });
 run("fog does not leak invisible guards into route previews but movement reveals and encounters them",[]{
  auto g=openAdventure();g.hero.pos={8,10};g.objects.push_back(guardObject(100,{11,11}));g.tile({11,11})->seen=false;
  CHECK(!g.guardingMonster({10,10},true)&&g.guardingMonster({10,10}));auto route=g.path({10,10});CHECK(!route.empty());
  for(auto s:route)g.move({s.x,s.y});CHECK(g.battle&&g.tile({11,11})->seen);
 });
 run("adjacent direct attack pays movement but never occupies the monster tile",[]{
  auto g=openAdventure();g.objects.push_back(guardObject(100,{11,10}));auto route=g.path({11,10});CHECK(route.size()==1&&route[0].cost==100);
  int movement=g.hero.movement;g.move({11,10});CHECK(g.battle&&(g.hero.pos==Point{10,10})&&g.hero.movement==movement-100&&g.validate());auto before=g.toJson();g.move({9,10});g.visit(100);CHECK(g.toJson()==before);
 });
 run("guarded treasure is granted once after victory and only after its guard dies",[]{
  auto g=openAdventure();g.objects.push_back(guardObject(100,{12,11}));Object o;o.id=101;o.pos={11,10};o.type=ObjectType::Treasure;o.resource=Gold;o.amount=750;g.objects.push_back(o);
  g.hero.army={Stack{Archer,200}};auto gold=g.resources[Gold];g.move(o.pos);CHECK(g.battle&&g.battle->deferredVisitId==101&&g.resources[Gold]==gold&&g.objectById(101));
  auto enemy=g.battle->units.back();CHECK(g.act("attack",{enemy.x,enemy.y}).empty());CHECK(!g.battle&&g.report&&g.report->won&&!g.objectById(100)&&!g.objectById(101)&&g.resources[Gold]==gold+750&&g.validate());
  auto before=g.toJson();g.visit(101);CHECK(g.toJson()==before);
 });
 run("current-tile visit cannot bypass a guard and defeat grants no deferred reward",[]{
  auto g=openAdventure();g.objects.push_back(guardObject(100,{11,10}));Object o;o.id=101;o.pos=g.hero.pos;o.type=ObjectType::Artifact;g.objects.push_back(o);g.hero.army={Stack{Pikeman,1}};g.objects[0].army={Stack{Archer,200}};auto before=g.resources;
  g.visit(101);CHECK(g.battle&&g.battle->deferredVisitId==101&&g.hero.artifacts.empty());for(int i=0;i<100&&g.battle;++i)g.ai();CHECK(!g.battle&&g.hero.artifacts.empty()&&g.objectById(101)&&g.resources==before);
 });
 run("one encounter per step leaves another overlapping guard and its treasure intact",[]{
  auto g=openAdventure();g.hero.army={Stack{Archer,200}};g.objects={guardObject(100,{12,10}),guardObject(101,{12,11})};Object o;o.id=102;o.pos={11,10};o.type=ObjectType::Treasure;o.resource=Gold;o.amount=750;g.objects.push_back(o);int gold=g.resources[Gold];
  g.move(o.pos);CHECK(g.battle->targetId==100);auto enemy=g.battle->units.back();g.act("attack",{enemy.x,enemy.y});CHECK(!g.battle&&g.report&&g.objectById(101)&&g.objectById(102)&&g.resources[Gold]==gold);
 });
 run("deferred encounter survives roundtrip and legacy battles default to no visit",[]{
  auto g=openAdventure();g.objects.push_back(guardObject(100,{12,11}));Object o;o.id=101;o.pos={11,10};o.type=ObjectType::Mine;g.objects.push_back(o);g.move(o.pos);CHECK(g.validate());auto path=testDirectory()/"guarded-visit.json";std::string error;CHECK(g.save(path,error));Game loaded;CHECK(loaded.load(path,error)&&loaded.toJson()==g.toJson());
  auto state=g.toJson();state["battle"].erase("deferredVisitId");{std::ofstream f(path);f<<state;}CHECK(loaded.load(path,error)&&loaded.battle->deferredVisitId==0);
  g.battle->deferredVisitId=999;CHECK(!g.validate());
 });
}
