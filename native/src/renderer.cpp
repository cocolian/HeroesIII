#include "h3/renderer.hpp"
#include "h3/localization.hpp"
#include "h3/battle_layout.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace h3 {
namespace {
std::string lower(std::string s){for(auto&c:s)c=char(std::tolower(static_cast<unsigned char>(c)));return s;}
constexpr SDL_Color GoldColor{236,213,130,255},Muted{189,171,135,255},Brown{54,38,24,255};
bool inside(SDL_Rect r,Point p){return p.x>=r.x&&p.y>=r.y&&p.x<r.x+r.w&&p.y<r.y+r.h;}
}
Desktop::Desktop(std::filesystem::path root,bool hidden,std::filesystem::path saveDirectory,std::filesystem::path inputTrace,bool startFullscreen):root_(std::move(root)),saveDir_(saveDirectory.empty()?root_/"saves/native":std::move(saveDirectory)),inputTrace_(std::move(inputTrace)){
 asynchronousSave_=!hidden;
 if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)!=0)throw std::runtime_error(SDL_GetError());if((IMG_Init(IMG_INIT_PNG|IMG_INIT_JPG)&IMG_INIT_PNG)==0)throw std::runtime_error(IMG_GetError());if(TTF_Init()!=0)throw std::runtime_error(TTF_GetError());SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"0");
 SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS,"0");SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS,"0");
#ifdef __IPHONEOS__
 SDL_SetHint(SDL_HINT_ORIENTATIONS,"LandscapeLeft LandscapeRight");SDL_SetHint(SDL_HINT_IOS_HIDE_HOME_INDICATOR,"2");SDL_DisableScreenSaver();
#endif
 int initialWidth=1000,initialHeight=750;SDL_Rect usable{};if(!hidden&&SDL_GetDisplayUsableBounds(0,&usable)==0){initialWidth=std::max(640,int(usable.w*.96));initialHeight=std::max(480,int((usable.h-32)*.96));}
 Uint32 windowFlags=SDL_WINDOW_ALLOW_HIGHDPI|SDL_WINDOW_RESIZABLE;
 if(!hidden&&startFullscreen)windowFlags|=SDL_WINDOW_FULLSCREEN_DESKTOP;
#ifdef __IPHONEOS__
 windowFlags=SDL_WINDOW_ALLOW_HIGHDPI|SDL_WINDOW_FULLSCREEN_DESKTOP;
