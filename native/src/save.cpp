#include "h3/game.hpp"
#include "h3/localization.hpp"
#include <fstream>
#include <set>
namespace h3 {
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Stack,unit,count)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Point,x,y)
void to_json(nlohmann::json&j,const Tile&t){j={{"terrain",t.terrain},{"decor",t.decor},{"seen",t.seen},{"road",t.road},{"variant",t.variant},{"river",t.river},{"originalView",t.originalView},{"originalFlags",t.originalFlags},{"roadType",t.roadType},{"roadView",t.roadView},{"riverType",t.riverType},{"riverView",t.riverView},{"blocked",t.blocked}};}
void from_json(const nlohmann::json&j,Tile&t){j.at("terrain").get_to(t.terrain);j.at("decor").get_to(t.decor);j.at("seen").get_to(t.seen);j.at("road").get_to(t.road);j.at("variant").get_to(t.variant);t.river=j.value("river",false);t.originalView=j.value("originalView",-1);t.originalFlags=j.value("originalFlags",0);t.roadType=j.value("roadType",1);t.roadView=j.value("roadView",-1);t.riverType=j.value("riverType",1);t.riverView=j.value("riverView",-1);t.blocked=j.value("blocked",false);}
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Object,id,type,pos,owner,name,resource,amount,visited,army,level,sprite,anchor,sourceClass,sourceSubtype,properties,ground)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(MapDecoration,anchor,level,sprite,ground)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(LevelChoice,level,primary,skills)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(AdventureBonus,sourceClass,sourceId,morale,luck,movement,expiresDay,untilBattle)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Hero,adventureSpellDay,adventureSpellMastery,doorDay,doorCasts,pos,level,xp,attack,defense,power,knowledge,mana,movement,maxMovement,army,skills,spells,spellbook,artifacts,levelChoices,tightFormation,wellDay,name,originalId,onBoat,lastEmbark,adventureGoal,artifactIds,wornArtifacts,visitedSites,equipmentSlots,keys,adventureBonuses,townLessons,aiVisitedSites,stableWeek,comboLocks)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Town,built,pool,garrison,builtDay,faction,stableWeek,lessons,pos,guild,objectId,level,vortexWeek,bonusWeek,pondResource,pondAmount,portalUnit,portalStock,merchantMonth,merchantStock)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(MapHero,value,owner,level,sleeping,patrolOrigin,patrolRadius,sprite,properties)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(CombatStack,id,slot,unit,team,count,initialCount,hp,x,y,retaliations,shots,defending,waited,preparedRound,defenseReduction,defendingBonus,effects,effectMastery,origin,permanentCount,cloneOf,cloneTurns,deathOrder,corpse,acted,moraleUsed,poisonTurns,baseHealth,casts,creatureSpell,abilityRound,poison,aged,disease,petrified,paralyzed,boundBy,effectBonuses)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Siege,faction,fort,attackerTeam,townId,buildings,hp,spentMines)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(RetiredHero,value,owner,week)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(BattleObstacle,spell,team,power,mastery,turns,revealed,cells,asset,anchor)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Battle,terrain,initialArmies,obstacles,lastDamageRound,targetId,round,spellRound,deferredVisitId,units,queue,waiting,waitPhase,log,summonedType,deathSequence,moraleWaiting,moralePhase,defenderPrimary,defenderSkills,defendingTown,returnHero,siege,defenderMana,defenderSpellRound,defenderSpellbook,defenderSpells,spellHistory,tacticsTeam,tacticsRange,tacticsStack,catapultRound,exitType,noMagic)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(NeutralOffer,objectId,unit,count,cost,deferredVisitId,intent)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(BattleReport,won,xp,before,after,enemies,raised,learnedSpells,exitType,raisedPending)
nlohmann::json Game::toJson()const{nlohmann::json j={{"format","heroes3-native"},{"version",19},{"day",day},{"seed",seed},{"randomState",randomState},{"resources",resources},{"bannedSpells",bannedSpells},{"hero",hero},{"town",town},{"tiles",tiles},{"objects",objects},{"result",result},{"log",log},{"grailPos",grailPos},{"grailFound",grailFound}};auto roster=heroes;if(activeHeroId&&roster.contains(activeHeroId)){roster[activeHeroId].value=hero;roster[activeHeroId].level=mapLevel;}j["retiredHeroes"]=retiredHeroes;j["heroes"]=roster;j["activeHeroId"]=activeHeroId;auto banks=playerResources;if(imported())banks[actingOwner]=resources;j["playerResources"]=banks;auto fog=playerFog;if(imported()){fog[actingOwner].resize(tiles.size());for(size_t i=0;i<tiles.size();++i)fog[actingOwner][i]=tiles[i].seen;}j["playerFog"]=fog;j["computerQueue"]=computerQueue;j["mapWidth"]=mapWidth;j["mapHeight"]=mapHeight;j["mapLevels"]=mapLevels;j["mapLevel"]=mapLevel;j["scenarioId"]=scenarioId;j["scenarioName"]=scenarioName;j["decorations"]=decorations;j["settlements"]=settlements;j["activeTownId"]=activeTownId;j["scenarioRules"]=scenarioRules;j["battle"]=battle?nlohmann::json(*battle):nlohmann::json(nullptr);j["report"]=report?nlohmann::json(*report):nlohmann::json(nullptr);j["neutralOffer"]=neutralOffer?nlohmann::json(*neutralOffer):nlohmann::json(nullptr);return j;}
bool Game::validate()const{if(neutralOffer){const auto&o=*neutralOffer;auto at=std::find_if(objects.begin(),objects.end(),[&](auto&x){return x.id==o.objectId;});if(battle||report||result||at==objects.end()||at->type!=ObjectType::Monster||o.unit<0||o.unit>=CreatureUnitCount||o.count<1||o.count>MaxArmyStack||o.count!=armyCount(at->army)||armyStacks(at->army)!=1||std::none_of(at->army.begin(),at->army.end(),[&](auto&s){return s.unit==o.unit;})||o.cost<0||o.cost>1000000000||int(o.intent)<1||int(o.intent)>3||at->level!=mapLevel||std::max(std::abs(hero.pos.x-at->pos.x),std::abs(hero.pos.y-at->pos.y))>1)return false;}for(int n:hero.skills)if(n<0||n>3)return false;for(int n:{hero.attack,hero.defense,hero.power,hero.knowledge})if(n<0||n>1000)return false;auto validPoint=[&](Point p){return p.x>=0&&p.y>=0&&p.x<mapWidth&&p.y<mapHeight;};
 if(mapWidth<1||mapWidth>144||mapHeight<1||mapHeight>144||mapLevels<1||mapLevels>2||mapLevel<0||mapLevel>=mapLevels||tiles.size()!=size_t(mapWidth*mapHeight*mapLevels)||scenarioId.size()>160||scenarioName.size()>512||decorations.size()>60000||settlements.size()>1000)return false;
 if(imported()&&(!scenarioRules.is_object()||!scenarioRules.contains("additional")||!scenarioRules["additional"].is_object()||!scenarioRules["additional"].contains("victory_condition")||!scenarioRules["additional"]["victory_condition"].is_object()||!scenarioRules.contains("originalPlayer")||!scenarioRules["originalPlayer"].is_number_integer()||scenarioRules["originalPlayer"].get<int>()<0||scenarioRules["originalPlayer"].get<int>()>7||!scenarioRules.contains("defeated")||!scenarioRules["defeated"].is_array()))return false;
 for(const auto&d:decorations)if(d.anchor.x<0||d.anchor.x>255||d.anchor.y<0||d.anchor.y>255||d.level<0||d.level>=mapLevels||d.sprite.size()>128)return false;auto validArmy=[](const Army&a){for(auto&s:a)if((s.empty()&&(s.unit!=-1||s.count!=0))||(!s.empty()&&(s.unit>=CreatureUnitCount||s.count>1000000)))return false;return true;};
 if(retiredHeroes.size()>64)return false;for(const auto&r:retiredHeroes)if(r.owner<0||r.owner>7||r.week<0||r.week>(day-1)/7||!validArmy(r.value.army))return false;
 if(heroes.size()>64||computerQueue.size()>64||actingOwner<0||actingOwner>7)return false;for(const auto&[id,h]:heroes)if(id<=0||h.owner<0||h.owner>7||h.level<0||h.level>=mapLevels||!validPoint(h.value.pos)||!validArmy(h.value.army))return false;for(const auto&fog:playerFog)if(!fog.empty()&&fog.size()!=tiles.size())return false;
 auto validBonuses=[](const Hero&who){if(who.adventureBonuses.size()>64||who.mana<0||who.mana>1000000)return false;std::set<int>classes;for(const auto&b:who.adventureBonuses)if(b.sourceClass<0||b.sourceClass>255||b.sourceId<0||b.morale<-3||b.morale>3||b.luck<-3||b.luck>3||b.movement<0||b.movement>400||b.expiresDay<0||b.expiresDay>100008||!classes.insert(b.sourceClass).second)return false;return true;};
 if(!validBonuses(hero))return false;for(const auto&r:retiredHeroes)if(!validBonuses(r.value))return false;for(const auto&[id,h]:heroes)if(!validBonuses(h.value))return false;
 auto validMagic=[&](const Hero&h){if(h.aiVisitedSites.size()>60000)return false;for(int id:h.aiVisitedSites)if(id<=0)return false;if(h.doorDay<0||h.doorDay>day||h.doorCasts<0||h.doorCasts>4)return false;for(int spell=0;spell<SpellCount;++spell)if(h.adventureSpellDay[spell]<0||h.adventureSpellDay[spell]>day||h.adventureSpellMastery[spell]<0||h.adventureSpellMastery[spell]>3)return false;return true;};if(!validMagic(hero))return false;for(const auto&[id,h]:heroes)if(!validMagic(h.value))return false;
 if(!validArtifactLocks(hero))return false;for(const auto&[id,h]:heroes)if(!validArtifactLocks(h.value))return false;
 if(hero.wornArtifacts.size()>32||hero.equipmentSlots.size()>32)return false;for(int id:hero.wornArtifacts)if(std::find(hero.artifactIds.begin(),hero.artifactIds.end(),id)==hero.artifactIds.end())return false;
 if(std::count_if(hero.skills.begin(),hero.skills.end(),[](int level){return level>0;})>8)return false;
 auto validServices=[&](const Town&t){if(t.vortexWeek<-1||t.vortexWeek>(day-1)/7||t.bonusWeek<-1||t.bonusWeek>(day-1)/7||t.merchantMonth<-1||t.merchantMonth>(day-1)/28||t.pondResource<-1||t.pondResource>=ResourceCount||t.pondAmount<0||t.pondAmount>4||t.portalUnit<-1||t.portalUnit>=CreatureUnitCount||t.portalStock<0||t.portalStock>MaxArmyStack||t.merchantStock.size()>7)return false;for(int id:t.merchantStock)if(id<-1||id>=128)return false;return true;};if(!validServices(town))return false;for(const auto&[id,t]:settlements)if(!validServices(t))return false;
 if(town.faction<0||town.faction>=FactionCount||town.builtDay<0||town.builtDay>day||town.stableWeek<-1||town.stableWeek>(day-1)/7)return false;
 for(int level=1;level<=5;level++){
  const auto&spells=town.guild[level-1];std::set<int>known;
  if(spells.size()>size_t(6-level+(town.faction==2?1:0))||(level>townDef().maxMageGuild&&!spells.empty()))return false;
  for(int spell:spells)if(spell<0||spell>=SpellCount||Spells[spell].level!=level||Spells[spell].special||bannedSpells[spell]||!known.insert(spell).second)return false;
 }
 if(hero.wellDay<0||hero.wellDay>day||!validPoint(grailPos))return false;
 if(day<1||day>100000||result<-1||result>1||!validPoint(hero.pos)||!validPoint(town.pos)||hero.level<1||hero.level>70||hero.xp<0||hero.xp>1000000000||hero.mana<0||hero.mana>1000000||hero.movement<0||hero.movement>100000||hero.maxMovement<0||hero.maxMovement>100000||hero.artifacts.size()>(imported()?1000:100)||hero.levelChoices.size()>70)return false;
 for(int n:hero.skills)if(n<0||n>3)return false;for(int n:{hero.attack,hero.defense,hero.power,hero.knowledge})if(n<0||n>1000)return false;
 if(!validArmy(hero.army)||!validArmy(town.garrison)||(result>=0&&armyCount(hero.army)==0&&!(imported()&&hasTown()&&activeHeroId==0)))return false;for(int n:resources)if(n<0||n>1000000000)return false;for(int n:town.pool)if(n<0||n>1000000)return false;
 for(auto&t:tiles)if(int(t.terrain)<0||int(t.terrain)>9||int(t.decor)<0||int(t.decor)>2||t.variant<0||t.variant>=10000)return false;
 for(auto&c:hero.levelChoices){if(c.level<2||c.level>hero.level||c.primary<0||c.primary>3||c.skills.empty()||c.skills.size()>2)return false;for(int n:c.skills)if(n<0||n>=SkillCount)return false;}
 std::set<int>ids;std::set<std::pair<int,int>>positions;if(objects.size()>(imported()?30000:10000)||log.size()>1000)return false;for(auto&o:objects)if(o.id<=0||!ids.insert(o.id).second||(!imported()&&!positions.emplace(o.pos.x,o.pos.y).second)||!validPoint(o.pos)||int(o.type)<0||int(o.type)>(imported()?17:11)||o.owner<-1||o.owner>(imported()?7:1)||o.resource<0||o.resource>=ResourceCount||o.amount<0||o.amount>1000000||!validArmy(o.army)||o.name.size()>512||o.level<0||o.level>=mapLevels||o.sprite.size()>128)return false;
 if(report){const auto&r=*report;if(r.raisedPending&&(!r.won||r.raised.empty()))return false;if(r.exitType<0||r.exitType>2||r.learnedSpells.size()>SpellCount||(!r.raised.empty()&&(r.raised.unit<0||r.raised.unit>=CreatureUnitCount||r.raised.count>MaxArmyStack)))return false;std::set<int>learned;for(int spell:r.learnedSpells)if(spell<0||spell>=SpellCount||!learned.insert(spell).second)return false;if(battle||r.xp<0||r.xp>1000000000||!validArmy(r.before)||!validArmy(r.after)||!validArmy(r.enemies))return false;for(int i=0;i<ArmySlots;i++){if(r.after[i].count>r.before[i].count||(!r.after[i].empty()&&r.after[i].unit!=r.before[i].unit))return false;}}if(battle){
  const auto&b=*battle;if(b.obstacles.size()>500)return false;for(const auto&o:b.obstacles){if(o.spell<-1||o.spell>=SpellCount||o.team<0||o.team>1||o.mastery<0||o.mastery>3||o.power<0||o.power>1000||o.turns<-1||o.turns>1000||o.revealed<0||o.revealed>3||o.cells.empty()||o.cells.size()>50||o.asset.size()>128)return false;for(auto p:o.cells)if(p.x<0||p.x>=BattleW||p.y<0||p.y>=BattleH)return false;}
  if(b.exitType!=0||b.lastDamageRound<0||b.lastDamageRound>b.round)return false;if(b.tacticsTeam<-1||b.tacticsTeam>1||b.tacticsRange<0||b.tacticsRange>7||b.catapultRound<0||b.catapultRound>b.round)return false;if(b.tacticsTeam>=0&&(b.tacticsRange<3||b.tacticsStack<0||!std::any_of(b.units.begin(),b.units.end(),[&](auto&u){return u.id==b.tacticsStack&&u.team==b.tacticsTeam&&u.origin==StackOrigin::Army&&u.hp>0;})))return false;
  if(b.siege.fort<0||b.siege.fort>3||b.siege.attackerTeam<0||b.siege.attackerTeam>1||b.defenderMana<0||b.defenderMana>1000000||b.defenderSpellRound<0||b.defenderSpellRound>b.round)return false;
  if(int(b.terrain)<0||int(b.terrain)>int(Terrain::Rock))return false;
  if(b.siege.fort){if(b.siege.faction<0||b.siege.faction>=FactionCount)return false;for(int n:b.siege.hp)if(n<0||n>3)return false;for(auto p:b.siege.spentMines)if(p.x<0||p.x>=BattleW||p.y<0||p.y>=BattleH)return false;}
  if(b.deferredVisitId){auto found=std::find_if(objects.begin(),objects.end(),[&](const auto&o){return o.id==b.deferredVisitId;});if(found==objects.end()||(!found->visitableAt(hero.pos)&&!(extendedLocation(found->sourceClass)&&canVisitLocation(*found)))||found->type==ObjectType::Monster||b.deferredVisitId==b.targetId)return false;}
  if(result||b.spellRound<0||b.spellRound>b.round||!ids.contains(b.targetId)||b.units.empty()||b.units.size()>40||b.round<1||b.round>100000||b.queue.empty()||b.queue.size()>40||b.waiting.size()>40||b.moraleWaiting.size()>40||(b.waitPhase&&b.moralePhase)||((b.waitPhase||b.moralePhase)&&!b.moraleWaiting.empty())||b.deathSequence<0||b.deathSequence>10000000)return false;
  for(int type:b.summonedType)if(type!=-1&&type!=114&&type!=116&&type!=118&&type!=120)return false;
  std::set<int>unitIds,activeIds,deathOrders;std::array<int,2>allocated{};std::set<std::pair<int,int>>occupied;
  for(const auto&u:b.units){
   if(u.unit<0||u.unit>=UnitCount||u.slot<0||u.slot>=26||u.team<0||u.team>1||(u.id/100<0||u.id/100>1||u.id%100!=u.slot||u.team!=(u.effects[Hypnotize]>0?1-u.id/100:u.id/100))||!unitIds.insert(u.id).second||++allocated[u.id/100]>MaxBattleStacksPerSide)return false;
   if(int(u.origin)<0||int(u.origin)>3||(u.origin==StackOrigin::Army?u.slot>=7:u.slot<7))return false;
   if(u.origin==StackOrigin::WarMachine){if(u.unit<CreatureUnitCount||u.slot!=(u.unit==CatapultUnit?22:23+u.unit-CreatureUnitCount)||u.initialCount!=1||u.permanentCount!=0)return false;}
   else if(u.unit>=CreatureUnitCount)return false;
   if(u.initialCount<1||u.initialCount>1000000||u.count<0||u.count>u.initialCount||u.hp<0||int64_t(u.hp)>int64_t(u.initialCount)*stackHealth(u)||u.count!=(u.hp+stackHealth(u)-1)/stackHealth(u)||u.permanentCount<0||u.permanentCount>u.count)return false;
   if(u.baseHealth<0||u.baseHealth>2000||u.casts<-1||u.casts>creatureAbility(u.unit).casts||u.creatureSpell<-1||(u.creatureSpell>=SpellCount&&u.creatureSpell!=1000)||u.abilityRound<-1||u.abilityRound>b.round+3||u.poison<0||u.poison>30||u.poisonTurns<0||u.poisonTurns>3||u.aged<0||u.aged>3||u.disease<0||u.disease>3||u.petrified<0||u.petrified>3||u.paralyzed<0||u.paralyzed>3||u.boundBy<-1)return false;
   for(int value:u.effectBonuses)if(value<-100000||value>100000)return false;
   if(u.defenseReduction<0||u.defenseReduction>1000000||u.defendingBonus<0||u.defendingBonus>10000||(!u.defending&&u.defendingBonus!=0)||u.preparedRound<-1||u.preparedRound>b.round||u.shots<0||u.shots>Units[u.unit].shots)return false;
   if(u.moraleUsed&&!u.acted)return false;
   if(u.corpse){if(u.hp>0||u.origin==StackOrigin::Clone||u.deathOrder<1||u.deathOrder>b.deathSequence||!deathOrders.insert(u.deathOrder).second)return false;}
   else if(u.deathOrder!=0)return false;
   if(u.origin==StackOrigin::Clone){
    if(u.cloneOf==u.id||u.cloneOf<0||u.cloneTurns<0||u.cloneTurns>1000||(u.hp>0&&u.cloneTurns==0)||u.permanentCount!=0)return false;
    auto source=std::find_if(b.units.begin(),b.units.end(),[&](const auto&other){return other.id==u.cloneOf;});
    if(source==b.units.end()||source->origin==StackOrigin::Clone||source->team!=u.team||source->unit!=u.unit||(u.hp>0&&source->hp==0))return false;
   }else if(u.cloneOf!=-1||u.cloneTurns!=0)return false;
   if(u.origin==StackOrigin::Summoned&&(u.unit!=b.summonedType[u.team]||u.permanentCount!=0))return false;
   for(int spell=0;spell<SpellCount;spell++)if(u.effects[spell]<0||u.effects[spell]>1000||u.effectMastery[spell]<0||u.effectMastery[spell]>3||(u.effects[spell]==0&&u.effectMastery[spell]!=0))return false;
   for(auto p:footprint(u,{u.x,u.y})){if(p.x<0||p.x>=BattleW||p.y<0||p.y>=BattleH)return false;if(u.hp>0&&!occupied.emplace(p.x,p.y).second)return false;}
  }
  for(int id:b.queue){auto at=std::find_if(b.units.begin(),b.units.end(),[=](const auto&u){return u.id==id&&u.hp>0;});if(at==b.units.end()||!activeIds.insert(id).second)return false;}
  for(int id:b.waiting){auto at=std::find_if(b.units.begin(),b.units.end(),[=](const auto&u){return u.id==id&&u.hp>0;});if(at==b.units.end()||!activeIds.insert(id).second)return false;}
  for(int id:b.moraleWaiting){auto at=std::find_if(b.units.begin(),b.units.end(),[=](const auto&u){return u.id==id&&u.hp>0&&u.moraleUsed&&u.acted&&u.waited;});if(at==b.units.end()||!activeIds.insert(id).second)return false;}
 }
 return true;}
