#pragma once
#include <cstdint>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace h3 {
struct PixelMask {
 int width=0,height=0;std::vector<uint8_t> pixels;
 bool contains(int x,int y)const{return x>=0&&y>=0&&x<width&&y<height&&pixels[std::size_t(y)*width+x]!=0;}
};
std::optional<PixelMask> readPixelMask(const std::filesystem::path&,std::string&error);
}
