#pragma once

#include <boost/cstdint.hpp>

#include "float3.h"


void init_rng();
void init_rng(boost::uint32_t seed);

float randfloat();
float randfloat(float start, float end);

/// start and end inclusive
int randint(int start, int end);

float3 random_direction();
float3 random_offset_pos(const float3& basePos, float minoffset, float maxoffset);