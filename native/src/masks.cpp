#include "h3/masks.hpp"
#include <fstream>
#include <stdexcept>

namespace h3 {
std::optional<PixelMask> readPixelMask(const std::filesystem::path&path,std::string&error){
 try{
  std::ifstream file(path,std::ios::binary);unsigned char header[12]{};
  if(!file.read(reinterpret_cast<char*>(header),12)||std::string(reinterpret_cast<char*>(header),4)!="H3MK")throw std::runtime_error("Invalid town mask header");
  auto number=[&](int at){uint32_t n=0;for(int i=0;i<4;i++)n|=uint32_t(header[at+i])<<(8*i);return n;};
  uint32_t width=number(4),height=number(8);
  if(!width||!height||width>4096||height>4096||uint64_t(width)*height>16777216||std::filesystem::file_size(path)!=12+uint64_t(width)*height)throw std::runtime_error("Invalid town mask dimensions");
  PixelMask mask{int(width),int(height),std::vector<uint8_t>(size_t(width)*height)};
  if(!file.read(reinterpret_cast<char*>(mask.pixels.data()),mask.pixels.size()))throw std::runtime_error("Truncated town mask");
  for(auto pixel:mask.pixels)if(pixel>1)throw std::runtime_error("Invalid town mask pixel");
  return mask;
 }catch(const std::exception&exception){error=exception.what();return {};}
}
}
