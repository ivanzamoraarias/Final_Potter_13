#include "Light.h"
#include <algorithm>

namespace pathtracer
{
	std::pair<vec3, vec3> DiskLight::sample()
	{
		float diskSampleX;
		float diskSampleY;

		concentricSampleDisk(&diskSampleX, &diskSampleY);

		vec3 diskSample = vec3(diskSampleX * radius, 0.0f, diskSampleY * radius) + position;

		vec3 light_color(color);
		if (texture.valid)
		{
			light_color = texSampleRGBA(texture, (diskSampleX + 1) / 2, (diskSampleY + 1) / 2);
		}

		return std::make_pair(diskSample, light_color);
	}

	std::pair<vec3, vec3> QuadLight::sample()
	{

		return std::make_pair(vec3(0.0f), vec3(0.0f));
	}

	std::pair<vec3, vec3> SphereLight::sample(vec3 point)
	{

		// Calculate coordinate system for sphere sampling
		vec3 wc = normalize(position - point);
		vec3 wcX, wcY;
		if (std::abs(wc.x) > std::abs(wc.y))
			wcX = vec3(-wc.z, 0, wc.x) /
			std::sqrt(wc.x * wc.x + wc.z * wc.z);
		else
			wcX = vec3(0, wc.z, -wc.y) /
			std::sqrt(wc.y * wc.y + wc.z * wc.z);
		wcY = cross(wc, wcX);


		float distance = length(position - point);

		vec3 sphereSample(0.0f);

		uniformSphereCone(&sphereSample, -wcX, -wcY, wc, distance);

		normal = -sphereSample;

		vec3 light_color(color);
		if (texture.valid)
		{
			light_color = texSampleRGBA(texture, (sphereSample.x + 1) / 2, (sphereSample.y + 1) / 2);
		}

		sphereSample = radius * sphereSample;

		// reproject to sphere surface
		sphereSample = sphereSample * (radius / length(sphereSample - vec3(0.0f)));

		// place in world
		sphereSample = sphereSample + position;

		

		return std::make_pair(sphereSample, light_color);
	}
}