#endif
 window_=SDL_CreateWindow("英雄无敌 III · C++ 原生版",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,initialWidth,initialHeight,windowFlags|(hidden?SDL_WINDOW_HIDDEN:SDL_WINDOW_SHOWN));if(!window_)throw std::runtime_error(SDL_GetError());SDL_SetWindowMinimumSize(window_,640,480);
 fullscreen_=(SDL_GetWindowFlags(window_)&SDL_WINDOW_FULLSCREEN_DESKTOP)!=0;
 renderer_=SDL_CreateRenderer(window_,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);if(!renderer_){SDL_SetHint(SDL_HINT_RENDER_BATCHING,"0");renderer_=SDL_CreateRenderer(window_,-1,SDL_RENDERER_SOFTWARE);}if(!renderer_)throw std::runtime_error(SDL_GetError());SDL_RenderSetLogicalSize(renderer_,0,0);SDL_SetRenderDrawBlendMode(renderer_,SDL_BLENDMODE_BLEND);
 for(const std::string&candidate:std::vector<std::string>{"/System/Library/Fonts/STHeiti Medium.ttc","/System/Library/Fonts/STHeiti Light.ttc",(root_/"assets/fonts/NotoSansSC.ttf").string(),"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"})if(std::filesystem::exists(candidate)){fontPath_=candidate;break;}if(fontPath_.empty())throw std::runtime_error("缺少中文字体，请重新安装完整应用");
 titleFontPath_=std::filesystem::exists("/System/Library/Fonts/Supplemental/Songti.ttc")?"/System/Library/Fonts/Supplemental/Songti.ttc":fontPath_;
 std::ifstream f(root_/"assets/manifest.json");if(!f)throw std::runtime_error("找不到素材索引，请通过 --data-root 指定项目目录");f>>manifest_;
 std::ifstream demoFile(root_/"assets/demo/manifest.json");if(demoFile){demoFile>>demo_;if(demo_.value("schema",0)!=1)throw std::runtime_error("不支持的原版素材索引版本");}
 std::ifstream libraryFile(root_/"assets/demo/map-library.json");if(!libraryFile)throw std::runtime_error("缺少地图列表，请重新安装完整应用");libraryFile>>mapLibrary_;
 localizeMapData(mapLibrary_,{});
 if(mapLibrary_.value("schema",0)!=1||!mapLibrary_.contains("entries")||!mapLibrary_["entries"].is_array()||mapLibrary_["entries"].empty()||mapLibrary_["entries"].size()>128)throw std::runtime_error("地图列表格式无效");
 std::ifstream hdFile(root_/"assets/hd/manifest.json");if(hdFile){hdFile>>hd_;if(hd_.value("schema",0)!=1)hd_=nlohmann::json{};}
 std::ifstream mapFile(root_/"assets/demo/adventure.json");if(mapFile){mapFile>>mapArt_;if(mapArt_.value("schema",0)!=1)throw std::runtime_error("不支持的地图素材索引版本");}
 std::ifstream terrainFile(root_/"assets/demo/terrain.json");if(!terrainFile)throw std::runtime_error("缺少地形过渡索引，请重新安装完整应用");nlohmann::json terrainArt;terrainFile>>terrainArt;if(terrainArt.value("schema",0)!=1)throw std::runtime_error("不支持的地形过渡索引版本");mapArt_["terrainFrames"]=std::move(terrainArt["frames"]);
 std::ifstream ownerFile(root_/"assets/demo/ownership.json");if(!ownerFile)throw std::runtime_error("缺少玩家旗帜素材，请重新安装完整应用");ownerFile>>ownerArt_;if(ownerArt_.value("schema",0)!=1)throw std::runtime_error("不支持的玩家旗帜素材版本");
 for(const auto&entry:demo_["def"].items())if(entry.value()["groups"].contains("0")&&!entry.value()["groups"]["0"].empty()){
  const auto&f=entry.value()["groups"]["0"][0];MapSpriteInfo info;info.width=f["canvas"][0];info.height=f["canvas"][1];
  if(mapArt_["templates"].contains(entry.key())){const auto&o=mapArt_["templates"][entry.key()]["visitOffset"];info.visitOffset={o[0],o[1]};}
  mapSpriteInfo_.emplace(entry.key(),info);
 }
 if(!hidden){std::string error;if(!settings_.load(saveDir_/"client-options.json",error))std::cerr<<"Options: "<<error<<'\n';audio_=std::make_unique<AudioEngine>(root_);audio_->settings(settings_);}
 if(!hidden)pointerInput_=std::make_unique<NativePointerInput>(window_);
}
Desktop::~Desktop(){try{flushAutosave();autosave_.reset();}catch(const std::exception&e){std::cerr<<"Autosave shutdown: "<<e.what()<<'\n';}restoreSystemCursor();pointerInput_.reset();audio_.reset();clearArtworkCache();if(canvas_.texture)SDL_DestroyTexture(canvas_.texture);for(auto&[k,i]:images_)if(i.texture)SDL_DestroyTexture(i.texture);for(auto&[k,i]:texts_)if(i.texture)SDL_DestroyTexture(i.texture);for(auto&[n,f]:fonts_)TTF_CloseFont(f);if(renderer_)SDL_DestroyRenderer(renderer_);if(window_)SDL_DestroyWindow(window_);TTF_Quit();IMG_Quit();SDL_Quit();}
Image& Desktop::image(const std::string&name,bool keyed,bool blue){
 std::string cache=name+(keyed?"#key":"#raw")+(blue?"#blue":"");
 if(auto it=images_.find(cache);it!=images_.end())return it->second;
 SDL_Surface*surface=readSurface(name,keyed,blue);if(!surface)return images_[cache];
 Image out;out.w=surface->w;out.h=surface->h;out.texture=SDL_CreateTextureFromSurface(renderer_,surface);SDL_FreeSurface(surface);
 if(out.texture){SDL_SetTextureBlendMode(out.texture,SDL_BLENDMODE_BLEND);SDL_SetTextureScaleMode(out.texture,SDL_ScaleModeNearest);}
 return images_[cache]=out;
}
SDL_Surface* Desktop::readSurface(const std::string&name,bool keyed,bool blue){
 std::filesystem::path file=root_/name;
 if(name.starts_with("original/")){
  auto id=lower(name.substr(9));
  if(!manifest_.contains(id)){std::cerr<<"Missing artwork: "<<name<<'\n';return nullptr;}
  file=root_/manifest_[id]["url"].get<std::string>().substr(1);
 }
 SDL_Surface*source=IMG_Load(file.string().c_str());
 if(!source){std::cerr<<"Unable to load "<<file<<": "<<IMG_GetError()<<'\n';return nullptr;}
 SDL_Surface*surface=SDL_ConvertSurfaceFormat(source,SDL_PIXELFORMAT_RGBA32,0);SDL_FreeSurface(source);
 if(!surface)return nullptr;
 if(keyed||blue){
  SDL_LockSurface(surface);
  for(int y=0;y<surface->h;y++){
   auto*pixel=static_cast<Uint8*>(surface->pixels)+y*surface->pitch;
   for(int x=0;x<surface->w;x++,pixel+=4){
    if(keyed){
     if(pixel[0]<25&&pixel[1]>220&&pixel[2]>220)pixel[3]=0;
     else if(pixel[0]>190&&pixel[2]>190&&pixel[1]<180){pixel[0]=pixel[1]=pixel[2]=0;pixel[3]=65;}
    }
    if(blue&&pixel[3]>0){
     int brightness=(54*pixel[0]+183*pixel[1]+19*pixel[2])/256;
     pixel[0]=Uint8(20+brightness/8);pixel[1]=Uint8(30+brightness/3);
     pixel[2]=Uint8(80+brightness*175/255);pixel[3]=Uint8(pixel[3]*3/4);
    }
   }
  }
  SDL_UnlockSurface(surface);
 }
 return surface;
}
void Desktop::art(const std::string&name,SDL_Rect dest,std::optional<SDL_Rect>src,bool keyed,bool flip,bool blue){auto&i=image(name,keyed,blue);if(!i.texture)return;if(name.starts_with("assets/hd/"))SDL_SetTextureScaleMode(i.texture,SDL_ScaleModeLinear);auto crop=src.value_or(SDL_Rect{0,0,i.w,i.h});if(settings_.hdArtwork&&!name.starts_with("assets/demo/battle-ui/")&&crop.w>0&&crop.h>0&&pixelScale_*dest.w/crop.w>1.05&&pixelScale_*dest.h/crop.h>1.05&&enhancedArt(name,dest,crop,keyed,flip,blue))return;SDL_RenderCopyEx(renderer_,i.texture,src?&*src:nullptr,&dest,0,nullptr,flip?SDL_FLIP_HORIZONTAL:SDL_FLIP_NONE);}
bool Desktop::originalImage(const std::string&name,SDL_Rect dest,std::optional<SDL_Rect>src){
 if(game.imported()&&ownerArt_.contains("interface")&&ownerArt_["interface"].contains(lower(name))){auto color=ownerName(0);const auto&files=ownerArt_["interface"][lower(name)]["files"];if(files.contains(color)){auto file=files[color].get<std::string>();if(image(file,false).texture){art(file,dest,src,false);return true;}}}
 if(settings_.hdArtwork&&!src&&hd_.contains("images")&&hd_["images"].contains(lower(name))){auto file=hd_["images"][lower(name)].get<std::string>();if(file.starts_with("assets/hd/")&&std::filesystem::exists(root_/file)&&image(file,false).texture){art(file,dest,{},false);return true;}}
 if(!demo_.contains("pcx"))return false;
 auto it=demo_["pcx"].find(lower(name));if(it==demo_["pcx"].end())return false;
 auto file=(*it)["file"].get<std::string>();if(!image(file,false).texture)return false;
 art(file,dest,src,false);return true;
}
int Desktop::frameCount(const std::string&name,int group)const{
 if(!demo_.contains("def"))return 0;
 auto it=demo_["def"].find(lower(name));if(it==demo_["def"].end())return 0;
 auto sequence=(*it)["groups"].find(std::to_string(group));
 return sequence==(*it)["groups"].end()?0:int(sequence->size());
}
bool Desktop::frame(const std::string&name,int group,int index,SDL_Rect canvas,bool flip,bool blue,bool vertical,int owner){
 int count=frameCount(name,group);if(!count||canvas.w<=0||canvas.h<=0)return false;
 const auto&animation=demo_["def"][lower(name)];
 const auto&f=animation["groups"][std::to_string(group)][index<0?count-1:index%count];
 auto file=animation["pages"][f["page"].get<int>()]["file"].get<std::string>();
 if(mapArt_.contains("paletteCycles")&&mapArt_["paletteCycles"].contains(lower(name))){const auto&cycle=mapArt_["paletteCycles"][lower(name)];auto phase=size_t(animationTime_/180)%cycle.size();file=cycle[phase][f["page"].get<int>()].get<std::string>();}
 if(owner>=-1){auto colored=ownerPage(name,f["page"],owner);if(!colored.empty())file=std::move(colored);}
 if(!image(file,false,blue).texture)return false;
 SDL_Rect source{f["rect"][0],f["rect"][1],f["rect"][2],f["rect"][3]};
 int fw=f["canvas"][0],fh=f["canvas"][1],left=f["offset"][0],top=f["offset"][1];
 if(flip)left=fw-left-source.w;
 if(vertical)top=fh-top-source.h;
 auto scale=[](int value,int size,int full){return int(std::lround(double(value)*size/full));};
 int x=scale(left,canvas.w,fw),y=scale(top,canvas.h,fh);
 SDL_Rect destination{canvas.x+x,canvas.y+y,scale(left+source.w,canvas.w,fw)-x,scale(top+source.h,canvas.h,fh)-y};
 if(vertical||name=="tshrc.def"||name=="tshre.def"){
  auto&texture=image(file,false,blue);
  if(SDL_RenderCopyEx(renderer_,texture.texture,&source,&destination,0,nullptr,SDL_RendererFlip((vertical?SDL_FLIP_VERTICAL:0)|(flip?SDL_FLIP_HORIZONTAL:0)))!=0)throw std::runtime_error(SDL_GetError());
 }else art(file,destination,source,false,flip,blue);
 return true;
}
bool Desktop::townAnimation(const TownLayer&layer){
 int count=frameCount(layer.animation);if(!count)return false;
 const auto&frames=demo_["def"][lower(layer.animation)]["groups"]["0"];
 const auto&base=frames[0];int index=int(animationTime_/100%count);const auto&current=frames[index];
 SDL_Rect canvas{layer.pos.x,layer.pos.y,base["canvas"][0],base["canvas"][1]};
 // Original town DEFs can store a complete building followed by smaller
 // flag/flame patches. Repaint the base before a patch on each fresh scene.
 if(index&&current["rect"][2].get<int>()*current["rect"][3].get<int>()<base["rect"][2].get<int>()*base["rect"][3].get<int>())
  if(!frame(layer.animation,0,0,canvas))return false;
 return frame(layer.animation,0,index,canvas);
}
std::optional<Desktop::CombatVisual> Desktop::combatVisual(const CombatStack&stack,int group,bool last)const{
 const auto&unit=Units[stack.unit];auto pose=battleMotion_.pose(stack.id);if(!pose)pose=battleMotion_.corpsePose(stack);if(pose){group=pose->group;if(!frameCount(unit.animation,group))group=stack.corpse?5:2;}if(!frameCount(unit.animation,group))return {};
 const auto&f=demo_["def"][lower(unit.animation)]["groups"][std::to_string(group)][0];
 int fw=f["canvas"][0],fh=f["canvas"][1];
 auto center=battleHexCenter({stack.x,stack.y});int cx=pose?int(std::lround(pose->x)):center.x,cy=pose?int(std::lround(pose->y)):center.y;bool flip=pose?pose->flip:stack.team==1;
 // Preserve the shared original canvas across frames. These battlefield
 // anchors are provisional until original-executable placement is compared.
 int anchor=unit.size==2?242:196;
 SDL_Rect canvas{cx-(flip?fw-anchor:anchor),cy-258,fw,fh};
 int index=pose?pose->frame:last?-1:int(animationTime_/100)+stack.id%7;
 return CombatVisual{unit.animation,group,index,canvas,flip};
}
bool Desktop::combatImage(const CombatStack&stack,int group,bool last,bool selected,bool targeted){
 auto visual=combatVisual(stack,group,last);if(!visual)return false;
 const auto&unit=Units[stack.unit];auto canvas=visual->canvas;bool flip=visual->flip;int index=visual->index;group=visual->group;int fw=canvas.w;
 if(selected||targeted){
  const auto&animation=demo_["def"][lower(unit.animation)];const auto&frames=animation["groups"][std::to_string(group)];
  const auto&current=frames[index<0?int(frames.size())-1:index%frames.size()];
  auto file=animation["pages"][current["page"].get<int>()]["file"].get<std::string>();
  SDL_Rect source{current["rect"][0],current["rect"][1],current["rect"][2],current["rect"][3]};
  auto key=file+"#selection:"+std::to_string(source.x)+":"+std::to_string(source.y);
  auto&mask=images_[key];
  if(!mask.texture){
   auto*page=readSurface(file,false,false);auto*surface=SDL_CreateRGBSurfaceWithFormat(0,source.w+2,source.h+2,32,SDL_PIXELFORMAT_RGBA32);
   if(page&&surface){
    SDL_FillRect(surface,nullptr,0);
    auto solid=[&](int x,int y){return x>=0&&y>=0&&x<source.w&&y<source.h&&static_cast<const Uint8*>(page->pixels)[(source.y+y)*page->pitch+(source.x+x)*4+3]>=192;};
    for(int y=-1;y<=source.h;++y)for(int x=-1;x<=source.w;++x){
     if(solid(x,y)||!(solid(x-1,y)||solid(x+1,y)||solid(x,y-1)||solid(x,y+1)))continue;
     auto*out=static_cast<Uint8*>(surface->pixels)+(y+1)*surface->pitch+(x+1)*4;out[0]=out[1]=out[2]=out[3]=255;
    }
    mask={SDL_CreateTextureFromSurface(renderer_,surface),source.w+2,source.h+2};
    if(mask.texture)SDL_SetTextureBlendMode(mask.texture,SDL_BLENDMODE_BLEND);
   }
   if(page)SDL_FreeSurface(page);if(surface)SDL_FreeSurface(surface);
  }
  if(mask.texture){
   int left=current["offset"][0],top=current["offset"][1];if(flip)left=fw-left-source.w;
   SDL_Color color=targeted?SDL_Color{0,215,255,255}:SDL_Color{244,226,40,255};SDL_SetTextureColorMod(mask.texture,color.r,color.g,color.b);
   SDL_Rect dest{canvas.x+left-1,canvas.y+top-1,source.w+2,source.h+2};SDL_RenderCopyEx(renderer_,mask.texture,nullptr,&dest,0,nullptr,flip?SDL_FLIP_HORIZONTAL:SDL_FLIP_NONE);SDL_SetTextureColorMod(mask.texture,255,255,255);
  }
 }
 return frame(unit.animation,group,index,canvas,flip,stack.origin==StackOrigin::Clone);
}
void Desktop::box(SDL_Rect r,SDL_Color c){SDL_SetRenderDrawColor(renderer_,c.r,c.g,c.b,c.a);SDL_RenderFillRect(renderer_,&r);}
void Desktop::outline(SDL_Rect r,SDL_Color c){SDL_SetRenderDrawColor(renderer_,c.r,c.g,c.b,c.a);SDL_RenderDrawRect(renderer_,&r);}
void Desktop::panel(SDL_Rect r){
 if(!settings_.hdArtwork){box(r,Brown);outline(r,{205,173,91,255});outline({r.x+2,r.y+2,r.w-4,r.h-4},{105,79,41,255});return;}
 box({r.x+3,r.y+4,r.w,r.h},{0,0,0,105});
 for(int y=0;y<std::max(1,r.h-1);y++){double t=double(y)/std::max(1,r.h-1);int grain=(y*37)%5;box({r.x,r.y+y,r.w,std::min(2,r.h-y)},{Uint8(47-15*t+grain),Uint8(37-14*t+grain),Uint8(29-12*t+grain),255});}
 outline(r,{170,132,66,255});outline({r.x+1,r.y+1,r.w-2,r.h-2},{71,50,27,255});outline({r.x+3,r.y+3,r.w-6,r.h-6},{99,74,41,255});
 box({r.x+1,r.y+1,r.w-2,1},{226,195,123,255});box({r.x+1,r.y+r.h-2,r.w-2,1},{41,26,12,255});
 for(Point p:{Point{r.x+6,r.y+6},Point{r.x+r.w-7,r.y+6},Point{r.x+6,r.y+r.h-7},Point{r.x+r.w-7,r.y+r.h-7}}){box({p.x-1,p.y-1,3,3},{112,83,42,255});box({p.x,p.y,1,1},{216,183,112,255});}
}
void Desktop::text(const std::string&s,int x,int y,int size,SDL_Color c,int wrap){if(s.empty())return;int face=fontKey(size);std::string key=s+"#"+std::to_string(face)+"#"+std::to_string(c.r)+":"+std::to_string(c.g)+":"+std::to_string(c.b)+":"+std::to_string(wrap);auto it=texts_.find(key);if(it==texts_.end()){if(texts_.size()>1200){for(auto&[k,i]:texts_)SDL_DestroyTexture(i.texture);texts_.clear();}auto*f=font(size);SDL_Surface*srf=wrap?TTF_RenderUTF8_Blended_Wrapped(f,s.c_str(),c,Uint32(wrap*pixelScale_)):TTF_RenderUTF8_Blended(f,s.c_str(),c);if(!srf)return;Image im{SDL_CreateTextureFromSurface(renderer_,srf),srf->w,srf->h};SDL_FreeSurface(srf);SDL_SetTextureScaleMode(im.texture,SDL_ScaleModeLinear);it=texts_.emplace(key,im).first;}auto&i=it->second;SDL_Rect r{x,y,int(std::round(i.w/pixelScale_)),int(std::round(i.h/pixelScale_))};SDL_RenderCopy(renderer_,i.texture,nullptr,&r);}
void Desktop::button(SDL_Rect r,const std::string&s,std::function<void()>action,bool enabled,bool invisible,const std::string& hint){
 std::string description=hint;
 if(description.empty()){
  static const std::map<std::string,std::string> help={{"−","减少拆分后转移的部队数量"},{"+","增加拆分后转移的部队数量"},{"一半","将来源部队的一半设为转移数量"},{"全部","将来源部队全部设为转移数量；英雄必须保留至少一名士兵"},{"◀","翻到上一页"},{"▶","翻到下一页"},{"返回","关闭当前界面，返回上一界面"},{"部队管理","查看部队，交换、合并、拆分或升级兵种"},{"魔法书","查看已学法术、魔法消耗和施法效果；战斗中可选择目标"}};
  auto found=help.find(s);description=found==help.end()?s:found->second;
 }
 buttons_.push_back({r,std::move(action),enabled,{},description});bool hover=inside(r,{mouseX_,mouseY_});
 if(!invisible){
  if(settings_.hdArtwork){bool pressed=mouseLeftDown_&&pressedScene_==scene&&pressedButton_==int(buttons_.size())-1;
   for(int y=0;y<std::max(1,r.h-1);y++){double t=double(y)/std::max(1,r.h-1);if(pressed)t=1-t;box({r.x,r.y+y,r.w,std::min(2,r.h-y)},enabled?SDL_Color{Uint8(115-53*t),Uint8(39-19*t),Uint8(32-17*t),255}:SDL_Color{Uint8(51-17*t),Uint8(43-14*t),Uint8(34-11*t),255});}
   outline(r,enabled?SDL_Color{185,145,74,255}:SDL_Color{99,83,57,255});outline({r.x+2,r.y+2,r.w-4,r.h-4},{63,29,17,255});box({r.x+1,r.y+1,r.w-2,1},pressed?SDL_Color{60,30,18,255}:SDL_Color{219,181,111,255});
   centerText(s,{r.x+1,r.y+1+(pressed?1:0),r.w,r.h},12,{12,8,5,210});centerText(s,{r.x,r.y+(pressed?1:0),r.w,r.h},12,enabled?GoldColor:SDL_Color{126,113,90,255});
  }else{box(r,enabled?SDL_Color{105,29,17,255}:SDL_Color{48,37,26,255});outline(r,enabled?GoldColor:Muted);outline({r.x+2,r.y+2,r.w-4,r.h-4},{139,105,50,255});text(s,r.x+7,r.y+(r.h-14)/2,12,enabled?GoldColor:SDL_Color{116,104,81,255});}
 }
 if(enabled&&hover){box(r,{238,207,111,settings_.hdArtwork?Uint8(18):Uint8(35)});outline(r,GoldColor);}
}
void Desktop::portrait(int unit,SDL_Rect dest,bool blue){if(unit<0||unit>=UnitCount)return;if(Units[unit].portraitFrame>=0&&frame("TwCrPort.def",0,Units[unit].portraitFrame,dest,false,blue))return;int original=Units[unit].originalId,col=0,row=0;if(Units[unit].faction>=0){row=Units[unit].faction;col=row==8?unit%14:original%14;}else{row=10;switch(original){case 116:col=2;break;case 117:col=3;break;case 138:col=6;break;case 139:col=7;break;case 140:col=8;break;case 141:col=9;break;case 142:col=0;break;case 143:col=1;break;case 144:row=9;col=0;break;case 136:col=4;break;case 137:col=5;break;case 134:col=10;break;case 135:col=11;break;case 133:col=12;break;case 132:col=13;break;}}art("original/creature-portraits/CreaturePortraits.png",dest,SDL_Rect{8+63*col,5+69*row,58,64},true,false,blue);}
void Desktop::say(const std::string&s){if(s.empty())return;toast_=s;toastUntil_=SDL_GetTicks64()+4500;}
void Desktop::persist(){
 if(!asynchronousSave_){std::string error;if(!game.save(saveDir_/"autosave.json",error))std::cerr<<"Autosave: "<<error<<'\n';return;}
 autosaveDirty_=true;auto now=fixedAnimationTime_.value_or(SDL_GetTicks64());if(marching_&&now<nextAutosave_)return;
 auto path=saveDir_/"autosave.json";if(!autosave_||autosave_->path()!=path)autosave_=std::make_unique<AsyncSaveWriter>(path);
 autosave_->submit(game);autosaveDirty_=false;nextAutosave_=now+1000;
}
void Desktop::flushAutosave(){if(autosaveDirty_){bool was=marching_;marching_=false;persist();marching_=was;}if(autosave_)autosave_->flush();}
void Desktop::load(bool automatic){flushAutosave();std::string error;auto p=saveDir_/(automatic?"autosave.json":"manual.json");if(!std::filesystem::exists(p))p=saveDir_/"autosave.json";if(game.load(p,error)){battleMotion_.clear();autoCombat_=false;if(audio_)audio_->clear();scene=game.battle?Scene::Battle:Scene::Adventure;camX_=game.hero.pos.x+.5f;camY_=game.hero.pos.y+.5f;route_.clear();marching_=false;sourceSlot_=-1;armyDrag_.reset();recruitDragging_=false;armyReturn_=Scene::Adventure;visitPopup_.reset();pendingSpell_=sacrificeTarget_=-1;marketObject_=marketLots_=0;marketDragging_=false;optionsDragging_=-1;say("原生存档已读取。");settle();}else say("读取失败："+error);}
void Desktop::settle(){if(battleMotion_.active()){scene=Scene::Battle;return;}if(!game.battle)autoCombat_=false;if(game.battle)scene=Scene::Battle;else if(game.report)scene=Scene::Report;else if(!game.hero.levelChoices.empty())scene=Scene::Level;else if(game.result&&scene==Scene::Adventure)scene=Scene::Outcome;else if(scene==Scene::Adventure){if(auto notice=game.takeAdventureVisit()){visitPopup_=std::move(notice);visitPage_=0;visitMessage_.clear();scene=visitPopup_->type==ObjectType::Obelisk?Scene::Puzzle:Scene::Visit;focus_=-1;marching_=false;route_.clear();destination_.reset();}else if(const auto*o=game.objectAt(game.hero.pos);o&&o->type==ObjectType::TradingPost)openMarket(o->id);}}
void Desktop::title(const std::string&s,std::optional<Scene> back){panel({12,10,776,580});text(s,34,27,21,GoldColor);if(back)button({700,22,65,28},"返回",[this,back]{scene=*back;sourceSlot_=-1;});}
std::string Desktop::cost(const Resources&r)const{std::string s;for(int i=0;i<ResourceCount;i++)if(r[i]){if(!s.empty())s+="  ";s+=std::to_string(r[i])+ResourceNames[i];}return s;}
void Desktop::resourceBar(int y,int width){
 int cell=width/ResourceCount;const int order[]={Wood,Mercury,Ore,Sulfur,Crystal,Gems,Gold};
 const SDL_Rect crops[]={{375,427,34,40},{18,426,33,40},{214,427,40,40},{180,427,33,40},{263,427,35,40},{298,427,30,40},{336,427,39,40}};
 for(int column=0;column<ResourceCount;column++){
  int resource=order[column],x=5+column*cell;
  bool original=false;if(cell>85){original=originalImage("aresbar.pcx",{x,y,25,19},SDL_Rect{85*column,0,25,22});original=originalImage("aresbar.pcx",{x+25,y,cell-27,19},SDL_Rect{85*column+25,0,58,22})&&original;original=originalImage("aresbar.pcx",{x+cell-2,y,2,19},SDL_Rect{85*column+83,0,2,22})&&original;}else original=originalImage("aresbar.pcx",{x,y,cell,19},SDL_Rect{85*column,0,85,22});
  if(!original){
   box({x,y,cell,19},{132,18,12,255});outline({x,y,cell,19},GoldColor);
   art("original/resources/Resources.png",{x+3,y+2,17,15},crops[resource]);
  }
  text(std::to_string(game.resources[resource]),x+25,y+2,10);
 }
}

