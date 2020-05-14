#pragma once
#include <glm/glm.hpp>
#include "Pathtracer.h"
#include "sampling.h"

using namespace glm;

namespace pathtracer
{
///////////////////////////////////////////////////////////////////////////
// The interface for any BRDF.
///////////////////////////////////////////////////////////////////////////
class BRDF
{
public:
	// Return the value of the brdf for specific directions
	virtual vec3 f(const vec3& wi, const vec3& wo, const vec3& n) = 0;
	// Sample a suitable direction and return the brdf in that direction as
	// well as the pdf (~probability) that the direction was chosen.
	virtual vec3 sample_wi(vec3& wi, const vec3& wo, const vec3& n, float& p) = 0;
};

///////////////////////////////////////////////////////////////////////////
// A Lambertian (diffuse) material
///////////////////////////////////////////////////////////////////////////
class Diffuse : public BRDF
{
public:
	vec3 color;
	Diffuse(vec3 c) : color(c)
	{
	}
	virtual vec3 f(const vec3& wi, const vec3& wo, const vec3& n) override;
	virtual vec3 sample_wi(vec3& wi, const vec3& wo, const vec3& n, float& p) override;
};

///////////////////////////////////////////////////////////////////////////
// A Blinn Phong Dielectric Microfacet BRFD
///////////////////////////////////////////////////////////////////////////
class BlinnPhong : public BRDF
{
public:
	float shininess;
	float R0;
	BRDF* refraction_layer;
	BlinnPhong(float _shininess, float _R0, BRDF* _refraction_layer = NULL)
	    : shininess(_shininess), R0(_R0), refraction_layer(_refraction_layer)
	{
	}
	virtual vec3 refraction_brdf(const vec3& wi, const vec3& wo, const vec3& n);
	virtual vec3 reflection_brdf(const vec3& wi, const vec3& wo, const vec3& n);
	virtual vec3 f(const vec3& wi, const vec3& wo, const vec3& n) override;
	virtual vec3 sample_wi(vec3& wi, const vec3& wo, const vec3& n, float& p) override;
};

///////////////////////////////////////////////////////////////////////////
// A Blinn Phong Metal Microfacet BRFD (extends the BlinnPhong class)
///////////////////////////////////////////////////////////////////////////
class BlinnPhongMetal : public BlinnPhong
{
public:
	vec3 color;
	BlinnPhongMetal(vec3 c, float _shininess, float _R0) : color(c), BlinnPhong(_shininess, _R0)
	{
	}
	virtual vec3 refraction_brdf(const vec3& wi, const vec3& wo, const vec3& n);
	virtual vec3 reflection_brdf(const vec3& wi, const vec3& wo, const vec3& n);
};

///////////////////////////////////////////////////////////////////////////
// A Linear Blend between two BRDFs
///////////////////////////////////////////////////////////////////////////
class LinearBlend : public BRDF
{
public:
	float w;
	BRDF* bsdf0;
	BRDF* bsdf1;
	LinearBlend(float _w, BRDF* a, BRDF* b) : w(_w), bsdf0(a), bsdf1(b){};
	virtual vec3 f(const vec3& wi, const vec3& wo, const vec3& n) override;
	virtual vec3 sample_wi(vec3& wi, const vec3& wo, const vec3& n, float& p) override;
};

class BTDF : public BRDF
{
public:
	float shininess;
	float R0;
	float refr_index_i;
	float refr_index_o;
	BRDF* refraction_layer;
	float transparency;
	bool isRefracted;
	BTDF(float _shininess, float _R0, float _refr_index_i = 1.0f, float _refr_index_o = 1.0f, float _transparency = 1.0f, BRDF* _refraction_layer = NULL)
		: shininess(_shininess), R0(_R0), refr_index_i(_refr_index_i), refr_index_o(_refr_index_o), transparency(_transparency), refraction_layer(_refraction_layer)
	{
		isRefracted = false;
	}
	virtual vec3 refraction_brdf(const vec3& wi, const vec3& wo, const vec3& n);
	virtual vec3 reflection_brdf(const vec3& wi, const vec3& wo, const vec3& n);
	virtual vec3 f(const vec3& wi, const vec3& wo, const vec3& n) override;
	virtual vec3 sample_wi(vec3& wi, const vec3& wo, const vec3& n, float& p) override;
};


class Distributions
{
public:
	static float FresnelExact(const vec3& wi, const vec3& wh, const float refr_idx_i, const float refr_idx_o);
	static float FresnelSchlick(const vec3& wi, const vec3& wh, const float R0);
	static float GGX_D(const vec3& n, const vec3& wh, const float shininess);
	static float GGXSmith_G1(const vec3& v, const vec3& wh, const vec3& n, const float shininess);
	static float GGXSmith_G(const vec3& wi, const vec3& wo, const vec3& wh, const vec3& n, const float shininess);
};

} // namespace pathtracer