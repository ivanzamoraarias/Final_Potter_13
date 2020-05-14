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
	float nwh = dot(n, wh);
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

////////////////////////////////////////////////////////////////////////////
// BTDF
///////////////////////////////////////////////////////////////////////////
vec3 BTDF::refraction_brdf(const vec3& wi, const vec3& wo, const vec3& n)
{
	// Half angle between wo and wi	
	vec3 wh = -((refr_index_i * wi) + (refr_index_o * wo));
	wh = normalize(wh);

	// Fresnel
	float F = Distributions::FresnelSchlick(wi, wh, R0);
	//float F = Distributions::FresnelExact(wi, wh, refr_index_i, refr_index_o);

	// Microfacet distribution
	float D = Distributions::GGX_D(n, wh, shininess);

	// Shadowing-masking term
	float G = Distributions::GGXSmith_G(wi, wo, wh, n, shininess);

	// BRDF
	float wiwh = dot(wi, wh);
	float wowh = dot(wo, wh);

	float numerator = abs(wiwh) * abs(wowh) * (refr_index_o * refr_index_o) * (1.0f - F) * G * D;
	float denominator = abs(dot(n, wi)) * abs(dot(n, wo)) * pow((refr_index_i * wiwh) + (refr_index_o * wowh), 2.0f);
	float brdf = numerator / denominator;

	return vec3(brdf);


}
vec3 BTDF::reflection_brdf(const vec3& wi, const vec3& wo, const vec3& n)
{

	// Half angle between wo and wi
	float signWH = dot(n, wi) < 0.0f ? -1.0f : 1.0f;

	vec3 wh = signWH * (wi + wo);
	wh = normalize(wh);

	if (dot(wi, n) <= 0.f || !sameHemisphere(wi, wo, n)) return vec3(0.0f);

	// Fresnel
	float F = Distributions::FresnelSchlick(wi, wh, R0);
	//float F = Distributions::FresnelExact(wi, wh, refr_index_i, refr_index_o);

	// Microfacet distribution
	float D = Distributions::GGX_D(n, wh, shininess);

	// Shadowing-masking term
	float G = Distributions::GGXSmith_G(wi, wo, wh, n, shininess);

	// BRDF
	float brdf = (F * D * G) / (4 * abs(dot(n, wo)) * abs(dot(n, wi)));

	return vec3(brdf);

}

vec3 BTDF::f(const vec3& wi, const vec3& wo, const vec3& n)
{
	vec3 brdf = reflection_brdf(wi, wo, n) + refraction_brdf(wi, wo, n);
	return brdf;

}

vec3 BTDF::sample_wi(vec3& wi, const vec3& wo, const vec3& n, float& p)
{
	// Calculate wh
	vec3 tangent = normalize(perpendicular(n));
	vec3 bitangent = normalize(cross(tangent, n));
	float phi = 2.0f * M_PI * randf();

	float r = randf();
	float cos_theta = sqrt(max(0.0f, (1.0f - r) / ((((shininess * shininess) - 1.0f) * r) + 1.0f)));
	float sin_theta = sqrt(max(0.0f, 1.0f - (cos_theta * cos_theta)));

	vec3 wh = normalize(sin_theta * cos(phi) * tangent +
		sin_theta * sin(phi) * bitangent +
		cos_theta * n);


	// Fresnel
	float F = Distributions::FresnelSchlick(wo, wh, R0);
	

	float eta = refr_index_i / refr_index_o;
	float cosX = dot(n, -wo);
	float sinX = (eta * eta) * (1.0f - (cosX * cosX));
	if (sinX > 1.0f) {
		F = 1.0f;
	}
	
	/*float eta = refr_index_i / refr_index_o;
	float F = Distributions::FresnelExact(wo, wh, refr_index_i, refr_index_o);*/

	float pwh = Distributions::GGX_D(n, wh, shininess);
	pwh *= abs(dot(n, wh));

	// Decide based on Fresnel
	if (randf() <= F) {

		// Reflection vector
		wi = reflect(-wo, wh);

		// PDF
		float j = 1.0f / (4 * abs(dot(wo, wh))); // jacobian
		p = pwh * j;
		p = p * F;

		// BRDF
		return reflection_brdf(wi, wo, n);

	}
	else {

		isRefracted = true;

		// Calculate transmission vector
		int signNI = (dot(n, wo) < 0.0f) ? -1 : 1;


		float wowh = dot(wo, wh);
		float sqr = 1.0f + (eta * eta) * ((wowh * wowh) - 1.0f);
		if (sqr < 0.0f) {
			return vec3(0.0f);
		}
		wi = (eta * wowh - (signNI * sqrt(sqr))) * wh - eta * wo;

		// PDF
		
		float j = ((refr_index_o * refr_index_o) * abs(dot(wo, wh))) / pow(refr_index_i * dot(wi, wh) + refr_index_o * dot(wo, wh), 2.0f);  // jacobian
		p = pwh * j;
		p = 1.0f * (1.0f - F);

		// BRDF
		vec3 brdf = vec3(1.0f - F) / dot(wo, n);
		//vec3 brdf = refraction_brdf(wi, wo, n);
		return brdf;
	}

}


///////////////////////////////////////////////////////////////////////////
// Distributions
///////////////////////////////////////////////////////////////////////////
float Distributions::FresnelExact(const vec3& wi, const vec3& wh, const float refr_idx_i, const float refr_idx_o) {
	float eta = (refr_idx_o * refr_idx_o) / (refr_idx_i * refr_idx_i);
	float c = abs(dot(wi, wh));

	float g1 = eta - 1.0f + (c * c);

	if (g1 < 0) {
		return 1.0f;
	}
	float g = sqrt(g1);

	float F1 = pow(g - c, 2.0f) / (2 * pow(g + c, 2.0f));
	float F2 = 1.0f + pow(c * (g + c) - 1.0f, 2.0f) / pow(c * (g - c) + 1.0f, 2.0f);
	float F = F1 * F2;

	return F;
}

float Distributions::FresnelSchlick(const vec3& wi, const vec3& wh, const float R0) {
	float F = R0 + ((1 - R0) * pow(1.0f - abs(dot(wh, wi)), 5.0f));

	return F;
}

float Distributions::GGX_D(const vec3& n, const vec3& wh, const float shininess) {
	float nwh = dot(n, wh);
	int posFuncNWH = nwh > 0 ? 1 : 0;

	float pwr = ((nwh * nwh) * ((shininess * shininess) - 1.0f)) + 1.0f;

	if (pwr == 0.0f) return 0.0f;

	float D = (shininess * shininess * posFuncNWH) / (M_PI * pwr * pwr);

	return D;
}

float Distributions::GGXSmith_G1(const vec3& v, const vec3& wh, const vec3& n, const float shininess) {
	float v_m = dot(v, wh);
	float v_n = dot(v, n);
	if (v_n != 0 && v_m / v_n > 0) {
		float tan_v = sqrt(max(0.f, 1 - v_n * v_n)) / v_n;
		return 2.f / (1.f + sqrt(1.f + shininess * shininess * tan_v * tan_v));
	}
	else return 0.f;
}

float Distributions::GGXSmith_G(const vec3& wi, const vec3& wo, const vec3& wh, const vec3& n, const float shininess) {
	float Gi = GGXSmith_G1(wi, wh, n, shininess);
	float Go = GGXSmith_G1(wo, wh, n, shininess);

	// Bidirectional G
	float G = Gi * Go;

	return G;
}

} // namespace pathtracer