void Desktop::armyRow(const Army&a,int y,bool garrison,int x,int slotW){for(int i=0;i<7;i++){SDL_Rect r{x+i*slotW,y,slotW-4,65};panel(r);if(!a[i].empty()){portrait(a[i].unit,{r.x+3,r.y+3,slotW-10,56});text(std::to_string(a[i].count),r.x+5,r.y+47,12);}if(sourceSlot_==i&&sourceGarrison_==garrison)outline({r.x-1,r.y-1,r.w+2,r.h+2},{255,241,125,255});button(r,"",[this,garrison,i]{selectStack(garrison,i,(pointerModifiers_.value_or(SDL_GetModState())&KMOD_SHIFT)!=0);},true,true,(a[i].empty()?std::string("空部队栏"):Units[a[i].unit].name+" × "+std::to_string(a[i].count))+"；点击选择与交换，Shift 点击拆分");}}
void Desktop::selectStack(bool garrison,int slot,bool shift){auto&a=garrison?game.town.garrison:game.hero.army;if(sourceSlot_<0){if(a[slot].empty())return;sourceSlot_=slot;sourceGarrison_=garrison;return;}if(sourceSlot_==slot&&sourceGarrison_==garrison){sourceSlot_=-1;return;}targetSlot_=slot;targetGarrison_=garrison;auto&from=sourceGarrison_?game.town.garrison:game.hero.army;if(shift){splitReturn_=scene;splitAmount_=std::max(1,from[sourceSlot_].count/2);scene=Scene::Split;return;}say(game.transfer(sourceGarrison_,sourceSlot_,garrison,slot,from[sourceSlot_].count));sourceSlot_=-1;persist();}
void Desktop::army(){title("部队管理",armyReturn_);text("先点击来源，再点击目标：整队移动、同类合并或不同兵种交换。",42,76,13);text("Shift 点击或触屏长按目标可拆分；英雄至少保留一个单位。",42,103,12,Muted);text("英雄部队",70,142,15,GoldColor);armyRow(game.hero.army,170,false);if(game.visiting()||armyReturn_==Scene::Town){text("城镇驻军",70,259,15,GoldColor);armyRow(game.town.garrison,287,true);}else text("进入狮鹫之巢后可交换城镇驻军。",70,287,14,Muted);
 if(sourceSlot_>=0){auto&s=(sourceGarrison_?game.town.garrison:game.hero.army)[sourceSlot_];if(!s.empty()){text(Units[s.unit].name+" × "+std::to_string(s.count),70,385,18,GoldColor);text("攻击 "+std::to_string(Units[s.unit].attack)+"  防御 "+std::to_string(Units[s.unit].defense)+"  生命 "+std::to_string(Units[s.unit].hp),70,421,13);button({70,469,250,35},"升级选中部队",[this]{say(game.upgrade(sourceGarrison_,sourceSlot_));persist();});button({370,469,250,35},"取消选择",[this]{sourceSlot_=-1;});}}resourceBar(562,775);}
void Desktop::split(){title("拆分部队",splitReturn_);if(sourceSlot_<0||targetSlot_<0){scene=splitReturn_;return;}auto&s=(sourceGarrison_?game.town.garrison:game.hero.army)[sourceSlot_];portrait(s.unit,{100,122,116,128});text(Units[s.unit].name+" × "+std::to_string(s.count),272,143,23,GoldColor);text("转移到"+std::string(targetGarrison_?"城镇驻军":"英雄部队")+"第 "+std::to_string(targetSlot_+1)+" 栏",272,190,16);
 button({105,312,55,40},"−",[this]{splitAmount_=std::max(1,splitAmount_-1);});text(std::to_string(splitAmount_),206,322,23);button({320,312,55,40},"+",[this,&s]{splitAmount_=std::min(s.count,splitAmount_+1);});button({416,312,99,40},"一半",[this,&s]{splitAmount_=std::max(1,s.count/2);});button({553,312,99,40},"全部",[this,&s]{splitAmount_=s.count;});
 button({105,423,250,40},"确认转移",[this]{say(game.transfer(sourceGarrison_,sourceSlot_,targetGarrison_,targetSlot_,splitAmount_));scene=splitReturn_;sourceSlot_=-1;persist();});button({418,423,235,40},"取消",[this]{scene=splitReturn_;sourceSlot_=-1;});}
