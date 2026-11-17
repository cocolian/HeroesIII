#include "h3/renderer.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <tuple>

namespace h3 {
SDL_Rect Desktop::mapSpriteCanvas(const std::string&name,Point position,bool visitable)const{
 return mapSpriteCanvasAt(name,position,visitable,adventureMapRect(),mapCamera());
}
SDL_Rect Desktop::mapSpriteCanvasAt(const std::string&name,Point position,bool visitable,SDL_Rect map,const std::array<float,2>&camera)const{
 auto found=mapSpriteInfo_.find(name);if(found==mapSpriteInfo_.end())return {};const auto&info=found->second;Point offset=visitable?info.visitOffset:Point{};
 Point anchor{int(map.x+(position.x-offset.x+.5-camera[0])*32*zoom_+map.w/2),int(map.y+(position.y-offset.y+.5-camera[1])*32*zoom_+map.h/2)};
 auto scale=[&](int n){return int(std::lround(n*zoom_));};
 int w=scale(info.width),h=scale(info.height);
 return {anchor.x+scale(16)-w,anchor.y+scale(16)-h,w,h};
}
void Desktop::mapSprite(const std::string&name,Point position,bool visitable,int owner){
 auto canvas=mapSpriteCanvas(name,position,visitable);
 auto clip=adventureMapRect();if(!SDL_HasIntersection(&canvas,&clip))return;
 frame(name,0,int(animationTime_/180),canvas,false,false,false,owner);
}
std::string Desktop::mapObjectAsset(const Object&o)const{return game.mapObjectAsset(o);}
void Desktop::adventureObjects(){
 struct Draw {Point pos;std::string asset;bool visitable=true,hero=false;int owner=-2;bool heroFlag=false,boat=false,ground=false;};
 std::vector<Draw> draws;
 auto clip=adventureMapRect();auto camera=mapCamera();
 auto onScreen=[&](const std::string&asset,Point p,bool visitable){auto rect=mapSpriteCanvasAt(asset,p,visitable,clip,camera);return SDL_HasIntersection(&rect,&clip);};
 auto visible=[&](const std::string&asset,Point p,bool visitable){
  if(!onScreen(asset,p,visitable))return false;
  const auto&t=adventureTemplate(asset);Point at=visitable?Point{p.x-t.visitOffset.x,p.y-t.visitOffset.y}:p;
  for(int i=0;i<48;++i)if((t.blocked|t.visitable)&(uint64_t(1)<<i))if(auto*cell=game.tile({at.x-i%8,at.y-i/8});cell&&cell->seen)return true;
  return false;
 };
 for(int y=0;y<game.mapHeight;++y)for(int x=0;x<game.mapWidth;++x){
  const auto&t=*game.tile({x,y});
  if(t.decor==Decor::Tree&&visible("avlpntr0.def",{x,y},false))draws.push_back({{x,y},"avlpntr0.def",false});
  // Placement and pathfinding share the original four-cell mountain footprint.
  if(t.decor==Decor::Mountain&&visible("avlmtgr5.def",{x,y},false))draws.push_back({{x,y},"avlmtgr5.def",false});
 }
 if(game.imported()){
  auto revealed=[&](Point p){for(int dy=-5;dy<=0;++dy)for(int dx=-7;dx<=0;++dx)if(auto*t=game.tile({p.x+dx,p.y+dy});t&&t->seen)return true;return false;};
  for(const auto&d:game.decorations)if(d.level==game.mapLevel&&onScreen(d.sprite,d.anchor,false)&&revealed(d.anchor))draws.push_back({d.anchor,d.sprite,false,false,-2,false,false,d.ground});
  for(const auto&o:game.objects)if(o.level==game.mapLevel&&o.type!=ObjectType::MapEvent){bool boat=game.heroes.contains(o.id)&&game.heroes.at(o.id).value.onBoat;auto asset=boat?"ab01_.def":mapObjectAsset(o);if(onScreen(asset,o.anchor,false)&&revealed(o.anchor))draws.push_back({o.anchor,std::move(asset),false,false,o.type==ObjectType::Town||o.type==ObjectType::Mine||o.type==ObjectType::EnemyHero?o.owner:-2,o.sourceClass==34||o.sourceClass==70,boat,o.ground});}
 }else for(const auto&o:game.objects){auto asset=mapObjectAsset(o);if(visible(asset,o.pos,true))draws.push_back({o.pos,std::move(asset),true,false,o.type==ObjectType::Town||o.type==ObjectType::Mine?o.owner:-2});}
 draws.push_back({game.hero.pos,"",false,true});
 std::sort(draws.begin(),draws.end(),[](const Draw&a,const Draw&b){return std::tuple(!a.ground,a.pos.y,a.pos.x,a.hero,a.asset)<std::tuple(!b.ground,b.pos.y,b.pos.x,b.hero,b.asset);});
 bool routeDrawn=false;
 for(const auto&draw:draws){if(!draw.ground&&!routeDrawn){adventureRoute();routeDrawn=true;}if(draw.hero)adventureHero();else{mapSprite(draw.asset,draw.pos,draw.visitable,draw.owner);if(draw.heroFlag)mapSprite(draw.boat?ownerArt_["colors"][ownerName(draw.owner)].value("boatFlag",std::string("abf01g.def")):ownerHeroFlag(draw.owner),draw.pos,false);}}
}
Desktop::TerrainVisual Desktop::terrainVisual(Point p)const{
 auto*t=game.tile(p);if(!t)return {};
 const char*assets[]={"grastl.def","dirttl.def","sandtl.def","watrtl.def","snowtl.def","swmptl.def","rougtl.def","subbtl.def","lavatl.def","rocktl.def"};auto asset=assets[int(t->terrain)];
 if(t->originalView>=0)return {asset,t->originalView,(t->originalFlags&1)!=0,(t->originalFlags&2)!=0};
 if(t->terrain==Terrain::Sand)return {asset,t->variant%12};
 const Point around[]={{-1,-1},{0,-1},{1,-1},{1,0},{1,1},{0,1},{-1,1},{-1,0}};
 int code=0,multiplier=1,base=t->terrain==Terrain::Grass?3:2;
 for(auto offset:around){auto*n=game.tile({p.x+offset.x,p.y+offset.y});int digit=0;
  if(n){if(t->terrain==Terrain::Grass)digit=n->terrain==Terrain::Dirt?1:n->terrain==Terrain::Sand||n->terrain==Terrain::Water?2:0;
   else if(t->terrain==Terrain::Dirt)digit=n->terrain==Terrain::Sand||n->terrain==Terrain::Water;
   else digit=n->terrain!=Terrain::Water;}
  code+=digit*multiplier;multiplier*=base;
 }
 const auto&choices=mapArt_["terrainFrames"][asset][code];const auto&f=choices[t->variant%choices.size()];return {asset,f[0],f[1],f[2]};
}
void Desktop::adventureTerrain(){
 auto clip=adventureMapRect();
 auto camera=mapCamera();int firstX=std::max(0,int(std::floor(camera[0]-clip.w/(64*zoom_)))-1),lastX=std::min(game.mapWidth,int(std::ceil(camera[0]+clip.w/(64*zoom_)))+1),firstY=std::max(0,int(std::floor(camera[1]-clip.h/(64*zoom_)))-1),lastY=std::min(game.mapHeight,int(std::ceil(camera[1]+clip.h/(64*zoom_)))+1);
 const Point around[]={{-1,-1},{0,-1},{1,-1},{1,0},{1,1},{0,1},{-1,1},{-1,0}};
 auto connected=[&](const char*name,Point tile,bool river,SDL_Rect rect){
  unsigned mask=0;for(int i=0;i<8;++i){if(river&&i%2==0)continue;auto*t=game.tile({tile.x+around[i].x,tile.y+around[i].y});if(t&&(river?t->river:t->road))mask|=1u<<i;}
  if(mapArt_.contains("connectionFrames")){
   const auto&f=mapArt_["connectionFrames"][name][mask];frame(name,0,f["frame"],rect,f["flipX"],false,f["flipY"]);
  }
 };
 for(int y=firstY;y<lastY;++y)for(int x=firstX;x<lastX;++x){
  Point tile{x,y};auto*t=game.tile(tile);bool visible=t->seen;
  if(!visible)for(auto d:around){auto*n=game.tile({x+d.x,y+d.y});visible=visible||(n&&n->seen);}
  if(!visible)continue;
  auto p=mapScreen(tile),next=mapScreen({x+1,y+1});int half=int(std::lround(16*zoom_));SDL_Rect rect{p.x-half,p.y-half,next.x-p.x,next.y-p.y};
  if(!SDL_HasIntersection(&rect,&clip))continue;
  auto visual=terrainVisual(tile);frame(visual.asset,0,visual.frame,rect,visual.flipX,false,visual.flipY);
  if(t->originalView>=0){
   const char*rivers[]={"","clrrvr.def","icyrvr.def","mudrvr.def","lavrvr.def"};const char*roads[]={"","dirtrd.def","gravrd.def","cobbrd.def"};
   if(t->river&&t->riverType>0&&t->riverType<5)frame(rivers[t->riverType],0,t->riverView,rect,(t->originalFlags&4)!=0,false,(t->originalFlags&8)!=0);
   if(t->road&&t->roadType>0&&t->roadType<4)frame(roads[t->roadType],0,t->roadView,rect,(t->originalFlags&16)!=0,false,(t->originalFlags&32)!=0);
  }else if(t->terrain!=Terrain::Water){if(t->river)connected("clrrvr.def",tile,true,rect);if(t->road)connected("dirtrd.def",tile,false,rect);}
 }
}
void Desktop::mapArtworkRenderTests(const std::filesystem::path&directory){
 auto check=[](bool ok,const char*message){if(!ok)throw std::runtime_error(message);};
 int oldW=0,oldH=0;SDL_GetWindowSize(window_,&oldW,&oldH);SDL_SetWindowSize(window_,1280,800);
 game=Game::create();scene=Scene::Adventure;settings_=ClientSettings{};settings_.hdArtwork=false;
 for(auto&t:game.tiles){t.seen=true;t.decor=Decor::None;t.road=t.river=false;t.terrain=Terrain::Grass;}
 for(int y=0;y<MapH;++y)for(int x=26;x<MapW;++x)game.tile({x,y})->terrain=Terrain::Water;
 for(int y=12;y<=18;++y)game.tile({22,y})->river=true;
 for(int x=22;x<=26;++x)game.tile({x,18})->river=true;
 for(int x=6;x<=24;++x)game.tile({x,16})->road=true;
 for(int x=7;x<=11;++x)game.tile({x,8})->decor=Decor::Tree;
 game.tile({8,14})->decor=Decor::Tree;game.tile({20,7})->decor=Decor::Mountain;
 game.objects.clear();
 auto add=[&](ObjectType type,Point p,const char*name,int resource=Gold){Object o;o.id=int(game.objects.size())+1;o.type=type;o.pos=p;o.name=name;o.resource=resource;game.objects.push_back(o);};
 add(ObjectType::Windmill,{14,12},"风车");add(ObjectType::WaterWheel,{22,12},"水车");
 add(ObjectType::Treasure,{10,15},"宝箱");add(ObjectType::Artifact,{12,15},"示例宝物");add(ObjectType::Resource,{14,15},"水晶",Crystal);
 add(ObjectType::Monster,{18,15},"骷髅");game.objects.back().army={Stack{Skeleton,30}};
 add(ObjectType::Mine,{7,11},"伐木场",Wood);add(ObjectType::Mine,{18,9},"水晶矿",Crystal);
 game.hero.pos={8,13};camX_=18;camY_=14;zoom_=1;heroDirection_={1,0};marching_=false;route_.clear();destination_.reset();mouseX_=mouseY_=-1;toastUntil_=0;
 auto state=game.toJson();
 auto pixels=[&](SDL_Rect logical){
  SDL_Rect area{int(logical.x*pixelScale_),int(logical.y*pixelScale_),int(logical.w*pixelScale_),int(logical.h*pixelScale_)};
  std::vector<Uint32> result(size_t(area.w)*area.h);check(SDL_SetRenderTarget(renderer_,canvas_.texture)==0,"Map art canvas unavailable");
  bool ok=SDL_RenderReadPixels(renderer_,&area,SDL_PIXELFORMAT_RGBA32,result.data(),area.w*4)==0;SDL_SetRenderTarget(renderer_,nullptr);check(ok,"Map art pixel read failed");return result;
 };
 auto shot=[&](const char*name,uint64_t time){fixedAnimationTime_=time;render();check(screenshot(directory/(std::string(name)+".png")),"Map art screenshot failed");};
 shot("map-objects-classic-0000",0);
 auto all=pixels(adventureMapRect()),mill=pixels(mapSpriteCanvas("avmwndd0.def",{14,12}));
 auto water=pixels({mapScreen({29,13}).x-16,mapScreen({29,13}).y-16,32,32});
 // Prove the foreground actually overlaps the hero, then require opaque tree
 // pixels to stay identical with and without that hero behind them.
 auto treeCanvas=mapSpriteCanvas("avlpntr0.def",{8,14},false);auto treeAndHero=pixels(treeCanvas);
 game.hero.pos={4,4};render();auto treeOnly=pixels(treeCanvas);
 game.tile({8,14})->decor=Decor::None;game.hero.pos={8,13};render();auto heroOnly=pixels(treeCanvas);
 game.hero.pos={4,4};render();auto empty=pixels(treeCanvas);game.hero.pos={8,13};game.tile({8,14})->decor=Decor::Tree;
 const auto&tree=demo_["def"]["avlpntr0.def"];const auto&f=tree["groups"]["0"][0];auto*surface=readSurface(tree["pages"][f["page"].get<int>()]["file"],false,false);check(surface!=nullptr,"Original tree mask unavailable");
 int overlaps=0,physicalWidth=int(treeCanvas.w*pixelScale_);
 for(int y=0;y<f["rect"][3].get<int>();++y)for(int x=0;x<f["rect"][2].get<int>();++x){
  auto*source=static_cast<Uint8*>(surface->pixels)+(y+f["rect"][1].get<int>())*surface->pitch+(x+f["rect"][0].get<int>())*4;if(source[3]!=255)continue;
  int px=int((x+f["offset"][0].get<int>())*pixelScale_),py=int((y+f["offset"][1].get<int>())*pixelScale_);size_t at=size_t(py)*physicalWidth+px;
  if(heroOnly[at]!=empty[at]){++overlaps;check(treeAndHero[at]==treeOnly[at],"Hero painted over opaque foreground scenery");}
 }
 SDL_FreeSurface(surface);check(overlaps>10,"Occlusion fixture never overlapped the hero");fixedAnimationTime_=0;render();
 std::reverse(game.objects.begin(),game.objects.end());render();check(pixels(adventureMapRect())==all,"Object insertion order changed map occlusion");std::reverse(game.objects.begin(),game.objects.end());
 shot("map-objects-classic-0180",180);check(pixels(mapSpriteCanvas("avmwndd0.def",{14,12}))!=mill,"Windmill animation remained static");
 check(pixels({mapScreen({29,13}).x-16,mapScreen({29,13}).y-16,32,32})!=water,"Water palette did not animate");
 shot("map-objects-classic-1440",1440);check(pixels(mapSpriteCanvas("avmwndd0.def",{14,12}))==mill,"Windmill lost its original eight-frame loop");
 shot("map-objects-classic-2160",2160);check(pixels({mapScreen({29,13}).x-16,mapScreen({29,13}).y-16,32,32})==water,"Water palette cycle did not repeat");
 nlohmann::json sizes=nlohmann::json::array();
 for(int percent:{65,100,180}){
  zoom_=percent/100.f;auto canvas=mapSpriteCanvas("avmwndd0.def",{14,12});
  check(canvas.w==int(std::lround(160*zoom_))&&canvas.h==int(std::lround(96*zoom_)),"Windmill was stretched outside its original canvas");
  auto treasure=mapSpriteCanvas("avtchst0.def",{10,15});check(treasure.w==int(std::lround(64*zoom_))&&treasure.h==int(std::lround(32*zoom_)),"Treasure used a UI portrait size");
  check(mapPoint(mapScreen({14,12}))==Point{14,12},"Original visit anchor lost tile hit alignment");
  fixedAnimationTime_=0;render();check(screenshot(directory/("map-objects-zoom-"+std::to_string(percent)+".png")),"Map zoom fixture failed");
  sizes.push_back({{"zoomPercent",percent},{"windmillCanvas",{canvas.w,canvas.h}},{"chestCanvas",{treasure.w,treasure.h}}});
 }
 check(game.toJson()==state,"Map rendering consumed RNG or mutated gameplay");
 zoom_=1;settings_.hdArtwork=true;shot("map-objects-hd-0000",0);
 std::ofstream(directory/"map-art-audit.json")<<nlohmann::json{{"source","verified original demo DEF and OBJECTS.TXT"},{"originalCanvasPreserved",true},{"objectOrderIndependent",true},{"opaqueForegroundHidesHero",true},{"verifiedHeroOverlapPixels",overlaps},{"windmillEightFrameLoop",true},{"waterTwelvePhaseLoop",true},{"gameStateAndRngUnchanged",true},{"zooms",sizes},{"referencePixelParityVerified",false}}.dump(2)<<'\n';
 game=Game::create();scene=Scene::Town;zoom_=1;heroDirection_={1,0};fixedAnimationTime_=0;SDL_SetWindowSize(window_,oldW,oldH);
}
}
