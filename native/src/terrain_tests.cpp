#include "h3/renderer.hpp"
#include <fstream>
#include <stdexcept>

namespace h3 {
void Desktop::terrainRenderTests(const std::filesystem::path& directory){
 auto check=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
 int w=0,h=0;SDL_GetWindowSize(window_,&w,&h);auto previous=settings_;SDL_SetWindowSize(window_,1280,800);
 game=Game::create();settings_.hdArtwork=false;scene=Scene::Adventure;zoom_=1;fixedAnimationTime_=0;mouseX_=mouseY_=-1;toastUntil_=0;
 route_.clear();destination_.reset();marching_=false;camX_=18;camY_=14;
 int patterns=0;
 for(const auto&[name,table]:mapArt_["terrainFrames"].items())for(const auto&choices:table){
  check(!choices.empty(),"Terrain neighbourhood has no original frame");++patterns;
  for(const auto&f:choices)check(f[0].get<int>()>=0&&f[0].get<int>()<frameCount(name),"Terrain transition references a nonexistent original frame");
 }
 for(auto&t:game.tiles){t.terrain=Terrain::Grass;t.seen=true;t.decor=Decor::None;t.road=t.river=false;t.variant=0;}
 auto native=terrainVisual({14,14});check(native.frame>=49&&native.frame<=56,"Grass interior uses a border tile");
 for(int y=0;y<MapH;y++)for(int x=0;x<9;x++)game.tile({x,y})->terrain=Terrain::Sand;
 auto grass=terrainVisual({9,14});check(grass.frame>=24&&grass.frame<=27&&!grass.flipX,"Grass/sand west border lost the original transition bank");
 for(int y=0;y<MapH;y++)for(int x=9;x<MapW;x++)game.tile({x,y})->terrain=Terrain::Dirt;
 auto dirt=terrainVisual({9,14});check(dirt.frame>=4&&dirt.frame<=7&&!dirt.flipX,"Dirt/sand west border lost its transition");
 for(int y=0;y<MapH;y++)for(int x=9;x<MapW;x++)game.tile({x,y})->terrain=Terrain::Water;
 auto coast=terrainVisual({9,14});check(coast.frame>=4&&coast.frame<=7&&!coast.flipX,"West shoreline lost its sandy coast");
 // Independent original-map cases, withheld from calibration. Coordinates
 // refer to Oscar Sedell's Seven Lakes, surface layer. Texture alternatives
 // vary; the transition family and geometrically active flips must agree.
 const Point offsets[]={{-1,-1},{0,-1},{1,-1},{1,0},{1,1},{0,1},{-1,1},{-1,0}};
 auto corner=[&](Terrain center,Terrain bank,std::array<int,8> neighbours,int first,int last,bool fx,bool fy){
  for(auto&t:game.tiles)t.terrain=center;
  for(int i=0;i<8;++i)game.tile({14+offsets[i].x,14+offsets[i].y})->terrain=neighbours[i]?bank:center;
  for(int v=0;v<32;++v){game.tile({14,14})->variant=v;auto f=terrainVisual({14,14});
   check(f.frame>=first&&f.frame<=last&&f.flipX==fx&&f.flipY==fy,"Original-map corner geometry/flip regression");}
 };
 corner(Terrain::Water,Terrain::Grass,{0,0,1,0,0,0,0,0},18,19,false,true); // 107,116
 corner(Terrain::Water,Terrain::Grass,{0,0,0,0,1,0,0,0},18,19,false,false); // 111,119
 corner(Terrain::Grass,Terrain::Dirt,{0,0,0,0,1,0,0,0},18,19,false,false); // 83,127
 corner(Terrain::Dirt,Terrain::Sand,{0,0,0,0,1,0,0,0},18,19,false,false); // 110,114
 corner(Terrain::Dirt,Terrain::Sand,{1,1,1,0,0,0,0,1},16,17,false,false); // 112,120
 corner(Terrain::Grass,Terrain::Sand,{1,1,1,1,0,0,0,0},36,37,true,false); // 102,117
 game=Game::create();for(auto&t:game.tiles){t.seen=true;t.decor=Decor::None;t.road=t.river=false;t.terrain=Terrain::Sand;}
 game.objects.clear();game.hero.pos={18,14};
 // A peninsula, concave bay, straight edges, inner and outer corners.
 for(int y=3;y<=23;y++)for(int x=3;x<=27;x++)if(!(x>19&&y<11))game.tile({x,y})->terrain=x<15?Terrain::Dirt:Terrain::Grass;
 for(int y=14;y<=20;y++)for(int x=8;x<=12;x++)game.tile({x,y})->terrain=Terrain::Sand;
 for(int y=0;y<MapH;y++)for(int x=29;x<MapW;x++)game.tile({x,y})->terrain=Terrain::Water;
 for(int x=4;x<=26;x++)game.tile({x,12})->road=true;
 for(int y=13;y<=25;y++)game.tile({24,y})->river=true;
 auto state=game.toJson();nlohmann::json zooms=nlohmann::json::array();
 for(int percent:{65,100,180}){
  zoom_=percent/100.f;render();check(screenshot(directory/("terrain-transitions-"+std::to_string(percent)+".png")),"Terrain transition screenshot failed");
  check(game.toJson()==state,"Terrain rendering modified the map or consumed RNG");zooms.push_back(percent);
 }
 game=Game::create();for(auto&t:game.tiles)t.seen=true;zoom_=1;camX_=9;camY_=20;render();
 check(screenshot(directory/"terrain-restored-map.png"),"Restored adventure map screenshot failed");
 settings_.hdArtwork=true;render();check(screenshot(directory/"terrain-restored-map-hd.png"),"HD transition screenshot failed");
 std::ofstream(directory/"terrain-audit.json")<<nlohmann::json{{"originalDEFFrames",true},{"neighbourhoodsValidated",patterns},{"withheldOriginalCornerCases",6},{"variantsPerCorner",32},{"grassDirtAndSandTransitions",true},{"shorelineTransitions",true},{"innerAndOuterCorners",true},{"roadsAndRiversAboveTerrain",true},{"zooms",zooms},{"gameStateAndRngUnchanged",true},{"arbitraryOriginalMapPixelParityVerified",false},{"selection","stored H3M geometry calibration with original-pixel fallback; full scenarios not imported"}}.dump(2)<<'\n';
 settings_=previous;game=Game::create();scene=Scene::Town;zoom_=1;fixedAnimationTime_=0;SDL_SetWindowSize(window_,w,h);
}
}