void Desktop::university(){title("魔法大学",Scene::Town);text("教授四系魔法的初级技能，每门学费 2000 金币。",46,81,15,GoldColor);text("英雄最多掌握八项第二技能；已掌握的学派不能在这里提升等级。",46,114,13,Muted);const SDL_Rect icons[]={{265,307,82,94},{526,307,82,94},{787,307,82,94},{265,408,82,94}};int known=int(std::count_if(game.hero.skills.begin(),game.hero.skills.end(),[](int level){return level>0;}));for(int n=0;n<4;n++){int skill=AirMagic+n,x=42+n*184;panel({x,166,170,333});art("original/secondary-skills/sec_skills.png",{x+44,186,82,94},icons[n]);text(SkillNames[skill],x+21,303,19,GoldColor);text(game.hero.skills[skill]?SkillLevels[game.hero.skills[skill]]:"尚未掌握",x+35,341,13,Muted);text("学费 2000 金币",x+25,383,12);button({x+13,433,144,37},game.hero.skills[skill]?"已掌握":"学习",[this,skill]{say(game.learnMagic(skill));persist();},game.visiting()&&known<8&&game.hero.skills[skill]==0&&game.resources[Gold]>=2000,false,"学习初级"+SkillNames[skill]+"，花费 2000 金币；需要英雄在城内、有技能空位且尚未掌握此学派");}text("已掌握 "+std::to_string(known)+" / 8 项技能",46,521,14,GoldColor);resourceBar(562,775);}
void Desktop::openSpellbook(){
 if(!game.hero.spellbook){
  if(!game.battle&&game.visiting()&&game.mageGuildLevel()>0)scene=Scene::MageGuild;
  say("英雄需要在魔法行会花费 500 金币购买魔法书。");return;
 }
 if(game.battle)spellAdventure_=false;
 scene=Scene::Spells;
}
void Desktop::mageGuild(){
 originalImage("tpmage.pcx",{0,0,800,600});
 const char* views[]={"tpmagecs.pcx",nullptr,nullptr,nullptr,"tpmagenc.pcx","tpmagedn.pcx",nullptr,nullptr,nullptr};
 if(views[game.town.faction]&&demo_["pcx"].contains(views[game.town.faction]))art(demo_["pcx"][views[game.town.faction]]["file"].get<std::string>(),{333,76,141,217},{},true);
 std::string rollover;
 for(int level=1;level<=5;level++){
  auto spells=game.guildSpells(level);int slots=level<=game.townDef().maxMageGuild?int(spells.size()):0;
  for(int n=0;n<slots;n++){
   Point p;
   if(level==1)p={slots==6?254+n*86:264+n*104,449};
   else if(level==2)p={42+n%2*144,48+n/2*94};
   else if(level==3)p={564+n%2*113,79+n/2*81};
   else if(level==4)p={42+n/2*144,334+n%2*94};
   else p={slots==2?564+n*113:676,297};
   bool built=level<=game.mageGuildLevel();frame("tpmages.def",0,built?0:1,{p.x,p.y,83,61});
   if(!built)continue;
   int spell=spells[n],id=Spells[spell].originalId;
   art("original/spells/spells.png",{p.x+17,p.y+10,49,41},SDL_Rect{20+83*(id%11),500+66*(id/11),76,64});
   std::string status=game.hero.spells[spell]?"已学会":!game.hero.spellbook?"需要魔法书":level>game.learnableSpellLevel()?"需要智慧术":"英雄未入城";
   SDL_Rect rect{p.x,p.y,83,61};
   if(inside(rect,{mouseX_,mouseY_}))rollover=Spells[spell].name+" · "+status+" · "+game.spellDescription(spell);
   button(rect,"",[this,spell]{say(Spells[spell].name+"："+game.spellDescription(spell));},true,true,game.spellDescription(spell));
  }
 }
 // Purchasing is explicit until the original yes/no book dialog is available.
 if(!game.hero.spellbook)button({355,515,231,32},"购买魔法书 · 500 金币",[this]{say(game.buySpellbook());persist();},game.visiting()&&game.resources[Gold]>=500);
 else button({465,515,231,32},"打开魔法书",[this]{openSpellbook();});
 originalButton({750,556,48,40},"tpmage1.def",0,[this]{scene=Scene::Town;focus_=-1;});
 centerText(!rollover.empty()?rollover:toastUntil_>SDL_GetTicks64()?toast_:"魔法行会 · "+Factions[game.town.faction].name+" · 符合条件的法术会自动学会",{8,556,733,18},10);
 resourceBar(579,735);
}
void Desktop::spellbook(){
 box({0,0,800,600},{0,0,0,255});
 if(!originalImage("spelback.pcx",{90,2,620,595}))art("original/spellbook/spellbook.png",{90,2,620,596},SDL_Rect{0,0,620,596},false);
 const SDL_Color ink{56,30,13,255};
 const char*schoolNames[]={"全部法术","气系魔法","火系魔法","","水系魔法","","","","土系魔法"};
 text(schoolNames[spellFilter_],178,80,15,ink);text(spellAdventure_?"冒险法术":"战斗法术",438,80,12,ink);
 std::vector<int>available;
 for(int i=0;i<SpellCount;i++)if(game.hero.spells[i]&&Spells[i].adventure==spellAdventure_&&(spellFilter_==0||(Spells[i].school&spellFilter_)))available.push_back(i);
 std::stable_sort(available.begin(),available.end(),[](int a,int b){return Spells[a].level!=Spells[b].level?Spells[a].level<Spells[b].level:Spells[a].originalId<Spells[b].originalId;});
 int pages=std::max(1,(int(available.size())+11)/12);spellPage_=std::clamp(spellPage_,0,pages-1);
 std::string description;
 for(int n=0;n<12&&spellPage_*12+n<int(available.size());n++){
  int spell=available[spellPage_*12+n],page=n/6,cell=n%6;
  int x=167+page*247+(cell%2)*104,y=105+(cell/2)*100;
  SDL_Rect rect{x,y,100,97};
  bool enabled=Spells[spell].implemented&&(!game.battle||(game.current()&&game.current()->team==0&&game.battle->spellRound!=game.battle->round&&game.hero.mana>=game.spellCost(spell)));
  int id=Spells[spell].originalId;
  art("original/spells/spells.png",{x+12,y,76,64},SDL_Rect{20+83*(id%11),500+66*(id/11),76,64});
  int glyphs=int(std::count_if(Spells[spell].name.begin(),Spells[spell].name.end(),[](unsigned char c){return(c&0xc0)!=0x80;}));
  text(Spells[spell].name,x+50-glyphs*10/2,y+62,10,enabled?ink:SDL_Color{117,94,65,255});
  text(std::to_string(Spells[spell].level)+"级 / "+SkillLevels[game.spellMastery(spell)],x+21,y+75,9,ink);
  text("魔法 "+std::to_string(game.spellCost(spell))+(Spells[spell].kind==SpellKind::Summon?" · 召唤":game.spellAutomatic(spell)?" · 群体":""),x+19,y+86,9,ink);
  std::string spellHelp=game.spellDescription(spell)+"\n消耗 "+std::to_string(game.spellCost(spell))+" 魔法值";
  if(!Spells[spell].implemented)spellHelp+="；当前版本尚未实现";
  else if(game.battle){if(game.battle->spellRound==game.battle->round)spellHelp+="；本轮已经施放过法术";else if(game.hero.mana<game.spellCost(spell))spellHelp+="；魔法值不足";else if(!game.current()||game.current()->team!=0)spellHelp+="；请等到我方行动";}
  button(rect,"",[this,spell]{if(game.battle){if(game.spellAutomatic(spell)){battleAction([this,spell]{return game.cast(spell);});pendingSpell_=-1;}else{pendingSpell_=spell;sacrificeTarget_=-1;say(game.spellDescription(spell));}scene=Scene::Battle;settle();}else say(game.spellDescription(spell));},enabled,true,spellHelp);
  if(inside(rect,{mouseX_,mouseY_}))description=game.spellDescription(spell);
 }
 const int filters[]={AirSchool,EarthSchool,FireSchool,WaterSchool,0};
 const SDL_Rect ribbonSources[]={{626,197,60,50},{646,259,60,50},{646,318,60,50},{650,378,60,50},{646,439,60,50}};
 for(int n=0;n<5;n++){SDL_Rect r{642,139+n*53,60,50};art("original/spellbook/spellbook.png",r,ribbonSources[n]);button(r,"",[this,filter=filters[n]]{spellFilter_=filter;spellPage_=0;},true,true,std::string("筛选")+schoolNames[filters[n]]);if(spellFilter_==filters[n])outline({r.x+3,r.y+4,49,37},GoldColor);}
 text(std::to_string(game.hero.mana),518,420,12,GoldColor);
 text(description.empty()?"魔法值 "+std::to_string(game.hero.mana)+" / "+std::to_string(game.maxMana()):description,132,570,10,GoldColor,530);
 button({308,407,36,42},"",[this]{spellAdventure_=false;spellPage_=0;},true,true,"查看战斗法术；只有战斗中可以施放");
 button({444,407,36,42},"",[this]{spellAdventure_=true;spellPage_=0;},!game.battle,true,"查看冒险法术；战斗中不能切换到此页");
 if(spellPage_>0)button({146,410,36,28},"◀",[this]{spellPage_--;});
 if(spellPage_+1<pages)button({610,410,36,28},"▶",[this]{spellPage_++;});
 button({570,407,36,42},"",[this]{scene=game.battle?Scene::Battle:Scene::Hero;},true,true,"关闭魔法书，返回之前的界面（Esc）");
}
void Desktop::level(){if(game.hero.levelChoices.empty()){afterReport();return;}title("英雄升级",std::nullopt);auto&choice=game.hero.levelChoices.front();const char*primary[]={"攻击","防御","力量","知识"};heroPortrait(game.hero,{70,91,116,128});text(game.hero.name+"升至 "+std::to_string(choice.level)+" 级",231,106,24,GoldColor);text(std::string(primary[choice.primary])+" +1",231,154,18);text("选择一项次要技能",231,194,15,Muted);
 for(size_t i=0;i<choice.skills.size();i++){int id=choice.skills[i],x=82+int(i)*345;panel({x,283,310,165});text(SkillLevels[game.hero.skills[id]+1],x+20,305,14,Muted);text(SkillNames[id],x+20,336,23,GoldColor);button({x+20,391,269,35},"学习这项技能",[this,id]{say(game.chooseSkill(id));persist();if(game.hero.levelChoices.empty())afterReport();},true,false,"选择"+std::string(SkillLevels[game.hero.skills[id]+1])+SkillNames[id]+"；必须选定一项才能继续冒险");}
 // Upgrade choices cannot be dismissed before selecting.
}
void Desktop::report(){
 if(!game.report){settle();return;}
 if(battleMotion_.retainedView())battle();else{originalImage("cmbkgrtr.pcx",{0,0,800,556});originalImage("cbar.pcx",{0,556,800,44});}
 buttons_.clear();box({0,0,800,600},{0,0,0,100});originalImage("cpresult.pcx",{165,20,470,561});
 const auto&r=*game.report;
 heroPortrait(game.hero,{184,56,58,64});
 int enemy=-1;for(const auto& stack:r.enemies)if(!stack.empty()){enemy=stack.unit;break;}if(enemy>=0)portrait(enemy,{558,56,58,64});
 centerText(game.hero.name,{250,55,140,21},13);centerText("守军",{405,55,140,21},13);
 centerText(r.won?"胜利":"失败",{181,137,77,22},12);centerText(r.won?"失败":"胜利",{547,137,77,22},12);
 // The original result movie is unavailable in the verified demo assets.
 // Retain its exact inset with a static original battlefield illustration.
 originalImage("cmbkgrtr.pcx",{272,90,256,119},SDL_Rect{100,20,600,279});
 centerText(r.won?"战斗胜利":"战斗失败",{272,128,256,35},23,GoldColor);
 centerText(game.result>0?(game.imported()?"已达成地图的胜利条件！":"黑鸦堡已占领，地图胜利！"):game.result<0?"全军覆没。":r.won?"一场辉煌的胜利！":"你的部队战败了。",{232,229,336,23},15,GoldColor);
 centerText(game.hero.name+"获得 "+std::to_string(r.xp)+" 经验",{232,264,336,23},13);
 centerText("战场损失",{205,309,390,27},20,GoldColor);
 auto losses=[&](const Army&before,const Army*after,int y){
  std::vector<std::pair<int,int>> lost;
  for(int i=0;i<ArmySlots;i++)if(!before[i].empty()){int n=before[i].count-(after?(*after)[i].count:0);if(n>0)lost.push_back({before[i].unit,n});}
  if(lost.empty()){centerText("无",{207,y+4,386,30},13);return;}
  int x=400-int(lost.size())*24;
  for(auto [unit,count]:lost){portrait(unit,{x+5,y,34,37});centerText(std::to_string(count),{x,y+38,44,17},11);x+=48;}
 };
 centerText("进攻方",{205,339,390,25},18,GoldColor);losses(r.before,&r.after,366);
 centerText("防守方",{205,435,390,25},18,GoldColor);
 if(r.won)losses(r.enemies,nullptr,462);
 else if(const auto* final=battleMotion_.retainedView()){
  Army remaining{};for(const auto& stack:final->units)if(stack.team==1&&stack.origin==StackOrigin::Army&&stack.slot>=0&&stack.slot<ArmySlots&&stack.permanentCount>0)remaining[stack.slot]={stack.unit,stack.permanentCount};
  losses(r.enemies,&remaining,462);
 }else centerText("敌方损失未记录",{207,466,386,30},12);
 originalButton({550,526,64,32},"iok6432.def",0,[this]{game.report.reset();afterReport();camX_=game.hero.pos.x+.5f;camY_=game.hero.pos.y+.5f;mouseX_=mouseY_=-1;scrollTick_=0;persist();});
}
void Desktop::help(){
#ifdef __IPHONEOS__
 title("触屏操作指南",Scene::Menu);
 text("点击目标规划路线，再次点击同一目标开始移动。",40,100,16);
 text("单指拖动地图，双指分开放大、合拢缩小。",40,160,16);
 text("先选部队来源，长按目标栏位，可选择拆分数量。",40,220,16);
 text("战斗中点击敌军攻击，点击空地移动，点魔法书施法。",40,280,16);
 text("选中法术后，长按战场可取消，不消耗魔法值。",40,340,16);
 text("支持横屏游玩；切到后台时停止行军并暂停声音。",40,400,16);
 button({40,515,220,35},"返回游戏",[this]{scene=Scene::Adventure;settle();});return;
#endif
 title("C++ 原生版 · 游戏指南",Scene::Menu);int ww,wh,rw,rh;SDL_GetWindowSize(window_,&ww,&wh);SDL_GetRendererOutputSize(renderer_,&rw,&rh);text("C++20 / SDL · 窗口 "+std::to_string(ww)+"×"+std::to_string(wh)+" / 像素 "+std::to_string(rw)+"×"+std::to_string(rh),40,82,15,GoldColor);text("冒险地图：点击目标规划路线，再次点击或 M 行军。双指滑动或滚轮移动地图；按住后上下拖动缩放，上移放大、下移缩小。",40,126,13,Muted,710);text("H 英雄，T 城镇，C 魔法书，B 市场，O 系统选项，F10 静音，E 结束回合，F4 / Cmd+F 全屏。",40,175,13,Muted,710);text("S 保存到 saves/native/manual.json，L 读取原生存档。战斗中同样可以存读档。",40,224,13,Muted,710);text("城镇：每日建设一次，每周补充兵源。可以远程招募进驻军，英雄入城后才能交换。",40,273,13,Muted,710);text("部队：点击来源和目标整队交换；Shift 点击目标拆分；高级巢穴允许升级已有部队。",40,322,13,Muted,710);text("战斗：点击敌军攻击，点击空地移动。D 或空格防御，W 等待，C 施法，右键取消。",40,371,13,Muted,710);text("已接入九阵营建设与招募。特殊建筑、完整兵种能力、魔法、战役、联机、攻城及动画仍未完成。",40,420,13,Muted,710);button({40,515,220,35},"继续游戏",[this]{scene=Scene::Adventure;settle();});button({306,515,220,35},"保存游戏",[this]{std::string e;say(game.save(saveDir_/"manual.json",e)?"原生存档已保存。":e);});}
void Desktop::render(){animationTime_=fixedAnimationTime_.value_or(SDL_GetTicks64());int w=0,h=0;SDL_GetRendererOutputSize(renderer_,&w,&h);int ww=0,wh=0;SDL_GetWindowSize(window_,&ww,&wh);auto layout=displayLayout(ww,wh,w,h,scene==Scene::Adventure);pixelScale_=layout.pixelScale;
 int cw=layout.pixelWidth,ch=layout.pixelHeight;
 if(!canvas_.texture||canvas_.w!=cw||canvas_.h!=ch){
  if(canvas_.texture)SDL_DestroyTexture(canvas_.texture);
  canvas_={SDL_CreateTexture(renderer_,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,cw,ch),cw,ch};
  if(!canvas_.texture)throw std::runtime_error(SDL_GetError());SDL_SetTextureBlendMode(canvas_.texture,SDL_BLENDMODE_NONE);SDL_SetTextureScaleMode(canvas_.texture,SDL_ScaleModeNearest);
 }
 if(SDL_SetRenderTarget(renderer_,canvas_.texture)!=0)throw std::runtime_error(SDL_GetError());
 SDL_RenderSetViewport(renderer_,nullptr);SDL_RenderSetScale(renderer_,float(pixelScale_),float(pixelScale_));SDL_RenderSetClipRect(renderer_,nullptr);buttons_.clear();SDL_SetRenderDrawColor(renderer_,7,6,5,255);SDL_RenderClear(renderer_);switch(scene){case Scene::Menu:menu();break;case Scene::Adventure:adventure();break;case Scene::Town:town();break;case Scene::Build:build();break;case Scene::Recruit:recruit();break;case Scene::Army:army();break;case Scene::Split:split();break;case Scene::Hero:hero();break;case Scene::Spells:spellbook();break;case Scene::Battle:battle();break;case Scene::Level:level();break;case Scene::Report:report();break;case Scene::Help:help();break;case Scene::Codex:codex();break;case Scene::Scenario:scenario();break;case Scene::University:university();break;case Scene::MageGuild:mageGuild();break;case Scene::Market:market();break;case Scene::BuildConfirm:buildConfirm();break;case Scene::Fort:fort();break;case Scene::Options:options();break;case Scene::Outcome:outcome();break;case Scene::Visit:visitDialog();break;case Scene::Puzzle:puzzle();break;case Scene::Maps:mapLibrary();break;case Scene::Tavern:tavern();break;case Scene::Journal:journal();break;case Scene::Meeting:meeting();break;}if(scene!=Scene::Menu&&focus_>=0&&focus_<int(buttons_.size())){auto r=buttons_[focus_].rect;outline({r.x-2,r.y-2,r.w+4,r.h+4},{255,245,170,255});}if(toastUntil_>SDL_GetTicks64()&&scene!=Scene::Adventure&&scene!=Scene::Market&&scene!=Scene::Town&&scene!=Scene::Build&&scene!=Scene::BuildConfirm&&scene!=Scene::Fort&&scene!=Scene::Recruit&&scene!=Scene::Options&&scene!=Scene::Battle&&scene!=Scene::MageGuild&&scene!=Scene::Report&&scene!=Scene::Level&&scene!=Scene::Outcome&&scene!=Scene::Visit&&scene!=Scene::Puzzle&&scene!=Scene::Maps){panel({80,555,640,35});text(toast_,92,565,12,GoldColor,610);}drawButtonHint();drawAdventureCursor();SDL_RenderFlush(renderer_);SDL_SetRenderTarget(renderer_,nullptr);SDL_RenderSetViewport(renderer_,nullptr);SDL_RenderSetScale(renderer_,1,1);SDL_RenderSetClipRect(renderer_,nullptr);
 SDL_SetRenderDrawColor(renderer_,0,0,0,255);SDL_RenderClear(renderer_);SDL_Rect full{(w-canvas_.w)/2,(h-canvas_.h)/2,canvas_.w,canvas_.h};SDL_RenderCopy(renderer_,canvas_.texture,nullptr,&full);SDL_RenderPresent(renderer_);updateAudio();}
void Desktop::click(Point p,bool shift){if(scene==Scene::Adventure&&inside(adventureMapRect(),p)){if(game.result)return;if(marching_){marching_=false;say("已停止移动；原路线保留。");return;}auto*object=adventureObjectAt(p);if(object&&object->type==ObjectType::EnemyHero&&object->owner==0&&game.heroes.contains(object->id)&&std::max(std::abs(object->pos.x-game.hero.pos.x),std::abs(object->pos.y-game.hero.pos.y))<=1){meetingHero_=object->id;scene=Scene::Meeting;return;}auto goal=object?object->visitPosition(mapPoint(p)):mapPoint(p);if(goal==game.hero.pos){if(game.visiting()&&!game.guardingMonster(goal))scene=Scene::Town;else if(object){int id=object->id;say(game.visit(id));persist();settle();}else scene=Scene::Hero;return;}if(destination_&&*destination_==goal){marching_=!route_.empty();return;}auto route=game.path(goal);if(route.empty()){route_.clear();destination_.reset();say("尚未探索，或没有安全路线；请先攻击挡路的野兵。");return;}route_=std::move(route);destination_=goal;if(!settings_.showPath)marching_=true;int n=0;for(auto&s:route_)n+=s.cost;say("路线 "+std::to_string(route_.size())+" 格 / "+std::to_string(n)+" 行动力"+(game.guardingMonster(goal,true)?"；目标有野兵守卫，抵达时先进入战斗。":"")+(settings_.showPath?"；再次点击或按 M 行军。":"；正在移动。"));return;}
 if(scene==Scene::Adventure){auto mini=adventureMinimapRect();if(inside(mini,p)){camX_=float(p.x-mini.x)/mini.w*game.mapWidth;camY_=float(p.y-mini.y)/mini.h*game.mapHeight;return;}}(void)shift;for(auto it=buttons_.rbegin();it!=buttons_.rend();++it)if(it->contains(p)){if(!it->enabled)return;auto action=it->action;focus_=-1;marching_=false;if(audio_)audio_->effect("button.wav");action();return;}}