bool Game::save(const std::filesystem::path&p,std::string&error)const{try{if(!validate())throw std::runtime_error("游戏状态不完整，未写入存档");if(!p.parent_path().empty())std::filesystem::create_directories(p.parent_path());auto temp=p;temp+=".tmp";{std::ofstream f(temp,std::ios::binary|std::ios::trunc);if(!f)throw std::runtime_error("无法创建存档文件");f<<toJson().dump(imported()?-1:2);f.flush();if(!f)throw std::runtime_error("写入存档失败");}std::filesystem::rename(temp,p);return true;}catch(const std::exception&e){error=e.what();return false;}}
bool Game::load(const std::filesystem::path&p,std::string&error){
 try{
  if(std::filesystem::file_size(p)>64*1024*1024)throw std::runtime_error("存档文件过大");
  std::ifstream file(p,std::ios::binary);auto j=nlohmann::json::parse(file);
  localizeMapData(j,j.value("scenarioId",std::string{}));
  int version=j.at("version");
  if(j.at("format")!="heroes3-native"||version<1||version>19)throw std::runtime_error("存档版本不兼容；原生版与浏览器版不通用");
  if(version<11){j["hero"]["wellDay"]=0;j["grailPos"]={{"x",6},{"y",22}};j["grailFound"]=false;}
  if(!j.at("hero").at("wellDay").is_number_integer()||!j.at("grailFound").is_boolean())throw std::runtime_error("访问状态数据无效");
  if(version<9)j["hero"]["tightFormation"]=false;
  if(!j.at("hero").at("tightFormation").is_boolean())throw std::runtime_error("阵型数据无效");
  if(version==1){
   auto migrate=[&](auto&&self,nlohmann::json&node)->void{
    if(node.is_object()){if(node.contains("unit")&&node["unit"].is_number_integer()){int id=node["unit"];if(id==14)node["unit"]=Skeleton;else if(id==15)node["unit"]=Wolf;else if(id==16)node["unit"]=Orc;}for(auto&value:node.items())self(self,value.value());}
    else if(node.is_array())for(auto&value:node)self(self,value);
   };migrate(migrate,j);
  }
  auto expand=[](nlohmann::json&array,size_t oldSize,size_t newSize,const nlohmann::json&value){if(!array.is_array()||array.size()!=oldSize)throw std::runtime_error("旧存档数组不完整");while(array.size()<newSize)array.push_back(value);};
  if(version<3){expand(j.at("town").at("built"),27,BuildingCount,false);j["town"]["built"][VillageHall]=true;}
  if(version>=3&&version<18){auto migrate=[&](auto&&self,nlohmann::json&node)->void{if(node.is_object()){if(node.contains("built")&&node["built"].size()!=BuildingCount)expand(node["built"],37,BuildingCount,false);for(auto&item:node.items())self(self,item.value());}else if(node.is_array())for(auto&item:node)self(self,item);};migrate(migrate,j);}
  if(version>=4&&version<10)expand(j.at("hero").at("skills"),12,SkillCount,0);
  if(version<4){expand(j.at("hero").at("skills"),8,SkillCount,0);if(!j.at("battle").is_null())for(auto&unit:j["battle"]["units"])unit["effectMastery"]=std::array<int,9>{};}
  if(version<5){
   expand(j.at("hero").at("spells"),9,SpellCount,false);
   if(!j.at("battle").is_null())for(auto&unit:j["battle"]["units"]){
    expand(unit.at("effects"),9,SpellCount,0);expand(unit.at("effectMastery"),9,SpellCount,0);unit["defenseReduction"]=0;unit["defendingBonus"]=0;
    if(unit.at("defending").get<bool>()){
     int id=unit.at("unit");if(id<0||id>=UnitCount)throw std::runtime_error("兵种编号无效");
     int base=Units[id].defense+(unit.at("team")==0?j.at("hero").at("defense").get<int>():0);
     if(unit["effects"][StoneSkin].get<int>()>0){int mastery=unit["effectMastery"][StoneSkin];if(mastery<0||mastery>3)throw std::runtime_error("法术熟练度无效");base+=Spells[StoneSkin].values[mastery];}
     unit["defendingBonus"]=base/5;
    }
   }
  }
  if(version==5){
   expand(j.at("hero").at("spells"),35,SpellCount,false);
   if(!j.at("battle").is_null())for(auto&unit:j["battle"]["units"]){expand(unit.at("effects"),35,SpellCount,0);expand(unit.at("effectMastery"),35,SpellCount,0);}
  }
  if(version<6){j["hero"]["spellbook"]=true;j["bannedSpells"]=std::array<bool,SpellCount>{};j["town"]["guild"]=std::array<std::vector<int>,5>{};}
  if(version<7&&!j.at("battle").is_null()){
   auto&combat=j["battle"];combat["summonedType"]=std::array<int,2>{-1,-1};int order=0;
   for(auto&u:combat["units"]){
    int id=u.at("id");u["origin"]=int(StackOrigin::Army);u["permanentCount"]=u.at("count");u["cloneOf"]=-1;u["cloneTurns"]=0;
    u["corpse"]=u.at("hp")==0;u["deathOrder"]=u["corpse"].get<bool>()?++order:0;
    bool pending=false;for(const auto&queued:combat["queue"])if(queued==id)pending=true;for(const auto&queued:combat["waiting"])if(queued==id)pending=true;
    u["acted"]=!pending&&(u.at("hp").get<int>()>0||u.value("preparedRound",-1)==combat["round"].get<int>());
   }
   combat["deathSequence"]=order;
  }
  if(!j.at("battle").is_null()){
   auto&combat=j["battle"];
   if(version<10){combat["moraleWaiting"]=nlohmann::json::array();combat["moralePhase"]=false;for(auto&u:combat["units"])u["moraleUsed"]=false;}
   if(!combat.at("moraleWaiting").is_array()||!combat.at("moralePhase").is_boolean())throw std::runtime_error("Invalid morale phase");
   for(const auto&u:combat["units"])if(!u.at("moraleUsed").is_boolean())throw std::runtime_error("Invalid morale state");
  }
  if(!j.at("hero").at("spellbook").is_boolean()||!j.at("bannedSpells").is_array()||j.at("bannedSpells").size()!=SpellCount||!j.at("town").at("guild").is_array()||j.at("town").at("guild").size()!=5)throw std::runtime_error("魔法行会或法术书数据不完整");
  if(version<16){auto skills=[&](auto&&self,nlohmann::json&node)->void{if(node.is_object()){for(auto&item:node.items()){if((item.key()=="skills"||item.key()=="defenderSkills")&&item.value().is_array()&&item.value().size()==13)while(item.value().size()<SkillCount)item.value().push_back(0);else self(self,item.value());}}else if(node.is_array())for(auto&item:node)self(self,item);};skills(skills,j);}
  if(j.at("town").at("built").size()!=BuildingCount||j.at("hero").at("skills").size()!=SkillCount||j.at("hero").at("spells").size()!=SpellCount)throw std::runtime_error("存档数组数量不匹配");
  if(!j.at("battle").is_null()){
   const auto&combat=j.at("battle");
   if(!combat.at("summonedType").is_array()||combat.at("summonedType").size()!=2||!combat.at("deathSequence").is_number_integer())throw std::runtime_error("召唤与尸体数据不完整");
   for(const auto&unit:combat.at("units")){
    if(unit.at("effects").size()!=SpellCount||unit.at("effectMastery").size()!=SpellCount)throw std::runtime_error("法术效果数组不完整");
    for(auto field:{"origin","permanentCount","cloneOf","cloneTurns","deathOrder"})if(!unit.at(field).is_number_integer())throw std::runtime_error("临时部队状态不完整");
    if(!unit.at("corpse").is_boolean()||!unit.at("acted").is_boolean())throw std::runtime_error("部队行动状态不完整");
   }
  }
  Game g;if(j.contains("retiredHeroes"))j["retiredHeroes"].get_to(g.retiredHeroes);if(j.contains("heroes"))j["heroes"].get_to(g.heroes);g.activeHeroId=j.value("activeHeroId",0);if(j.contains("playerResources"))j["playerResources"].get_to(g.playerResources);if(j.contains("playerFog"))j["playerFog"].get_to(g.playerFog);g.computerQueue=j.value("computerQueue",std::vector<int>{});g.mapWidth=j.value("mapWidth",MapW);g.mapHeight=j.value("mapHeight",MapH);g.mapLevels=j.value("mapLevels",1);g.mapLevel=j.value("mapLevel",0);g.scenarioId=j.value("scenarioId",std::string{});g.scenarioName=j.value("scenarioName",std::string{});if(j.contains("decorations"))j["decorations"].get_to(g.decorations);if(j.contains("settlements"))j["settlements"].get_to(g.settlements);g.activeTownId=j.value("activeTownId",0);g.scenarioRules=j.value("scenarioRules",nlohmann::json{});g.day=j.at("day");g.seed=j.at("seed");g.randomState=j.at("randomState");j.at("resources").get_to(g.resources);j.at("bannedSpells").get_to(g.bannedSpells);j.at("hero").get_to(g.hero);j.at("town").get_to(g.town);j.at("tiles").get_to(g.tiles);j.at("objects").get_to(g.objects);g.result=j.at("result");j.at("log").get_to(g.log);j.at("grailPos").get_to(g.grailPos);g.grailFound=j.at("grailFound");
  if(j.contains("neutralOffer")&&!j["neutralOffer"].is_null())g.neutralOffer=j["neutralOffer"].get<NeutralOffer>();if(!j.at("battle").is_null())g.battle=j.at("battle").get<Battle>();if(!j.at("report").is_null())g.report=j.at("report").get<BattleReport>();
  if(g.imported()){for(size_t i=0;i<g.decorations.size();++i)if(!j["decorations"][i].contains("ground")){auto&d=g.decorations[i];d.ground=originalGroundLayer(g.scenarioId,d.sprite,d.anchor,d.level);}for(size_t i=0;i<g.objects.size();++i)if(!j["objects"][i].contains("ground")){auto&o=g.objects[i];o.ground=originalGroundLayer(g.scenarioId,o.sprite,o.anchor,o.level);}}
  for(auto&o:g.objects)if(o.type==ObjectType::Site&&(o.name=="地图物件"||o.name.starts_with("地图物件（类型 ")))o.name=adventureSiteName(o.sourceClass);
  if(g.battle&&!j["battle"].contains("terrain")){auto*target=g.objectById(g.battle->targetId);if(auto*t=g.tile(target?target->pos:g.hero.pos))g.battle->terrain=t->terrain;}
  if(version<19){g.repairArtifactLocks(g.hero);for(auto&[id,h]:g.heroes)g.repairArtifactLocks(h.value);}
  if(version<6)g.generateGuild();if(g.imported()&&g.heroes.empty())g.resetMapHeroes();
  if(!g.validate())throw std::runtime_error("存档校验失败");
  if(version<11){
   // Older builds let sprite footprints cover the hero's apparent square.
   g.clearObstacleFootprints({g.hero.pos});
   if(!g.passable(g.hero.pos)){
    Point original=g.hero.pos;int best=MapW+MapH;
    for(int y=0;y<MapH;++y)for(int x=0;x<MapW;++x){int d=std::abs(x-original.x)+std::abs(y-original.y);if(d<best&&g.tile({x,y})->seen&&g.passable({x,y})){best=d;g.hero.pos={x,y};}}
   }
  }
  *this=std::move(g);return true;
 }catch(const std::exception&e){error=e.what();return false;}
}
}
