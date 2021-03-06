#include "Pathtracer.h"
#include <memory>
#include <iostream>
#include <map>
#include <algorithm>
#include "material.h"
#include "embree_copy.h"
#include "sampling.h"
#include "Model.h"


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
	DiskLight disk_light[1];
	SphereLight sphere_light[1];
	CameraSettings cam_settings;

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
		if (phi < 0.0f)
			phi = phi + 2.0f * M_PI;
		vec2 lookup = vec2(phi / (2.0 * M_PI), theta / M_PI);
		if (isnan(lookup.x) || isnan(lookup.y)) {
			int x = 0;
		}
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

			vec3 diffuse_color = vec3(hit.material->m_color);
			if (hit.material->m_color_texture.valid) {
				diffuse_color = texSampleRGBA(hit.material->m_color_texture, hit.textCoord.x, hit.textCoord.y);
				//diffuse_color = vec4(hit.material->m_color, hit.material->m_transparency);
			}

			float roughness = clamp(hit.material->m_shininess, 0.003f, 1.0f);
			if (hit.material->m_shininess_texture.valid)
			{
				roughness = texSampleR(hit.material->m_shininess_texture, hit.textCoord.x, hit.textCoord.y);
				roughness = clamp(roughness, 0.003f, 0.2f);
			}

			vec3 normal = hit.shading_normal;
			if (hit.material->m_bump_texture.valid)
			{
				vec3 t = hit.tangent;
				vec3 b = normalize(cross(normal, t));
				vec3 n = (texSampleRGB(hit.material->m_bump_texture, hit.textCoord.x, hit.textCoord.y));
				n = normalize((n * 2.0f) - 1.0f);
				mat3 tbn(t, b, normal);
				
				normal = normalize(tbn * n);
			}

			// check if a ray would change (enter or exit) medium
			float ni, no;
			bool isEnteringMaterial = false;
			
			if (dot(current_ray.d, normal) < 0.0f) {
				ni = 1.0f;
				no = 1.5f;
				isEnteringMaterial = true;
			}
			else {
				ni = 1.5f;
				no = 1.0f;				
			}

			Diffuse diffuse(diffuse_color);
			BTDF transparent(roughness, hit.material->m_fresnel, ni, no);
			BTDF_Metal metal(diffuse_color, roughness, hit.material->m_fresnel, ni, no);
			LinearBlend metal_blend(hit.material->m_metalness, &metal, &transparent);
			LinearBlend reflectivity_blend(hit.material->m_reflectivity, &metal_blend, &diffuse);
			BRDF& mat = reflectivity_blend;


			// Sample light point in disk


			/*if (randf() < 0.5f) {
				disk_light = disk_lights[0];
			}
			else {
				disk_light = disk_lights[1];
			}*/
			
			DiskLight light = disk_light[0];
			std::pair<vec3, vec3> light_sample = light.sample();

			/*SphereLight light = sphere_light[0];
			std::pair<vec3, vec3> light_sample = light.sample(hit.position);*/

			vec3 shapeSample = light_sample.first;
			vec3 lightColor = light_sample.second;

			// Check if in shadow
			Ray shadow_ray;

			shadow_ray.o = hit.position + (EPSILON * hit.shading_normal);
			shadow_ray.d = normalize(shapeSample - hit.position);

			if (!occluded(shadow_ray)) {
				// Direct illumination
				float area = M_PI * (light.radius * light.radius);
				const float distance_to_light = length(shapeSample - hit.position);
				const float falloff_factor = 1.0f / (distance_to_light * distance_to_light);
				vec3 wi = normalize(shapeSample - hit.position);
				vec3 Li = 2 * light.intensity_multiplier * lightColor * falloff_factor * dot(-wi, light.normal) * area;
				Li /= 500.0f;

				L += path_throughput * (mat.f(wi, hit.wo, normal) * Li * std::max(0.0f, dot(wi, normal)));
			}

			// Emitted radiance from intersection (need to check)
			L += path_throughput * hit.material->m_emission;

			// Sample an incoming direction (and the brdf and pdf for that direction)
			vec3 rand_wi = vec3(0.0f);
			float pdf = 0.0f;

			vec3 brdf = mat.sample_wi(rand_wi, hit.wo, normal, pdf);

			
			// Calculate cosine term to attenuate incoming light based on incident angle
			float cos_term = abs(dot(rand_wi, normal));

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

			if (isEnteringMaterial && transparent.isRefracted) {
				nextRayInPath.o = hit.position - (EPSILON * normal);
			}
			else {
				nextRayInPath.o = hit.position + (EPSILON * normal);
			}

			nextRayInPath.d = rand_wi;

			if (!intersect(nextRayInPath)) {
				return L + (path_throughput * Lenvironment(nextRayInPath.d));
			}

			current_ray = nextRayInPath;

		}
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
		if ((int(rendered_image.number_of_samples) > settings.max_paths_per_pixel)
			&& (settings.max_paths_per_pixel != 0))
		{
			return;
		}
		vec3 camera_pos = vec3(glm::inverse(V) * vec4(0.0f, 0.0f, 0.0f, 1.0f));
		vec3 camera_forward = -vec3(V[0][2], V[1][2], V[2][2]);
		// Trace one path per pixel (the omp parallel stuf magically distributes the
		// pathtracing on all cores of your CPU).
		int num_rays = 0;
		vector<vec4> local_image(rendered_image.width * rendered_image.height, vec4(0.0f));