void Desktop::key(SDL_Keycode code,SDL_Keymod mod){
 if(scene==Scene::Maps){
  if(code==SDLK_ESCAPE){scene=Scene::Scenario;focus_=-1;return;}
  if(code==SDLK_UP||code==SDLK_DOWN){selectMap(std::clamp(selectedMap_+(code==SDLK_DOWN?1:-1),0,int(mapLibrary_["entries"].size())-1));return;}
  if(code==SDLK_PAGEUP||code==SDLK_PAGEDOWN){selectMap(std::clamp(selectedMap_+(code==SDLK_PAGEDOWN?6:-6),0,int(mapLibrary_["entries"].size())-1));return;}
 }
 if(scene==Scene::Scenario){
  if(code>=SDLK_1&&code<=SDLK_9){scenarioFaction_=int(code-SDLK_1);focus_=-1;return;}
  if((code==SDLK_RETURN||code==SDLK_KP_ENTER)&&focus_<0){startScenario();return;}
  if(code==SDLK_ESCAPE){scene=Scene::Menu;focus_=-1;return;}
 }
 if(scene==Scene::Visit||scene==Scene::Puzzle){
  if(code==SDLK_ESCAPE){scene=Scene::Adventure;visitPopup_.reset();focus_=-1;return;}
  if(code!=SDLK_TAB&&code!=SDLK_UP&&code!=SDLK_DOWN&&code!=SDLK_RETURN&&code!=SDLK_KP_ENTER&&code!=SDLK_F4&&code!=SDLK_F11&&code!=SDLK_F7&&code!=SDLK_F10&&!((mod&KMOD_GUI)&&(code==SDLK_f||code==SDLK_q)))return;
 }
 // A terminal result stays visible until the player acknowledges its button.
 if(scene==Scene::Outcome&&code!=SDLK_TAB&&code!=SDLK_UP&&code!=SDLK_DOWN&&code!=SDLK_RETURN&&code!=SDLK_KP_ENTER&&code!=SDLK_F4&&code!=SDLK_F11&&code!=SDLK_F7&&code!=SDLK_F10&&!((mod&KMOD_GUI)&&(code==SDLK_f||code==SDLK_q)))return;
 if(scene==Scene::Menu){
  if(menuView_!=MenuView::Main&&code==SDLK_ESCAPE){menuView_=MenuView::Main;focus_=-1;return;}
  if(menuView_==MenuView::Main){
   if(code==SDLK_n){scene=Scene::Scenario;focus_=-1;return;}
   if(code==SDLK_h){menuView_=MenuView::Scores;focus_=-1;return;}
   if(code==SDLK_c){menuView_=MenuView::Credits;focus_=-1;return;}
   if(code==SDLK_i){scene=Scene::Codex;focus_=-1;return;}
   if(code==SDLK_q){running_=false;return;}
  }
 }
 if(code==SDLK_F7){settings_.hdArtwork=!settings_.hdArtwork;applyOptions();return;}
 if(code==SDLK_F6&&(game.battle||battleMotion_.active())){settings_.battleGrid=!settings_.battleGrid;applyOptions();return;}
 if(code==SDLK_ESCAPE&&autoCombat_&&(game.battle||battleMotion_.active())){autoCombat_=false;return;}
 if(code==SDLK_F10){settings_.muted=!settings_.muted;applyOptions();return;}
 if(code==SDLK_F1){scene=Scene::Help;focus_=-1;return;}
 if(code==SDLK_o&&(scene==Scene::Menu||scene==Scene::Adventure||scene==Scene::Town||scene==Scene::Battle)){openOptions();return;}
 if(scene==Scene::Options&&code==SDLK_ESCAPE){scene=optionsReturn_;optionsDragging_=-1;focus_=-1;return;}
 if(scene==Scene::Options&&(code==SDLK_LEFT||code==SDLK_RIGHT||code==SDLK_HOME||code==SDLK_END)){
  int&volume=(mod&KMOD_SHIFT)?settings_.effects:settings_.music;volume=code==SDLK_HOME?0:code==SDLK_END?9:std::clamp(volume+(code==SDLK_RIGHT?1:-1),0,9);applyOptions();return;
 }
if(code==SDLK_TAB||code==SDLK_DOWN||code==SDLK_UP){if(buttons_.empty())return;int direction=(code==SDLK_UP||(mod&KMOD_SHIFT))?-1:1;int n=int(buttons_.size());if(focus_<0||focus_>=n)focus_=direction>0?-1:0;for(int i=0;i<n;i++){focus_=(focus_+direction+n)%n;if(buttons_[focus_].enabled)break;}return;}if((code==SDLK_RETURN||code==SDLK_KP_ENTER)&&scene==Scene::Recruit&&focus_<0){
  purchaseRecruit();return;
 }
 if((code==SDLK_RETURN||code==SDLK_KP_ENTER)&&(scene==Scene::Visit||scene==Scene::Hero)&&focus_<0&&!buttons_.empty()){auto action=buttons_.back().action;focus_=-1;action();return;}
 if((code==SDLK_RETURN||code==SDLK_KP_ENTER)&&scene==Scene::Market&&focus_<0&&marketLots_>0){say(game.trade(marketSold_,marketBought_,marketLots_,marketObject_));marketLots_=0;persist();return;}
 if(code==SDLK_RETURN||code==SDLK_KP_ENTER){if(!buttons_.empty()){int n=focus_>=0&&focus_<int(buttons_.size())?focus_:0;if(buttons_[n].enabled){auto action=buttons_[n].action;focus_=-1;marching_=false;if(audio_)audio_->effect("button.wav");action();}}return;}if((mod&KMOD_GUI)&&code==SDLK_q){running_=false;return;}if(code==SDLK_F4||code==SDLK_F11||((mod&KMOD_GUI)&&code==SDLK_f)){toggleFullscreen();return;}if(code==SDLK_s){std::string e;say(game.save(saveDir_/"manual.json",e)?"C++ 原生存档已保存。":e);return;}if(code==SDLK_l){load(bool(mod&KMOD_SHIFT));return;}if(code==SDLK_ESCAPE&&scene==Scene::Tavern){scene=Scene::Town;return;}if(code==SDLK_ESCAPE&&(scene==Scene::Journal||scene==Scene::Meeting)){scene=Scene::Adventure;return;}if(code==SDLK_ESCAPE&&(scene==Scene::Build||scene==Scene::Fort||scene==Scene::MageGuild||scene==Scene::University)){scene=Scene::Town;focus_=-1;return;}
 if(code==SDLK_ESCAPE&&scene==Scene::BuildConfirm){scene=buildingReturn_;focus_=-1;return;}
 if(code==SDLK_ESCAPE&&scene==Scene::Recruit){scene=recruitReturn_;recruitDragging_=false;focus_=-1;return;}
 if(code==SDLK_ESCAPE&&scene==Scene::Split){scene=splitReturn_;sourceSlot_=-1;focus_=-1;return;}
 if(code==SDLK_ESCAPE&&scene==Scene::Army){scene=armyReturn_;sourceSlot_=-1;focus_=-1;return;}
 if(code==SDLK_a&&scene==Scene::Battle&&(game.battle||battleMotion_.active())){autoCombat_=!autoCombat_;pendingSpell_=sacrificeTarget_=-1;nextAI_=0;return;}
 if(battleMotion_.active()&&scene==Scene::Battle)return;
 if(code==SDLK_ESCAPE&&scene==Scene::Market){scene=marketReturn_;marketDragging_=false;return;}if(code==SDLK_ESCAPE){pendingSpell_=sacrificeTarget_=-1;if(scene==Scene::Level||scene==Scene::Report)return;scene=(game.battle||battleMotion_.active())?Scene::Battle:scene==Scene::Adventure?Scene::Menu:Scene::Adventure;sourceSlot_=-1;return;}
 if(game.battle){if(autoCombat_)return;if(scene==Scene::Battle&&game.current()&&game.current()->team==0){if(code==SDLK_d||code==SDLK_SPACE){battleAction([this]{return game.act("defend");});}if(code==SDLK_w){battleAction([this]{return game.act("wait");});}if(code==SDLK_c)openSpellbook();}return;}
 if(scene==Scene::Market){int limit=game.tradeLimit(marketSold_,marketBought_,marketObject_);if(code==SDLK_LEFT)marketLots_=std::max(0,marketLots_-1);if(code==SDLK_RIGHT)marketLots_=std::min(limit,marketLots_+1);if(code==SDLK_HOME)marketLots_=0;if(code==SDLK_END)marketLots_=limit;return;}
 if(scene==Scene::Recruit){
  int limit=recruitLimit();if(code==SDLK_LEFT)recruitAmount_=std::max(0,recruitAmount_-1);if(code==SDLK_RIGHT)recruitAmount_=std::min(limit,recruitAmount_+1);if(code==SDLK_HOME)recruitAmount_=0;if(code==SDLK_END)recruitAmount_=limit;
  if(code==SDLK_g&&game.visiting()){recruitGarrison_=!recruitGarrison_;recruitAmount_=std::min(recruitAmount_,recruitLimit());}
  if(code==SDLK_u&&game.town.built[UpGuardhouse+baseTier(recruitUnit_)]){recruitUnit_^=1;recruitAmount_=std::min(recruitAmount_,recruitLimit());}return;
 }
 if(scene==Scene::Town){if(code==SDLK_h){scene=Scene::Build;focus_=-1;return;}if(code==SDLK_r){scene=Scene::Fort;focus_=-1;return;}if(code==SDLK_a){armyReturn_=Scene::Town;scene=Scene::Army;sourceSlot_=-1;focus_=-1;return;}}
 if((scene==Scene::Town||scene==Scene::Adventure)&&code==SDLK_b){openMarket();return;}
 if(scene!=Scene::Adventure||!game.hero.levelChoices.empty())return;switch(code){case SDLK_p:scene=Scene::Puzzle;marching_=false;break;case SDLK_d:say(game.dig());persist();break;case SDLK_t:if(game.hasTown())scene=Scene::Town;break;case SDLK_h:scene=Scene::Hero;break;case SDLK_n:{auto ids=game.ownedHeroes();std::erase_if(ids,[this](int id){return game.heroes[id].sleeping;});if(!ids.empty()){auto at=std::find(ids.begin(),ids.end(),game.activeHeroId);activateHero(at==ids.end()||++at==ids.end()?ids.front():*at);}break;}case SDLK_c:openSpellbook();break;case SDLK_e:say(game.endDay());persist();settle();break;case SDLK_m:marching_=!route_.empty();break;case SDLK_SPACE:camX_=game.hero.pos.x+.5f;camY_=game.hero.pos.y+.5f;if(game.visiting())scene=Scene::Town;else if(auto*o=game.objectAt(game.hero.pos))say(game.visit(o->id));persist();settle();break;default:break;}}
void Desktop::tick(){uint64_t now=fixedAnimationTime_.value_or(SDL_GetTicks64());bool moving=battleMotion_.active();battleMotion_.tick(now,scene==Scene::Battle);if(moving){if(!battleMotion_.active()){settle();nextAI_=now+550;}return;}
 if(scene==Scene::Adventure&&!drag_&&!marching_&&(SDL_GetWindowFlags(window_)&SDL_WINDOW_MOUSE_FOCUS)&&inside(adventureMapRect(),{mouseX_,mouseY_})){
  uint64_t elapsed=scrollTick_?std::min<uint64_t>(now-scrollTick_,50):0;float distance=float(elapsed)*std::array<float,3>{3,6,10}[settings_.scrollSpeed]/1000;
  auto map=adventureMapRect();if(mouseX_<map.x+8)camX_=std::max(0.f,camX_-distance);if(mouseX_>=map.x+map.w-8)camX_=std::min(float(game.mapWidth),camX_+distance);
  if(mouseY_<map.y+8)camY_=std::max(0.f,camY_-distance);if(mouseY_>=map.y+map.h-8)camY_=std::min(float(game.mapHeight),camY_+distance);
 }scrollTick_=now;
 if(marching_&&scene==Scene::Adventure&&now>=nextStep_){nextStep_=now+std::array<int,4>{340,220,160,110}[settings_.heroSpeed];if(route_.empty()){marching_=false;return;}Point p{route_.front().x,route_.front().y};auto before=game.hero.pos;int oldLevel=game.mapLevel;say(game.move(p));if(game.hero.pos==before&&game.mapLevel==oldLevel){marching_=false;if(game.battle){route_.clear();destination_.reset();persist();}settle();return;}if(game.mapLevel!=oldLevel||game.hero.pos!=p){route_.clear();destination_.reset();marching_=false;camX_=game.hero.pos.x+.5f;camY_=game.hero.pos.y+.5f;persist();settle();return;}heroDirection_={p.x-before.x,p.y-before.y};route_.erase(route_.begin());persist();settle();if(game.battle||!game.hero.levelChoices.empty()){marching_=false;route_.clear();destination_.reset();}auto sc=mapScreen(game.hero.pos);auto map=adventureMapRect();if(sc.x<map.x+37||sc.x>map.x+map.w-35||sc.y<map.y+37||sc.y>map.y+map.h-29){camX_=game.hero.pos.x+.5f;camY_=game.hero.pos.y+.5f;}}
 if(scene==Scene::Battle&&game.current()&&(game.current()->team==1||autoCombat_)&&now>=nextAI_){nextAI_=now+550;battleAction([this]{game.ai();return std::string{};});}}
