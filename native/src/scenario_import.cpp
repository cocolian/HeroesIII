#include "h3/game.hpp"
#include "h3/localization.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <set>

namespace h3 {
namespace {
using Json=nlohmann::json;
constexpr const char*Players[]={"red","blue","tan","green","orange","purple","teal","pink"};
constexpr int ResourcesByOriginal[]={Wood,Mercury,Ore,Sulfur,Crystal,Gems,Gold};
constexpr int SkillsByOriginal[]={-1,Archery,Logistics,-1,-1,-1,Leadership,Wisdom,Mysticism,LuckSkill,-1,-1,-1,-1,FireMagic,AirMagic,WaterMagic,EarthMagic,-1,-1,-1,-1,Offense,Armorer,Intelligence,-1,-1,-1};
int unitId(int original){for(int i=0;i<UnitCount;++i)if(Units[i].originalId==original)return i;return -1;}
Army army(const Json&j){Army result{};int slot=0;for(const auto&s:j){if(slot==ArmySlots)break;int u=unitId(s.value("type",-1)),count=s.value("quantity",0);if(u>=0&&count>0)result[slot]={u,std::min(MaxArmyStack,count)};++slot;}return result;}
int factionOf(int hero){return hero>=0&&hero<144?hero/16:0;}
Point coordinates(const Json&j){return {j.at("x"),j.at("y")};}
Point visitPoint(const Json&t,Point anchor){for(int row=5;row>=0;--row)for(int col=7;col>=0;--col)if(t["actionability"][row].get<int>()&(1<<col))return {anchor.x+col-7,anchor.y+row-5};return anchor;}
void primary(Hero&h,const Json&j){h.attack=j.value("attack",h.attack);h.defense=j.value("defense",h.defense);h.power=j.value("spell_power",h.power);h.knowledge=j.value("knowledge",h.knowledge);}
void skills(Hero&h,const Json&j){for(const auto&s:j){int id=s.value("type",-1);if(id>=0&&id<28&&SkillsByOriginal[id]>=0)h.skills[SkillsByOriginal[id]]=std::clamp(s.value("level",0),0,3);}}
void townBuildings(Town&t,const Json&p){
 t.built.fill(false);t.built[VillageHall]=true;
 if(!p.contains("buildings")){t.built[Fort]=p.value("has_fort",0)!=0;t.built[Tavern]=t.built[Guardhouse]=true;return;}
 const auto&b=p["buildings"]["is_built"];
 const std::pair<const char*,int> pairs[]={{"town_hall",TownHall},{"city_hall",CityHall},{"capitol",Capitol},{"fort",Fort},{"citadel",Citadel},{"castle",Castle},{"tavern",Tavern},{"marketplace",Marketplace},{"blacksmith",Blacksmith},{"resource_silo",ResourceSilo},{"shipyard",Shipyard},{"mage_guild_1",MageGuild},{"mage_guild_2",MageGuild2},{"mage_guild_3",MageGuild3},{"mage_guild_4",MageGuild4},{"mage_guild_5",MageGuild5},{"special_building_1",Special1},{"special_building_2",Special2},{"special_building_3",Special3},{"special_building_4",Special4}};
 for(auto[key,id]:pairs)t.built[id]=b.value(key,false);
 for(int tier=0;tier<7;++tier){t.built[Guardhouse+tier]=b.value("dwelling_"+std::to_string(tier+1),false);t.built[UpGuardhouse+tier]=b.value("dwelling_"+std::to_string(tier+1)+"u",false);}
}
std::string objectName(int cls){switch(cls){case 8:return "船只";case 9:return "边境守卫";case 10:return "钥匙大师帐篷";case 26:return "地图事件";case 33:return "要塞守军";case 34:case 70:return "英雄";case 43:case 44:case 45:return "传送门";case 49:return "魔法井";case 53:return "矿山";case 54:return "野外部队";case 57:return "方尖碑";case 79:return "资源";case 83:return "先知小屋";case 88:case 89:case 90:return "魔法神殿";case 91:return "路标";case 98:case 77:return "城镇";case 99:return "贸易站";case 101:return "宝箱";case 103:return "地下通道";case 109:return "水车";case 112:return "风车";case 215:return "任务守卫";default:return adventureSiteName(cls);}}
}
bool Game::hasTown()const{if(!imported())return true;return std::any_of(objects.begin(),objects.end(),[&](const auto&o){return o.type==ObjectType::Town&&o.owner==currentOwner();});}
void Game::selectTown(int id){
 auto it=settlements.find(id);if(it==settlements.end())return;
 auto*o=objectById(id);if(!o||o->owner!=currentOwner())return;
 if(activeTownId&&settlements.contains(activeTownId))settlements[activeTownId]=town;
 activeTownId=id;town=it->second;
}
bool Game::importScenario(const std::filesystem::path&path,std::string&error,int randomFaction,int startingHero,int difficulty){try{
 if(std::filesystem::file_size(path)>32*1024*1024)throw std::runtime_error("地图数据过大");
 std::ifstream stream(path);Json data;stream>>data;
 if(data.at("schema")!=1)throw std::runtime_error("地图格式不支持");
 localizeMapData(data,data.at("id").get<std::string>());
 Game g;if(difficulty==4)g.resources={};g.scenarioId=data.at("id");g.scenarioName=data.at("title");g.mapWidth=data.at("width");g.mapHeight=data.at("height");g.mapLevels=data.at("levels");
 if(g.mapWidth<1||g.mapWidth>144||g.mapHeight<1||g.mapHeight>144||g.mapLevels<1||g.mapLevels>2||data["tiles"].size()!=size_t(g.mapWidth*g.mapHeight*g.mapLevels)||data["objects"].size()>60000)throw std::runtime_error("地图尺寸或物件数量无效");
 g.tiles.resize(g.mapWidth*g.mapHeight*g.mapLevels);g.objects.clear();g.hero.army={};
 const Terrain terrains[]={Terrain::Dirt,Terrain::Sand,Terrain::Grass,Terrain::Snow,Terrain::Swamp,Terrain::Rough,Terrain::Subterranean,Terrain::Lava,Terrain::Water,Terrain::Rock};
 for(size_t i=0;i<g.tiles.size();++i){const auto&r=data["tiles"][i];int type=r[0];if(type<0||type>9)throw std::runtime_error("地图地形无效");auto&t=g.tiles[i];t.terrain=terrains[type];t.originalView=r[1];t.variant=t.originalView;t.riverType=r[2];t.riverView=r[3];t.roadType=r[4];t.roadView=r[5];t.originalFlags=r[6];t.road=t.roadType!=0;t.river=t.riverType!=0;}
 int player=-1;for(int i=0;i<8;++i)if(data["players"][Players[i]].value("can_be_human",0)){player=i;break;}if(player<0)throw std::runtime_error("地图没有可选的人类玩家");
 g.scenarioRules={{"difficulty",difficulty},{"originalPlayer",player},{"additional",data["additionalInfo"]},{"timedEvents",data["timedEvents"]},{"spellNames",data["spellNames"]},{"sourceSha256",data["sourceSha256"]},{"defeated",Json::array()},{"grailExists",false}};
 for(int i=0;i<SpellCount;++i){int id=Spells[i].originalId;if(id>=0&&id<int(data["spellNames"].size()))g.bannedSpells[i]=data["additionalInfo"]["disabled_spells"].value(data["spellNames"][id].get<std::string>(),false);}
 const auto&templates=data.at("templates");int chosenHero=-1;Json heroProperties;int id=0;
 for(const auto&source:data["objects"]){
  ++id;int tid=source.at("template_idx");if(tid<0||tid>=int(templates.size()))throw std::runtime_error("地图物件索引无效");const auto&t=templates[tid];const auto&c=source["coordinates"];Point anchor=coordinates(c);int level=c.at("z");
  if(anchor.x<0||anchor.x>255||anchor.y<0||anchor.y>255||level<0||level>=g.mapLevels)throw std::runtime_error("地图物件坐标无效");
  int cls=t["object_class"],sub=t["object_subclass"];std::string asset=t["def"];std::transform(asset.begin(),asset.end(),asset.begin(),[](unsigned char c){return char(std::tolower(c));});
  const auto&p=source.value("properties",Json::object());Point visit=visitPoint(t,anchor);bool actionable=false;
  for(int row=0;row<6;++row)for(int col=0;col<8;++col){int x=anchor.x+col-7,y=anchor.y+row-5;bool action=t["actionability"][row].get<int>()&(1<<col);actionable|=action;if(x<0||y<0||x>=g.mapWidth||y>=g.mapHeight)continue;if(!(t["passability"][row].get<int>()&(1<<col))&&!action)g.tiles[(level*g.mapHeight+y)*g.mapWidth+x].blocked=true;}
  if(cls==36){g.grailPos=anchor;g.scenarioRules["grailExists"]=true;g.scenarioRules["grailLevel"]=level;continue;}
  int originalOwner=p.value("owner",-1);if(originalOwner==255)originalOwner=-1;
  if(visit.x<0||visit.x>=g.mapWidth||visit.y<0||visit.y>=g.mapHeight){g.decorations.push_back({anchor,level,asset,t.value("is_ground",0)!=0});continue;}
  if((cls==34||cls==70)&&originalOwner==player&&chosenHero<0){chosenHero=id;g.hero.pos=visit;g.mapLevel=level;heroProperties=p;continue;}
  if(!actionable&&cls!=26){g.decorations.push_back({anchor,level,asset,t.value("is_ground",0)!=0});continue;}
  Object o;o.ground=t.value("is_ground",0)!=0;o.id=id;o.pos=visit;o.anchor=anchor;o.level=level;o.sprite=asset;o.sourceClass=cls;o.sourceSubtype=sub;o.properties=p;o.name=p.value("name",objectName(cls));o.owner=originalOwner<0?-1:originalOwner==player?0:originalOwner<player?originalOwner+1:originalOwner;o.type=ObjectType::Site;
  if(cls==98||cls==77){
   o.type=ObjectType::Town;Town town;town.faction=cls==77?std::clamp(randomFaction,0,8):std::clamp(sub,0,8);town.objectId=id;town.level=level;town.pos=visit;townBuildings(town,p);if(p.contains("garrison"))town.garrison=army(p["garrison"]);o.army=town.garrison;
   for(int tier=0;tier<7;++tier)if(town.built[Guardhouse+tier]||town.built[UpGuardhouse+tier])town.pool[tier]=Units[town.faction*14+tier*2].growth*(town.built[Castle]?2:1);
   g.settlements[id]=town;if(cls==77){Object image=o;image.sprite.clear();image.owner=0;g.town=town;o.sprite=g.mapObjectAsset(image);}
  }else if(cls==53||cls==220){o.type=ObjectType::Mine;o.resource=ResourcesByOriginal[std::clamp(sub,0,6)];o.name=ResourceNames[o.resource]+"矿";}
  else if(cls==79||cls==76){o.type=ObjectType::Resource;if(cls==76)o.sprite.clear();o.resource=ResourcesByOriginal[cls==76?int(g.random()*7):std::clamp(sub,0,6)];int n=p.value("quantity",0);o.amount=n?n*(o.resource==Gold?100:1):o.resource==Gold?500+100*int(g.random()*6):5+int(g.random()*6);}
  else if(cls==101||cls==82){o.type=ObjectType::Treasure;o.amount=1000+500*int(g.random()*3);}
  else if(cls==54||(cls>=71&&cls<=75)||(cls>=162&&cls<=164)){
   o.type=ObjectType::Monster;int u=unitId(sub);if(cls!=54){int tier=cls==71?int(g.random()*7):std::clamp(cls>=162?cls-157:cls-72,0,6);u=int(g.random()*9)*14+tier*2;o.sprite.clear();}if(u<0)throw std::runtime_error("地图包含未实现的生物："+std::to_string(sub));int count=p.value("quantity",0);o.army[0]={u,count>0?std::min(count,MaxArmyStack):std::max(1,Units[u].growth*2)};o.name=Units[u].name;
  }else if(cls==34||cls==70||cls==33||cls==219){o.type=(cls==33||cls==219||originalOwner==player)?ObjectType::Site:ObjectType::EnemyHero;if(p.contains("creatures"))o.army=army(p["creatures"]);if(!armyCount(o.army)&&o.type==ObjectType::EnemyHero)o.army[0]={factionOf(p.value("type",0))*14,20};}
  else if(cls==49)o.type=ObjectType::Well;
  else if(cls==57)o.type=ObjectType::Obelisk;
  else if(cls==88||cls==89||cls==90){o.type=ObjectType::Shrine;int spell=p.value("spell",-1);for(int i=0;i<SpellCount;++i)if(Spells[i].originalId==spell)o.amount=i+1;}
  else if(cls==99)o.type=ObjectType::TradingPost;
  else if(cls==112)o.type=ObjectType::Windmill;
  else if(cls==109)o.type=ObjectType::WaterWheel;
  else if(cls==8)o.type=ObjectType::Boat;
  else if(cls==26)o.type=ObjectType::MapEvent;
  else if(cls==9||cls==212||cls==215)o.type=ObjectType::Gate;
  else if(cls==43||cls==44||cls==45||cls==103||cls==111)o.type=ObjectType::Portal;
  else if(cls==5||(cls>=65&&cls<=69)||cls==93){o.type=ObjectType::Artifact;if(cls>=65&&cls<=69){o.sourceSubtype=g.rollArtifact(cls);auto number=std::to_string(o.sourceSubtype);o.sprite="ava"+std::string(4-number.size(),'0')+number+".def";}o.name=artifactName(o.sourceSubtype);}
  if(p.contains("guardians")&&p["guardians"].contains("creatures"))o.army=army(p["guardians"]["creatures"]);
  g.objects.push_back(std::move(o));
 }
 int home=0;for(const auto&o:g.objects)if(o.type==ObjectType::Town&&o.owner==0){home=o.id;break;}
 if(home){g.activeTownId=home;g.town=g.settlements.at(home);}else{g.town=Town{};g.town.faction=factionOf(heroProperties.value("type",0));g.town.pos=g.hero.pos;g.town.level=g.mapLevel;}
 for(auto&[id,town]:g.settlements){auto previous=g.town;g.town=town;g.generateGuild();town=g.town;g.town=previous;}if(home)g.town=g.settlements.at(home);
 if(chosenHero<0){if(!home)throw std::runtime_error("地图没有可控制的起始英雄或城镇");g.hero.pos=g.town.pos;g.mapLevel=g.town.level;g.hero.army={Stack{g.town.faction*14,20},Stack{g.town.faction*14+2,6}};if(difficulty<0)g.resources[Gold]-=2500;heroDefaults(g.hero,startingHero>=0?startingHero:g.town.faction*16);if(startingHero==131)g.hero.army={Stack{112,15+int(g.random()*11)},Stack{116,3+int(g.random()*3)}};g.scenarioRules["generatedStartingHero"]=true;}
 else{
  g.hero.originalId=heroProperties.value("type",0);heroDefaults(g.hero,g.hero.originalId);g.hero.name=heroProperties.value("name",g.hero.name);g.hero.army=heroProperties.contains("creatures")?army(heroProperties["creatures"]):Army{Stack{factionOf(g.hero.originalId)*14,20},Stack{factionOf(g.hero.originalId)*14+2,6}};
  if(!armyCount(g.hero.army))throw std::runtime_error("起始英雄没有可支持的部队");g.hero.xp=std::clamp(heroProperties.value("experience",0),0,1000000000);while(g.hero.level<70&&g.hero.xp>=experienceForLevel(g.hero.level+1))++g.hero.level;
  if(heroProperties.contains("primary_skills"))primary(g.hero,heroProperties["primary_skills"]);if(heroProperties.contains("secondary_skills"))skills(g.hero,heroProperties["secondary_skills"]);
  if(heroProperties.contains("spells"))for(int i=0;i<SpellCount;++i){int n=Spells[i].originalId;if(n>=0&&n<int(data["spellNames"].size()))g.hero.spells[i]=heroProperties["spells"].value(data["spellNames"][n].get<std::string>(),false);}
  if(heroProperties.contains("artifacts")){const auto&a=heroProperties["artifacts"];g.hero.equipmentSlots=a["equipped"].get<std::map<std::string,int>>();for(const auto&[slot,id]:a["equipped"].items()){int item=id;if(slot=="spellbook"){g.hero.spellbook=item>=0;continue;}if(item>=0){g.hero.wornArtifacts.push_back(item);g.hero.artifactIds.push_back(item);g.hero.artifacts.push_back(artifactName(item));}}for(int item:a["backpack"])if(item>=0){g.hero.artifactIds.push_back(item);g.hero.artifacts.push_back(artifactName(item));}}
 }
 g.hero.onBoat=g.tile(g.hero.pos)->terrain==Terrain::Water;g.hero.mana=g.maxMana();g.hero.maxMovement=g.hero.movement=g.movementAllowance();
 // Some editor maps place the starting hero over a town/decoration mask.
 // Preserve that coordinate; movement still requires a genuinely open destination.
 for(auto&o:g.objects)if(o.type==ObjectType::Portal&&o.sourceClass!=44){
  const Object*target=nullptr;int best=std::numeric_limits<int>::max();
  for(const auto&candidate:g.objects){if(candidate.id==o.id||candidate.type!=ObjectType::Portal)continue;bool match=o.sourceClass==103?candidate.sourceClass==103&&candidate.level!=o.level:o.sourceClass==43?candidate.sourceClass==44&&candidate.sourceSubtype==o.sourceSubtype:candidate.sourceClass==o.sourceClass&&candidate.sourceSubtype==o.sourceSubtype;if(!match)continue;int dx=o.pos.x-candidate.pos.x,dy=o.pos.y-candidate.pos.y,d=dx*dx+dy*dy;if(d<best){target=&candidate;best=d;}}
  if(target)o.properties["destination"]={{"x",target->pos.x},{"y",target->pos.y},{"level",target->level}};
 }
 for(const auto&o:g.objects)if(o.type==ObjectType::Town&&o.owner==0){int previous=g.mapLevel;g.mapLevel=o.level;auto p=g.hero.pos;g.hero.pos=o.pos;g.reveal();g.hero.pos=p;g.mapLevel=previous;}
 g.scenarioRules["hadEnemy"]=std::any_of(g.objects.begin(),g.objects.end(),[](const auto&o){return o.owner>0&&(o.type==ObjectType::Town||o.type==ObjectType::EnemyHero);});
 const auto&victory=data["additionalInfo"]["victory_condition"];if(victory.contains("details")&&victory["details"].contains("coordinates")){const auto&c=victory["details"]["coordinates"];for(const auto&o:g.objects)if(o.anchor==coordinates(c)&&o.level==c["z"]&&((victory.value("type",255)==5&&o.type==ObjectType::EnemyHero)||(victory.value("type",255)==7&&o.type==ObjectType::Monster)))g.scenarioRules["victoryTarget"]=o.id;}
 g.scenarioRules["initialHeroProperties"]=heroProperties;g.scenarioRules["initialHeroSourceId"]=chosenHero;
 const auto&loss=data["additionalInfo"]["loss_condition"];if(loss.contains("details")&&loss["details"].contains("coordinates")){const auto&c=loss["details"]["coordinates"];int index=0;for(const auto&source:data["objects"]){++index;int cls=templates[source["template_idx"].get<int>()]["object_class"].get<int>();if(source["coordinates"]==c&&((loss.value("type",255)==0&&(cls==98||cls==77))||(loss.value("type",255)==1&&(cls==34||cls==70)))){g.scenarioRules["lossTarget"]=index;g.scenarioRules["lossTargetWasOwned"]=source.value("properties",Json::object()).value("owner",-1)==player;}}}g.resetMapHeroes();g.reveal();g.message("已载入原图「"+g.scenarioName+"」：可探索、拾取、占矿和战斗；部分剧情与英雄能力仍待实现。");
 g.importedNewDay();
 if(!g.validate())throw std::runtime_error("地图开局数据校验失败");*this=std::move(g);return true;
 }catch(const std::exception&e){error=e.what();return false;}}
}
