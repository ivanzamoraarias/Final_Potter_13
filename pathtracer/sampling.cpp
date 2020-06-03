#include "sampling.h"
#include <random>
#include "labhelper.h"
#include <omp.h>
#include <iostream>
#include <glm/glm.hpp>

using namespace glm;

namespace pathtracer
{
///////////////////////////////////////////////////////////////////////////////
// Get a random float. Note that we need one "generator" per thread, or we
// would need to lock everytime someone called randf().
///////////////////////////////////////////////////////////////////////////////
std::mt19937 generators[24]; // Assuming no more than 24 cores
float randf()
{
	return float(generators[omp_get_thread_num()]() / double(generators[omp_get_thread_num()].max()));
}

///////////////////////////////////////////////////////////////////////////
// Generate uniform points on a disc
///////////////////////////////////////////////////////////////////////////
void concentricSampleDisk(float* dx, float* dy)
{
	float r, theta;
	float u1 = randf();
	float u2 = randf();
	// Map uniform random numbers to $[-1,1]^2$
	float sx = 2 * u1 - 1;
	float sy = 2 * u2 - 1;
	// Map square to $(r,\theta)$
	// Handle degeneracy at the origin
	if(sx == 0.0 && sy == 0.0)
	{
		*dx = 0.0;
		*dy = 0.0;
		return;
	}
	if(sx >= -sy)
	{
		if(sx > sy)
		{ // Handle first region of disk
			r = sx;
			if(sy > 0.0)
				theta = sy / r;
			else
				theta = 8.0f + sy / r;
		}
		else
		{ // Handle second region of disk
			r = sy;
			theta = 2.0f - sx / r;
		}
	}
	else
	{
		if(sx <= sy)
		{ // Handle third region of disk
			r = -sx;
			theta = 4.0f - sy / r;
		}
		else
		{ // Handle fourth region of disk
			r = -sy;
			theta = 6.0f + sx / r;
		}
	}
	theta *= float(M_PI) / 4.0f;
	*dx = r * cosf(theta);
	*dy = r * sinf(theta);
}

///////////////////////////////////////////////////////////////////////////
// Generate uniform points on a sphere
///////////////////////////////////////////////////////////////////////////
void uniformSphere(float* dx, float* dy, float* dz)
{
	float u1 = randf();
	float u2 = randf();

	float z = 1 - 2 * u1;
	float r = std::sqrt(std::max((float)0, (float)1 - z * z));
	float phi = 2 * M_PI * u2;
	//return Vector3f(r * std::cos(phi), r * std::sin(phi), z);
	*dx = r * std::cos(phi);
	*dy = r * std::sin(phi);
	*dz = z;

}

///////////////////////////////////////////////////////////////////////////
// Generate uniform points on a sphere inside subtended cone
///////////////////////////////////////////////////////////////////////////
void uniformSphereCone(glm::vec3* sample, glm::vec3 coordSysX, glm::vec3 coordSysY, glm::vec3 coordSysZ, float distance)
{
	float u1 = randf();
	float u2 = randf();

	// Compute theta and phi values for sample in cone
	float sinThetaMax2 = 1 / (distance * distance);
	float cosThetaMax = std::sqrt(std::max((float)0, 1 - sinThetaMax2));
	float cosTheta = (1 - u1) + u1 * cosThetaMax;
	float sinTheta = std::sqrt(std::max((float)0, 1 - cosTheta * cosTheta));
	float phi = u2 * 2 * M_PI;

	// Compute angle alpha from center of sphere to sampled point on surface
	float dc = distance;
	float ds = dc * cosTheta - std::sqrt(std::max((float)0, 1 - dc * dc * sinTheta * sinTheta));
	float cosAlpha = (dc * dc + 1 - ds * ds) / (2 * dc * 1);
	float sinAlpha = std::sqrt(std::max((float)0, 1 - cosAlpha * cosAlpha));

	*sample = (sinAlpha * std::cos(phi) * coordSysX) + (sinAlpha * std::sin(phi) * coordSysY) + (cosAlpha * coordSysZ);
}

///////////////////////////////////////////////////////////////////////////
// Generate points with a cosine distribution on the hemisphere
///////////////////////////////////////////////////////////////////////////
glm::vec3 cosineSampleHemisphere()
{
	glm::vec3 ret;
	concentricSampleDisk(&ret.x, &ret.y);
	ret.z = sqrt(max(0.f, 1.f - ret.x * ret.x - ret.y * ret.y));
	return ret;
}

///////////////////////////////////////////////////////////////////////////
// Generate a vector that is perpendicular to another
///////////////////////////////////////////////////////////////////////////
glm::vec3 perpendicular(const glm::vec3& v)
{
	if(fabsf(v.x) < fabsf(v.y))
	{
		return glm::vec3(0.0f, -v.z, v.y);
	}
	return glm::vec3(-v.z, 0.0f, v.x);
}

///////////////////////////////////////////////////////////////////////////
// Check if wi and wo are on the same side of the plane defined by n
///////////////////////////////////////////////////////////////////////////
bool sameHemisphere(const vec3& i, const vec3& o, const vec3& n)
{
	return sign(dot(o, n)) == sign(dot(i, n));
}

glm::vec4 texSampleRGBA(labhelper::Texture t, float u, float v)
{
	int x = int(floor(u * t.width));
	int y = int(floor(v * t.height));

	glm::vec4 c = glm::vec4(
		t.data[4 * (y * t.width + x) + 0] / 255.0f,
		t.data[4 * (y * t.width + x) + 1] / 255.0f,
		t.data[4 * (y * t.width + x) + 2] / 255.0f,
		t.data[4 * (y * t.width + x) + 3] / 255.0f
	);
	return c;
}

glm::vec3 texSampleRGB(labhelper::Texture t, float u, float v)
{
	int x = int(floor(u * t.width));
	int y = int(floor(v * t.height));

	glm::vec3 c = glm::vec3(
		t.data[3 * (y * t.width + x) + 0] / 255.0f,
		t.data[3 * (y * t.width + x) + 1] / 255.0f,
		t.data[3 * (y * t.width + x) + 2] / 255.0f
	);
	return c;
}

float texSampleR(labhelper::Texture t, float u, float v)
{
	int x = int(floor(u * t.width));
	int y = int(floor(v * t.height));

	float c = t.data[(y * t.width + x)] / 255.0f;
	return c;
}

} // namespace pathtracer