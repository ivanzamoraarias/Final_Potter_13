#include "Pathtracer.h"
#include <memory>
#include <iostream>
#include <map>
#include <algorithm>
#include "material.h"
#include "embree.h"
#include "sampling.h"

using namespace std;
using namespace glm;

namespace pathtracer
{
///////////////////////////////////////////////////////////////////////////////
// Global variables
///////////////////////////////////////////////////////////////////////////////
Settings settings;
Environment environment;
Image rendered_image;
PointLight point_light;
DiskLight disk_lights[2];

///////////////////////////////////////////////////////////////////////////
// Restart rendering of image
///////////////////////////////////////////////////////////////////////////
void restart()
{
	// No need to clear image,
	rendered_image.number_of_samples = 0;
}

///////////////////////////////////////////////////////////////////////////
// On window resize, window size is passed in, actual size of pathtraced
// image may be smaller (if we're subsampling for speed)
///////////////////////////////////////////////////////////////////////////
void resize(int w, int h)
{
	rendered_image.width = w / settings.subsampling;
	rendered_image.height = h / settings.subsampling;
	rendered_image.data.resize(rendered_image.width * rendered_image.height);
	restart();
}

///////////////////////////////////////////////////////////////////////////
// Return the radiance from a certain direction wi from the environment
// map.
///////////////////////////////////////////////////////////////////////////
vec3 Lenvironment(const vec3& wi)
{
	const float theta = acos(std::max(-1.0f, std::min(1.0f, wi.y)));
	float phi = atan(wi.z, wi.x);
	if(phi < 0.0f)
		phi = phi + 2.0f * M_PI;
	vec2 lookup = vec2(phi / (2.0 * M_PI), theta / M_PI);
	return environment.multiplier * environment.map.sample(lookup.x, lookup.y);
}

///////////////////////////////////////////////////////////////////////////
// Calculate the radiance going from one point (r.hitPosition()) in one
// direction (-r.d), through path tracing.
///////////////////////////////////////////////////////////////////////////
vec3 Li(Ray& primary_ray)
{
	vec3 L = vec3(0.0f);
	vec3 path_throughput = vec3(1.0);
	Ray current_ray = primary_ray;

	for (int bounces = 0; bounces <= settings.max_bounces; bounces++) {
		
		// Get Intersection
		Intersection hit = getIntersection(current_ray);

		// Create Material tree
		/*
		Diffuse diffuse(hit.material->m_color);
		BRDF& mat = diffuse;
		*/
		
		Diffuse diffuse(hit.material->m_color);
		BlinnPhong dielectric(hit.material->m_shininess, hit.material->m_fresnel, &diffuse);
		BlinnPhongMetal metal(hit.material->m_color, hit.material->m_shininess,
			hit.material->m_fresnel);
		LinearBlend metal_blend(hit.material->m_metalness, &metal, &dielectric);
		LinearBlend reflectivity_blend(hit.material->m_reflectivity, &metal_blend, &diffuse);
		BRDF& mat = reflectivity_blend;
		

		// Sample light point in disk

		DiskLight disk_light;

		if (randf() < 0.5f) {
			disk_light = disk_lights[0];
		}
		else {
			disk_light = disk_lights[1];
		}

		float diskSampleX;
		float diskSampleY;

		concentricSampleDisk(&diskSampleX, &diskSampleY);

		vec3 diskSample = vec3(diskSampleX * disk_light.radius, 0.0f, diskSampleY * disk_light.radius) + disk_light.position;


		// Check if in shadow
		Ray shadow_ray;

		shadow_ray.o = hit.position + (EPSILON * hit.shading_normal);
		shadow_ray.d = normalize(diskSample - hit.position);

		if (!occluded(shadow_ray)) {
			// Direct illumination
			float diskArea = M_PI * (disk_light.radius * disk_light.radius);
			const float distance_to_light = length(diskSample - hit.position);
			const float falloff_factor = 1.0f / (distance_to_light * distance_to_light);
			vec3 wi = normalize(diskSample - hit.position);
			vec3 Li = 2 * disk_light.intensity_multiplier * disk_light.color * falloff_factor * dot(-wi, disk_light.normal) * diskArea;
			Li /= 500.0f;

			L += path_throughput * (mat.f(wi, hit.wo, hit.shading_normal) * Li * std::max(0.0f, dot(wi, hit.shading_normal)));
		}

		// Emitted radiance from intersection (need to check)
		 L += path_throughput * hit.material->m_emission;

		// Sample an incoming direction (and the brdf and pdf for that direction)
		vec3 rand_wi = vec3(0.0f);
		float pdf = 0.0f;
		vec3 brdf = mat.sample_wi(rand_wi, hit.wo, hit.shading_normal, pdf);

		// Calculate cosine term to attenuate incoming light based on incident angle
		float cos_term = abs(dot(rand_wi, hit.shading_normal));

		// There are case where the pdf value doesnt get changed before the refraction layer is NULL
		// Therefore, causing NaN errors
		if (pdf == 0.0f) {
			path_throughput = vec3(0.0f);
		}
		else {
			path_throughput = path_throughput * (brdf * cos_term) / pdf;
		}
		

		// If path throughput is 0, there is no need to continue
		if (path_throughput == vec3(0.0f)) {
			return L;
		}

		// Create next ray on path
		Ray nextRayInPath;

		nextRayInPath.o = hit.position + (EPSILON * hit.shading_normal);
		nextRayInPath.d = rand_wi;

		if (!intersect(nextRayInPath)) {
			return L + (path_throughput * Lenvironment(nextRayInPath.d));
		}

		current_ray = nextRayInPath;

	}

	/*  Simple ray tracer

	///////////////////////////////////////////////////////////////////
	// Get the intersection information from the ray
	///////////////////////////////////////////////////////////////////
	Intersection hit = getIntersection(current_ray);

	Ray shadow_ray;

	shadow_ray.o = hit.position + EPSILON;
	shadow_ray.d = normalize(point_light.position - shadow_ray.o);

	if (occluded(shadow_ray)) {

		// Possibly not, since these would be super hard shadows
		return L;
	}
	///////////////////////////////////////////////////////////////////
	// Create a Material tree for evaluating brdfs and calculating
	// sample directions.
	///////////////////////////////////////////////////////////////////

	Diffuse diffuse(hit.material->m_color);
	BlinnPhong dielectric(hit.material->m_shininess, hit.material->m_fresnel, &diffuse);
	BlinnPhongMetal metal(hit.material->m_color, hit.material->m_shininess,
		hit.material->m_fresnel);
	LinearBlend metal_blend(hit.material->m_metalness, &metal, &dielectric);
	LinearBlend reflectivity_blend(hit.material->m_reflectivity, &metal_blend, &diffuse);
	BRDF& mat = reflectivity_blend;

	///////////////////////////////////////////////////////////////////
	// Calculate Direct Illumination from light.
	///////////////////////////////////////////////////////////////////
	{
		const float distance_to_light = length(point_light.position - hit.position);
		const float falloff_factor = 1.0f / (distance_to_light * distance_to_light);
		vec3 Li = point_light.intensity_multiplier * point_light.color * falloff_factor;
		vec3 wi = normalize(point_light.position - hit.position);
		L = mat.f(wi, hit.wo, hit.shading_normal) * Li * std::max(0.0f, dot(wi, hit.shading_normal));
	}
	// Return the final outgoing radiance for the primary ray
	return L;

	*/
}

///////////////////////////////////////////////////////////////////////////
// Used to homogenize points transformed with projection matrices
///////////////////////////////////////////////////////////////////////////
inline static glm::vec3 homogenize(const glm::vec4& p)
{
	return glm::vec3(p * (1.f / p.w));
}

///////////////////////////////////////////////////////////////////////////
// Trace one path per pixel and accumulate the result in an image
///////////////////////////////////////////////////////////////////////////
void tracePaths(const glm::mat4& V, const glm::mat4& P)
{
	// Stop here if we have as many samples as we want
	if((int(rendered_image.number_of_samples) > settings.max_paths_per_pixel)
	   && (settings.max_paths_per_pixel != 0))
	{
		return;
	}
	vec3 camera_pos = vec3(glm::inverse(V) * vec4(0.0f, 0.0f, 0.0f, 1.0f));
	// Trace one path per pixel (the omp parallel stuf magically distributes the
	// pathtracing on all cores of your CPU).
	int num_rays = 0;
	vector<vec4> local_image(rendered_image.width * rendered_image.height, vec4(0.0f));

#pragma omp parallel for
	for(int y = 0; y < rendered_image.height; y++)
	{
		for(int x = 0; x < rendered_image.width; x++)
		{
			vec3 color;
			Ray primaryRay;
			primaryRay.o = camera_pos;
			// Create a ray that starts in the camera position and points toward
			// the current pixel on a virtual screen.
			vec2 screenCoord = vec2(float(x) / float(rendered_image.width),
			                        float(y) / float(rendered_image.height));
			// Calculate direction
			vec4 viewCoord = vec4(screenCoord.x * 2.0f - 1.0f + ((randf()* 2.0f - 1.0f)/400.0f), screenCoord.y * 2.0f - 1.0f + ((randf() * 2.0f - 1.0f) / 400.0f), 1.0f, 1.0f);
			vec3 p = homogenize(inverse(P * V) * viewCoord);
			primaryRay.d = normalize(p - camera_pos);
			// Intersect ray with scene
			if(intersect(primaryRay))
			{
				// If it hit something, evaluate the radiance from that point
				vec3 radiance = Li(primaryRay);

				if (isnan(radiance.x)) {
					int x = 1;
				}

				color = radiance;
			}
			else
			{
				// Otherwise evaluate environment
				color = Lenvironment(primaryRay.d);
			}
			// Accumulate the obtained radiance to the pixels color
			float n = float(rendered_image.number_of_samples);
			rendered_image.data[y * rendered_image.width + x] =
			    rendered_image.data[y * rendered_image.width + x] * (n / (n + 1.0f))
			    + (1.0f / (n + 1.0f)) * color;
		}
	}
	rendered_image.number_of_samples += 1;
}
}; // namespace pathtracer
