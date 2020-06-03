#pragma once
#include <glm/glm.hpp>
#include "sampling.h"

using namespace glm;

namespace pathtracer
{
	
class Light
{
public:
	float intensity_multiplier;
	vec3 color;
	vec3 position;

	//virtual std::pair<vec3, vec3> sample() = 0;
};

class AreaLight : public Light
{
public:
	vec3 normal;

	labhelper::Texture texture;
	
};

class DiskLight : public AreaLight
{
public:
	float radius;
	virtual std::pair<vec3, vec3> sample();
};

class QuadLight : public AreaLight
{
public:
	virtual std::pair<vec3, vec3> sample();
};

class SphereLight : public AreaLight
{
public:
	float radius;

	virtual std::pair<vec3, vec3> sample(vec3 point);
};

}