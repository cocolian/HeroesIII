#include "h3/renderer.hpp"
#include "h3/pixel_scaler.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace h3 {
void Desktop::townArtworkRenderTests(const std::filesystem::path& directory){
 auto check=[](bool ok,const char*message){if(!ok)throw std::runtime_error(message);};
 toastUntil_=0; // Keep the preceding load notification out of timed captures.
 struct Hall{const char*animation;const char*name;int stage;};
 const Hall halls[]={{"tbcshall.def","village-hall",0},{"tbcshal2.def","town-hall",1},{"tbcshal3.def","city-hall",2},{"tbcshal4.def","capitol",3}};
 nlohmann::json audit=nlohmann::json::array();
 auto*previous=SDL_GetRenderTarget(renderer_);float sx=1,sy=1;SDL_RenderGetScale(renderer_,&sx,&sy);double previousPixelScale=pixelScale_;
 for(const auto&hall:halls){
  const auto asset=hd_["animatedTownBuildings"][hall.animation];int width=asset["canvas"][0],height=asset["canvas"][1];
  std::vector<SDL_Rect>regions;for(const auto&r:asset["originalRegions"])regions.push_back({r[0],r[1],r[2],r[3]});
  auto inside=[&](int x,int y){return std::any_of(regions.begin(),regions.end(),[&](SDL_Rect r){return x>=r.x&&y>=r.y&&x<r.x+r.w&&y<r.y+r.h;});};
  auto*target=SDL_CreateTexture(renderer_,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,width,height);check(target,"Animated hall test target failed");
  TownLayer probe;probe.animation=hall.animation;probe.pos={0,0};
  const auto&animation=demo_["def"][hall.animation];std::vector<std::vector<Rgba>>pages;std::vector<int>pageWidths;
  for(const auto&page:animation["pages"]){auto*source=readSurface(page["file"].get<std::string>(),false,false);check(source,"Original hall alpha source failed");std::vector<Rgba>pixels(size_t(source->w)*source->h);SDL_LockSurface(source);for(int y=0;y<source->h;y++)std::memcpy(pixels.data()+size_t(y)*source->w,static_cast<Uint8*>(source->pixels)+y*source->pitch,size_t(source->w)*4);SDL_UnlockSurface(source);pageWidths.push_back(source->w);pages.push_back(std::move(pixels));SDL_FreeSurface(source);}
  auto opaqueSource=[&](int frameIndex){std::vector<bool>opaque(size_t(width)*height);auto apply=[&](int index){const auto&f=animation["groups"]["0"][index];int page=f["page"],left=f["offset"][0],top=f["offset"][1],x=f["rect"][0],y=f["rect"][1],w=f["rect"][2],h=f["rect"][3];for(int row=0;row<h;row++)for(int col=0;col<w;col++)if(pages[page][size_t(y+row)*pageWidths[page]+x+col].a==255)opaque[size_t(top+row)*width+left+col]=true;};apply(0);if(frameIndex)apply(frameIndex);return opaque;};
  auto capture=[&](int frameIndex,bool redrawn){
   check(SDL_SetRenderTarget(renderer_,target)==0,"Animated hall target unavailable");SDL_RenderSetViewport(renderer_,nullptr);SDL_RenderSetScale(renderer_,1,1);SDL_RenderSetClipRect(renderer_,nullptr);SDL_SetRenderDrawColor(renderer_,0,0,0,0);SDL_RenderClear(renderer_);
   pixelScale_=1;settings_.hdArtwork=true;animationTime_=uint64_t(frameIndex)*100;
   check(redrawn?redrawnAnimatedTownBuilding(probe):townAnimation(probe),"Animated hall source could not render");
   std::vector<Rgba>pixels(size_t(width)*height);check(SDL_RenderReadPixels(renderer_,nullptr,SDL_PIXELFORMAT_RGBA32,pixels.data(),width*4)==0,"Animated hall readback failed");return pixels;
  };
  auto originalBase=capture(0,false),redrawnBase=capture(0,true);std::vector<bool>changed(originalBase.size()),redrawnChanged(originalBase.size());int opaqueMatched=0,bodyDifferent=0,regionBodyDifferent=0;auto baseOpaque=opaqueSource(0);
  for(size_t i=0;i<changed.size();i++)if(!inside(int(i)%width,int(i)/width)&&originalBase[i]!=redrawnBase[i])bodyDifferent++;
  check(bodyDifferent>1000,"Animated hall did not display its newly drawn body");
  for(size_t i=0;i<changed.size();i++){auto p=originalBase[i];bool blue=p.b>=32&&p.b*100>p.r*115&&p.b*100>p.g*105;if(inside(int(i)%width,int(i)/width)&&baseOpaque[i]&&!blue&&originalBase[i]!=redrawnBase[i])regionBodyDifferent++;}
  check(regionBodyDifferent>100,"Animated hall retained a rectangular original backdrop patch");
  int count=frameCount(hall.animation);check(count==10,"Animated hall original frame count changed");
  for(int frameIndex=0;frameIndex<count;frameIndex++){
   auto original=capture(frameIndex,false),redrawn=capture(frameIndex,true);auto opaque=opaqueSource(frameIndex);
   for(size_t i=0;i<changed.size();i++){
    bool inRegion=inside(int(i)%width,int(i)/width);
    if(!inRegion)check(redrawn[i]==redrawnBase[i],"Animated hall roof or walls shimmered outside original motion regions");
    if(original[i]!=originalBase[i]){check(inRegion,"Original hall motion escaped recorded regions");changed[i]=true;}
    if(redrawn[i]!=redrawnBase[i])redrawnChanged[i]=true;
    // Keep opaque blue cloth colors exact. Surrounding original wall pixels
    // must be omitted so the newly painted body remains visible there.
    auto p=original[i];bool blue=p.b>=32&&p.b*100>p.r*115&&p.b*100>p.g*105;
    if(inRegion&&opaque[i]&&blue){check(redrawn[i]==original[i],"Animated hall altered an opaque original blue cloth pixel");opaqueMatched++;}
   }
  }
  int changedCount=int(std::count(changed.begin(),changed.end(),true));check(changedCount>100,"Original flags were frozen by a static hall redraw");
  int redrawnChangedCount=int(std::count(redrawnChanged.begin(),redrawnChanged.end(),true));check(redrawnChangedCount>100,"Composited cloth animation was frozen");
  // Missing, stale, overlapping or invalid metadata must use the old animation,
  // never silently replace it with a frozen body.
  auto&entry=hd_["animatedTownBuildings"][hall.animation];
  entry.erase("originalRegions");check(!redrawnAnimatedTownBuilding(probe),"Animated hall without regions was accepted");entry=asset;
  entry["originalFrameCount"]=9;check(!redrawnAnimatedTownBuilding(probe),"Animated hall stale frame count was accepted");entry=asset;
  entry["originalRegions"]={{0,0,width+1,height}};check(!redrawnAnimatedTownBuilding(probe),"Animated hall out-of-bounds region was accepted");entry=asset;
  entry["originalRegions"]={{0,0,10,10},{5,5,10,10}};check(!redrawnAnimatedTownBuilding(probe),"Animated hall overlapping regions were accepted");entry=asset;
  SDL_RenderSetClipRect(renderer_,nullptr);SDL_SetRenderDrawColor(renderer_,0,0,0,0);SDL_RenderClear(renderer_);std::vector<Rgba>empty(size_t(width)*height);check(SDL_RenderReadPixels(renderer_,nullptr,SDL_PIXELFORMAT_RGBA32,empty.data(),width*4)==0,"Animated hall clear readback failed");SDL_Rect clip{20,30,64,40};SDL_RenderSetClipRect(renderer_,&clip);
  check(redrawnAnimatedTownBuilding(probe),"Animated hall clipped render failed");SDL_Rect after{};SDL_RenderGetClipRect(renderer_,&after);check(SDL_RenderIsClipEnabled(renderer_)&&after.x==clip.x&&after.y==clip.y&&after.w==clip.w&&after.h==clip.h,"Animated hall failed to restore parent clip");
  std::vector<Rgba>clipped(size_t(width)*height);check(SDL_RenderReadPixels(renderer_,nullptr,SDL_PIXELFORMAT_RGBA32,clipped.data(),width*4)==0,"Animated hall clipped readback failed");
  for(int y=0;y<height;y++)for(int x=0;x<width;x++)if(x<clip.x||y<clip.y||x>=clip.x+clip.w||y>=clip.y+clip.h)check(clipped[size_t(y)*width+x]==empty[size_t(y)*width+x],"Animated hall drew outside parent clip");
  SDL_RenderSetClipRect(renderer_,nullptr);SDL_SetRenderTarget(renderer_,previous);SDL_RenderSetScale(renderer_,sx,sy);SDL_DestroyTexture(target);pixelScale_=previousPixelScale;
  auto*surface=readSurface(asset["file"].get<std::string>(),false,false);check(surface,"Animated hall body source failed");int nativeWidth=surface->w,nativeHeight=surface->h;check(nativeWidth/float(width)>=3.2f&&nativeHeight/float(height)>=3.2f,"Animated hall body density is below the 2560 output requirement");SDL_FreeSurface(surface);
  audit.push_back({{"animation",hall.animation},{"file",asset["file"]},{"nativePixels",{nativeWidth,nativeHeight}},{"logicalCanvas",asset["canvas"]},{"originalFrameCount",count},{"originalChangedPixels",changedCount},{"compositedChangedPixels",redrawnChangedCount},{"opaqueBlueClothPixelsMatched",opaqueMatched},{"stableBodyPixelsDifferentFromOriginal",bodyDifferent},{"newBodyPixelsInsideMotionRegions",regionBodyDifferent},{"motionRegions",asset["originalRegions"]},{"bodyStableOutsideRegions",true},{"parentClipRestored",true}});

  game=Game::create();scene=Scene::Town;game.town.built[TownHall]=hall.stage>=1;game.town.built[CityHall]=hall.stage>=2;game.town.built[Capitol]=hall.stage>=3;auto state=game.toJson();fixedAnimationTime_=0;
  settings_.hdArtwork=false;render();check(redrawnAnimatedBuildingsDrawn_==0,"Classic hall used a redrawn body");check(screenshot(directory/("quality-town-"+std::string(hall.name)+"-classic.png")),"Classic hall capture failed");
  std::vector<int>hits;for(int y=0;y<374;y+=5)for(int x=0;x<216;x+=5)hits.push_back(townLayerAt({x,y}));
  settings_.hdArtwork=true;render();check(redrawnAnimatedBuildingsDrawn_==1&&game.toJson()==state,"Hall quality toggle changed state or stacked upgrade bodies");
  size_t h=0;for(int y=0;y<374;y+=5)for(int x=0;x<216;x+=5)check(hits[h++]==townLayerAt({x,y}),"Hall redraw changed original click regions");
  check(screenshot(directory/("quality-town-"+std::string(hall.name)+"-hd.png")),"HD hall capture failed");
  fixedAnimationTime_=300;render();check(screenshot(directory/("quality-town-"+std::string(hall.name)+"-flags-middle.png")),"Hall middle flag capture failed");
  fixedAnimationTime_=900;render();check(screenshot(directory/("quality-town-"+std::string(hall.name)+"-flags-end.png")),"Hall last flag capture failed");check(game.toJson()==state,"Hall animation changed gameplay or RNG");
 }
 std::ofstream(directory/"hd-animated-buildings-audit.json")<<audit.dump(2)<<'\n';fixedAnimationTime_=0;settings_.hdArtwork=true;
}
}
