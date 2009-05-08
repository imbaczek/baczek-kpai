#include <cmath>
#include <ctime>
#include <boost/random.hpp>
#include <boost/math/constants/constants.hpp>

#include "float3.h"

#include "RNG.h"

using namespace std;

static boost::mt19937 rng;

static bool is_initialized = false;

void init_rng()
{
	if (is_initialized)
		return;

	is_initialized = true;
	rng.seed(time(NULL));
}


void init_rng(boost::uint32_t seed)
{
	is_initialized = true;
	rng.seed(seed);
}


// thanks http://www.bnikolic.co.uk/blog/cpp-boost-uniform01.html for pitfall warning
float random()
{
  static boost::uniform_01<boost::mt19937> zeroone(rng);
  return zeroone();
}

float random(float start, float end)
{
	boost::uniform_real<float> rnd(start, end);
	boost::variate_generator<boost::mt19937&, boost::uniform_real<float> >
			 gimme_random(rng, rnd);
	return gimme_random();
}

int randint(int start, int end)
{
	boost::uniform_int<> rnd(start, end);
	boost::variate_generator<boost::mt19937&, boost::uniform_int<> >
		 gimme_random(rng, rnd);
	return gimme_random();
}

float3 random_direction()
{
	float x = random(0, 2*boost::math::constants::pi<float>());
	return float3(sin(x), 0, cos(x));
}


float3 random_offset_pos(const float3& basePos, float minoffset, float maxoffset)
{
	float3 dest;
	do {
		float r = random(minoffset, maxoffset);
		float3 modDir = random_direction();
		dest = basePos + modDir * r;
	} while (!dest.IsInBounds());
	return dest;
}