void Desktop::movePointer(Point p){
 mouseX_=p.x;mouseY_=p.y;
 if(minimapDragging_&&scene==Scene::Adventure){auto mini=adventureMinimapRect();camX_=std::clamp(float(p.x-mini.x)/mini.w*game.mapWidth,0.f,float(game.mapWidth));camY_=std::clamp(float(p.y-mini.y)/mini.h*game.mapHeight,0.f,float(game.mapHeight));auto camera=mapCamera();camX_=camera[0];camY_=camera[1];}
 if(marketDragging_&&scene==Scene::Market)marketAmount(p.x);
 if(recruitDragging_&&scene==Scene::Recruit)recruitAmount(p.x);
 if(optionsDragging_>=0&&scene==Scene::Options)optionVolume(optionsDragging_,mouseX_);
 if(armyDrag_&&scene==Scene::Town&&std::abs(mouseX_-armyDrag_->start.x)+std::abs(mouseY_-armyDrag_->start.y)>5)armyDrag_->moving=true;
 if(drag_){int dx=p.x-dragX_,dy=p.y-dragY_;if(std::abs(dx)+std::abs(dy)>5)dragged_=true;
  if(dragged_){
   if(touchPrimary_){camX_=std::clamp(dragCamX_-dx/(32*zoom_),0.f,float(game.mapWidth));camY_=std::clamp(dragCamY_-dy/(32*zoom_),0.f,float(game.mapHeight));}
   else{
    zoom_=std::clamp(dragZoom_*std::exp(std::clamp(-dy/180.f,-2.f,2.f)),.65f,1.8f);
    auto map=adventureMapRect();float offset=(1/dragZoom_-1/zoom_)/32;
    camX_=dragCamX_+(dragX_-map.x-map.w/2)*offset;camY_=dragCamY_+(dragY_-map.y-map.h/2)*offset;
    auto camera=mapCamera();camX_=camera[0];camY_=camera[1];
   }
  }}
}
void Desktop::handleEvent(const SDL_Event&e){
 switch(e.type){
 case SDL_FINGERDOWN:case SDL_FINGERMOTION:case SDL_FINGERUP:touchEvent(e.tfinger);break;
 case SDL_APP_WILLENTERBACKGROUND:background_=true;marching_=false;cancelPointer();if(audio_)audio_->pause(true);persist();flushAutosave();break;
 case SDL_APP_DIDENTERFOREGROUND:background_=false;if(audio_)audio_->pause(false);break;
 case SDL_QUIT:running_=false;break;
 case SDL_RENDER_TARGETS_RESET:resetRenderTextures(false);break;
 case SDL_RENDER_DEVICE_RESET:resetRenderTextures(true);break;
 case SDL_KEYDOWN:if(!e.key.repeat)key(e.key.keysym.sym,SDL_Keymod(e.key.keysym.mod));break;
 case SDL_WINDOWEVENT:
  if(e.window.event==SDL_WINDOWEVENT_LEAVE||e.window.event==SDL_WINDOWEVENT_HIDDEN)restoreSystemCursor();
  if(e.window.event==SDL_WINDOWEVENT_FOCUS_LOST||e.window.event==SDL_WINDOWEVENT_SIZE_CHANGED)cancelPointer();
  break;
 case SDL_MOUSEMOTION:
  movePointer({e.motion.x,e.motion.y});
  break;
 case SDL_MOUSEBUTTONDOWN:
  if(e.button.button==SDL_BUTTON_RIGHT&&scene==Scene::Battle&&pendingSpell_>=0&&!battleMotion_.active()){pendingSpell_=sacrificeTarget_=-1;mouseLeftDown_=false;pressedButton_=-1;say("已取消施法。");break;}
  if(e.button.button==SDL_BUTTON_RIGHT){rightHelp_=true;movePointer({e.button.x,e.button.y});break;}
  if(e.button.button!=SDL_BUTTON_LEFT)break;
  rightHelp_=false;mouseX_=e.button.x;mouseY_=e.button.y;mouseLeftDown_=true;pressedButton_=-1;pressedScene_=scene;
  for(int index=int(buttons_.size())-1;index>=0;index--)if(buttons_[index].enabled&&buttons_[index].contains({mouseX_,mouseY_})){pressedButton_=index;break;}
  if(scene==Scene::Options)for(int kind=0;kind<2;kind++)if(inside(volumeTrack(kind),{mouseX_,mouseY_})){optionsDragging_=kind;optionVolume(kind,mouseX_);}
  if(scene==Scene::Adventure&&inside(adventureMinimapRect(),{mouseX_,mouseY_})){minimapDragging_=true;movePointer({mouseX_,mouseY_});}
  if(scene==Scene::Town){if(auto slot=townArmyAt({mouseX_,mouseY_});slot&&!(slot->first?game.town.garrison:game.hero.army)[slot->second].empty())armyDrag_=ArmyDrag{slot->first,slot->second,{mouseX_,mouseY_},false};}
  if(scene==Scene::Recruit&&inside(recruitTrack(),{mouseX_,mouseY_})){recruitDragging_=true;recruitAmount(mouseX_);}
  else if(scene==Scene::Market&&inside(marketTrack(),{mouseX_,mouseY_})){marketDragging_=true;marketAmount(mouseX_);}
  else if(scene==Scene::Adventure&&inside(adventureMapRect(),{mouseX_,mouseY_})){
   auto camera=mapCamera();drag_=true;dragged_=false;dragX_=mouseX_;dragY_=mouseY_;dragCamX_=camera[0];dragCamY_=camera[1];dragZoom_=zoom_;
  }
  break;
 case SDL_MOUSEBUTTONUP:{
  if(e.button.button==SDL_BUTTON_RIGHT){rightHelp_=false;hintSince_=animationTime_;break;}
  if(e.button.button!=SDL_BUTTON_LEFT)break;
  if(mouseLeftDown_)movePointer({e.button.x,e.button.y});else{mouseX_=e.button.x;mouseY_=e.button.y;}mouseLeftDown_=false;
  if(minimapDragging_){minimapDragging_=false;pressedButton_=-1;break;}
  if(optionsDragging_>=0){int kind=optionsDragging_;optionsDragging_=-1;if(scene==Scene::Options){optionVolume(kind,mouseX_);if(kind&&audio_)audio_->effect("button.wav");}pressedButton_=-1;break;}
  if(recruitDragging_){if(scene==Scene::Recruit)recruitAmount(mouseX_);recruitDragging_=false;pressedButton_=-1;break;}
  if(armyDrag_){auto from=*armyDrag_;armyDrag_.reset();if(from.moving){
   if(scene==Scene::Town)if(auto to=townArmyAt({mouseX_,mouseY_});to&&*to!=std::pair{from.garrison,from.slot}){
    sourceSlot_=from.slot;sourceGarrison_=from.garrison;selectStack(to->first,to->second,(pointerModifiers_.value_or(SDL_GetModState())&KMOD_SHIFT)!=0);
   }
   pressedButton_=-1;break;
  }}
  if(marketDragging_){if(scene==Scene::Market)marketAmount(mouseX_);marketDragging_=false;pressedButton_=-1;break;}
  bool mapPress=drag_,wasDragged=drag_&&dragged_;drag_=false;
  int released=-1;for(int index=int(buttons_.size())-1;index>=0;index--)if(buttons_[index].enabled&&buttons_[index].contains({mouseX_,mouseY_})){released=index;break;}
  bool sameButton=pressedScene_==scene&&pressedButton_>=0&&released==pressedButton_;pressedButton_=-1;
  if(!wasDragged&&((mapPress&&scene==Scene::Adventure&&inside(adventureMapRect(),{mouseX_,mouseY_}))||(!mapPress&&sameButton)))click({mouseX_,mouseY_},(pointerModifiers_.value_or(SDL_GetModState())&KMOD_SHIFT)!=0);
  break;
 }
 case SDL_MOUSEWHEEL:
  if(scene==Scene::Maps&&e.wheel.y){int last=(int(mapLibrary_["entries"].size())-1)/6;mapPage_=std::clamp(mapPage_+(e.wheel.y<0?1:-1),0,last);focus_=-1;}
  else if(scene==Scene::Visit&&e.wheel.y){visitPage_=std::max(0,visitPage_+(e.wheel.y<0?1:-1));}
  else if(scene==Scene::Adventure&&!background_&&!drag_&&!minimapDragging_&&inside(adventureMapRect(),{e.wheel.mouseX,e.wheel.mouseY})){
   // AppKit/SDL deltas already follow the system's natural-scroll setting.
   // Keep fractional trackpad motion and both axes; do not flip it a second time.
   float x=e.wheel.preciseX?e.wheel.preciseX:float(e.wheel.x),y=e.wheel.preciseY?e.wheel.preciseY:float(e.wheel.y);
   auto camera=mapCamera();camX_=camera[0]+x*24/(32*zoom_);camY_=camera[1]-y*24/(32*zoom_);
   camera=mapCamera();camX_=camera[0];camY_=camera[1];scrollTick_=0;
  }
  break;
 default:break;
 }
}
Point Desktop::windowPoint(Point p)const{
 int width=0,height=0;SDL_GetWindowSize(window_,&width,&height);auto layout=displayLayout(width,height,width,height,scene==Scene::Adventure);if(layout.pointScale<=0)return {-1,-1};
 return {int(std::floor((p.x-layout.pointX)/layout.pointScale+1e-7)),int(std::floor((p.y-layout.pointY)/layout.pointScale+1e-7))};
}
void Desktop::handleNativeEvent(SDL_Event event,bool precisePointer,SDL_Keymod modifiers){
 const auto raw=event;int beforeScene=int(scene);
 // Cocoa supplies each event's own position and modifier snapshot. The SDL
 // fallback retains its legacy stale-release workaround; it is not used for
 // the native macOS source, which must honor a release without motion events.
 if(event.type==SDL_WINDOWEVENT&&(event.window.event==SDL_WINDOWEVENT_FOCUS_LOST||event.window.event==SDL_WINDOWEVENT_SIZE_CHANGED))nativePointer_.reset();
 if(event.type==SDL_MOUSEBUTTONDOWN)nativePointer_=Point{event.button.x,event.button.y};
 if(event.type==SDL_MOUSEMOTION){nativePointer_=Point{event.motion.x,event.motion.y};auto p=windowPoint(*nativePointer_);event.motion.x=p.x;event.motion.y=p.y;}
 if(event.type==SDL_MOUSEWHEEL){auto p=windowPoint({event.wheel.mouseX,event.wheel.mouseY});event.wheel.mouseX=p.x;event.wheel.mouseY=p.y;}
 if(event.type==SDL_MOUSEBUTTONDOWN||event.type==SDL_MOUSEBUTTONUP){auto p=windowPoint(precisePointer?Point{event.button.x,event.button.y}:nativePointer_.value_or(Point{event.button.x,event.button.y}));event.button.x=p.x;event.button.y=p.y;}
 auto previousModifiers=pointerModifiers_;if(precisePointer)pointerModifiers_=modifiers;handleEvent(event);pointerModifiers_=previousModifiers;
 if(!inputTrace_.empty()&&(raw.type==SDL_MOUSEMOTION||raw.type==SDL_MOUSEBUTTONDOWN||raw.type==SDL_MOUSEBUTTONUP||raw.type==SDL_MOUSEWHEEL)){
  int ww=0,wh=0,rw=0,rh=0,wx=0,wy=0,gx=0,gy=0,mx=0,my=0,lw=0,lh=0;float sx=0,sy=0;
  SDL_GetWindowSize(window_,&ww,&wh);SDL_GetRendererOutputSize(renderer_,&rw,&rh);SDL_GetWindowPosition(window_,&wx,&wy);SDL_GetGlobalMouseState(&gx,&gy);SDL_GetMouseState(&mx,&my);SDL_RenderGetLogicalSize(renderer_,&lw,&lh);SDL_RenderGetScale(renderer_,&sx,&sy);
  Point source=raw.type==SDL_MOUSEWHEEL?Point{raw.wheel.mouseX,raw.wheel.mouseY}:raw.type==SDL_MOUSEMOTION?Point{raw.motion.x,raw.motion.y}:Point{raw.button.x,raw.button.y};
  auto logical=raw.type==SDL_MOUSEWHEEL?Point{event.wheel.mouseX,event.wheel.mouseY}:Point{mouseX_,mouseY_};
  nlohmann::json record={{"source",precisePointer?"cocoa":"sdl"},{"event",raw.type==SDL_MOUSEWHEEL?"wheel":raw.type==SDL_MOUSEMOTION?"motion":raw.type==SDL_MOUSEBUTTONDOWN?"down":"up"},{"time",SDL_GetTicks64()},{"raw",{source.x,source.y}},{"logical",{logical.x,logical.y}},{"modifiers",precisePointer?int(modifiers):int(SDL_GetModState())},{"polled",{mx,my}},{"global",{gx,gy}},{"windowPosition",{wx,wy}},{"windowSize",{ww,wh}},{"outputPixels",{rw,rh}},{"rendererLogicalSize",{lw,lh}},{"rendererScale",{sx,sy}},{"sceneBefore",beforeScene},{"sceneAfter",int(scene)}};
  if(raw.type==SDL_MOUSEWHEEL){record["wheel"]={raw.wheel.preciseX,raw.wheel.preciseY};record["wheelDirection"]=raw.wheel.direction;}
  record["polledModifiers"]=int(SDL_GetModState());record["camera"]={camX_,camY_,zoom_};record["leftPressed"]=mouseLeftDown_;record["mapDragging"]=drag_;
  if(!inputTrace_.parent_path().empty())std::filesystem::create_directories(inputTrace_.parent_path());std::ofstream(inputTrace_,std::ios::app)<<record.dump()<<'\n';
 }
}
void Desktop::frameStep(){SDL_Event event;while(SDL_PollEvent(&event)){
 if(pointerInput_&&pointerInput_->enabled()){
  if(event.type==pointerInput_->eventType()){if(auto pointer=pointerInput_->take())handleNativeEvent(pointer->event,true,pointer->modifiers);continue;}
  if(event.type==SDL_MOUSEMOTION||event.type==SDL_MOUSEBUTTONDOWN||event.type==SDL_MOUSEBUTTONUP||event.type==SDL_MOUSEWHEEL)continue;
 }
 handleNativeEvent(event);
 }if(!background_&&running_){tick();if(autosaveDirty_&&(!marching_||fixedAnimationTime_.value_or(SDL_GetTicks64())>=nextAutosave_))persist();if(autosave_){auto error=autosave_->takeError();if(!error.empty())say("自动保存失败："+error);}if(!(SDL_GetWindowFlags(window_)&SDL_WINDOW_MINIMIZED))render();}}
