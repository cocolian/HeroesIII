#include "h3/renderer.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>

namespace h3 {
std::string Desktop::ownerName(int owner)const{if(owner<0)return "neutral";if(game.imported()&&game.scenarioRules.is_object()&&game.scenarioRules.contains("originalPlayer")){const char*colors[]={"red","blue","tan","green","orange","purple","teal","pink"};int human=game.scenarioRules["originalPlayer"],original=owner==0?human:owner<=human?owner-1:owner;if(original>=0&&original<8)return colors[original];}return ownerArt_["ownerColors"].value(std::to_string(owner),std::string("neutral"));}
SDL_Color Desktop::ownerColor(int owner)const{
 auto color=ownerName(owner);const auto&rgb=ownerArt_["colors"][color]["rgb"];
 return {rgb[0],rgb[1],rgb[2],255};
}
std::string Desktop::ownerHeroFlag(int owner)const{
 auto color=ownerName(owner);return ownerArt_["colors"][color].value("heroFlag",std::string{});
}
std::string Desktop::ownerPage(const std::string&asset,int page,int owner)const{
 auto name=asset;for(auto&c:name)c=char(std::tolower(static_cast<unsigned char>(c)));
 auto it=ownerArt_["assets"].find(name);if(it==ownerArt_["assets"].end())return {};
 auto color=ownerName(owner);const auto&pages=(*it)["pages"][color];return page>=0&&page<int(pages.size())?pages[page].get<std::string>():std::string{};
}
void Desktop::ownershipRenderTests(const std::filesystem::path&directory){
 auto check=[](bool ok,const char*message){if(!ok)throw std::runtime_error(message);};
 int oldW=0,oldH=0;SDL_GetWindowSize(window_,&oldW,&oldH);SDL_SetWindowSize(window_,1280,800);auto previous=settings_;
 game=Game::create();for(auto&t:game.tiles){t.seen=true;t.decor=Decor::None;t.road=t.river=false;t.terrain=Terrain::Grass;}
 scene=Scene::Adventure;settings_.hdArtwork=false;fixedAnimationTime_=0;zoom_=1;camX_=18;camY_=14;mouseX_=mouseY_=-1;focus_=-1;toastUntil_=0;marching_=false;route_.clear();destination_.reset();
 game.objects.clear();auto add=[&](ObjectType type,Point point,int owner,int resource=Gold){Object o;o.id=int(game.objects.size())+1;o.type=type;o.pos=point;o.owner=owner;o.resource=resource;o.name="归属颜色验证";game.objects.push_back(o);return o.id;};
 int friendly=add(ObjectType::Town,{9,11},0);add(ObjectType::Town,{27,10},1);game.town.pos={9,11};game.hero.pos={12,14};
 std::vector<int>mines;for(int i=0;i<7;i++)mines.push_back(add(ObjectType::Mine,{16+i%3*5,12+i/3*6},0,i));
 auto shot=[&](const char*name){toastUntil_=0;render();check(screenshot(directory/(std::string(name)+".png")),"Owner color screenshot failed");};
 auto pixel=[&](Point p){SDL_Rect area{int((p.x+.5)*pixelScale_),int((p.y+.5)*pixelScale_),1,1};Uint8 bytes[4]{};check(SDL_SetRenderTarget(renderer_,canvas_.texture)==0,"Owner canvas unavailable");bool ok=SDL_RenderReadPixels(renderer_,&area,SDL_PIXELFORMAT_RGBA32,bytes,4)==0;SDL_SetRenderTarget(renderer_,nullptr);check(ok,"Owner pixel readback failed");return SDL_Color{bytes[0],bytes[1],bytes[2],bytes[3]};};
 auto same=[](SDL_Color a,SDL_Color b){return a.r==b.r&&a.g==b.g&&a.b==b.b;};
 auto flagPoint=[&](const Object&o){auto name=mapObjectAsset(o);auto canvas=mapSpriteCanvas(name,o.pos);const auto&f=demo_["def"][name]["groups"]["0"][0];const auto&p=ownerArt_["assets"][name]["samples"][0]["point"];return Point{canvas.x+int((p[0].get<int>()+.5)*canvas.w/f["canvas"][0].get<int>()),canvas.y+int((p[1].get<int>()+.5)*canvas.h/f["canvas"][1].get<int>())};};
 auto flagColor=[&](const Object&o){auto p=flagPoint(o);auto expected=ownerColor(o.owner);for(int y=-2;y<=2;y++)for(int x=-2;x<=2;x++)if(same(pixel({p.x+x,p.y+y}),expected))return true;return false;};
 auto checkAll=[&]{for(const auto&o:game.objects)check(flagColor(o),"Owner flag color disagrees with object ownership");};
 auto state=game.toJson();shot("ownership-all-blue-classic");checkAll();check(ownerHeroFlag(0)=="af01.def"&&ownerHeroFlag(1)=="af00.def","Game owner IDs were confused with classic palette IDs");check(game.toJson()==state,"Drawing owner colors changed game state or RNG");
 nlohmann::json factions=nlohmann::json::array();for(int faction=0;faction<FactionCount;faction++){game.town.faction=faction;render();check(flagColor(*game.objectById(friendly)),"A faction town did not use its owner's flag");factions.push_back(Factions[faction].name);}game.town.faction=0;
 auto*mine=game.objectById(mines[1]);int mineId=mine->id;mine->owner=-1;shot("ownership-neutral-mine");check(flagColor(*mine),"Neutral mine used a player flag");
 game.hero.pos=mine->pos;check(!game.visit(mineId).empty()&&game.objectById(mineId)->owner==0,"Mine capture fixture failed");game.hero.pos={12,14};shot("ownership-mine-captured");check(flagColor(*game.objectById(mineId)),"Capturing a cached neutral mine did not immediately make its flag blue");
 auto mini=adventureMinimapRect();int scale=mini.w/MapW;for(const auto&o:game.objects)check(same(pixel({mini.x+o.pos.x*scale+scale/2,mini.y+o.pos.y*scale+scale/2}),ownerColor(o.owner)),"Minimap uses a different ownership color");
 state=game.toJson();for(int percent:{65,100,180}){zoom_=percent/100.f;camX_=18;camY_=14;render();
  // At large zoom, use a centered sample instead of offscreen fixtures.
  auto*o=game.objectById(friendly);camX_=o->pos.x+.5f;camY_=o->pos.y+.5f;render();check(flagColor(*o),"Map zoom changed the owner color");check(screenshot(directory/("ownership-zoom-"+std::to_string(percent)+".png")),"Owner zoom screenshot failed");
 }
 zoom_=1;camX_=18;camY_=14;settings_.hdArtwork=true;shot("ownership-all-blue-hd");checkAll();check(game.toJson()==state,"Color changes or zoom mutated ownership/RNG");
 std::ofstream(directory/"ownership-audit.json")<<nlohmann::json{{"friendlyColor","blue"},{"enemyColor","red"},{"neutralColor","gray"},{"heroFlag","af01.def"},{"classicBluePaletteIndex",1},{"gameFriendlyOwnerId",0},{"townFactions",factions},{"mineResourceTypes",7},{"neutralMineCaptureRefresh",true},{"sameAssetDifferentOwners",true},{"minimapMatches",true},{"zoomChecks",{65,100,180}},{"hdAndClassic",true},{"renderingStateAndRngUnchanged",true},{"originalFlagMarker",5}}.dump(2)<<'\n';
 game=Game::create();scene=Scene::Town;settings_=previous;fixedAnimationTime_=0;zoom_=1;heroDirection_={1,0};SDL_SetWindowSize(window_,oldW,oldH);
}
}
