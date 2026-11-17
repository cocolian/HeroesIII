#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <map>
#include <string>
#include <string_view>
#include <vector>
#include <utility>
#include <nlohmann/json.hpp>

namespace h3 {
constexpr int MapW=36,MapH=28,BattleW=15,BattleH=11,ArmySlots=7,MaxBattleStacksPerSide=20,MaxArmyStack=1000000;
enum Resource {Gold,Wood,Ore,Mercury,Sulfur,Crystal,Gems,ResourceCount};
using Resources=std::array<int,ResourceCount>;
extern const std::array<std::string,ResourceCount> ResourceNames;
enum UnitId {Pikeman,Halberdier,Archer,Marksman,Griffin,RoyalGriffin,Swordsman,Crusader,Monk,Zealot,Cavalier,Champion,Angel,Archangel,Skeleton=56,Wolf=86,Orc=88,UnitCount=141};
struct UnitDef {
 std::string name,asset; int hp,attack,defense,minDamage,maxDamage,speed,growth,cost,shots,size,retaliations;
 bool flying=false,doubleAttack=false,noMeleePenalty=false,joust=false; int gemCost=0;
 int faction=-1,tier=-1,originalId=-1;bool upgraded=false,noRetaliation=false,undead=false,nonLiving=false;int spellImmunity=0,magicResistance=0,spellDamageReduction=0,regeneration=0,manaDrain=0;Resources extraCost{};std::string abilitySummary;bool portraitOnly=false;
 int enemyDefenseReduction=0,spellCostDiscount=0,enemySpellCostIncrease=0,schoolImmunity=0;bool noDistancePenalty=false;std::vector<std::pair<int,int>> hates,spellVulnerabilities;std::vector<int> spellImmunities;
 int fireShield=0;bool mindImmune=false,living=true;int level=1;std::string animation;int portraitFrame=-1;int allyMorale=0,enemyMorale=0,enemyLuck=0,moraleFloor=-3,luckFloor=0;
};
extern const std::array<UnitDef,UnitCount> Units;
struct Stack {int unit=-1,count=0; bool empty() const{return unit<0||count<=0;} };
using Army=std::array<Stack,ArmySlots>;
struct Point {int x=0,y=0; bool operator==(const Point&)const=default;};
struct Step {int x=0,y=0,cost=0;};
struct AttackPlan {Point from{};int traveled=0;bool ranged=false;};
enum class Terrain {Grass,Dirt,Sand,Water,Snow,Swamp,Rough,Subterranean,Lava,Rock};
enum class Decor {None,Tree,Mountain};
struct Tile {Terrain terrain=Terrain::Grass;Decor decor=Decor::None;bool seen=false,road=false;int variant=0;bool river=false;int originalView=-1,originalFlags=0,roadType=1,roadView=-1,riverType=1,riverView=-1;bool blocked=false;};
enum class ObjectType {Town,Mine,Treasure,Resource,Monster,Shrine,Well,Artifact,Obelisk,TradingPost,Windmill,WaterWheel,Portal,Boat,MapEvent,Site,EnemyHero,Gate};
struct Object {int id=0;ObjectType type=ObjectType::Resource;Point pos;int owner=-1;std::string name;int resource=Gold,amount=0;bool visited=false;Army army{};int level=0;std::string sprite;Point anchor{};int sourceClass=-1,sourceSubtype=-1;nlohmann::json properties;bool ground=false;bool visitableAt(Point point)const{return pos==point||(sourceClass==48&&point==Point{pos.x-1,pos.y});}Point visitPosition(Point point)const{return visitableAt(point)?point:pos;}};
struct MapDecoration {Point anchor;int level=0;std::string sprite;bool ground=false;};
bool originalGroundLayer(std::string_view scenario,std::string_view sprite,Point anchor,int level);
struct AdventureTemplate {std::string_view asset;Point visitOffset{};uint64_t blocked=0,visitable=0;};
const AdventureTemplate& adventureTemplate(std::string_view);
struct AdventureVisit {ObjectType type;std::string title,message;Resources reward{};int spell=-1;int morale=0,luck=0;int sourceClass=-1;};
enum SkillId {Offense,Archery,Armorer,Logistics,Wisdom,Intelligence,Mysticism,Leadership,AirMagic,FireMagic,WaterMagic,EarthMagic,LuckSkill,SkillCount};
extern const std::array<std::string,SkillCount> SkillNames;
extern const std::array<std::string,4> SkillLevels;
enum SpellId {MagicArrow,Cure,Haste,Slow,Shield,StoneSkin,Bless,Curse,LightningBolt,IceBolt,Implosion,ChainLightning,FrostRing,Fireball,Inferno,MeteorShower,DeathRipple,DestroyUndead,Armageddon,TitanBolt,AirShield,FireShield,ProtectAir,ProtectFire,ProtectWater,ProtectEarth,AntiMagic,Bloodlust,Precision,Weakness,DisruptingRay,Prayer,Counterstrike,Forgetfulness,Dispel,SummonBoat,ScuttleBoat,Visions,ViewEarth,Disguise,ViewAir,Fly,WaterWalk,DimensionDoor,TownPortal,Quicksand,LandMine,ForceField,FireWall,Earthquake,MagicMirror,Resurrection,AnimateDead,Sacrifice,Mirth,Sorrow,Fortune,Misfortune,Slayer,Frenzy,Berserk,Hypnotize,Blind,Teleport,RemoveObstacle,Clone,SummonFireElemental,SummonEarthElemental,SummonWaterElemental,SummonAirElemental,SpellCount};
enum MagicSchool {AirSchool=1,FireSchool=2,WaterSchool=4,EarthSchool=8,AllSchools=15};
enum class SpellKind {Damage,Timed,Healing,Dispel,Unsupported,Resurrection,Sacrifice,Clone,Summon};
enum class SpellTarget {Single,Radius,Ring,Chain,Global};
enum class SpellFilter {Enemy,Ally,Any,Undead,NonUndead};
struct SpellDef {std::string name;int level,cost,skilledCost;bool enemy;int school,power;std::array<int,4> values;bool massAtExpert=false;int originalId=-1;SpellKind kind=SpellKind::Timed;SpellTarget target=SpellTarget::Single;SpellFilter filter=SpellFilter::Enemy;int radius=0;bool persistent=false,special=false;bool implemented=true,adventure=false;std::array<int,9> guildWeights{};};
extern const std::array<SpellDef,SpellCount> Spells;
struct LevelChoice {int level=0,primary=0;std::vector<int> skills;};
struct AdventureBonus {int sourceClass=-1,sourceId=0,morale=0,luck=0,movement=0,expiresDay=0;bool untilBattle=true;};
struct Hero {Point pos{4,20};int level=1,xp=0,attack=2,defense=2,power=1,knowledge=2,mana=20,movement=1560,maxMovement=1560;Army army{};std::array<int,SkillCount> skills{};std::array<bool,SpellCount> spells{};bool spellbook=false;std::vector<std::string> artifacts;std::vector<LevelChoice> levelChoices;bool tightFormation=false;int wellDay=0;std::string name="凯瑟琳";int originalId=-1;bool onBoat=false;Point lastEmbark{-1000,-1000};int adventureGoal=0;std::vector<int> artifactIds,wornArtifacts,visitedSites;std::map<std::string,int> equipmentSlots;std::array<bool,8> keys{};std::vector<AdventureBonus> adventureBonuses;};
enum BuildingId {TownHall,CityHall,Capitol,Fort,Citadel,Castle,Tavern,Marketplace,Blacksmith,MageGuild,MageGuild2,MageGuild3,Guardhouse,ArcherTower,GriffinTower,Barracks,Monastery,TrainingGrounds,Portal,UpGuardhouse,UpArcherTower,UpGriffinTower,UpBarracks,UpMonastery,UpTrainingGrounds,UpPortal,GriffinBastion,ResourceSilo,Shipyard,Special1,Special2,Special3,Special4,MageGuild4,MageGuild5,Horde2,VillageHall,BuildingCount};
struct BuildingDef {std::string name,description;Resources cost{};std::vector<std::vector<int>> requirements;int unit=-1;bool available=true,implemented=true;int icon=-1,upIcon=-1;};
struct TownLayer {std::string art;Point pos{};int z=0;std::vector<int> needs,excludes;std::string animation,mask,outline;bool originalMask=false;};
struct TownDef {std::array<BuildingDef,BuildingCount> buildings;std::vector<TownLayer> layers;Resources silo{};std::array<int,2> hordeTier{},hordeGrowth{};int maxMageGuild=5;std::string hallIcons,hallBackground;std::vector<std::vector<std::vector<int>>> hallSlots;};
enum class BuildState {Ready,Busy,Invalid,Unavailable,Owned,Unsupported,AlreadyBuiltToday,MissingRequirements,NoResources};
constexpr int FactionCount=9;
extern const std::array<TownDef,FactionCount> TownDefs;
extern const std::array<BuildingDef,BuildingCount>& Buildings;
struct Town {std::array<bool,BuildingCount> built{};std::array<int,7> pool{};Army garrison{};int builtDay=0,faction=0,stableWeek=-1;std::array<bool,4> lessons{};Point pos{4,20};std::array<std::vector<int>,5> guild{};int objectId=0,level=0;};
struct MapHero {Hero value;int owner=0,level=0;bool sleeping=false;Point patrolOrigin{};int patrolRadius=-1;std::string sprite="ah00_.def";nlohmann::json properties=nlohmann::json::object();};
struct GuildPlayer {int owner=0;std::array<int64_t,9> totals{};std::optional<Hero> bestHero;int bestCreature=-1;};
enum class StackOrigin {Army,Summoned,Clone};
struct CombatStack {int id=0,slot=0,unit=0,team=0,count=0,initialCount=0,hp=0,x=0,y=0,retaliations=0,shots=0;bool defending=false,waited=false;int preparedRound=-1,defenseReduction=0,defendingBonus=0;std::array<int,SpellCount> effects{},effectMastery{};StackOrigin origin=StackOrigin::Army;int permanentCount=0,cloneOf=-1,cloneTurns=0,deathOrder=0;bool corpse=false,acted=false,moraleUsed=false;};
struct Siege {int faction=-1,fort=0,attackerTeam=0,townId=0,buildings=0;std::array<int,8> hp{};std::vector<Point> spentMines;};
struct Battle {int targetId=0,round=1,spellRound=0,deferredVisitId=0;std::vector<CombatStack> units;std::vector<int> queue,waiting;bool waitPhase=false;std::vector<std::string> log;std::array<int,2> summonedType{-1,-1};int deathSequence=0;std::vector<int> moraleWaiting;bool moralePhase=false;std::array<int,4> defenderPrimary{};std::array<int,SkillCount> defenderSkills{};int defendingTown=0,returnHero=0;Siege siege;int defenderMana=0,defenderSpellRound=0;bool defenderSpellbook=false;std::array<bool,SpellCount> defenderSpells{};};
struct BattleReport {bool won=false;int xp=0;Army before{},after{},enemies{};};
struct TradeRate {int give=0,receive=0;bool operator==(const TradeRate&)const=default;};
enum class GameEventType {HeroMove,Visit,NewDay,Build,Recruit,Upgrade,Level,CombatStart,CombatMove,CombatAttack,CombatHit,CombatDeath,SpellCast,CombatEnd,CombatMorale,CombatLuck,CombatSpellEffect};
struct GameEvent {GameEventType type;int unit=-1,detail=0;Point pos{};int stackId=-1,targetId=-1;std::vector<Point> path{};std::optional<CombatStack> state{};bool scheduled=false;};
std::optional<TradeRate> marketRate(int sold,int bought,int marketplaces);
class Game {
public:
 int day=1;uint32_t seed=1999,randomState=1999;Resources resources{10000,20,20,5,5,5,5};Hero hero;Town town;std::array<bool,SpellCount> bannedSpells{};
 int mapWidth=MapW,mapHeight=MapH,mapLevels=1,mapLevel=0;
 std::string scenarioId,scenarioName;std::vector<MapDecoration> decorations;std::map<int,Town> settlements;int activeTownId=0;nlohmann::json scenarioRules;
 std::map<int,MapHero> heroes;int activeHeroId=0,actingOwner=0;std::array<Resources,8> playerResources{};std::array<std::vector<bool>,8> playerFog;std::vector<int> computerQueue;
 std::vector<Tile> tiles=std::vector<Tile>(MapW*MapH);std::vector<Object> objects;std::optional<Battle> battle;std::optional<BattleReport> report;int result=0;std::vector<std::string> log;
 Point grailPos{6,22};bool grailFound=false;
 static Game create(uint32_t seed=1999,int faction=0);
 bool importScenario(const std::filesystem::path&,std::string&,int randomFaction=0,int startingHero=-1,int difficulty=-1);bool imported()const{return !scenarioId.empty();}bool hasTown()const;void selectTown(int);void checkScenarioVictory();std::optional<std::string> importedVisit(int);void triggerMapEvents();void importedNewDay();void runScenarioEvents();bool questReady(const Object&)const;
 int currentOwner()const{return actingOwner;}void storeHero();bool selectHero(int);std::vector<int> ownedHeroes()const;std::string recruitHero();std::string buyBoat(int shipyard=0);bool coastalTown()const;std::string transferHeroArmy(int,int,int,int);void syncHeroObjects();void resetMapHeroes();void computerTurns();void mapNewDay();void manageKingdom(bool recruitMore=false);std::vector<Step> adventurePlan(int*targetId=nullptr);static double armyPower(const Army&);void recordStory(const std::string&,const std::string&);
 double random();void message(const std::string&);
 void emit(GameEvent);std::vector<GameEvent> takeEvents();
 Tile* tile(Point);const Tile* tile(Point)const;Object* objectAt(Point);const Object* objectAt(Point)const;Object* objectById(int);
 const Object* guardingMonster(Point,bool visibleOnly=false)const;bool canStep(Point,Point)const;
 bool passable(Point)const;int moveCost(Point from,Point to)const;std::vector<Step> path(Point)const;void reveal();std::string move(Point);std::string visit(int objectId);
 std::string mapObjectAsset(const Object&)const;std::vector<bool> movementMask()const;void clearObstacleFootprints(const std::vector<Point>&);
 std::optional<AdventureVisit> takeAdventureVisit();int obelisksVisited()const;int obelisksTotal()const;std::string dig();
 std::optional<std::string> visitAdventureSite(Object&);int adventureModifier(const Hero&,int stat)const;bool hasAdventureBonus(const Hero&,int sourceClass)const;void consumeAdventureBonuses();
 int seaMovementBonus(int owner)const;std::vector<GuildPlayer> thievesGuild()const;
 bool visiting()const{return (!imported()||activeHeroId!=0)&&hasTown()&&mapLevel==town.level&&hero.pos==town.pos;}
 static int mineYield(int resource);Resources mineIncome()const;Resources dailyIncome()const;
 int primarySkill(int)const;int artifactLandMovement()const;bool artifactSpellImmune(int)const;void equipPickedArtifact(int);int rollArtifact(int sourceClass);
 int income()const;int weeklyGrowth(int tier)const;int maxMana()const;int movementAllowance(int owner=-1)const;static int experienceForLevel(int);
 std::string endDay();std::string build(int);int recruitMax(int unit)const;std::string recruit(int unit,int count,bool garrison=false);std::string upgrade(bool garrison,int slot);
 std::string transfer(bool fromGarrison,int fromSlot,bool toGarrison,int toSlot,int amount);std::string dismiss(bool garrison,int slot);std::string chooseSkill(int);
 void gainXP(int);bool canAfford(const Resources&)const;std::array<int,7> recruitableUnits()const;
 const TownDef& townDef()const{return TownDefs[town.faction];}bool buildingRequirementsMet(int)const;void visitTownBuildings();
 BuildState constructionState(int)const;int hallBuilding(int row,int column)const;int hallIcon(int building)const;
 std::string learnMagic(int skill);
 int marketplaces(int marketObject=0)const;int tradeLimit(int sold,int bought,int marketObject=0)const;std::string trade(int sold,int bought,int lots,int marketObject=0);
 void generateGuild();int mageGuildLevel()const;int learnableSpellLevel()const;std::vector<int> guildSpells(int level)const;void learnTownSpells();std::string buySpellbook();
 void startBattle(int,int defendingTown=0,int returnHero=0,int heroDefendingTown=0);CombatStack* current();const CombatStack* current()const;CombatStack* combatAt(Point);const CombatStack* combatAt(Point)const;
 int armyMorale(int team=0)const;int armyLuck(int team=0)const;int morale(const CombatStack&)const;int luck(const CombatStack&)const;
 static int hexDistance(Point,Point);std::vector<Point> footprint(const CombatStack&,Point)const;int distance(const CombatStack&,const CombatStack&)const;
 int speed(const CombatStack&)const;int defense(const CombatStack&)const;std::vector<Point> battleRoute(const CombatStack&,Point)const;std::optional<int> battlePath(const CombatStack&,Point)const;bool canShoot(const CombatStack&)const;int damage(const CombatStack&,const CombatStack&,bool ranged,int traveled=0,double roll=.5,int sampledBase=-1,bool ignoreReductions=false,bool lucky=false)const;int spellCost(int spell,int team=-1)const;bool spellImmune(const CombatStack&,int spell)const;
 int spellMastery(int spell)const;bool spellMass(int spell)const;bool spellAutomatic(int spell)const;int spellEffect(const CombatStack&,int spell)const;int spellDamage(const CombatStack&,int spell,int raw)const;std::string spellDescription(int spell)const;std::vector<Point> spellArea(int spell,Point)const;std::vector<int> spellTargets(int spell,Point={})const;
 const CombatStack* spellTarget(int spell,Point)const;std::vector<int> sacrificeVictims(int targetId)const;
 std::optional<AttackPlan> attackPlan(const CombatStack&,const CombatStack&)const;
 std::string act(const std::string&,Point={});std::string cast(int spell,Point={},Point sacrifice={-1,-1});void ai();void legacyBattleAi();bool aiSpell();std::string retreat();
 bool siegeCellAllowed(const CombatStack&,Point)const;bool inMoat(const CombatStack&,Point)const;bool gateOpen()const;bool wallPenalty(const CombatStack&,const CombatStack&)const;int siegePartAt(Point)const;void setupSiege(int townId,int attackerTeam);void siegeTowers();void siegeCatapult();void moatHit(int);
 nlohmann::json toJson()const;bool save(const std::filesystem::path&,std::string&)const;bool load(const std::filesystem::path&,std::string&);bool validate()const;
private:
 int spellTeam_=0;
 int casterPower()const;int& casterMana();int& casterRound();
 std::optional<AdventureVisit> adventureVisit_;
 std::string applyVisit(int);
 int moraleBase(int team)const;int luckBase(int team)const;
 std::vector<GameEvent> events_;
 void prepareCurrent();void nextRound(bool prepare=true);void nextAction(bool prepare=true);void completeAction(bool allowMorale);bool finishBattle();void hurt(CombatStack&,int);void strike(int,int,bool,int=0,bool=false);void refreshQueue();int sampleBaseDamage(const CombatStack&);
 bool revivalEligible(const CombatStack&,int spell)const;std::optional<Point> reinforcementPosition(const CombatStack&,const CombatStack* near=nullptr)const;
 std::string castReinforcement(int spell,Point,Point);void finishCast(int active);void revive(CombatStack&,int hp,bool permanent,bool cleanse);void removeStack(CombatStack&,bool leaveCorpse);
};
int armyCount(const Army&);int armyStacks(const Army&);int baseTier(int unit);int baseUnit(int unit);
void heroDefaults(Hero&,int originalId);
std::string adventureSiteName(int sourceClass);
std::string artifactName(int);
Resources unitCost(int unit,int count=1);
struct FactionDef {std::string key,name,background,mapSprite;};
extern const std::array<FactionDef,FactionCount> Factions;
}
