#include <algorithm>
#include "material.h"
#include "sampling.h"

#define PI 3.14159265359


namespace pathtracer
{
///////////////////////////////////////////////////////////////////////////
// A Lambertian (diffuse) material
///////////////////////////////////////////////////////////////////////////
vec3 Diffuse::f(const vec3& wi, const vec3& wo, const vec3& n)
{
	if(dot(wi, n) <= 0.0f)
		return vec3(0.0f);
	if(!sameHemisphere(wi, wo, n))
		return vec3(0.0f);
	return (1.0f / M_PI) * color;
}

vec3 Diffuse::sample_wi(vec3& wi, const vec3& wo, const vec3& n, float& p)
{
	vec3 tangent = normalize(perpendicular(n));
	vec3 bitangent = normalize(cross(tangent, n));
	vec3 sample = cosineSampleHemisphere();
	wi = normalize(sample.x * tangent + sample.y * bitangent + sample.z * n);
	if(dot(wi, n) <= 0.0f)
		p = 0.0f;
	else
		p = max(0.0f, dot(n, wi)) / M_PI;
	return f(wi, wo, n);
}

///////////////////////////////////////////////////////////////////////////
// A Blinn Phong Dielectric Microfacet BRFD
///////////////////////////////////////////////////////////////////////////
vec3 BlinnPhong::refraction_brdf(const vec3& wi, const vec3& wo, const vec3& n)
{
	if (refraction_layer == NULL) {
		return vec3(0.0f);
	}

	// Half angle between wo and wi
	vec3 wh = normalize(wi + wo);

	// Fresnel term
	float F = R0 + ((1 - R0) * pow(1.0f - abs(dot(wh, wi)), 5.0f));

	vec3 brdf = (1.0f - F) * refraction_layer->f(wi, wo, n);

	return brdf;
	
}
vec3 BlinnPhong::reflection_brdf(const vec3& wi, const vec3& wo, const vec3& n)
{
	// Half angle between wo and wi

	vec3 wh = normalize(wi + wo);

	// Fresnel term
	float F = R0 + ((1 - R0) * pow(1.0f - abs(dot(wh, wi)), 5.0f));

	// Microfacet Distributon Function
	float nwh =  dot(n, wh);
	float D = ((shininess + 2.0) / (2 * PI)) * pow(std::max(0.0f, nwh), shininess);

	// Shadowing/Masking Function
	float nwi = dot(n, wi);

	if (nwi <= 0.0f) {
		return vec3(0.0f);
	}

	float nwo = dot(n, wo);
	float wowh = dot(wo, wh);
	float G = std::min(1.0f, min(2 * ((nwh * nwo) / wowh), 2 * ((nwh * nwi) / wowh)));

	float brdf = (F * D * G) / (4 * nwo * nwi);

	//return vec3(D);

	return vec3(brdf);



}

vec3 BlinnPhong::f(const vec3& wi, const vec3& wo, const vec3& n)
{
	vec3 brdf =	reflection_brdf(wi, wo, n) + refraction_brdf(wi, wo, n);

	return brdf;
}

vec3 BlinnPhong::sample_wi(vec3& wi, const vec3& wo, const vec3& n, float& p)
{
	// Calculate wh
	vec3 tangent = normalize(perpendicular(n));
	vec3 bitangent = normalize(cross(tangent, n));
	float phi = 2.0f * M_PI * randf();
	float cos_theta = pow(randf(), 1.0f / (shininess + 1));
	float sin_theta = sqrt(max(0.0f, 1.0f - cos_theta * cos_theta));
	vec3 wh = normalize(sin_theta * cos(phi) * tangent +
		sin_theta * sin(phi) * bitangent +
		cos_theta * n);
	if (dot(wo, n) <= 0.0f) return vec3(0.0f);

	

	// Russian roulette
	if (randf() < 0.5f) {
		wi = reflect(-wo, wh);

		float pwh = (shininess + 1) * (pow(dot(n, wh), shininess)) / (2.0f * M_PI);

		p = pwh / (4 * dot(wo, wh));

		p = p * 0.5f;

		return reflection_brdf(wi, wo, n);
	}
	else {
		if (refraction_layer == NULL) {
			return vec3(0.0f);
		}

		vec3 brdf = refraction_layer->sample_wi(wi, wo, n, p);

		p = p * 0.5f;

		float F = R0 + (1.0f - R0) * pow(1.0f - abs(dot(wh, wi)), 5.0f);

		return (1 - F) * brdf;
	}
	

	/*
	vec3 tangent = normalize(perpendicular(n));
	vec3 bitangent = normalize(cross(tangent, n));
	vec3 sample = cosineSampleHemisphere();
	wi = normalize(sample.x * tangent + sample.y * bitangent + sample.z * n);
	if(dot(wi, n) <= 0.0f)
		p = 0.0f;
	else
		p = max(0.0f, dot(n, wi)) / M_PI;
	return f(wi, wo, n);
	*/
}

///////////////////////////////////////////////////////////////////////////
// A Blinn Phong Metal Microfacet BRFD (extends the BlinnPhong class)
///////////////////////////////////////////////////////////////////////////
vec3 BlinnPhongMetal::refraction_brdf(const vec3& wi, const vec3& wo, const vec3& n)
{
	return vec3(0.0f);
}
vec3 BlinnPhongMetal::reflection_brdf(const vec3& wi, const vec3& wo, const vec3& n)
{
	return BlinnPhong::reflection_brdf(wi, wo, n) * color;
};

///////////////////////////////////////////////////////////////////////////
// A Linear Blend between two BRDFs
///////////////////////////////////////////////////////////////////////////
vec3 LinearBlend::f(const vec3& wi, const vec3& wo, const vec3& n)
{
	vec3 blend = (w * bsdf0->f(wi, wo, n)) + ((1 - w) * bsdf1->f(wi, wo, n));
	
	return blend;
}

vec3 LinearBlend::sample_wi(vec3& wi, const vec3& wo, const vec3& n, float& p)
{

	vec3 brdf = vec3(0.0f);
	if (randf() < w) {
		brdf = bsdf0->sample_wi(wi, wo, n, p);

		p *= w;
	}
	else {
		brdf = bsdf1->sample_wi(wi, wo, n, p);

		p *= (1 - w);
	}

	return brdf;
}

///////////////////////////////////////////////////////////////////////////
// A perfect specular refraction.
///////////////////////////////////////////////////////////////////////////
} // namespace pathtracer