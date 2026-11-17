#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace h3 {
// Adventure view uses window points as layout units, while Retina only changes
// the pixel density. Small windows retain the classic minimum canvas.
inline std::array<int,2> logicalCanvasSize(int width,int height,bool expandMap){
 if(!expandMap)return {800,600};
 double scale=std::min({1.,width/800.,height/600.});
 if(scale<=0)return {800,600};
 return {std::max(800,int(std::floor(width/scale+.00001))),std::max(600,int(std::floor(height/scale+.00001)))};
}
struct DisplayLayout {
 int width=800,height=600,pixelWidth=800,pixelHeight=600,pixelX=0,pixelY=0;
 double pointScale=1,pixelScale=1,pointX=0,pointY=0;
};
inline DisplayLayout displayLayout(int windowWidth,int windowHeight,int outputWidth,int outputHeight,bool expandMap){
 auto size=logicalCanvasSize(windowWidth,windowHeight,expandMap);DisplayLayout out;out.width=size[0];out.height=size[1];
 out.pointScale=std::min(windowWidth/double(out.width),windowHeight/double(out.height));
 out.pixelScale=std::min(outputWidth/double(out.width),outputHeight/double(out.height));
 out.pointX=(windowWidth-out.width*out.pointScale)/2;out.pointY=(windowHeight-out.height*out.pointScale)/2;
 out.pixelWidth=int(std::lround(out.width*out.pixelScale));out.pixelHeight=int(std::lround(out.height*out.pixelScale));
 out.pixelX=(outputWidth-out.pixelWidth)/2;out.pixelY=(outputHeight-out.pixelHeight)/2;return out;
}
}