#pragma omp parallel for
		for (int y = 0; y < rendered_image.height; y++)
		{
			for (int x = 0; x < rendered_image.width; x++)
			{
				vec3 color;
				Ray primaryRay;
				primaryRay.o = camera_pos;
				// Create a ray that starts in the camera position and points toward
				// the current pixel on a virtual screen.
				vec2 screenCoord = vec2(float(x) / float(rendered_image.width),
					float(y) / float(rendered_image.height));
				// Calculate direction
				vec4 viewCoord = vec4(screenCoord.x * 2.0f - 1.0f + ((randf() * 2.0f - 1.0f) / 400.0f), screenCoord.y * 2.0f - 1.0f + ((randf() * 2.0f - 1.0f) / 400.0f), 1.0f, 1.0f);
				vec3 p = homogenize(inverse(P * V) * viewCoord);
				primaryRay.d = normalize(p - camera_pos);

				// Depth of Field
				vec4 sensor_plane(camera_forward, 0.0f);
				sensor_plane.w = -dot(camera_forward, (camera_pos - camera_forward));

				float t = -(dot(camera_pos, vec3(sensor_plane)) + sensor_plane.w) / dot(primaryRay.d, vec3(sensor_plane));
				vec3 sensor_pos = camera_pos + primaryRay.d * t;

				// convert sensor pos to camera space
				vec3 cameraSpaceSensorPos = V * vec4(sensor_pos, 1.0f);

				cameraSpaceSensorPos.z *= cam_settings.focal_length;

				// back to world space
				sensor_pos = glm::inverse(V) * vec4(cameraSpaceSensorPos, 1.0f);


				// Point on aperture
				float angle = randf() * 2.0f * M_PI;
				float radius = sqrt(randf());
				vec2 offset(cos(angle), sin(angle));

				offset = offset * radius * cam_settings.aperture;
				
				vec3 cameraRight = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f) * V;
				vec3 cameraUp = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f) * V;

				vec3 aperturePos = camera_pos + (cameraRight * offset.x) + (cameraUp * offset.y);

				vec3 focal_point = primaryRay.o + (cam_settings.focal_distance * primaryRay.d);

				primaryRay.o = aperturePos;
				primaryRay.d = normalize(focal_point - aperturePos);

				// Intersect ray with scene
				if (intersect(primaryRay))
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
					color = vec4(Lenvironment(primaryRay.d), 1.0f);
				}

				//exposure
				color *= cam_settings.exposure;


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
