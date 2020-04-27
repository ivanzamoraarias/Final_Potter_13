#include "Noise.h"
#include <stdlib.h> 

float Noise::getPerlingNoise(float x, float y)
{
	float result = rand() % 100 + 1;
	return result;
}
