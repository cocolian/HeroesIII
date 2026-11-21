#include "h3/renderer.hpp"
#include "h3/pixel_scaler.hpp"
#include <algorithm>
#include <cstring>
#include <cmath>
#include <fstream>
#include <cctype>

namespace h3 {
namespace {constexpr size_t TextureBudget=128*1024*1024,SourceBudget=32*1024*1024;}
int Desktop::fontKey(int size)const{return std::max(8,int(std::lround(size*pixelScale_)))*4+(settings_.hdArtwork?1:0)+(settings_.hdArtwork&&size>=18?2:0);}
TTF_Font*Desktop::font(int size){int key=fontKey(size);if(!fonts_.contains(key)){auto*face=TTF_OpenFont((settings_.hdArtwork&&size>=18?titleFontPath_:fontPath_).c_str(),std::max(8,int(std::lround(size*pixelScale_))));if(!face)throw std::runtime_error(TTF_GetError());TTF_SetFontHinting(face,settings_.hdArtwork?TTF_HINTING_LIGHT:TTF_HINTING_NORMAL);fonts_[key]=face;}return fonts_[key];}
void Desktop::clearArtworkCache(){for(auto&[key,item]:enhanced_)SDL_DestroyTexture(item.texture);for(auto&[key,item]:artSources_)SDL_FreeSurface(item.surface);enhanced_.clear();artSources_.clear();redrawnMeshes_.clear();enhancedBytes_=sourceBytes_=0;}
bool Desktop::redrawnBattle(){auto*t=game.tile(game.hero.pos);if(t&&t->terrain!=Terrain::Grass){redrawnBattleDrawn_=false;return false;}redrawnBattleDrawn_=redrawnBackground("battle.grass",{0,0,800,556});return redrawnBattleDrawn_;}
bool Desktop::redrawnTown(){redrawnTownDrawn_=redrawnBackground("town."+Factions[game.town.faction].key,{0,0,800,374});return redrawnTownDrawn_;}
SDL_Texture*Desktop::cacheArtwork(const std::string&key,SDL_Surface*surface){
 if(auto found=enhanced_.find(key);found!=enhanced_.end()){found->second.used=++artUse_;return found->second.texture;}
 if(!surface)return nullptr;size_t bytes=size_t(surface->pitch)*surface->h;if(bytes>64*1024*1024)return nullptr;
 auto*texture=SDL_CreateTextureFromSurface(renderer_,surface);if(!texture)return nullptr;SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_BLEND);SDL_SetTextureScaleMode(texture,SDL_ScaleModeLinear);
 while(enhancedBytes_+bytes>TextureBudget&&!enhanced_.empty()){auto oldest=std::min_element(enhanced_.begin(),enhanced_.end(),[](const auto&a,const auto&b){return a.second.used<b.second.used;});enhancedBytes_-=oldest->second.bytes;SDL_DestroyTexture(oldest->second.texture);enhanced_.erase(oldest);}
 enhanced_.emplace(key,ArtTexture{texture,bytes,++artUse_});enhancedBytes_+=bytes;return texture;
}
bool Desktop::enhancedArt(const std::string&name,SDL_Rect destination,SDL_Rect crop,bool keyed,SDL_RendererFlip orientation,bool blue){
 const bool retina=smoothRetinaArtwork();
 // Isolate atlas frames before filtering so adjacent frames cannot bleed.
 // The mobile GPU can then sample their native pixels without a 4x cache.
 const int factor=retina?1:4;
 std::string sourceKey=name+(keyed?"#key":"#raw")+(blue?"#blue":"");
 std::string key=(retina?"retina:":"")+sourceKey+":"+std::to_string(crop.x)+","+std::to_string(crop.y)+","+std::to_string(crop.w)+","+std::to_string(crop.h);
 if(auto found=enhanced_.find(key);found!=enhanced_.end()){
  found->second.used=++artUse_;
  return SDL_RenderCopyEx(renderer_,found->second.texture,nullptr,&destination,0,nullptr,orientation)==0;
 }
 size_t bytes=size_t(crop.w)*crop.h*factor*factor*sizeof(Rgba);if(bytes>64*1024*1024)return false;
 auto raw=artSources_.find(sourceKey);
 if(raw==artSources_.end()){
  auto*surface=readSurface(name,keyed,blue);if(!surface)return false;size_t size=size_t(surface->pitch)*surface->h;
  if(size>SourceBudget){SDL_FreeSurface(surface);return false;}
  while(sourceBytes_+size>SourceBudget&&!artSources_.empty()){auto oldest=std::min_element(artSources_.begin(),artSources_.end(),[](const auto&a,const auto&b){return a.second.used<b.second.used;});sourceBytes_-=oldest->second.bytes;SDL_FreeSurface(oldest->second.surface);artSources_.erase(oldest);}
  raw=artSources_.emplace(sourceKey,ArtSource{surface,size,++artUse_}).first;sourceBytes_+=size;
 }else raw->second.used=++artUse_;
 auto*surface=raw->second.surface;if(crop.x<0||crop.y<0||crop.w<=0||crop.h<=0||crop.x>surface->w-crop.w||crop.y>surface->h-crop.h)return false;
 std::vector<Rgba> pixels(size_t(crop.w)*crop.h);SDL_LockSurface(surface);
 for(int row=0;row<crop.h;row++)std::memcpy(pixels.data()+size_t(row)*crop.w,static_cast<Uint8*>(surface->pixels)+(crop.y+row)*surface->pitch+crop.x*4,size_t(crop.w)*4);
 SDL_UnlockSurface(surface);
 if(!retina)pixels=scaleArtwork(pixels,crop.w,crop.h,factor);
 else if(std::any_of(pixels.begin(),pixels.end(),[](Rgba pixel){return pixel.a<=16;}))
  pixels=extendArtworkEdgeColors(pixels,crop.w,crop.h);
 const int width=crop.w*factor,height=crop.h*factor;
 auto*expanded=SDL_CreateRGBSurfaceWithFormatFrom(pixels.data(),width,height,32,width*sizeof(Rgba),SDL_PIXELFORMAT_RGBA32);if(!expanded)return false;
 SDL_Texture*texture=cacheArtwork(key,expanded);SDL_FreeSurface(expanded);if(!texture)return false;
 return SDL_RenderCopyEx(renderer_,texture,nullptr,&destination,0,nullptr,orientation)==0;
}
void Desktop::graphicsPreview(const std::filesystem::path&directory,bool townArtOnly){saveDir_=directory/"test-saves";std::filesystem::create_directories(directory);fixedAnimationTime_=0;if(townArtOnly){SDL_SetWindowSize(window_,2560,1920);scene=Scene::Town;render();townArtworkRenderTests(directory);}else{graphicsRenderTests(directory);moraleRenderTests(directory);}}
void Desktop::graphicsRenderTests(const std::filesystem::path&directory){
 auto check=[](bool ok,const char*message){if(!ok)throw std::runtime_error(message);};
 {
  // The gutter represents neighboring atlas frames. Filtering an isolated
  // crop must smooth the interior edge without sampling that gutter.
  SDL_Surface* atlas=SDL_CreateRGBSurfaceWithFormat(0,4,4,32,SDL_PIXELFORMAT_RGBA32);
  check(atlas,"Retina test atlas unavailable");
  SDL_FillRect(atlas,nullptr,SDL_MapRGBA(atlas->format,0,255,0,255));
  SDL_Rect black{1,1,1,2},white{2,1,1,2};
  SDL_FillRect(atlas,&black,SDL_MapRGBA(atlas->format,0,0,0,255));
  SDL_FillRect(atlas,&white,SDL_MapRGBA(atlas->format,255,255,255,255));
  const std::string key="retina-test-atlas#raw";
  auto* atlasTexture=SDL_CreateTextureFromSurface(renderer_,atlas);
  check(atlasTexture,"Retina test texture unavailable");
  images_[key]={atlasTexture,4,4};
  artSources_[key]={atlas,64,++artUse_};
  sourceBytes_+=64;
  auto* target=SDL_CreateTexture(renderer_,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,32,16);
  check(target&&SDL_SetRenderTarget(renderer_,target)==0,"Retina test target unavailable");
  auto savedRetina=retinaArtwork_;
  auto savedHD=settings_.hdArtwork;
  auto savedScale=pixelScale_;
  retinaArtwork_=settings_.hdArtwork=true;
  pixelScale_=1;
  SDL_RenderSetScale(renderer_,1,1);
  SDL_RenderSetClipRect(renderer_,nullptr);
  SDL_SetRenderDrawColor(renderer_,0,0,0,255);
  SDL_RenderClear(renderer_);
  art("retina-test-atlas",{0,0,16,16},SDL_Rect{1,1,2,2},false);
  art("retina-test-atlas",{16,0,16,16},SDL_Rect{1,1,2,2},false,true,false,true);
  std::array<Rgba,32*16> pixels;
  check(SDL_RenderReadPixels(renderer_,nullptr,SDL_PIXELFORMAT_RGBA32,pixels.data(),32*4)==0,
        "Retina test pixels unavailable");
  auto edge=pixels[8*32+7];
  check(edge.r>0&&edge.r<255&&edge.r==edge.g&&edge.g==edge.b,"Retina atlas kept a hard pixel edge");
  for(const auto& pixel:pixels)check(pixel.r==pixel.g&&pixel.g==pixel.b,"Retina atlas sampled another frame");
  check(pixels[8*32].r==0&&pixels[8*32+16].r==255,"Retina filtering lost the sprite flip");
  SDL_SetRenderTarget(renderer_,nullptr);
  SDL_DestroyTexture(target);
  SDL_DestroyTexture(atlasTexture);
  images_.erase(key);
  clearArtworkCache();
  retinaArtwork_=savedRetina;
  settings_.hdArtwork=savedHD;
  pixelScale_=savedScale;
 }
 int width=0,height=0;SDL_GetWindowSize(window_,&width,&height);SDL_SetWindowSize(window_,2560,1920);battleMotion_.clear();autoCombat_=false;pendingSpell_=sacrificeTarget_=-1;mouseX_=mouseY_=-1000;focus_=-1;toastUntil_=0;
 // TBCSHALL's later frames contain only an 82x52 flag patch. Its roof and
 // walls outside that patch must remain visible in both quality modes.
 auto*previousTarget=SDL_GetRenderTarget(renderer_);float scaleX=1,scaleY=1;SDL_RenderGetScale(renderer_,&scaleX,&scaleY);
 auto*target=SDL_CreateTexture(renderer_,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,216,129);check(target,"Town animation regression target failed");
 TownLayer hall;hall.animation="TBCSHALL.def";hall.pos={0,0};
 for(bool hd:{false,true}){
  settings_.hdArtwork=hd;std::array<std::vector<Rgba>,2> samples;
  for(int i=0;i<2;i++){
   check(SDL_SetRenderTarget(renderer_,target)==0,"Town animation regression target unavailable");SDL_RenderSetViewport(renderer_,nullptr);SDL_RenderSetScale(renderer_,1,1);SDL_RenderSetClipRect(renderer_,nullptr);SDL_SetRenderDrawColor(renderer_,7,6,5,255);SDL_RenderClear(renderer_);animationTime_=i*300;check(townAnimation(hall),"Town animation regression source failed");
   samples[i].resize(216*129);check(SDL_RenderReadPixels(renderer_,nullptr,SDL_PIXELFORMAT_RGBA32,samples[i].data(),216*4)==0,"Town animation regression readback failed");
  }
  int roof=0;for(int y=0;y<129;y++)for(int x=0;x<100;x++){size_t p=size_t(y)*216+x;check(samples[0][p]==samples[1][p],"Town flag patch removed the building base");if(samples[0][p].r!=7||samples[0][p].g!=6||samples[0][p].b!=5)roof++;}
  check(roof>1000&&samples[0]!=samples[1],"Town animation regression did not show a full building and moving flags");
 }
 SDL_SetRenderTarget(renderer_,previousTarget);SDL_RenderSetScale(renderer_,scaleX,scaleY);SDL_DestroyTexture(target);
 for(auto item:{std::pair{Scene::Town,"town"},std::pair{Scene::Battle,"battle"},std::pair{Scene::Hero,"hero"}}){
  game=Game::create();if(item.first==Scene::Battle)game.startBattle(15);scene=item.first;auto state=game.toJson();fixedAnimationTime_=0;
  settings_.hdArtwork=false;render();if(item.first==Scene::Town)check(redrawnBuildingsDrawn_==0,"Classic town used redrawn building sprites");check(screenshot(directory/("quality-"+std::string(item.second)+"-classic.png")),"Classic quality capture failed");
  key(SDLK_F7,KMOD_NONE);check(settings_.hdArtwork&&game.toJson()==state,"Quality toggle changed game state");render();
  if(item.first==Scene::Town||item.first==Scene::Battle){
   check(item.first==Scene::Town?redrawnTownDrawn_:redrawnBattleDrawn_,"Redrawn background failed");if(item.first==Scene::Town)check(redrawnBuildingsDrawn_==4,"Starter town did not draw all four static HD building layers");
   const auto&asset=hd_["backgrounds"][item.first==Scene::Town?"town.castle":"battle.grass"];
   if(asset["strips"].empty())check(image(asset["base"].get<std::string>(),false).w>=1800,"Town redraw source is below the recorded native width");
   else for(const auto&strip:asset["strips"])check(image(strip["file"].get<std::string>(),false).w/strip["width"].get<float>()>=2048,"Battle redraw density is below 2K");
  }
  for(const char*portrait:{"hpl128qc.pcx","hps128qc.pcx"}){const auto&file=hd_["images"][portrait];check(file.is_string()&&image(file.get<std::string>(),false).w>=1000,"HD portrait source failed");}
  check(screenshot(directory/("quality-"+std::string(item.second)+"-hd.png")),"HD quality capture failed");
  check(enhancedBytes_<=TextureBudget&&sourceBytes_<=SourceBudget,"HD artwork cache exceeded its budget");
 }
 for(int variant=0;variant<3;variant++){
  game=Game::create();scene=Scene::Town;if(variant==0)game.town.built[UpGuardhouse]=true;if(variant>=1)game.town.built[Citadel]=true;if(variant==2)game.town.built[Castle]=true;auto before=game.toJson();
  settings_.hdArtwork=false;render();std::vector<int> hits;for(int y=0;y<374;y+=7)for(int x=0;x<800;x+=7)hits.push_back(townLayerAt({x,y}));settings_.hdArtwork=true;render();check(redrawnBuildingsDrawn_==4&&game.toJson()==before,"HD building upgrade changed game state or lost a layer");
  size_t index=0;for(int y=0;y<374;y+=7)for(int x=0;x<800;x+=7)check(hits[index++]==townLayerAt({x,y}),"HD building quality changed original mask hits");
  const char*names[]={"quality-town-guardhouse-upgraded-hd.png","quality-town-citadel-hd.png","quality-town-castle-hd.png"};check(screenshot(directory/names[variant]),"HD building upgrade capture failed");
 }
 struct DwellingArt{int building,upgrade,unit;const char*animation;const char*name;};
 for(auto item:{DwellingArt{ArcherTower,-1,Archer,"tbcsdw_1.def","archers-tower"},DwellingArt{ArcherTower,UpArcherTower,Marksman,"tbcsup_1.def","archers-tower-upgraded"},DwellingArt{GriffinTower,-1,Griffin,"tbcsdw_2.def","griffin-tower"},DwellingArt{GriffinTower,UpGriffinTower,RoyalGriffin,"tbcsup_2.def","griffin-tower-upgraded"}}){
  game=Game::create();scene=Scene::Town;game.town.built[item.building]=true;if(item.upgrade>=0)game.town.built[item.upgrade]=true;game.town.pool[baseTier(item.unit)]=7;auto before=game.toJson();
  int layerIndex=-1;for(int i=0;i<int(game.townDef().layers.size());i++){auto name=game.townDef().layers[i].animation;std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return char(std::tolower(c));});if(name==item.animation&&layerVisible(game.townDef().layers[i]))layerIndex=i;}
  check(layerIndex>=0,"HD dwelling test lost its visible original layer");const auto&layer=game.townDef().layers[layerIndex];const auto&mask=pixelMask(layer.mask);std::vector<int>hits;Point hit{-1,-1};
  settings_.hdArtwork=false;render();for(int y=0;y<mask.height;y++)for(int x=0;x<mask.width;x++){Point p{layer.pos.x+x,layer.pos.y+y};int found=townLayerAt(p);hits.push_back(found);if(found==layerIndex)hit=p;}
  check(screenshot(directory/("quality-town-"+std::string(item.name)+"-classic.png")),"Classic dwelling capture failed");
  settings_.hdArtwork=true;render();check(redrawnBuildingsDrawn_==(item.building==ArcherTower?4:5)&&game.toJson()==before,"HD dwelling layer count or gameplay state changed");
  size_t i=0;for(int y=0;y<mask.height;y++)for(int x=0;x<mask.width;x++)check(hits[i++]==townLayerAt({layer.pos.x+x,layer.pos.y+y}),"HD dwelling changed an original click-mask result");
  check(screenshot(directory/("quality-town-"+std::string(item.name)+"-hd.png")),"HD dwelling capture failed");
  check(hit.x>=0,"HD dwelling has no exposed original hit pixel");click(hit,false);check(scene==Scene::Recruit&&recruitUnit_==item.unit&&game.toJson()==before,"HD dwelling original hit failed to open correct recruitment");key(SDLK_ESCAPE,KMOD_NONE);
 }
 struct ForegroundArt{int building;const char*animation;const char*name;};
 for(auto item:{ForegroundArt{Tavern,"tbcstvrn.def","tavern"},ForegroundArt{Special3,"tbcsext1.def","brotherhood"},ForegroundArt{Blacksmith,"tbcsblak.def","blacksmith"},ForegroundArt{Marketplace,"tbcsmark.def","marketplace"}}){
  game=Game::create();scene=Scene::Town;game.town.built[item.building]=true;auto before=game.toJson();int layerIndex=-1;
  for(int i=0;i<int(game.townDef().layers.size());i++){auto name=game.townDef().layers[i].animation;std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return char(std::tolower(c));});if(name==item.animation&&layerVisible(game.townDef().layers[i]))layerIndex=i;}
  check(layerIndex>=0,"HD foreground test lost its original layer");const auto&layer=game.townDef().layers[layerIndex];const auto&mask=pixelMask(layer.mask);std::vector<int>hits;Point hit{-1,-1};
  settings_.hdArtwork=false;render();for(int y=0;y<mask.height;y++)for(int x=0;x<mask.width;x++){Point p{layer.pos.x+x,layer.pos.y+y};int found=townLayerAt(p);hits.push_back(found);if(found==layerIndex)hit=p;}
  check(screenshot(directory/("quality-town-"+std::string(item.name)+"-classic.png")),"Classic foreground capture failed");
  settings_.hdArtwork=true;render();check(redrawnBuildingsDrawn_==(item.building==Blacksmith||item.building==Marketplace?5:4)&&game.toJson()==before,"HD foreground changed state or stacked tavern variants");
  size_t i=0;for(int y=0;y<mask.height;y++)for(int x=0;x<mask.width;x++)check(hits[i++]==townLayerAt({layer.pos.x+x,layer.pos.y+y}),"HD foreground changed an original click-mask result");
  check(screenshot(directory/("quality-town-"+std::string(item.name)+"-hd.png")),"HD foreground capture failed");
  if(item.building==Marketplace){check(hit.x>=0,"HD marketplace has no original exposed hit pixel");click(hit,false);check(scene==Scene::Market&&game.toJson()==before,"HD marketplace original hit failed to open the market");key(SDLK_ESCAPE,KMOD_NONE);}
 }
 TownLayer probe;probe.animation="TBCSHALL.def";check(!redrawnTownBuilding(probe),"Static HD building registry replaced an animated building");
 nlohmann::json buildingAudit=nlohmann::json::array();
 for(const auto&[name,asset]:hd_["townBuildings"].items()){
  auto*surface=readSurface(asset["file"].get<std::string>(),false,false);check(surface,"HD building source failed");check(surface->w/asset["canvas"][0].get<float>()>=3.2f&&surface->h/asset["canvas"][1].get<float>()>=3.2f,"HD building source lacks 2K output density");
  int w=surface->w,h=surface->h;std::vector<Rgba> pixels(size_t(w)*h);SDL_LockSurface(surface);for(int y=0;y<h;y++)std::memcpy(pixels.data()+size_t(y)*w,static_cast<Uint8*>(surface->pixels)+y*surface->pitch,size_t(w)*4);SDL_UnlockSurface(surface);SDL_FreeSurface(surface);
  if(name=="tbcstvrn.def")check(pixels[size_t(int(h*.65f))*w+int(w*.32f)].a>=240,"HD tavern doorway is accidentally transparent");
  if(name=="tbcsext1.def")check(pixels[size_t(int(h*.76f))*w+int(w*.1f)].a>=240,"HD brotherhood doorway is accidentally transparent");
  if(name=="tbcsdw_1.def"||name=="tbcsup_1.def")check(pixels[size_t(int(h*.9f))*w+int(w*.43f)].a>=240,"HD archers tower entrance is accidentally transparent");
  if(name=="tbcsdw_2.def"||name=="tbcsup_2.def")check(pixels[size_t(int(h*.8f))*w+int(w*.64f)].a>=240,"HD griffin tower entrance is accidentally transparent");
  auto corrected=extendArtworkEdgeColors(pixels,w,h);int transparent=0,covered=0,beforeMatte=0,afterMatte=0;
  auto matte=[](Rgba p){return p.a<=16&&p.a>0&&p.r==255&&((p.g==0&&p.b==0)||(p.g==255&&p.b==0));};
  for(size_t i=0;i<pixels.size();i++){transparent+=pixels[i].a==0;covered+=pixels[i].a>=240;beforeMatte+=matte(pixels[i]);afterMatte+=matte(corrected[i]);check(pixels[i].a==corrected[i].a,"Building edge repair changed source coverage");if(pixels[i].a>16)check(pixels[i]==corrected[i],"Building edge repair recolored a covered pixel");}
  check(transparent&&covered,"HD building source is not an alpha cutout");buildingAudit.push_back({{"animation",name},{"file",asset["file"]},{"nativePixels",{w,h}},{"logicalCanvas",asset["canvas"]},{"transparentTexels",transparent},{"coveredTexels",covered},{"lowAlphaRedYellowTexelsBefore",beforeMatte},{"lowAlphaRedYellowTexelsAfter",afterMatte},{"alphaUnchanged",true}});
 }
 std::ofstream(directory/"hd-buildings-audit.json")<<buildingAudit.dump(2)<<'\n';
 auto state=game.toJson();openOptions();render();click({330,348},false);check(!settings_.hdArtwork&&game.toJson()==state,"Classic quality option changed gameplay");render();click({230,348},false);check(settings_.hdArtwork&&game.toJson()==state,"HD quality option changed gameplay");ClientSettings saved;std::string error;check(saved.load(saveDir_/"client-options.json",error)&&saved.hdArtwork,"HD quality option did not persist");key(SDLK_ESCAPE,KMOD_NONE);
 game.resources[Gold]=12345;check(game.save(saveDir_/"autosave.json",error),"Preview autosave could not persist");game=Game::create();key(SDLK_l,KMOD_SHIFT);check(game.resources[Gold]==12345,"Shift L did not select the autosave");
 townArtworkRenderTests(directory);
 SDL_SetWindowSize(window_,width,height);game=Game::create();scene=Scene::Town;settings_.hdArtwork=true;fixedAnimationTime_=0;toastUntil_=0;
}
}
