
#include "heightfield.h"

#include <iostream>
#include <stdint.h>
#include <vector>
#include <glm/glm.hpp>
#include <stb_image.h>

using namespace glm;
using std::string;

HeightField::HeightField(void)
    : m_meshResolution(0)
    , m_vao(UINT32_MAX)
    , m_positionBuffer(UINT32_MAX)
    , m_uvBuffer(UINT32_MAX)
    , m_indexBuffer(UINT32_MAX)
    , m_numIndices(0)
    , m_texid_hf(UINT32_MAX)
    , m_texid_diffuse(UINT32_MAX)
    , m_heightFieldPath("")
    , m_diffuseTexturePath("")
{
}

void HeightField::loadHeightField(const std::string& heigtFieldPath)
{
	int width, height, components;
	stbi_set_flip_vertically_on_load(true);
	float* data = stbi_loadf(heigtFieldPath.c_str(), &width, &height, &components, 1);
	if(data == nullptr)
	{
		std::cout << "Failed to load image: " << heigtFieldPath << ".\n";
		return;
	}

	if(m_texid_hf == UINT32_MAX)
	{
		glGenTextures(1, &m_texid_hf);
	}
	glBindTexture(GL_TEXTURE_2D, m_texid_hf);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, width, height, 0, GL_RED, GL_FLOAT,
	             data); // just one component (float)

	m_heightFieldPath = heigtFieldPath;
	std::cout << "Successfully loaded heigh field texture: " << heigtFieldPath << ".\n";
}

void HeightField::loadDiffuseTexture(const std::string& diffusePath)
{
	int width, height, components;
	stbi_set_flip_vertically_on_load(true);
	uint8_t* data = stbi_load(diffusePath.c_str(), &width, &height, &components, 3);
	if(data == nullptr)
	{
		std::cout << "Failed to load image: " << diffusePath << ".\n";
		return;
	}

	if(m_texid_diffuse == UINT32_MAX)
	{
		glGenTextures(1, &m_texid_diffuse);
	}

	glBindTexture(GL_TEXTURE_2D, m_texid_diffuse);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data); // plain RGB
	glGenerateMipmap(GL_TEXTURE_2D);

	std::cout << "Successfully loaded diffuse texture: " << diffusePath << ".\n";
}


void HeightField::generateMesh(int tesselation)
{
	std::vector <glm::vec3> vertices;
	//std::vector<glm::vec3>vertices();
	float scl = 30;

	// generate a mesh in range -1 to 1 in x and z
	// (y is 0 but will be altered in height field vertex shader)
	int m_meshResolution = tesselation;

	int size = m_meshResolution * m_meshResolution * 2 * 3 * 3;
	float* positions = new float[size]();

	double triangleWidth = 2 / m_meshResolution;
	float startX = -1.0f;
	float startZ = 1.0f;
	float x1 = 0;
	float x2 = 0;
	float z1 = 0;
	float z2 = 0;
	int index = 0;
	//For each row
	for (int r = 0; r < m_meshResolution; r++) {
		//For each column
		for (int c = 0; c < m_meshResolution; c++) {
			x1 = startX + c * scl;
			x2 = startX + (c + 1) * scl;
			z1 = startZ - r * scl;
			z2 = startZ - (r + 1) * scl;
			//Generate one triangle
			vertices.push_back(glm::vec3(x1, 0, z1));
			vertices.push_back(glm::vec3(x2, 0, z2));
			vertices.push_back(glm::vec3(x2, 0, z1));

			//Generate other triangle
			vertices.push_back(glm::vec3(x1, 0, z1));
			vertices.push_back(glm::vec3(x1, 0, z2));
			vertices.push_back(glm::vec3(x2, 0, z2));

		}
	}

	
	glEnable(GL_PRIMITIVE_RESTART);
	glPrimitiveRestartIndex(UINT32_MAX);

	GLuint vertexArrayObject;
	GLuint positionBuffer;
	glGenBuffers(1, &positionBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, positionBuffer);
	glBufferData(
		GL_ARRAY_BUFFER, 
		vertices.size()*sizeof(glm::vec3), 
		&vertices[0].x, 
		GL_STATIC_DRAW
	);

	glGenVertexArrays(1, &vertexArrayObject);
	glBindVertexArray(vertexArrayObject);
	glBindBuffer(GL_ARRAY_BUFFER, positionBuffer);
	glVertexAttribPointer(0, 3, GL_FLOAT, false/*normalized*/, 0/*stride*/, 0/*offset*/);
	glEnableVertexAttribArray(0);

}

void HeightField::submitTriangles(void)
{
	if(m_vao == UINT32_MAX)
	{
		std::cout << "No vertex array is generated, cannot draw anything.\n";
		return;
	}

}