int Desktop::run(){
#ifdef __IPHONEOS__
 SDL_SetEventFilter([](void*self,SDL_Event*event){if(event->type==SDL_APP_WILLENTERBACKGROUND||event->type==SDL_APP_DIDENTERFOREGROUND){static_cast<Desktop*>(self)->handleEvent(*event);return 0;}return 1;},this);
 if(SDL_iPhoneSetAnimationCallback(window_,1,[](void*self){auto*app=static_cast<Desktop*>(self);if(!app->running_){app->running_=true;app->scene=Scene::Menu;}app->frameStep();},this)!=0)throw std::runtime_error(SDL_GetError());
#else
 while(running_){auto start=SDL_GetTicks64();frameStep();auto flags=SDL_GetWindowFlags(window_);uint64_t budget=(flags&(SDL_WINDOW_MINIMIZED|SDL_WINDOW_HIDDEN))?100:!(flags&SDL_WINDOW_INPUT_FOCUS)?66:16;auto elapsed=SDL_GetTicks64()-start;if(elapsed<budget)SDL_Delay(Uint32(budget-elapsed));}
 flushAutosave();
#endif
 return 0;}
bool Desktop::screenshot(const std::filesystem::path&path){
 if(!canvas_.texture)return false;
 auto*previous=SDL_GetRenderTarget(renderer_);if(SDL_SetRenderTarget(renderer_,canvas_.texture)!=0)return false;
 auto*surface=SDL_CreateRGBSurfaceWithFormat(0,canvas_.w,canvas_.h,32,SDL_PIXELFORMAT_RGBA32);bool ok=false;
 if(surface){ok=SDL_RenderReadPixels(renderer_,nullptr,SDL_PIXELFORMAT_RGBA32,surface->pixels,surface->pitch)==0;if(ok){if(!path.parent_path().empty())std::filesystem::create_directories(path.parent_path());ok=IMG_SavePNG(surface,path.string().c_str())==0;}SDL_FreeSurface(surface);}
 SDL_SetRenderTarget(renderer_,previous);return ok;
}
void Desktop::renderTests(const std::filesystem::path&dir){fixedAnimationTime_=0;std::filesystem::create_directories(dir);saveDir_=dir/"test-saves";auto check=[](bool ok,const char*message){if(!ok)throw std::runtime_error(message);};auto clickBuilding=[this,&check](int id){
  check(scene==Scene::Town,"Building click requires town scene");
  const auto&layers=game.townDef().layers;
  for(int i=int(layers.size())-1;i>=0;--i){const auto&layer=layers[i];if(!layerVisible(layer)||layer.needs.empty()||layer.needs.front()!=id||layer.mask.empty())continue;
   const auto&mask=pixelMask(layer.mask);for(int y=0;y<mask.height&&layer.pos.y+y<374;y++)for(int x=0;x<mask.width;x++)if(mask.contains(x,y)){
    Point p{layer.pos.x+x,layer.pos.y+y};if(townLayerAt(p)==i){click(p,false);return;}
   }
  }throw std::runtime_error("Requested town building has no exposed hit pixel: "+std::to_string(id));
 };scene=Scene::Menu;render();click({600,80},false);check(scene==Scene::Scenario,"Native New Game selection failed");render();click({630,530},false);check(scene==Scene::Adventure,"Native New Game button failed");key(SDLK_t,KMOD_NONE);render();click({38,475},false);check(scene==Scene::Recruit,"Native recruitment navigation failed");render();click({385,345},false);check(game.hero.army[0].count==29,"Native quantity recruitment failed");scene=Scene::Army;render();selectStack(false,0,false);selectStack(true,0,true);check(scene==Scene::Split,"Native split navigation failed");splitAmount_=10;render();check(screenshot(dir/"split.png"),"Split screenshot failed");click({200,440},false);check(game.hero.army[0].count==19&&game.town.garrison[0].count==10,"Native garrison transfer failed");std::string error;check(game.save(saveDir_/"ui-flow.json",error),"UI flow save failed");game=Game::create();check(game.load(saveDir_/"ui-flow.json",error),"UI flow restore failed");check(game.town.garrison[0].count==10,"UI flow restore count mismatch");game=Game::create();sourceSlot_=-1;focus_=-1;toastUntil_=0;const std::pair<Scene,const char*>scenes[]={{Scene::Menu,"menu"},{Scene::Adventure,"adventure"},{Scene::Town,"town"},{Scene::Build,"build"},{Scene::Recruit,"recruit"},{Scene::Army,"army"},{Scene::Hero,"hero"},{Scene::Spells,"spells"}};for(auto[s,name]:scenes){if(s==Scene::Spells){game.hero.spellbook=true;game.hero.spells[MagicArrow]=true;}scene=s;render();if(!screenshot(dir/(std::string(name)+".png")))throw std::runtime_error("截图失败");}for(int f=0;f<10;f++){codexFaction_=f;codexUnit_=f<9?f*14:126;scene=Scene::Codex;render();if(!screenshot(dir/("codex-"+std::to_string(f)+".png")))throw std::runtime_error("兵种图鉴渲染失败");}for(int f=0;f<FactionCount;f++){game=Game::create(1999,f);scene=Scene::Town;render();check(screenshot(dir/("town-"+std::to_string(f)+".png")),"Town screenshot failed");for(int b=0;b<BuildingCount;b++)if(game.townDef().buildings[b].available)game.town.built[b]=true;render();check(screenshot(dir/("town-full-"+std::to_string(f)+".png")),"Full town screenshot failed");recruitUnit_=f*14+13;game.town.pool[6]=10;scene=Scene::Recruit;render();check(screenshot(dir/("recruit-"+std::to_string(f)+".png")),"Faction recruitment screenshot failed");}game=Game::create(1999,8);game.build(MageGuild);game.endDay();game.build(Special2);check(game.town.built[Special2],"University construction failed");scene=Scene::Town;render();clickBuilding(Special2);check(scene==Scene::University,"University navigation failed");render();int gold=game.resources[Gold];click({100,450},false);check(game.hero.skills[AirMagic]==1&&game.resources[Gold]==gold-2000,"University learning flow failed");toastUntil_=0;render();check(screenshot(dir/"university.png"),"University screenshot failed");game=Game::create(1999,2);game.resources.fill(100000);game.hero.skills[Wisdom]=3;
 game.build(MageGuild);scene=Scene::Town;render();clickBuilding(MageGuild);
 check(scene==Scene::MageGuild,"Guild navigation failed");toastUntil_=0;render();
 check(screenshot(dir/"mage-guild-buy.png"),"Guild purchase screenshot failed");
 gold=game.resources[Gold];click({450,530},false);
 check(game.hero.spellbook&&game.resources[Gold]==gold-500,"Guild purchase flow failed");
 check(std::count(game.hero.spells.begin(),game.hero.spells.end(),true)==5,"Guild taught an incorrect spell count");
 for(int building:{Special3,MageGuild2,MageGuild3,MageGuild4,MageGuild5}){game.endDay();game.build(building);check(game.town.built[building],"Tower guild development failed");}
 check(std::count(game.hero.spells.begin(),game.hero.spells.end(),true)==20,"Library did not teach twenty distinct spells");
 toastUntil_=0;scene=Scene::MageGuild;render();check(screenshot(dir/"mage-guild-library.png"),"Tower library screenshot failed");
 game=Game::create();game.hero.spellbook=true;game.hero.spells.fill(true);for(int skill=AirMagic;skill<=EarthMagic;skill++)game.hero.skills[skill]=3;game.startBattle(15);scene=Scene::Spells;render();check(screenshot(dir/"spells-expert.png"),"Expert spellbook screenshot failed");click({625,424},false);render();check(screenshot(dir/"spells-page2.png"),"Spellbook second page failed");click({625,424},false);render();check(screenshot(dir/"spells-page3.png"),"Spellbook third page failed");click({625,424},false);render();check(screenshot(dir/"spells-page4.png"),"Spellbook fourth page failed");click({625,424},false);render();check(screenshot(dir/"spells-page5.png"),"Spellbook fifth page failed");click({462,428},false);check(!spellAdventure_,"Adventure tab was enabled in battle");spellFilter_=EarthSchool;spellPage_=0;render();check(screenshot(dir/"spells-earth.png"),"Earth spellbook filter screenshot failed");click({660,160},false);render();click({310,135},false);check(scene==Scene::Battle,"Mass casting navigation failed");check(game.battle->units[0].effects[Haste]>0&&game.battle->units[1].effects[Haste]>0&&game.battle->units[2].effects[Haste]>0&&game.battle->units.back().effects[Haste]==0,"Mass casting target selection failed");check(game.hero.mana==15,"Mass spell paid an incorrect mana cost");toastUntil_=0;render();check(screenshot(dir/"battle-mass-haste.png"),"Mass spell screenshot failed");game=Game::create();
 game.hero.army={Stack{Archangel,10},Stack{Pikeman,100}};
 game.objectById(15)->army={Stack{Griffin,100},Stack{Pikeman,100}};
 game.hero.spellbook=true;game.hero.spells[MagicArrow]=game.hero.spells[Fireball]=true;game.hero.skills[FireMagic]=3;
 game.startBattle(15);
 game.battle->units[1].x=7;game.battle->units[1].y=4;
 game.battle->units[2].x=7;game.battle->units[2].y=5;
 game.battle->units[3].x=13;game.battle->units[3].y=9;
 spellFilter_=FireSchool;spellPage_=0;scene=Scene::Spells;render();
 click({310,135},false);
 check(scene==Scene::Battle&&pendingSpell_==Fireball,"Area spell selection failed");
 key(SDLK_ESCAPE,KMOD_NONE);
 check(pendingSpell_==-1&&game.hero.mana==20,"Cancelling area selection spent mana");
 key(SDLK_c,KMOD_NONE);render();click({310,135},false);render();
 click(battleHexCenter({0,10}),false);
 check(pendingSpell_==Fireball&&game.hero.mana==20,"Empty area selection spent mana");
 mouseX_=battleHexCenter({6,4}).x;mouseY_=battleHexCenter({6,4}).y;toastUntil_=0;render();
 check(screenshot(dir/"battle-fireball-target.png"),"Area spell preview screenshot failed");
 int allyHP=game.battle->units[1].hp,enemyHP=game.battle->units[2].hp;
 click(battleHexCenter({6,4}),false);
 check(pendingSpell_==-1&&game.hero.mana==8,"Area cast did not pay exactly once");
 check(game.battle->units[1].hp==allyHP-70&&game.battle->units[2].hp==enemyHP-70,"Area UI cast did not affect both sides exactly once");
 check(game.validate(),"Area UI cast created invalid state");
 toastUntil_=0;render();check(screenshot(dir/"battle-fireball-result.png"),"Area spell result screenshot failed");
 game=Game::create();game.hero.spellbook=true;game.hero.power=3;game.hero.knowledge=10;game.hero.mana=100;game.hero.skills[EarthMagic]=2;game.hero.skills[WaterMagic]=3;
 game.hero.army={Stack{Archangel,10},Stack{Pikeman,5},Stack{Griffin,5}};game.objectById(15)->army={Stack{129,1000}};game.startBattle(15);
 game.battle->units[1].x=7;game.battle->units[1].y=5;game.battle->units[1].hp=game.battle->units[1].count=game.battle->units[1].permanentCount=0;game.battle->units[1].corpse=true;game.battle->units[1].deathOrder=game.battle->deathSequence=1;std::erase(game.battle->queue,1);
 game.battle->units[2].x=9;game.battle->units[2].y=5;spellFilter_=0;spellPage_=0;
 game.hero.spells.fill(false);game.hero.spells[Resurrection]=true;scene=Scene::Spells;render();click({200,135},false);toastUntil_=0;render();
 check(screenshot(dir/"battle-revive-target.png"),"Corpse selection screenshot failed");click(battleHexCenter({7,5}),false);check(game.battle->units[1].count==5&&game.battle->units[1].permanentCount==5&&!game.battle->units[1].corpse,"Corpse resurrection UI failed");toastUntil_=0;render();check(screenshot(dir/"battle-revive-result.png"),"Resurrection result screenshot failed");
 game.battle->spellRound=0;game.hero.spells.fill(false);game.hero.spells[Clone]=true;scene=Scene::Spells;render();click({200,135},false);render();click(battleHexCenter({9,5}),false);check(game.battle->units.size()==5&&game.battle->units.back().origin==StackOrigin::Clone,"Clone UI flow failed");toastUntil_=0;render();check(screenshot(dir/"battle-clone.png"),"Clone screenshot failed");
 game.battle->spellRound=0;game.hero.spells.fill(false);game.hero.spells[SummonEarthElemental]=true;scene=Scene::Spells;render();click({200,135},false);check(game.battle->units.size()==6&&game.battle->units.back().unit==120&&game.battle->units.back().count==9,"Summoning UI flow failed");toastUntil_=0;render();check(screenshot(dir/"battle-summon.png"),"Summon screenshot failed");
 game.battle->units[2].hp=75;game.battle->units[2].count=game.battle->units[2].permanentCount=3;game.battle->spellRound=0;game.hero.spells.fill(false);game.hero.spells[Sacrifice]=true;scene=Scene::Spells;render();click({200,135},false);render();click(battleHexCenter({9,5}),false);check(sacrificeTarget_==2,"Sacrifice first target failed");toastUntil_=0;render();check(screenshot(dir/"battle-sacrifice-target.png"),"Sacrifice selection screenshot failed");click(battleHexCenter({7,5}),false);check(pendingSpell_==-1&&sacrificeTarget_==-1&&game.battle->units[2].permanentCount==5&&game.battle->units[1].hp==0&&!game.battle->units[1].corpse,"Sacrifice UI result failed");check(game.validate(),"Reinforcement UI created invalid battle");toastUntil_=0;render();check(screenshot(dir/"battle-sacrifice-result.png"),"Sacrifice result screenshot failed");
 game=Game::create();game.hero.spellbook=true;game.hero.spells.fill(true);spellFilter_=0;spellPage_=0;scene=Scene::Spells;render();click({462,428},false);check(spellAdventure_,"Adventure spell tab failed");render();check(screenshot(dir/"spells-adventure.png"),"Adventure book screenshot failed");click({326,428},false);check(!spellAdventure_,"Combat spell tab failed");render();click({586,428},false);check(scene==Scene::Hero,"Original exit flag was not clickable");
 game=Game::create();pendingSpell_=-1;spellFilter_=0;spellPage_=0;
 scene=Scene::Scenario;render();check(screenshot(dir/"scenario.png"),"Scenario screenshot failed");
 game.startBattle(15);scene=Scene::Battle;render();
 check(screenshot(dir/"battle.png"),"Battle screenshot failed");
 // Original animation data: deterministic clock, native-sized crops, both
 // directions, original eye sprites and final corpse frame.
 game=Game::create();game.hero.army={Stack{Pikeman,20},Stack{Archangel,2}};
 game.objectById(15)->army={Stack{74,15},Stack{75,15}};game.startBattle(15);
 auto&units=game.battle->units;units[0].x=4;units[0].y=4;units[1].x=4;units[1].y=7;
 units[2].x=10;units[2].y=4;units[3].x=10;units[3].y=7;scene=Scene::Battle;toastUntil_=0;
 fixedAnimationTime_=0;render();check(screenshot(dir/"battle-original-idle-0.png"),"Original idle frame zero failed");
 fixedAnimationTime_=100;render();check(screenshot(dir/"battle-original-idle-1.png"),"Original idle frame one failed");
 auto contents=[](const std::filesystem::path&path){std::ifstream file(path,std::ios::binary);return std::string(std::istreambuf_iterator<char>(file),{});};
 check(contents(dir/"battle-original-idle-0.png")!=contents(dir/"battle-original-idle-1.png"),"Original idle animation did not advance");
 auto&dead=units[2];dead.hp=dead.count=dead.permanentCount=0;dead.corpse=true;dead.deathOrder=++game.battle->deathSequence;
 std::erase(game.battle->queue,dead.id);std::erase(game.battle->waiting,dead.id);check(game.validate(),"Original corpse preview invalid");
 render();check(screenshot(dir/"battle-original-corpse.png"),"Original corpse frame failed");
 game=Game::create();for(int b=0;b<BuildingCount;b++)if(game.townDef().buildings[b].available)game.town.built[b]=true;
 scene=Scene::Town;fixedAnimationTime_=0;render();check(screenshot(dir/"town-original-animation-0.png"),"Original town animation zero failed");
 fixedAnimationTime_=300;render();check(screenshot(dir/"town-original-animation-3.png"),"Original town animation three failed");
 check(contents(dir/"town-original-animation-0.png")!=contents(dir/"town-original-animation-3.png"),"Town animation did not advance");
 fixedAnimationTime_=0;
 game=Game::create();scene=Scene::Town;toastUntil_=0;render();key(SDLK_b,KMOD_NONE);check(scene==Scene::Town,"Market opened without a building");
 game.build(Marketplace);toastUntil_=0;render();clickBuilding(Marketplace);check(scene==Scene::Market&&marketObject_==0&&marketReturn_==Scene::Town,"Town market navigation failed");
 render();check(screenshot(dir/"market-empty.png"),"Market opening screenshot failed");
 click({255,378},false);render();click({537,218},false);render();
 check(marketSold_==Gold&&marketBought_==Mercury&&game.tradeLimit(Gold,Mercury)==1,"Market resource selection failed");
 click({354,549},false);check(marketLots_==1,"Market maximum button failed");render();check(screenshot(dir/"market-gold-rare.png"),"Market purchase quote screenshot failed");
 click({433,549},false);check(game.resources[Gold]==4500&&game.resources[Mercury]==6&&marketLots_==0,"Market purchase settlement failed");
 render();check(screenshot(dir/"market-traded.png"),"Market settlement screenshot failed");auto marketState=game.toJson();click({433,549},false);check(game.toJson()==marketState,"Disabled trade changed resources");
 click({172,218},false);render();click({537,378},false);render();
 auto track=marketTrack();marketAmount(track.x+8+(track.w-16)/2);check(marketLots_==8,"Market slider midpoint failed");
 key(SDLK_LEFT,KMOD_NONE);check(marketLots_==7,"Market left key failed");key(SDLK_RIGHT,KMOD_NONE);check(marketLots_==8,"Market right key failed");render();
 click({338,497},false);check(marketLots_==7,"Market left arrow failed");render();click({462,497},false);check(marketLots_==8,"Market right arrow failed");
 marketAmount(track.x-1000);check(marketLots_==0,"Market slider minimum clamp failed");marketAmount(track.x+1000);check(marketLots_==15,"Market slider maximum clamp failed");
 marketAmount(track.x+8+(track.w-16)/2);toastUntil_=0;render();check(screenshot(dir/"market-slider-mid.png"),"Market slider screenshot failed");
 SDL_Event input{};input.type=SDL_MOUSEBUTTONDOWN;input.button.button=SDL_BUTTON_LEFT;input.button.x=track.x+8;input.button.y=track.y+8;handleEvent(input);
 check(marketDragging_&&mouseLeftDown_&&marketLots_==0,"Market slider did not capture mouse down");
 input={};input.type=SDL_MOUSEMOTION;input.motion.x=track.x+track.w-8;input.motion.y=track.y+8;handleEvent(input);check(marketLots_==15,"Market drag motion did not update amount");
 input={};input.type=SDL_MOUSEBUTTONUP;input.button.button=SDL_BUTTON_LEFT;input.button.x=track.x+track.w-8;input.button.y=track.y+8;handleEvent(input);check(!marketDragging_&&!mouseLeftDown_&&marketLots_==15,"Market drag release failed");
 marketLots_=0;render();input.type=SDL_MOUSEBUTTONDOWN;input.button.x=354;input.button.y=549;handleEvent(input);render();
 check(screenshot(dir/"market-button-pressed.png"),"Original pressed button screenshot failed");input.type=SDL_MOUSEBUTTONUP;handleEvent(input);check(marketLots_==15,"Captured maximum button failed");render();
 input.type=SDL_MOUSEBUTTONDOWN;input.button.x=10;input.button.y=10;handleEvent(input);input.type=SDL_MOUSEBUTTONUP;input.button.x=433;input.button.y=549;handleEvent(input);check(game.toJson()==marketState,"Release on trade without pressing it executed a transaction");
 input.type=SDL_MOUSEBUTTONDOWN;input.button.x=433;input.button.y=549;handleEvent(input);input.type=SDL_MOUSEBUTTONUP;input.button.x=354;input.button.y=549;handleEvent(input);check(game.toJson()==marketState,"Releasing another button executed a transaction");
 input.type=SDL_MOUSEBUTTONDOWN;input.button.x=track.x+8;input.button.y=track.y+8;handleEvent(input);input={};input.type=SDL_WINDOWEVENT;input.window.event=SDL_WINDOWEVENT_FOCUS_LOST;handleEvent(input);check(!marketDragging_&&!mouseLeftDown_&&pressedButton_==-1,"Focus loss did not release mouse capture");
 key(SDLK_ESCAPE,KMOD_NONE);check(scene==Scene::Town&&game.toJson()==marketState,"Cancel market changed balances or return scene");render();key(SDLK_b,KMOD_NONE);render();key(SDLK_RETURN,KMOD_NONE);check(scene==Scene::Town,"Default Return did not close the market");
 game=Game::create();game.build(Marketplace);game.hero.pos={8,18};scene=Scene::Adventure;toastUntil_=0;key(SDLK_b,KMOD_NONE);render();
 check(scene==Scene::Market&&!game.visiting()&&marketReturn_==Scene::Adventure,"Remote market keyboard navigation failed");check(screenshot(dir/"market-remote.png"),"Remote market screenshot failed");key(SDLK_ESCAPE,KMOD_NONE);check(scene==Scene::Adventure,"Remote market did not return to adventure");
 game=Game::create();auto post=std::find_if(game.objects.begin(),game.objects.end(),[](const auto&o){return o.type==ObjectType::TradingPost;});check(post!=game.objects.end(),"Demo Trading Post missing");int postId=post->id;game.hero.pos=post->pos;scene=Scene::Adventure;settle();
 check(scene==Scene::Market&&marketObject_==postId&&game.marketplaces(postId)==5&&!game.town.built[Marketplace],"Trading Post visit navigation failed");
 toastUntil_=0;render();click({255,378},false);render();click({454,218},false);render();click({354,549},false);check(marketLots_==12,"Trading Post maximum quote failed");render();
 check(screenshot(dir/"tradingpost-quote.png"),"Trading Post quote screenshot failed");click({433,549},false);check(game.resources[Gold]==4&&game.resources[Wood]==32,"Trading Post settlement failed");render();
 check(screenshot(dir/"tradingpost-traded.png"),"Trading Post settlement screenshot failed");key(SDLK_ESCAPE,KMOD_NONE);check(scene==Scene::Adventure&&game.validate(),"Trading Post exit state invalid");
 game=Game::create();game.build(Marketplace);game.resources[Wood]=0;scene=Scene::Town;openMarket();toastUntil_=0;render();click({172,218},false);render();click({537,378},false);render();
 check(game.tradeLimit(Wood,Gold)==0&&marketLots_==0,"Empty stock created a tradable amount");marketState=game.toJson();click({354,549},false);click({433,549},false);check(game.toJson()==marketState,"Empty stock controls changed balances");check(screenshot(dir/"market-no-stock.png"),"Empty stock screenshot failed");
 click({255,218},false);render();click({537,378},false);render();check(marketSold_==Mercury&&marketBought_==Gold,"Rare resource sale selection failed");check(screenshot(dir/"market-sell-rare.png"),"Rare resource sale screenshot failed");
 // Show every classic creature in controlled combat deployments. Newly
 // recovered RoE units must use DEF data; expansion gaps stay visible.
 for(int group=0;group<11;group++){
  game=Game::create();game.hero.army={};auto*enemy=game.objectById(15);enemy->army={};
  int first=group*14;
  for(int slot=0;slot<7;slot++){if(first+slot<UnitCount)game.hero.army[slot]={first+slot,1};if(first+7+slot<UnitCount)enemy->army[slot]={first+7+slot,1};}
  if(group==10)enemy->army[0]={Pikeman,1};
  game.startBattle(15);check(game.battle.has_value()&&game.validate(),"Creature roster preview produced an invalid deployment");
  for(auto&unit:game.battle->units)unit.x=unit.team==0?4:10;
  check(game.validate(),"Creature roster preview moved a stack out of bounds");
  scene=Scene::Battle;pendingSpell_=sacrificeTarget_=-1;toastUntil_=0;fixedAnimationTime_=0;render();
  check(screenshot(dir/("battle-original-roster-"+std::to_string(group)+".png")),"Original roster screenshot failed");
 }
 scenarioRenderTests(dir);townRenderTests(dir);optionsRenderTests(dir);battleRenderTests(dir);graphicsRenderTests(dir);moraleRenderTests(dir);pointerRenderTests(dir);runtimeFlowRenderTests(dir);displayRenderTests(dir);menuRenderTests(dir);mineIncomeRenderTests(dir);terrainRenderTests(dir);adventureArtworkRenderTests(dir);mapArtworkRenderTests(dir);adventureVisitRenderTests(dir);cursorRenderTests(dir);combatCursorRenderTests(dir);outcomeRenderTests(dir);ownershipRenderTests(dir);touchRenderTests(dir);hintRenderTests(dir);encounterRenderTests(dir);campaignFlowRenderTests(dir);int screens=0;for(const auto&entry:std::filesystem::directory_iterator(dir))if(entry.path().extension()==".png")screens++;std::cout<<"Native UI flow passed. Rendered "<<screens<<" screens to "<<dir<<'\n';}
}

