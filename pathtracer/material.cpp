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
	float eta_i = refr_index_o;
	float eta_o = refr_index_i;
	
	// Half angle between wo and wi	
	vec3 wh = -(eta_i * wi + eta_o * wo);
	wh = normalize(wh);

	// Fresnel
	//float F = Distributions::FresnelSchlick(wi, wh, R0);
	float F = Distributions::FresnelExact(wi, wh, eta_i, eta_o);

	//float beckman_shininess = 1.2f - (0.2f * sqrt(abs(dot(wi, n))));
	
	// Microfacet distribution
	float D = Distributions::GGX_D(n, wh, shininess);
	//float D = Distributions::Beckmann_D(n, wh, shininess * beckman_shininess);

	// Shadowing-masking term
	float G = Distributions::GGXSmith_G(wi, wo, wh, n, shininess);
	//float G = Distributions::BeckmannSmith_G(wi, wo, wh, n, shininess * beckman_shininess);

	// BRDF
	float wiwh = dot(wi, wh);
	float wowh = dot(wo, wh);

	float frac = abs(wiwh) * abs(wowh) / (abs(dot(n, wo)) * abs(dot(n, wi)));
	float numerator = (eta_o * eta_o) * (1.0f - F) * G * D;
	float denominator = pow((eta_i * wiwh) + (eta_o * wowh), 2.0f);
	float brdf = frac *numerator / denominator;

	return vec3(brdf);

}
vec3 BTDF::reflection_brdf(const vec3& wi, const vec3& wo, const vec3& n)
{

	// Half angle between wo and wi
	float signWH = dot(n, wi) < 0.0f ? -1.0f : 1.0f;

	vec3 wh = signWH * (wi + wo);
	wh = normalize(wh);

	//if (dot(wi, n) <= 0.f) return vec3(0.0f);

	// Fresnel
	//float F = Distributions::FresnelSchlick(wi, wh, R0);
	float F = Distributions::FresnelExact(wi, wh, refr_index_i, refr_index_o);

	//float beckman_shininess = 1.2f - (0.2f * sqrt(abs(dot(wi, n))));
	
	// Microfacet distribution
	float D = Distributions::GGX_D(n, wh, shininess);
	//float D = Distributions::Beckmann_D(n, wh, shininess * beckman_shininess);

	// Shadowing-masking term
	float G = Distributions::GGXSmith_G(wi, wo, wh, n, shininess);
	//float G = Distributions::BeckmannSmith_G(wi, wo, wh, n, shininess * beckman_shininess);

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
	vec3 wh = Distributions::GGX_sample_wh(n, shininess);
	
	//float beckman_shininess = 1.2f - (0.2f * sqrt(abs(dot(-wo, n))));
	//vec3 wh = Distributions::Beckmann_sample_wh(n, shininess * beckman_shininess);


	// Fresnel
	//float F = Distributions::FresnelSchlick(-wo, wh, R0);

	////Total internal reflection
	//float eta = refr_index_i / refr_index_o;
	//float cosX = dot(n, -wo);
	//float sinX = (eta * eta) * (1.0f - (cosX * cosX));
	//if (sinX > 1.0f) {
	//	F = 1.0f;
	//}
	
	float eta = refr_index_i / refr_index_o;
	float F = Distributions::FresnelExact(wo, wh, refr_index_i, refr_index_o);

	float D = Distributions::GGX_D(n, wh, shininess);
	//float D = Distributions::Beckmann_D(n, wh, shininess * beckman_shininess);
	float pwh = D * abs(dot(n, wh));

	// Decide based on Fresnel
	if (randf() <= F) {

		// Reflection vector
		wi = reflect(-wo, wh);

		// PDF
		float j = 1.0f / (4 * abs(dot(wo, wh))); // jacobian
		p = pwh * j;
		p = p * F;

		// BRDF
		vec3 brdf = reflection_brdf(wi, wo, n);
		 
		return brdf;

	}
	else {
		// Calculate transmission vector
		int signNI = (dot(n, wo) < 0.0f) ? -1 : 1;

		float wowh = dot(wo, wh);
		float sqr = 1.0f + (eta * eta) * ((wowh * wowh) - 1.0f);
		if (sqr < 0.0f) {
			return vec3(0.0f);
		}
		wi = (eta * wowh - (signNI * sqrt(sqr))) * wh - eta * wo;
		isRefracted = true;

		// PDF
		vec3 ht = -(refr_index_o * wo + refr_index_i * wi);
		ht = normalize(ht);

		float j = ((refr_index_o * refr_index_o) * abs(dot(wo, ht))) / pow(refr_index_i * dot(wi, ht) + refr_index_o * dot(wo, ht), 2.0f);  // jacobian
		p = pwh * j;
		p = p * (1.0f - F);

		// BRDF
		vec3 brdf = refraction_brdf(wi, wo, n);
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
	if (pwr == 0.0f) {
		return 0.0f;
	}

	float D = (shininess * shininess * posFuncNWH) / (M_PI * pwr * pwr);
	return D;
}

float Distributions::GGXSmith_G1(const vec3& v, const vec3& wh, const vec3& n, const float shininess) {
	float vwh = dot(v, wh);
	float nv = dot(v, n);
	if (nv != 0.0f && vwh / nv > 0) {
		float tan_v = (1 - nv * nv) / (nv * nv);
		float G1 = 2.0f / (1.0f + sqrt(1.0f + (shininess * shininess * tan_v)));
		return G1;
	}
	else {
		return 0.0f;
	}
}

float Distributions::GGXSmith_G(const vec3& wi, const vec3& wo, const vec3& wh, const vec3& n, const float shininess) {
	float Gi = GGXSmith_G1(wi, wh, n, shininess);
	float Go = GGXSmith_G1(wo, wh, n, shininess);

	// Bidirectional G
	float G = Gi * Go;
	return G;
}

vec3 Distributions::GGX_sample_wh(const vec3& n, const float shininess) {
	vec3 tangent = normalize(perpendicular(n));
	vec3 bitangent = normalize(cross(tangent, n));
	float phi = 2.0f * M_PI * randf();

	float r = randf();
	float cos_theta = sqrt((1.0f - r) / ((((shininess * shininess) - 1.0f) * r) + 1.0f));
	float sin_theta = sqrt(max(0.0f, 1.0f - (cos_theta * cos_theta)));

	vec3 wh = normalize(sin_theta * cos(phi) * tangent +
		sin_theta * sin(phi) * bitangent +
		cos_theta * n);

	return wh;
}


float Distributions::Beckmann_D(const vec3& n, const vec3& wh, const float shininess) {
	float cosNWH = dot(n, wh);

	if (cosNWH > 0.0f) {
		float tanNWH = (1 - (cosNWH * cosNWH)) / (cosNWH * cosNWH); // tan^2(n, wh)
		float pwrTerm = -tanNWH / (shininess * shininess);
		float denominator = M_PI * (shininess * shininess) * pow(cosNWH, 4.0f);
		float D = exp(pwrTerm) / denominator;
		return D;
	}
	else {
		return 0.0f;
	}
}

float Distributions::BeckmannSmith_G1(const vec3& v, const vec3& wh, const vec3& n, const float shininess) {
	float vwh = dot(v, wh);
	float nv = dot(n, v);

	if (vwh / nv > 0.0f) {
		float tanNV = sqrt(1.0f - (nv * nv)) / nv;

		if (tanNV == 0.0f)
			return 0.0f;

		float alfa = 1.0f / (shininess * tanNV);

		if (alfa < 1.6f) {
			float num = (3.535f * alfa) + (2.181f * alfa * alfa);
			float den = 1.0f + (2.276f * alfa) + (2.577 * alfa * alfa);
			float G1 = num / den;

			return G1;
		}
		else {
			return 1.0f;
		}
	}
	else {
		return 0.0f;
	}
}

float Distributions::BeckmannSmith_G(const vec3& wi, const vec3& wo, const vec3& wh, const vec3& n, const float shininess) {
	float Gi = BeckmannSmith_G1(wi, wh, n, shininess);
	float Go = BeckmannSmith_G1(wo, wh, n, shininess);

	// Bidirectional G
	float G = Gi * Go;
	return G;
}

vec3 Distributions::Beckmann_sample_wh(const vec3& n, const float shininess) {
	vec3 tangent = normalize(perpendicular(n));
	vec3 bitangent = normalize(cross(tangent, n));
	float phi = 2.0f * M_PI * randf();

	float cos_theta = sqrt(1.0f / (1.0f - (shininess * shininess) * log(1 - randf())));
	float sin_theta = sqrt(max(0.0f, 1.0f - (cos_theta * cos_theta)));

	vec3 wh = normalize(sin_theta * cos(phi) * tangent +
		sin_theta * sin(phi) * bitangent +
		cos_theta * n);

	return wh;
}

} // namespace pathtracer