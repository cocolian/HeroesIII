#include "h3/renderer.hpp"
#include "h3/pixel_scaler.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cctype>

namespace h3 {
namespace {
struct Raster {int w=0,h=0;std::vector<Rgba> pixels;};
Rgba sample(const Raster&r,float u,float v){float x=std::clamp(u*r.w-.5f,0.f,float(r.w-1)),y=std::clamp(v*r.h-.5f,0.f,float(r.h-1));int x0=int(x),y0=int(y),x1=std::min(x0+1,r.w-1),y1=std::min(y0+1,r.h-1);float dx=x-x0,dy=y-y0;auto a=r.pixels[size_t(y0)*r.w+x0],b=r.pixels[size_t(y0)*r.w+x1],c=r.pixels[size_t(y1)*r.w+x0],d=r.pixels[size_t(y1)*r.w+x1];auto channel=[&](auto member){return uint8_t(std::lround((a.*member*(1-dx)+b.*member*dx)*(1-dy)+(c.*member*(1-dx)+d.*member*dx)*dy));};return{channel(&Rgba::r),channel(&Rgba::g),channel(&Rgba::b),255};}
}
SDL_Texture* Desktop::buildingArtworkTexture(const nlohmann::json&asset,const nlohmann::json&original){
 if(!asset.is_object()||!asset.contains("file")||!asset.contains("canvas")||!asset["file"].is_string()||asset["canvas"]!=original["canvas"])return nullptr;
 auto file=asset["file"].get<std::string>();std::filesystem::path path=file;if(!file.starts_with("assets/hd/")||path.is_absolute()||std::any_of(path.begin(),path.end(),[](const auto&part){return part=="..";})||!std::filesystem::is_regular_file(root_/path))return nullptr;
 std::string cache="redrawn-building:"+file;SDL_Texture*texture=nullptr;
 if(auto found=enhanced_.find(cache);found!=enhanced_.end()){found->second.used=++artUse_;texture=found->second.texture;}
 else{
  auto*surface=readSurface(file,false,false);if(!surface)return nullptr;int w=surface->w,h=surface->h;if(int64_t(w)*h>16777216){SDL_FreeSurface(surface);return nullptr;}
  std::vector<Rgba> pixels(size_t(w)*h);SDL_LockSurface(surface);for(int y=0;y<h;y++)std::memcpy(pixels.data()+size_t(y)*w,static_cast<Uint8*>(surface->pixels)+y*surface->pitch,size_t(w)*4);SDL_UnlockSurface(surface);SDL_FreeSurface(surface);
  auto corrected=extendArtworkEdgeColors(pixels,w,h);auto*clean=SDL_CreateRGBSurfaceWithFormatFrom(corrected.data(),w,h,32,w*4,SDL_PIXELFORMAT_RGBA32);texture=cacheArtwork(cache,clean);SDL_FreeSurface(clean);if(!texture)return nullptr;
 }
 return texture;
}
bool Desktop::redrawnTownBuilding(const TownLayer&layer){
 if(!settings_.hdArtwork||frameCount(layer.animation)!=1||!hd_.contains("townBuildings"))return false;
 auto key=layer.animation;std::transform(key.begin(),key.end(),key.begin(),[](unsigned char c){return char(std::tolower(c));});if(!hd_["townBuildings"].contains(key))return false;
 const auto&original=demo_["def"][key]["groups"]["0"][0];auto*texture=buildingArtworkTexture(hd_["townBuildings"][key],original);if(!texture)return false;
 SDL_Rect destination{layer.pos.x,layer.pos.y,original["canvas"][0],original["canvas"][1]};if(SDL_RenderCopy(renderer_,texture,nullptr,&destination)!=0)return false;++redrawnBuildingsDrawn_;return true;
}
SDL_Texture* Desktop::originalTownClothTexture(const std::string&key,int index,const std::vector<SDL_Rect>&regions){
 std::string cache="original-cloth:"+key+"#"+std::to_string(index);if(auto found=enhanced_.find(cache);found!=enhanced_.end()){found->second.used=++artUse_;return found->second.texture;}
 const auto&animation=demo_["def"][key];const auto&frames=animation["groups"]["0"];int width=frames[0]["canvas"][0],height=frames[0]["canvas"][1];std::vector<Rgba>pixels(size_t(width)*height);
 auto compose=[&](int frameIndex){const auto&f=frames[frameIndex];auto*source=readSurface(animation["pages"][f["page"].get<int>()]["file"].get<std::string>(),false,false);if(!source)return false;SDL_LockSurface(source);int sx=f["rect"][0],sy=f["rect"][1],w=f["rect"][2],h=f["rect"][3],left=f["offset"][0],top=f["offset"][1];
  for(int y=0;y<h;y++)for(int x=0;x<w;x++){Rgba a{};std::memcpy(&a,static_cast<Uint8*>(source->pixels)+(sy+y)*source->pitch+(sx+x)*4,4);auto&b=pixels[size_t(top+y)*width+left+x];if(a.a==255){b=a;continue;}if(!a.a)continue;int denominator=a.a*255+b.a*(255-a.a);auto channel=[&](int c,int d){return uint8_t((c*a.a*255+d*b.a*(255-a.a)+denominator/2)/denominator);};b={channel(a.r,b.r),channel(a.g,b.g),channel(a.b,b.b),uint8_t((denominator+127)/255)};}
  SDL_UnlockSurface(source);SDL_FreeSurface(source);return true;};
 if(!compose(0)||(index&&!compose(index)))return nullptr;
 std::vector<uint8_t>allowed(pixels.size()),distance(pixels.size(),255);std::vector<int>queue;
 for(auto r:regions)for(int y=r.y;y<r.y+r.h;y++)for(int x=r.x;x<r.x+r.w;x++)allowed[size_t(y)*width+x]=1;
 // Castle cloth uses a saturated blue field with gold trim. Keep those source
 // pixels and nearby dark folds; soften only the narrow surrounding fringe.
 for(size_t i=0;i<pixels.size();i++){auto p=pixels[i];if(allowed[i]&&p.a&&p.b>=32&&p.b*100>p.r*115&&p.b*100>p.g*105){distance[i]=0;queue.push_back(int(i));}}
 for(size_t head=0;head<queue.size();head++){int i=queue[head],x=i%width,y=i/width;if(distance[i]>=3)continue;for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++)if((dx||dy)&&x+dx>=0&&x+dx<width&&y+dy>=0&&y+dy<height){int n=(y+dy)*width+x+dx;if(allowed[n]&&distance[n]==255){distance[n]=distance[i]+1;queue.push_back(n);}}}
 int seeds=0;for(size_t i=0;i<pixels.size();i++){auto&p=pixels[i];int d=distance[i];bool gold=p.r>80&&p.g>50&&p.r>=p.g&&int(p.r)-p.b>45&&int(p.g)-p.b>20,dark=int(p.r)+p.g+p.b<150;int coverage=d==0||(d<=2&&(gold||dark))?255:d==1?140:d==2?55:0;seeds+=d==0;p.a=uint8_t(int(p.a)*coverage/255);}
 if(!seeds)return nullptr;auto corrected=extendArtworkEdgeColors(pixels,width,height);auto*surface=SDL_CreateRGBSurfaceWithFormatFrom(corrected.data(),width,height,32,width*4,SDL_PIXELFORMAT_RGBA32);auto*texture=cacheArtwork(cache,surface);SDL_FreeSurface(surface);return texture;
}
bool Desktop::redrawnAnimatedTownBuilding(const TownLayer&layer){
 int count=frameCount(layer.animation);if(!settings_.hdArtwork||count<2||!hd_.contains("animatedTownBuildings"))return false;
 auto key=layer.animation;std::transform(key.begin(),key.end(),key.begin(),[](unsigned char c){return char(std::tolower(c));});if(!hd_["animatedTownBuildings"].contains(key))return false;
 const auto&asset=hd_["animatedTownBuildings"][key];const auto&original=demo_["def"][key]["groups"]["0"][0];
 if(!asset.is_object()||!asset.contains("mode")||asset["mode"]!="original-cloth"||!asset.contains("originalFrameCount")||asset["originalFrameCount"]!=count||!asset.contains("originalRegions")||!asset["originalRegions"].is_array()||asset["originalRegions"].empty()||asset["originalRegions"].size()>16)return false;
 int width=original["canvas"][0],height=original["canvas"][1];std::vector<SDL_Rect>regions;
 for(const auto&record:asset["originalRegions"]){
  if(!record.is_array()||record.size()!=4)return false;
  for(const auto&value:record)if(!value.is_number_integer()||value<0||value>std::max(width,height))return false;
  SDL_Rect r{record[0],record[1],record[2],record[3]};if(r.w<=0||r.h<=0||r.x+r.w>width||r.y+r.h>height)return false;
  for(const auto&other:regions)if(SDL_HasIntersection(&r,&other))return false;regions.push_back(r);
 }
 auto*cloth=originalTownClothTexture(key,int(animationTime_/100%count),regions);if(!cloth)return false;auto*texture=buildingArtworkTexture(asset,original);if(!texture)return false;
 SDL_Rect destination{layer.pos.x,layer.pos.y,width,height};if(SDL_RenderCopy(renderer_,texture,nullptr,&destination)!=0)return false;
 if(SDL_RenderCopy(renderer_,cloth,nullptr,&destination)!=0)return false;++redrawnAnimatedBuildingsDrawn_;return true;
}
bool Desktop::redrawnBackground(const std::string&key,SDL_Rect field){
 if(!settings_.hdArtwork||!hd_.contains("backgrounds")||!hd_["backgrounds"].contains(key))return false;
 const auto&asset=hd_["backgrounds"][key];if(!asset.contains("base")||!asset.contains("strips")||!asset["strips"].is_array()||asset["strips"].size()>16)return false;
 auto safe=[&](const std::string&name){std::filesystem::path p=name;return name.starts_with("assets/hd/")&&!p.is_absolute()&&std::none_of(p.begin(),p.end(),[](const auto&part){return part=="..";})&&std::filesystem::exists(root_/p)&&image(name,false).texture;};
 std::string baseFile=asset["base"].get<std::string>();if(!safe(baseFile))return false;
 if(asset["strips"].empty()){auto*texture=image(baseFile,false).texture;SDL_SetTextureScaleMode(texture,SDL_ScaleModeLinear);return SDL_RenderCopy(renderer_,texture,nullptr,&field)==0;}
 std::string cache=key+"@"+std::to_string(field.x)+","+std::to_string(field.y)+","+std::to_string(field.w)+","+std::to_string(field.h);
 if(!redrawnMeshes_.contains(cache)){
  std::vector<Raster> rasters;std::vector<std::string> files;std::vector<float> starts,widths;
  for(const auto&strip:asset["strips"]){
   std::string file=strip.at("file").get<std::string>();float start=strip.at("x").get<float>(),width=strip.at("width").get<float>();
   if(!safe(file)||!std::isfinite(start)||!std::isfinite(width)||start<0||width<=0||start+width>1.001f)return false;
   auto*surface=readSurface(file,false,false);if(!surface)return false;Raster r{surface->w,surface->h,std::vector<Rgba>(size_t(surface->w)*surface->h)};SDL_LockSurface(surface);for(int y=0;y<r.h;y++)std::memcpy(r.pixels.data()+size_t(y)*r.w,static_cast<Uint8*>(surface->pixels)+y*surface->pitch,size_t(r.w)*4);SDL_UnlockSurface(surface);SDL_FreeSurface(surface);
   rasters.push_back(std::move(r));files.push_back(file);starts.push_back(start);widths.push_back(width);
  }
  if(starts.front()>.001f||starts.back()+widths.back()<.999f)return false;
  std::vector<std::vector<float>> cuts;
  for(size_t i=1;i<files.size();i++){
   float left=starts[i],right=std::min(starts[i-1]+widths[i-1],starts[i]+widths[i]);if(left>=right||starts[i]<=starts[i-1])return false;
   int columns=std::max(2,int(std::ceil((right-left)*field.w))),rows=field.h;
   std::vector<Rgba> a(size_t(columns)*rows),b(a.size());
   for(int y=0;y<rows;y++)for(int x=0;x<columns;x++){float u=left+(x+.5f)/columns*(right-left),v=(y+.5f)/rows;a[size_t(y)*columns+x]=sample(rasters[i-1],(u-starts[i-1])/widths[i-1],v);b[size_t(y)*columns+x]=sample(rasters[i],(u-starts[i])/widths[i],v);}
   auto seam=minimumArtworkSeam(a,b,columns,rows);std::vector<float> cut(rows+1);for(int y=0;y<rows;y++)cut[y]=field.x+(left+(seam[y]+.5f)/columns*(right-left))*field.w;cut.back()=cut[rows-1];cuts.push_back(std::move(cut));
  }
  std::vector<ArtMesh> meshes;
  for(size_t i=0;i<files.size();i++){
   ArtMesh mesh;mesh.file=files[i];
   for(int y=0;y<=field.h;y++){
    float left=i?cuts[i-1][y]:float(field.x),right=i+1<files.size()?cuts[i][y]:float(field.x+field.w);
    float xs[]={i?left-1:left,i?left+1:left,i+1<files.size()?right-1:right,i+1<files.size()?right+1:right};
    for(int x=0;x<4;x++){Uint8 alpha=(x==0&&i)||(x==3&&i+1<files.size())?0:255;float u=std::clamp(((xs[x]-field.x)/field.w-starts[i])/widths[i],0.f,1.f);mesh.vertices.push_back({{xs[x],float(field.y+y)},{255,255,255,alpha},{u,float(y)/field.h}});}
   }
   for(int y=0;y<field.h;y++)for(int x=0;x<3;x++){int a=y*4+x;for(int index:{a,a+1,a+4,a+1,a+5,a+4})mesh.indices.push_back(index);}meshes.push_back(std::move(mesh));
  }
  redrawnMeshes_.emplace(cache,std::move(meshes));
 }
 auto*base=image(baseFile,false).texture;SDL_SetTextureScaleMode(base,SDL_ScaleModeLinear);SDL_RenderCopy(renderer_,base,nullptr,&field);
 for(const auto&mesh:redrawnMeshes_.at(cache)){auto*texture=image(mesh.file,false).texture;SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_BLEND);SDL_SetTextureScaleMode(texture,SDL_ScaleModeLinear);if(SDL_RenderGeometry(renderer_,texture,mesh.vertices.data(),int(mesh.vertices.size()),mesh.indices.data(),int(mesh.indices.size()))!=0)return false;}
 return true;
}
}
