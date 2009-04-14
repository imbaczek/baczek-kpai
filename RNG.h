#pragma once

#include "float3.h"

float random();
float random(float start, float end);

/// start and end inclusive
int randint(int start, int end);

float3 random_direction();
float3 random_offset_pos(const float3& basePos, float minoffset, float maxoffset);