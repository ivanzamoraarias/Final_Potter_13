#version 420

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;

///////////////////////////////////////////////////////////////////////////////
// Material
///////////////////////////////////////////////////////////////////////////////
uniform vec3 material_color;
uniform float material_reflectivity;
uniform float material_metalness;
uniform float material_fresnel;
uniform float material_shininess;
uniform float material_emission;

uniform int has_emission_texture;
layout(binding = 5) uniform sampler2D emissiveMap;

///////////////////////////////////////////////////////////////////////////////
// Environment
///////////////////////////////////////////////////////////////////////////////
layout(binding = 6) uniform sampler2D environmentMap;
layout(binding = 7) uniform sampler2D irradianceMap;
layout(binding = 8) uniform sampler2D reflectionMap;
uniform float environment_multiplier;

///////////////////////////////////////////////////////////////////////////////
// Light source
///////////////////////////////////////////////////////////////////////////////
uniform vec3 point_light_color = vec3(1.0, 1.0, 1.0);
uniform float point_light_intensity_multiplier = 50.0;


///////////////////////////////////////////////////////////////////////////////
// Shadow Map
///////////////////////////////////////////////////////////////////////////////
layout(binding = 10) uniform sampler2DShadow shadowMapTex;

///////////////////////////////////////////////////////////////////////////////
// Constants
///////////////////////////////////////////////////////////////////////////////
#define PI 3.14159265359

///////////////////////////////////////////////////////////////////////////////
// Input varyings from vertex shader
///////////////////////////////////////////////////////////////////////////////
in vec2 texCoord;
in vec3 viewSpaceNormal;
in vec3 viewSpacePosition;
in vec4 shadowMapCoord;

///////////////////////////////////////////////////////////////////////////////
// Input uniform variables
///////////////////////////////////////////////////////////////////////////////
uniform mat4 viewInverse;
uniform vec3 viewSpaceLightPosition;
uniform vec3 viewSpaceLightDir;
uniform float spotInnerAngle;
uniform float spotOuterAngle;

///////////////////////////////////////////////////////////////////////////////
// Output color
///////////////////////////////////////////////////////////////////////////////
layout(location = 0) out vec4 fragmentColor;


vec3 calculateDirectIllumiunation(vec3 wo, vec3 n)
{
	vec3 direct_illum = material_color;


	// distance to light source
	float d = distance(viewSpacePosition, viewSpaceLightPosition);

	// radiance
	vec3 li = point_light_intensity_multiplier * point_light_color * (pow(d,-2));

	// direction to light source
	vec3 wi = normalize(viewSpaceLightPosition - viewSpacePosition);


	float nwi = dot(n, wi);

	// early exit in case of back side of triangle
	if(nwi <= 0) {
		return vec3(0.0f);
	}

	// diffuse term
	vec3 diffuse_term = material_color * (1.0f / PI) * nwi * li;


	// Half angle between wo and wi
	vec3 wh = normalize(wi + wo);

	// Fresnel term
	float F = material_fresnel + ((1 - material_fresnel) * pow(1 - dot(wh, wi), 5));

	// Microfacet Distributon Function
	float nwh = dot(n, wh);
	float D = ((material_shininess + 2) / (2 * PI)) * pow(nwh, material_shininess);

	// Shadowing/Masking Function
	float nwo = dot(n, wo);
	float wowh = dot(wo, wh);
	float G = min(1, min(2 * ((nwh * nwo) / wowh), 2 * ((nwh * nwi) / wowh)));

	// BRDF
	float brdf = (F * D * G) / (4 * nwo * nwi);


	
	vec3 dielectric_term = brdf * nwi * li + (1 - F) * diffuse_term;
	vec3 metal_term = brdf * material_color * nwi * li;

	// calculate microfacet term
	vec3 microfacet_term = (material_metalness * metal_term) + (dielectric_term * (1 - material_metalness));

	// final direct lighting
	direct_illum = (material_reflectivity * microfacet_term) + ((1 - material_reflectivity) * diffuse_term);

	return direct_illum;
}

vec3 calculateIndirectIllumination(vec3 wo, vec3 n)
{
	vec3 indirect_illum = vec3(0.f);

	// world space normal
	vec3 nws = normalize(viewInverse.xyz * n);

	// Calculate the spherical coordinates of the direction
	float theta = acos(max(-1.0f, min(1.0f, nws.y)));
	float phi = atan(nws.z, nws.x);
	if(phi < 0.0f)
	{
		phi = phi + 2.0f * PI;
	}

	// Use these to lookup the color in the irradiance map
	vec2 irradiance = vec2(phi / (2.0 * PI), theta / PI);

	vec3 diffuse_term = material_color * (1.0f / PI) * texture(irradianceMap, irradiance);


	// reflection vector
	vec3 wi = -normalize(viewInverse.xyz * reflect(wo, n));

	// Calculate the spherical coordinates of the direction
	theta = acos(max(-1.0f, min(1.0f, wi.y)));
	phi = atan(wi.z, wi.x);
	if(phi < 0.0f)
	{
		phi = phi + 2.0f * PI;
	}
	
	// lookup coordinate reflection in environment map
	vec2 lookup = vec2(phi / (2.0 * PI), theta / PI);

	float roughness = sqrt(sqrt(2 / (material_shininess + 2) ));

	// get color from preconvolved map in a certain LOD
	vec3 li = environment_multiplier * textureLod(reflectionMap, lookup, roughness * 7.0f).xyz;

	// Half angle between wo and wi
	vec3 wh = normalize(wi + wo);

	// Fresnel term
	float F = material_fresnel + ((1 - material_fresnel) * pow(1 - dot(wh, wi), 5));

	vec3 dielectric_term = (F * li) + ((1-F) * diffuse_term);
	vec3 metal_term = F * material_color * li;

	vec3 microfacet_term = (material_metalness * metal_term) + (dielectric_term * (1 - material_metalness));

	// final indirect lighting
	indirect_illum = (material_reflectivity * microfacet_term) + ((1 - material_reflectivity) * diffuse_term);

	return indirect_illum;
}

void main()
{
	float visibility = 1.0;
	float attenuation = 1.0;

	// Shadow Map
	visibility = textureProj(shadowMapTex, shadowMapCoord);

	
	// Spotlight
	vec3 posToLight = normalize(viewSpaceLightPosition - viewSpacePosition);
	float cosAngle = dot(posToLight, -viewSpaceLightDir);

	// Spotlight with hard border:
	//float spotAttenuation = (cosAngle > spotOuterAngle) ? 1.0 : 0.0;
	float spotAttenuation = smoothstep(spotOuterAngle, spotInnerAngle, cosAngle);
	visibility *= spotAttenuation;


	vec3 wo = -normalize(viewSpacePosition);
	vec3 n = normalize(viewSpaceNormal);

	// Direct illumination
	vec3 direct_illumination_term = visibility * calculateDirectIllumiunation(wo, n);

	// Indirect illumination
	vec3 indirect_illumination_term = calculateIndirectIllumination(wo, n);

	///////////////////////////////////////////////////////////////////////////
	// Add emissive term. If emissive texture exists, sample this term.
	///////////////////////////////////////////////////////////////////////////
	vec3 emission_term = material_emission * material_color;
	if(has_emission_texture == 1)
	{
		emission_term = texture(emissiveMap, texCoord).xyz;
	}

	vec3 shading = direct_illumination_term + indirect_illumination_term + emission_term;

	fragmentColor = vec4(shading, 1.0);
	return;
}
