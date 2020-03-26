#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <Model.h>
#include <omp.h>
#include "HDRImage.h"

#ifdef M_PI
#undef M_PI
#endif
#define M_PI 3.14159265359f
#define EPSILON 0.0001f


using namespace glm;

namespace pathtracer
{
///////////////////////////////////////////////////////////////////////////////
// Path Tracer settings
///////////////////////////////////////////////////////////////////////////////
extern struct Settings
{
	int subsampling;
	int max_bounces;
	int max_paths_per_pixel;
} settings;

///////////////////////////////////////////////////////////////////////////////
// Environment
///////////////////////////////////////////////////////////////////////////////
extern struct Environment
{
	float multiplier;
	HDRImage map;
} environment;

///////////////////////////////////////////////////////////////////////////
// The rendered image
///////////////////////////////////////////////////////////////////////////
extern struct Image
{
	int width, height, number_of_samples = 0;
	std::vector<glm::vec3> data;
	float* getPtr()
	{
		return &data[0].x;
	}
} rendered_image;

///////////////////////////////////////////////////////////////////////////////
// The light source
///////////////////////////////////////////////////////////////////////////////
extern struct PointLight
{
	float intensity_multiplier;
	vec3 color;
	vec3 position;
} point_light;


extern struct DiskLight
{
	float intensity_multiplier;
	vec3 color;
	vec3 position;
	float radius;
	vec3 normal;
}disk_lights[];

///////////////////////////////////////////////////////////////////////////
// Restart rendering of image
///////////////////////////////////////////////////////////////////////////
void restart();

///////////////////////////////////////////////////////////////////////////
// On window resize, window size is passed in, actual size of pathtraced
// image may be smaller (if we're subsampling for speed)
///////////////////////////////////////////////////////////////////////////
void resize(int w, int h);

///////////////////////////////////////////////////////////////////////////
// Trace one path per pixel
///////////////////////////////////////////////////////////////////////////
void tracePaths(const mat4& V, const mat4& P);
}; // namespace pathtracer