namespace h3 {
void Desktop::codex(){title("原版兵种资料 · 九阵营与中立生物",Scene::Menu);for(int f=0;f<9;f++)button({29+f*83,76,77,28},Factions[f].name,[this,f]{codexFaction_=f;codexUnit_=f*14;});button({29,113,150,28},"中立生物",[this]{codexFaction_=9;codexUnit_=126;});int start=codexFaction_<9?codexFaction_*14:126,count=codexFaction_<9?14:15;
 for(int i=0;i<count;i++){int unit=start+i,x=32+i%8*95,y=158+i/8*111;portrait(unit,{x,y,53,59});text(Units[unit].name,x,y+63,10,unit==codexUnit_?GoldColor:Muted);button({x-2,y-2,89,92},"",[this,unit]{codexUnit_=unit;},true,true,"查看"+Units[unit].name+"的属性、费用和能力");if(unit==codexUnit_)outline({x-2,y-2,89,92},GoldColor);}
 const auto&u=Units[codexUnit_];text(u.name,34,395,20,GoldColor);text("攻击 "+std::to_string(u.attack)+"   防御 "+std::to_string(u.defense)+"   生命 "+std::to_string(u.hp)+"   速度 "+std::to_string(u.speed),34,430,14);text("伤害 "+std::to_string(u.minDamage)+"–"+std::to_string(u.maxDamage)+"   增长 "+std::to_string(u.growth)+"   占格 "+std::to_string(u.size),34,460,14);text("费用 "+cost(unitCost(codexUnit_)),34,490,13,GoldColor);text(u.abilitySummary,34,521,11,Muted,720);}
}
