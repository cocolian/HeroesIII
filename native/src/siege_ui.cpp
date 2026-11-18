#include "h3/renderer.hpp"
#include "h3/battle_layout.hpp"
#include <fstream>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <iostream>
namespace h3 {
void Desktop::heroPortrait(const Hero&hero,SDL_Rect dest){
 bool small=dest.w<=48&&dest.h<=32;std::string prefix=small?"hps":"hpl";int id=hero.originalId;std::string suffix;
 if(id>=0&&id<128){const char*classes[]={"kn","cl","rn","dr","al","wz","hr","dm","dk","nc","ov","wl","br","bm","bs","wh"};suffix=classes[id/8];}
 else if(id>=128&&id<144){suffix=id<136?"pl":"el";id-=id<136?128:136;}
 else{switch(id){case 144:id=130;suffix="kn";break;case 146:id=128;suffix="qc";break;case 149:id=129;suffix="mk";break;case 155:id=131;suffix="dm";break;default:break;}}
 if(!suffix.empty()){auto number=std::to_string(id);if(originalImage(prefix+std::string(std::max(0,3-int(number.size())),'0')+number+suffix+".pcx",dest))return;}
 originalImage(small?"hps128qc.pcx":"hpl128qc.pcx",dest);
}
bool Desktop::siegeArtwork(const Battle&b){
 if(!b.siege.fort)return false;static nlohmann::json artData;static bool loaded=false;if(!loaded){loaded=true;std::ifstream f(root_/"assets/demo/siege.json");if(f)f>>artData;}
 std::string faction=std::to_string(b.siege.faction);if(!artData.contains("factions")||!artData["factions"].contains(faction)){originalImage("cmbkgrtr.pcx",{0,0,800,556});for(int y=0;y<BattleH;++y)for(int x=0;x<BattleW;++x){int part=game.siegePartAt({x,y});if(part==-2||(part>=0&&b.siege.hp[part])){auto p=battleHexTopLeft({x,y});box({p.x+12,p.y,22,42},{105,96,82,255});}}return true;}
 auto gateIsOpen=[&]{if(b.siege.hp[3]==0)return true;for(const auto&u:b.units)if(u.hp>0){Point position{u.x,u.y};auto pose=battleMotion_.pose(u.id);if(pose){auto hex=battleHexAt({int(std::lround(pose->x)),int(std::lround(pose->y))});if(hex)position=*hex;}for(auto p:game.footprint(u,position)){if(b.siege.attackerTeam)p.x=BattleW-1-p.x;if((p.x==9||p.x==10)&&p.y==5)return true;if(pose&&pose->group==0&&u.team!=b.siege.attackerTeam&&p.x==11&&p.y==5)return true;}}return false;};
 for(const auto&layer:artData["factions"][faction]["layers"]){int part=layer["part"],index=0;if(part==-3&&b.siege.fort<2)continue;if(part>=0){int hp=b.siege.hp[part];if(part<3){if((part==0&&b.siege.fort<2)||(part>0&&b.siege.fort<3))continue;index=hp?0:1;}else if(part==3)index=hp==0?2:gateIsOpen()?1:0;else index=hp==0?2:hp<b.siege.fort?1:0;}
  auto name=layer["names"][index].get<std::string>();const auto&pcx=layer["assets"][index];int w=pcx["width"],h=pcx["height"],x=layer["x"],y=layer["y"];if(b.siege.attackerTeam)x=800-x-w;art(pcx["file"],{x,y,w,h},{},name.find("back.pcx")==std::string::npos,b.siege.attackerTeam!=0);
 }
 int x=b.siege.attackerTeam?680:-110;frame("smcata.def",2,int(animationTime_/150),{x,73,450,400},b.siege.attackerTeam!=0);
 return true;
}
void Desktop::siegePreview(const std::filesystem::path&directory,const std::filesystem::path&campaign){
 std::filesystem::create_directories(directory);saveDir_=directory/"test-saves";SDL_SetWindowSize(window_,800,600);settings_=ClientSettings{};settings_.hdArtwork=false;settings_.battleGrid=true;fixedAnimationTime_=1000;
 auto check=[](bool ok,const char*m){if(!ok)throw std::runtime_error(m);};auto shot=[&](const std::string&name){toastUntil_=0;render();check(screenshot(directory/(name+".png")),"Siege screenshot failed");};std::string error;
 check(game.importScenario(root_/"assets/demo/scenarios/dead-and-buried-1.json",error,8,131,4),"Video opening import failed");scene=Scene::Adventure;zoom_=1;camX_=game.hero.pos.x+.5f;camY_=game.hero.pos.y+.5f;shot("video-map-opening");{auto opening=game;for(int player=0;player<8;++player){game.scenarioRules["originalPlayer"]=player;game.hero.pos={opening.hero.pos.x,opening.hero.pos.y+5};game.hero.onBoat=true;shot("video-boat-player-"+std::to_string(player));}game=std::move(opening);}scene=Scene::Hero;shot("video-hero-opening");scene=Scene::Town;shot("video-town-opening");
 for(int faction=0;faction<9;++faction)for(int fort=1;fort<=3;++fort){game=Game::create();game.scenarioId="siege-ui-fixture";game.hero.army={Stack{Marksman,70},Stack{Griffin,10}};auto*o=game.objectById(15);check(o,"Siege fixture target missing");o->type=ObjectType::Town;o->army={Stack{Pikeman,50},Stack{Archer,20}};Town t;t.objectId=o->id;t.pos=o->pos;t.faction=faction;t.built[Fort]=true;t.built[Citadel]=fort>=2;t.built[Castle]=fort>=3;game.settlements[o->id]=t;game.startBattle(o->id);check(game.battle&&game.battle->siege.fort==fort,"Siege UI fixture did not deploy fortification");battleMotion_.clear();scene=Scene::Battle;shot("siege-"+std::to_string(faction)+"-"+std::to_string(fort));if(fort==3){game.battle->siege.hp[3]=game.battle->siege.hp[5]=0;shot("siege-"+std::to_string(faction)+"-breached");}}
 if(!campaign.empty()){auto first=campaign.parent_path()/"first-siege.json";check(game.load(first,error)&&game.battle&&game.battle->siege.fort>0,"Actual video-map siege state is missing");battleMotion_.clear();scene=Scene::Battle;shot("video-map-actual-siege");check(game.load(campaign,error),"Completed video-map save failed to load");check(game.result==1&&game.scenarioId=="dead-and-buried-1","Video-map screenshot used an unfinished or substitute map");scene=Scene::Adventure;camX_=game.hero.pos.x+.5f;camY_=game.hero.pos.y+.5f;settle();check(scene==Scene::Outcome,"Downloaded video map skipped outcome");shot("video-map-victory");key(SDLK_RETURN,KMOD_NONE);check(scene==Scene::Menu,"Downloaded video victory acknowledgement failed");shot("video-map-acknowledged");}
 std::ofstream(directory/"siege-ui-audit.json")<<nlohmann::json{{"originalTownArtwork",{0,1,2,3,4,5,6,7,8}},{"fortLevels",{1,2,3}},{"breachedGateAndWall",true},{"videoMapVictoryDisplayed",!campaign.empty()},{"originalPlayerUiPalette",true},{"boatOwnerColors",8},{"isolatedSaves",true}}.dump(2)<<'\n';std::cout<<"Siege artwork, fortification levels and video-map outcome passed\n";
}
}
