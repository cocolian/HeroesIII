// Controlled turns use fixed next draws, independent of map-generation RNG.
constexpr uint32_t MoraleHitSeed=634785765u,MoraleMissSeed=653637408u;
auto moraleFixture=[](int ownStacks=1){auto g=Game::create();g.hero.skills={};g.hero.army={};for(int i=0;i<ownStacks;i++)g.hero.army[i]={Pikeman,20};g.objectById(15)->army={Stack{Skeleton,1000}};g.startBattle(15);g.takeEvents();return g;};
auto hasCue=[](const auto&events,GameEventType kind,int detail){return std::any_of(events.begin(),events.end(),[&](const auto&e){return e.type==kind&&e.detail==detail;});};
run("starting army morale includes same town and Leadership with neutral luck",[]{auto g=Game::create();CHECK(g.armyMorale()==2&&g.armyLuck()==0);});
run("mage spell discount also applies to the adventure spell book",[]{auto g=Game::create();g.hero.skills={};g.hero.army={Stack{34,1},Stack{35,1}};CHECK(g.spellCost(MagicArrow)==3&&g.spellCost(SummonBoat)==6);});
run("army mixing is summed before clamping and neutral units share one town",[]{
 auto g=Game::create();g.hero.skills={};g.hero.army={Stack{0,1},Stack{14,1}};CHECK(g.armyMorale()==0);g.hero.army[2]={28,1};CHECK(g.armyMorale()==-1);g.hero.army={Stack{0,1},Stack{14,1},Stack{28,1},Stack{42,1},Stack{70,1},Stack{84,1},Stack{98,1}};g.hero.skills[Leadership]=3;CHECK(g.armyMorale()==-2);g.hero.skills={};g.hero.army={Stack{128,1},Stack{129,1}};CHECK(g.armyMorale()==1);
});
run("undead mixing penalizes affected troops and a pure undead army stays neutral",[]{auto g=Game::create();g.hero.skills={};g.hero.army={Stack{Pikeman,1},Stack{Skeleton,1}};CHECK(g.armyMorale()==-1);g.hero.army={Stack{Skeleton,1},Stack{57,1}};CHECK(g.armyMorale()==0);});
run("morale immunity differs from resurrection eligibility for gargoyles",[&]{
 auto g=moraleFixture();g.hero.skills[Leadership]=3;auto u=g.battle->units[0];u.effects[Mirth]=5;u.effectMastery[Mirth]=3;for(int id:std::array<int,5>{Skeleton,32,33,114,122}){u.unit=id;CHECK(g.morale(u)==0&&g.spellImmune(u,Mirth)&&g.spellImmune(u,Sorrow));}u.unit=30;CHECK(!Units[30].living&&!Units[30].nonLiving&&g.morale(u)==3&&!g.spellImmune(u,Mirth));u.unit=125;CHECK(g.morale(u)==3);
});
run("creature morale auras do not stack and persist after casualties",[]{
 auto g=Game::create();g.hero.skills={};g.hero.army={Stack{Angel,1},Stack{Archangel,1},Stack{Pikeman,20}};g.objectById(15)->army={Stack{68,1},Stack{69,1}};g.startBattle(15);CHECK(g.armyMorale()==1);makeCorpse(g,0);makeCorpse(g,1);makeCorpse(g,3);makeCorpse(g,4);CHECK(g.armyMorale()==1);
});
run("minotaur and halfling floors apply after hostile spells",[&]{auto g=moraleFixture();auto u=g.battle->units[0];u.effects[Sorrow]=u.effects[Misfortune]=10;u.effectMastery[Sorrow]=u.effectMastery[Misfortune]=3;u.unit=78;CHECK(Units[u.unit].moraleFloor==1&&g.morale(u)==1);u.unit=79;CHECK(g.morale(u)==1);u.unit=128;CHECK(Units[u.unit].luckFloor==1&&g.luck(u)==1);});
run("morale cap applies after timed modifiers and surviving aura totals",[]{auto g=Game::create();g.hero.skills[Leadership]=3;g.hero.army={Stack{Angel,1}};g.objectById(15)->army={Stack{Skeleton,1000}};g.startBattle(15);auto&u=g.battle->units[0];CHECK(g.armyMorale()==3);u.effects[Sorrow]=10;u.effectMastery[Sorrow]=3;CHECK(g.morale(u)==3);});
run("Devil luck suppression is nonstacking and survives its original stacks",[]{auto g=Game::create();g.hero.skills[LuckSkill]=3;g.objectById(15)->army={Stack{54,1},Stack{55,1}};g.startBattle(15);CHECK(g.armyLuck()==2);makeCorpse(g,3);makeCorpse(g,4);CHECK(g.armyLuck()==2);});
run("expert Mirth targets affected living troops including gargoyles",[]{
 auto g=spellGame();g.hero.army={Stack{Pikeman,10},Stack{30,10},Stack{Skeleton,10},Stack{32,10}};g.hero.spells[Mirth]=true;g.hero.skills[WaterMagic]=3;g.hero.power=3;g.objectById(15)->army={Stack{Skeleton,1000}};g.startBattle(15);CHECK(g.spellMass(Mirth)&&g.spellTargets(Mirth).size()==2);CHECK(g.cast(Mirth).empty());for(int i=0;i<4;i++)CHECK(g.battle->units[i].effects[Mirth]==(i<2?3:0));CHECK(g.validate());
});
run("expert Sorrow targets enemy morale and respects nonliving immunity",[]{
 auto g=spellGame();g.hero.spells[Sorrow]=true;g.hero.skills[EarthMagic]=3;g.hero.power=3;g.objectById(15)->army={Stack{Pikeman,10},Stack{Archer,10},Stack{Skeleton,10},Stack{32,10}};g.startBattle(15);CHECK(g.spellTargets(Sorrow).size()==2);CHECK(g.cast(Sorrow).empty());int affected=0;for(auto&u:g.battle->units)if(u.effects[Sorrow]){affected++;CHECK(u.team==1&&g.morale(u)==-3);}CHECK(affected==2&&g.validate());
});
run("Fortune affects nonliving armies while Misfortune cannot create classic bad luck",[]{
 auto g=spellGame();g.hero.army={Stack{Skeleton,10},Stack{32,10},Stack{128,10}};g.hero.spells[Fortune]=true;g.hero.skills[AirMagic]=3;g.objectById(15)->army={Stack{Skeleton,1000}};g.startBattle(15);CHECK(g.spellTargets(Fortune).size()==3);CHECK(g.cast(Fortune).empty());for(auto&u:g.battle->units)if(u.team==0){CHECK(g.luck(u)==2);u.effects[Fortune]=0;u.effects[Misfortune]=10;u.effectMastery[Misfortune]=3;CHECK(g.luck(u)==(u.unit==128?1:0));}
});
run("morale and luck spells cancel their opposite effects",[]{
 auto g=spellGame();g.hero.army={Stack{Pikeman,20}};g.hero.knowledge=100;g.hero.mana=1000;g.hero.spells[Mirth]=g.hero.spells[Fortune]=g.hero.spells[Sorrow]=g.hero.spells[Misfortune]=true;g.objectById(15)->army={Stack{Pikeman,1000}};g.startBattle(15);auto&a=g.battle->units[0];auto&t=g.battle->units[1];a.effects[Sorrow]=a.effects[Misfortune]=4;t.effects[Mirth]=t.effects[Fortune]=4;
 CHECK(g.cast(Mirth,{a.x,a.y}).empty()&&!a.effects[Sorrow]&&a.effects[Mirth]);g.battle->spellRound=0;CHECK(g.cast(Fortune,{a.x,a.y}).empty()&&!a.effects[Misfortune]);g.battle->spellRound=0;CHECK(g.cast(Sorrow,{t.x,t.y}).empty()&&!t.effects[Mirth]);g.battle->spellRound=0;CHECK(g.cast(Misfortune,{t.x,t.y}).empty()&&!t.effects[Fortune]);
});
run("Cure clears morale and luck debuffs while timed modifiers expire by round",[]{
 auto g=spellGame();g.hero.army={Stack{Pikeman,20}};g.objectById(15)->army={Stack{Skeleton,1000}};g.startBattle(15);auto&u=g.battle->units[0];g.hero.spells[Cure]=true;u.effects[Sorrow]=u.effects[Misfortune]=u.effects[Haste]=4;CHECK(g.cast(Cure,{u.x,u.y}).empty());CHECK(!u.effects[Sorrow]&&!u.effects[Misfortune]&&u.effects[Haste]);u.effects[Haste]=0;for(int s:{Mirth,Sorrow,Fortune,Misfortune})u.effects[s]=1;g.act("defend");g.act("defend");CHECK(g.battle->round==2);for(int s:{Mirth,Sorrow,Fortune,Misfortune})CHECK(!g.battle->units[0].effects[s]);
});
run("good morale grants exactly one immediate additional action",[&]{
 auto g=moraleFixture();auto p=*g.current();g.randomState=MoraleHitSeed;CHECK(g.act("move",{p.x+1,p.y}).empty());CHECK(g.current()->id==p.id&&g.current()->moraleUsed&&g.current()->acted&&hasCue(g.takeEvents(),GameEventType::CombatMorale,1));g.randomState=MoraleHitSeed;CHECK(g.act("move",{p.x+2,p.y}).empty());CHECK(g.current()->id!=p.id&&g.randomState==MoraleHitSeed&&!hasCue(g.takeEvents(),GameEventType::CombatMorale,1)&&g.validate());
});
run("failed morale and non-action commands do not grant an extra action",[&]{
 auto g=moraleFixture();auto p=*g.current();g.randomState=MoraleMissSeed;CHECK(g.act("move",{p.x+1,p.y}).empty());CHECK(g.current()->id!=p.id&&!hasCue(g.takeEvents(),GameEventType::CombatMorale,1));for(auto action:{"wait","defend"}){g=moraleFixture();g.randomState=MoraleHitSeed;CHECK(g.act(action).empty());CHECK(g.randomState==MoraleHitSeed&&!hasCue(g.takeEvents(),GameEventType::CombatMorale,1));}
});
run("positive morale threshold selects one two or three of twenty-four uniform buckets",[&]{
 for(int rating=1;rating<=3;rating++){auto base=moraleFixture();base.hero.skills[Leadership]=rating-1;int procs=0;for(int bucket=0;bucket<24;bucket++){auto g=base;uint32_t draw=uint32_t((uint64_t(bucket)*2+1)*2147483648ull/24);g.randomState=(draw-1013904223u)*4276115653u;auto p=*g.current();g.act("move",{p.x+1,p.y});procs+=hasCue(g.takeEvents(),GameEventType::CombatMorale,1);}CHECK(procs==rating);}
});
run("morale wait phase is FIFO ahead of ordinary waits even after a speed spell",[&]{
 auto g=moraleFixture(3);g.hero.spellbook=true;g.hero.spells[Haste]=true;g.hero.power=4;g.act("wait");for(int id:{1,2}){CHECK(g.current()->id==id);auto p=*g.current();g.randomState=MoraleHitSeed;CHECK(g.act("move",{p.x+1,p.y}).empty()&&g.current()->moraleUsed);CHECK(g.act("wait").empty());}CHECK(g.current()->team==1);g.act("defend");CHECK(g.battle->moralePhase&&!g.battle->waitPhase&&g.battle->queue==std::vector<int>({1,2}));auto&fast=g.battle->units[2];CHECK(g.cast(Haste,{fast.x,fast.y}).empty());CHECK(g.battle->queue==std::vector<int>({1,2})&&g.validate());
 auto path=testDirectory()/"morale-fifo.json";std::string error;CHECK(g.save(path,error));Game loaded;CHECK(loaded.load(path,error)&&loaded.toJson()==g.toJson());loaded.act("defend");CHECK(loaded.current()->id==2);loaded.act("defend");CHECK(loaded.battle->waitPhase&&!loaded.battle->moralePhase&&loaded.current()->id==0);loaded.act("defend");CHECK(loaded.battle->round==2&&std::none_of(loaded.battle->units.begin(),loaded.battle->units.end(),[](const auto&u){return u.moraleUsed;})&&loaded.validate());
});
run("bad morale consumes the scheduled action and records the skipped stack",[]{
 auto g=Game::create();g.hero.skills={};g.hero.army={Stack{Pikeman,10},Stack{14,10},Stack{42,10},Stack{Skeleton,10},Stack{125,1}};g.objectById(15)->army={Stack{Skeleton,1000}};g.randomState=MoraleHitSeed;g.startBattle(15);auto events=g.takeEvents();CHECK(std::any_of(events.begin(),events.end(),[](const auto&e){return e.type==GameEventType::CombatMorale&&e.detail==-1&&e.stackId==4&&e.state->acted;}));CHECK(g.battle->units[4].moraleUsed&&g.battle->units[4].acted&&std::find(g.battle->queue.begin(),g.battle->queue.end(),4)==g.battle->queue.end());CHECK(g.validate());
});
run("nonliving movement never rolls morale or consumes randomness",[]{auto g=Game::create();g.hero.skills={};g.hero.army={Stack{Skeleton,20}};g.objectById(15)->army={Stack{Skeleton,1000}};g.randomState=MoraleHitSeed;g.startBattle(15);auto p=*g.current();CHECK(g.act("move",{p.x+1,p.y}).empty());CHECK(g.randomState==MoraleHitSeed);});
run("negative morale thresholds select one two or three of twelve uniform buckets",[]{
 for(int rating=1;rating<=3;rating++){
  auto base=spellGame();base.hero.army={Stack{Pikeman,20}};if(rating>=2)base.hero.army[1]={14,20};if(rating==3)base.hero.army[2]={28,20};base.hero.skills[Leadership]=3;base.objectById(15)->army={Stack{Skeleton,1000}};base.startBattle(15);base.hero.skills[Leadership]=0;base.hero.spells[Haste]=true;base.battle->units[0].effects[Sorrow]=10;base.battle->units[0].effectMastery[Sorrow]=3;base.battle->units[0].preparedRound=0;base.battle->queue.clear();for(const auto&u:base.battle->units)base.battle->queue.push_back(u.id);CHECK(base.morale(base.battle->units[0])==-rating);int procs=0;
  for(int bucket=0;bucket<12;bucket++){auto g=base;g.takeEvents();uint32_t draw=uint32_t((uint64_t(bucket)*2+1)*2147483648ull/12);g.randomState=(draw-1013904223u)*4276115653u;auto p=g.battle->units[0];CHECK(g.cast(Haste,{p.x,p.y}).empty());auto events=g.takeEvents();procs+=std::any_of(events.begin(),events.end(),[](const auto&e){return e.type==GameEventType::CombatMorale&&e.stackId==0&&e.detail==-1;});}CHECK(procs==rating);
 }
});
run("lucky damage adds base damage rather than doubling final modified damage",[&]{auto g=moraleFixture();g.hero.skills[Offense]=3;g.hero.attack=20;auto a=g.battle->units[0],t=g.battle->units[1];int normal=g.damage(a,t,false,0,.5,100),lucky=g.damage(a,t,false,0,.5,100,false,true);CHECK(normal>100&&lucky==normal+100&&lucky<normal*2);});
run("integer percentage damage floors only after all multiplicative reductions",[]{
 auto g=Game::create();g.hero.skills={};g.hero.skills[Offense]=3;g.hero.attack=20;CombatStack a,t;a.unit=Pikeman;a.team=0;a.count=50;t.unit=Skeleton;t.team=1;CHECK(g.damage(a,t,false,0,.5,100)==230&&g.damage(a,t,false,0,.5,100,false,true)==330);t.effects[Shield]=3;t.effectMastery[Shield]=3;CHECK(g.damage(a,t,false,0,.5,101)==int(int64_t(101)*230*70/10000));
});
run("a lucky shot emits a cue and applies the same bonus before damage reductions",[&]{
 auto g=Game::create();g.hero.skills={};g.hero.skills[LuckSkill]=3;g.hero.army={Stack{Archer,20}};g.objectById(15)->army={Stack{Skeleton,1000}};g.startBattle(15);auto&a=g.battle->units[0];auto&t=g.battle->units[1];a.effects[Bless]=4;int hp=t.hp,expected=g.damage(a,t,true,0,.5,-1,false,true);g.takeEvents();g.randomState=MoraleHitSeed;CHECK(g.act("attack",{t.x,t.y}).empty());auto cues=g.takeEvents();CHECK(cues.front().type==GameEventType::CombatLuck&&hasCue(cues,GameEventType::CombatLuck,1)&&g.battle->units[1].hp==hp-expected);
});
run("version ten saves retain bonus actions and migrate prior skill arrays without rerolls",[&]{
 auto g=moraleFixture();g.randomState=MoraleHitSeed;auto p=*g.current();g.act("move",{p.x+1,p.y});auto path=testDirectory()/"morale-bonus.json";std::string error;CHECK(g.save(path,error));Game loaded;CHECK(loaded.load(path,error)&&loaded.toJson()==g.toJson()&&loaded.toJson()["version"]==19);g=moraleFixture();auto old=g.toJson();old["version"]=9;while(old["hero"]["skills"].size()>12)old["hero"]["skills"].erase(old["hero"]["skills"].end()-1);old["battle"].erase("moralePhase");old["battle"].erase("moraleWaiting");for(auto&u:old["battle"]["units"])u.erase("moraleUsed");std::ofstream(path)<<old.dump();CHECK(loaded.load(path,error)&&loaded.toJson()==g.toJson()&&loaded.randomState==g.randomState&&loaded.hero.skills[LuckSkill]==0);
});
run("malformed morale phases and flags cannot replace the current game",[&]{
 auto g=moraleFixture();auto baseline=g.toJson();auto path=testDirectory()/"bad-morale-state.json";std::string error;for(int invalid=0;invalid<4;invalid++){auto j=baseline;if(invalid==0)j["battle"]["moralePhase"]="yes";if(invalid==1){j["battle"]["moralePhase"]=true;j["battle"]["waitPhase"]=true;}if(invalid==2)j["battle"]["units"][0]["moraleUsed"]=true;if(invalid==3)j["battle"]["moraleWaiting"]={0};std::ofstream(path)<<j.dump();CHECK(!g.load(path,error)&&g.toJson()==baseline);}
});
run("morale presentation is pausable and cannot consume game randomness",[&]{
 auto g=moraleFixture();auto before=*g.battle;auto state=g.toJson();auto u=before.units[0];u.moraleUsed=u.acted=true;std::array events{GameEvent{GameEventType::CombatMorale,u.unit,1,{u.x,u.y},u.id,-1,{},u}};BattleMotion motion;motion.start(before,events,actionFrames,0);CHECK(motion.active()&&motion.feedback()&&motion.feedback()->type==GameEventType::CombatMorale&&motion.view()->units[0].moraleUsed);motion.tick(1000,false);CHECK(motion.feedback());motion.tick(2999);CHECK(motion.feedback()&&motion.effects().front().frame==19);motion.tick(3000);CHECK(!motion.active()&&g.toJson()==state);
});
