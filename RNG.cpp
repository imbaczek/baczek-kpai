#include <boost/random.hpp>

#include "RNG.h"

static boost::mt19937 rng;                 


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