#pragma once
#include <vector>
class Noise
{
	// based on:
	// https://solarianprogrammer.com/2012/07/18/perlin-noise-cpp-11/
	std::vector<int> p;
public:
	Noise(int seed);
	//Nose(unsigned int seed);
	~Noise();
	double getNoise(double x, double y, double z);
	static float getPerlingNoise(float x, float y);
private:
	double fade(double t);
	double lerp(double t, double a, double b);
	double grad(int hash, double x, double y, double z);
};

