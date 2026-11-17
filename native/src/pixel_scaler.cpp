#include "h3/pixel_scaler.hpp"
#include <algorithm>
#include <array>
#include <stdexcept>
#include <limits>
#include <cstdlib>

namespace h3 {
std::vector<Rgba> scaleArtwork(std::span<const Rgba> source,int width,int height,int factor){
 int64_t cells=int64_t(width)*height;
 if(width<=0||height<=0||(factor!=2&&factor!=4)||cells!=int64_t(source.size())||cells>16777216/(factor*factor))throw std::invalid_argument("Invalid artwork dimensions");
 std::vector<Rgba> input(source.begin(),source.end());for(auto&pixel:input)if(!pixel.a)pixel={};
 // Derived from the published Scale2x neighborhood rule. No external
 // implementation is linked; equality tests preserve the source palette.
 for(int scale=1;scale<factor;scale*=2){
  std::vector<Rgba> output(size_t(width)*height*4);
  auto sample=[&](int x,int y){return input[size_t(std::clamp(y,0,height-1))*width+std::clamp(x,0,width-1)];};
  for(int y=0;y<height;y++)for(int x=0;x<width;x++){
   auto center=sample(x,y),north=sample(x,y-1),south=sample(x,y+1),west=sample(x-1,y),east=sample(x+1,y);
   const std::array<Rgba,4> vertical{north,north,south,south},horizontal{west,east,west,east};
   for(int quadrant=0;quadrant<4;quadrant++){
    bool corner=north!=south&&west!=east&&vertical[quadrant]==horizontal[quadrant];
    output[size_t(y*2+quadrant/2)*width*2+x*2+quadrant%2]=corner?vertical[quadrant]:center;
   }
  }
  input=std::move(output);width*=2;height*=2;
 }
 // Linear texture sampling should blend an edge's own color with zero
 // coverage, rather than introducing a dark fringe around transparent art.
 auto output=input;
 for(int y=0;y<height;y++)for(int x=0;x<width;x++)if(!input[size_t(y)*width+x].a){
  int red=0,green=0,blue=0,weight=0;
  for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++)if(x+dx>=0&&x+dx<width&&y+dy>=0&&y+dy<height){auto p=input[size_t(y+dy)*width+x+dx];red+=p.r*p.a;green+=p.g*p.a;blue+=p.b*p.a;weight+=p.a;}
  if(weight)output[size_t(y)*width+x]={uint8_t(red/weight),uint8_t(green/weight),uint8_t(blue/weight),0};
 }
 return output;
}
std::vector<Rgba> extendArtworkEdgeColors(std::span<const Rgba> source,int width,int height){
 int64_t cells=int64_t(width)*height;if(width<=0||height<=0||cells>16777216||cells!=int64_t(source.size()))throw std::invalid_argument("Invalid edge color dimensions");
 constexpr uint8_t cutoff=16,radius=4;std::vector<Rgba> output(source.begin(),source.end());std::vector<uint8_t> distance(size_t(cells),255);std::vector<int> queue;queue.reserve(size_t(cells)/8);
 auto visit=[&](int index,auto action){int x=index%width,y=index/width;for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++)if((dx||dy)&&x+dx>=0&&x+dx<width&&y+dy>=0&&y+dy<height)action((y+dy)*width+x+dx);};
 for(int i=0;i<cells;i++)if(source[i].a>cutoff){distance[i]=0;bool edge=false;visit(i,[&](int neighbor){edge|=source[neighbor].a<=cutoff;});if(edge)queue.push_back(i);}
 for(size_t head=0;head<queue.size();head++){
  int i=queue[head];if(distance[i]>=radius)continue;
  visit(i,[&](int neighbor){if(distance[neighbor]!=255)return;distance[neighbor]=distance[i]+1;auto color=output[i];color.a=source[neighbor].a;output[neighbor]=color;queue.push_back(neighbor);});
 }
 return output;
}
std::vector<int> minimumArtworkSeam(std::span<const Rgba> left,std::span<const Rgba> right,int width,int height){
 int64_t cells=int64_t(width)*height;if(width<=0||height<=0||cells>8388608||cells!=int64_t(left.size())||left.size()!=right.size())throw std::invalid_argument("Invalid seam dimensions");
 std::vector<int64_t> previous(width),current(width);std::vector<int> parents(size_t(cells),-1);
 auto difference=[&](int x,int y){auto a=left[size_t(y)*width+x],b=right[size_t(y)*width+x];return std::abs(int(a.r)-b.r)+std::abs(int(a.g)-b.g)+std::abs(int(a.b)-b.b);};
 for(int x=0;x<width;x++)previous[x]=difference(x,0);
 for(int y=1;y<height;y++){
  for(int x=0;x<width;x++){int best=x;int64_t cost=previous[x];for(int dx:{-1,1})if(x+dx>=0&&x+dx<width&&previous[x+dx]+4<cost){best=x+dx;cost=previous[best]+4;}current[x]=cost+difference(x,y);parents[size_t(y)*width+x]=best;}
  previous.swap(current);
 }
 std::vector<int> seam(height);seam.back()=int(std::min_element(previous.begin(),previous.end())-previous.begin());for(int y=height-1;y>0;y--)seam[y-1]=parents[size_t(y)*width+seam[y]];return seam;
}
}
