#pragma once
#include <cstdint>
#include <span>
#include <vector>

namespace h3 {
struct Rgba {uint8_t r=0,g=0,b=0,a=0;bool operator==(const Rgba&)const=default;};
static_assert(sizeof(Rgba)==4);
// Palette-preserving corner reconstruction followed by transparent edge
// color extension for linear sampling. Inputs and original files stay intact.
std::vector<Rgba> scaleArtwork(std::span<const Rgba>,int width,int height,int factor=4);
std::vector<int> minimumArtworkSeam(std::span<const Rgba> left,std::span<const Rgba> right,int width,int height);
// Extend nearby covered RGB into very low coverage texels before straight-alpha
// linear sampling. Coverage and opaque art are never changed.
std::vector<Rgba> extendArtworkEdgeColors(std::span<const Rgba>,int width,int height);
}
