run("original morale and luck effect metadata matches every stored frame canvas",[]{
 std::ifstream file(std::filesystem::path(H3_TEST_SOURCE_ROOT)/"assets/demo/manifest.json");auto manifest=nlohmann::json::parse(file);
 for(auto [type,detail]:{std::pair{GameEventType::CombatMorale,1},std::pair{GameEventType::CombatMorale,-1},std::pair{GameEventType::CombatLuck,1},std::pair{GameEventType::CombatSpellEffect,int(Misfortune)}}){
  auto*d=battleEffect(type,detail);CHECK(d);const auto&asset=manifest["def"][d->animation];CHECK(int(asset["groups"]["0"].size())==d->frames);
  for(const auto&f:asset["groups"]["0"]){CHECK(f["canvas"][0]==d->width&&f["canvas"][1]==d->height);CHECK(f["offset"][0].get<int>()>=0&&f["offset"][1].get<int>()>=0);CHECK(f["offset"][0].get<int>()+f["rect"][2].get<int>()<=d->width&&f["offset"][1].get<int>()+f["rect"][3].get<int>()<=d->height);}
 }
 CHECK(!battleEffect(GameEventType::CombatLuck,-1)&&!battleEffect(GameEventType::CombatMorale,0)&&battleEffect(GameEventType::CombatSpellEffect,MagicArrow));
});
run("morale and luck sequences play all original frames once without looping",[&]{
 auto g=moraleFixture();auto before=*g.battle;auto u=before.units[0];u.moraleUsed=u.acted=true;
 for(auto [type,detail]:{std::pair{GameEventType::CombatMorale,1},std::pair{GameEventType::CombatMorale,-1},std::pair{GameEventType::CombatLuck,1}}){
  std::array events{GameEvent{type,u.unit,detail,{u.x,u.y},u.id,-1,{},u}};BattleMotion motion;motion.start(before,events,actionFrames,0);auto*d=battleEffect(type,detail);std::vector<int> seen;
  for(int frame=0;frame<d->frames;frame++){motion.tick(frame*100);auto effects=motion.effects();CHECK(effects.size()==1&&effects[0].frame==frame&&effects[0].stackId==u.id);seen.push_back(effects[0].frame);}
  CHECK(int(seen.size())==(type==GameEventType::CombatLuck?15:20));motion.tick(d->frames*100);CHECK(!motion.active()&&motion.effects().empty());
 }
});
run("bad morale flinches once while the original eagle effect continues",[&]{
 auto g=moraleFixture();auto before=*g.battle;auto u=before.units[0];u.moraleUsed=u.acted=true;std::array events{GameEvent{GameEventType::CombatMorale,u.unit,-1,{u.x,u.y},u.id,-1,{},u}};BattleMotion motion;motion.start(before,events,actionFrames,0);CHECK(motion.pose(u.id)->group==3);motion.tick(399);CHECK(motion.pose(u.id)->frame==3);motion.tick(400);CHECK(!motion.pose(u.id)&&motion.effects().front().frame==4);motion.tick(2000);CHECK(!motion.active());
});
run("mass morale spell emits only affected stack snapshots and plays them together",[]{
 auto g=spellGame();g.hero.skills={};g.hero.skills[WaterMagic]=3;g.hero.power=3;g.hero.army={Stack{Pikeman,20},Stack{30,10},Stack{Skeleton,20},Stack{32,10}};g.hero.spells[Mirth]=true;g.objectById(15)->army={Stack{Skeleton,1000}};g.startBattle(15);g.takeEvents();auto before=*g.battle;CHECK(g.cast(Mirth).empty());auto committed=g.toJson();auto events=g.takeEvents();CHECK(events.size()==3&&events[0].type==GameEventType::SpellCast);CHECK(events[1].stackId==0&&events[2].stackId==1&&events[1].state->effects[Mirth]==3);
 BattleMotion motion;motion.start(before,events,actionFrames,0);CHECK(motion.effects().size()==2&&motion.view()->units[0].effects[Mirth]==3);auto sounds=motion.takeEvents();CHECK(sounds.size()==3&&std::all_of(sounds.begin(),sounds.end(),[](const auto&e){return e.scheduled;}));motion.tick(10000,false);CHECK(motion.effects()[0].frame==0);motion.tick(10900);CHECK(motion.effects()[0].frame==9&&motion.effects()[1].frame==9);motion.tick(12000);CHECK(!motion.active()&&g.toJson()==committed);
});
run("all four morale and luck spells identify their successful single target",[]{
 for(int spell:{int(Mirth),int(Sorrow),int(Fortune),int(Misfortune)}){
  auto g=spellGame();g.hero.skills={};g.hero.army={Stack{Archangel,5}};g.hero.spells[spell]=true;g.objectById(15)->army={Stack{Pikeman,1000}};g.startBattle(15);g.takeEvents();auto target=g.battle->units[Spells[spell].enemy?1:0];CHECK(g.cast(spell,{target.x,target.y}).empty());auto cues=g.takeEvents();CHECK(cues.size()==2&&cues[1].type==GameEventType::CombatSpellEffect&&cues[1].detail==spell&&cues[1].stackId==target.id&&cues[1].state->effects[spell]>0);CHECK(battleEffect(cues[1].type,spell));
 }
});
run("resisted morale spells produce no successful visual target",[]{
 auto g=spellGame();g.hero.skills={};g.hero.army={Stack{Archangel,5}};g.hero.spells[Sorrow]=true;g.objectById(15)->army={Stack{16,1000}};g.startBattle(15);auto before=*g.battle;auto target=g.battle->units[1];CHECK(Units[target.unit].magicResistance>0);g.takeEvents();g.randomState=634785765u;CHECK(g.cast(Sorrow,{target.x,target.y}).empty());auto events=g.takeEvents();CHECK(events.size()==1&&events[0].type==GameEventType::SpellCast&&!g.battle->units[1].effects[Sorrow]);BattleMotion motion;motion.start(before,events,actionFrames,0);CHECK(!motion.active()&&motion.effects().empty());
});
run("fortune mass effects include nonliving stacks and preserve independent targets",[]{
 auto g=spellGame();g.hero.skills={};g.hero.skills[AirMagic]=3;g.hero.army={Stack{Skeleton,20},Stack{32,10},Stack{128,10}};g.hero.spells[Fortune]=true;g.objectById(15)->army={Stack{Skeleton,1000}};g.startBattle(15);g.takeEvents();auto before=*g.battle;CHECK(g.cast(Fortune).empty());auto events=g.takeEvents();BattleMotion motion;motion.start(before,events,actionFrames,0);CHECK(motion.effects().size()==3);motion.tick(1400);for(const auto&e:motion.effects())CHECK(e.frame==14);motion.tick(1500);CHECK(!motion.active());
});
run("expert Misfortune uses Fire Magic and shares its effect across nonliving targets",[]{
 auto g=spellGame();g.hero.skills={};g.hero.skills[FireMagic]=3;g.hero.army={Stack{Archangel,5}};g.hero.spells[Misfortune]=true;g.objectById(15)->army={Stack{Pikeman,1000},Stack{30,1000},Stack{32,1000}};g.startBattle(15);g.takeEvents();auto before=*g.battle;CHECK(g.spellMass(Misfortune)&&g.cast(Misfortune).empty());auto events=g.takeEvents();CHECK(events.size()==4);BattleMotion motion;motion.start(before,events,actionFrames,0);CHECK(motion.feedback()->detail==Misfortune&&motion.effects().size()==3);motion.tick(1499);for(const auto&e:motion.effects())CHECK(e.frame==14);motion.tick(1500);CHECK(!motion.active());
});
run("invalid effect payloads cannot create a phantom animation or pause",[&]{
 auto g=moraleFixture();auto before=*g.battle;auto u=before.units[0];
 for(int invalid=0;invalid<3;invalid++){GameEvent event{GameEventType::CombatMorale,u.unit,1,{u.x,u.y},u.id,-1,{},u};if(invalid==0)event.state.reset();if(invalid==1)event.state->unit=Archer;if(invalid==2)event.pos.x++;BattleMotion motion;motion.start(before,std::span(&event,1),actionFrames,0);CHECK(!motion.active()&&motion.effects().empty());}
});
run("lucky attack damage waits for the full original effect and emits no duplicate cue",[]{
 auto g=Game::create();g.hero.skills={};g.hero.skills[LuckSkill]=3;g.hero.army={Stack{Archer,20}};g.objectById(15)->army={Stack{Skeleton,1000}};g.startBattle(15);g.battle->units[0].effects[Bless]=4;g.takeEvents();auto before=*g.battle;auto enemy=g.battle->units[1];g.randomState=634785765u;CHECK(g.act("attack",{enemy.x,enemy.y}).empty());auto events=g.takeEvents();auto saved=g.toJson();BattleMotion motion;motion.start(before,events,actionFrames,0);auto sounds=motion.takeEvents();CHECK(sounds.size()==1&&sounds[0].type==GameEventType::CombatLuck&&sounds[0].scheduled);motion.tick(1499);CHECK(motion.effects().front().frame==14&&motion.takeEvents().empty()&&motion.view()->units[1].hp==enemy.hp);motion.tick(1500);sounds=motion.takeEvents();CHECK(!sounds.empty()&&sounds.front().type==GameEventType::CombatAttack&&motion.effects().empty());motion.tick(100000);CHECK(!motion.active()&&g.toJson()==saved);
});
