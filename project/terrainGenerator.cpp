#include "terrainGenerator.h"

terrainGenerator::terrainGenerator(float w, float h, float scl)
{
	this->cols =(int) w / scl;
	this->rows =(int) h / scl;
	this->scl = scl;

	this->zValues = new float* [rows];
	for (int i = 0; i < rows; i++) {
		this->zValues[i] = new float[cols];
		
	}

	this->createTerrain();
}

terrainGenerator::~terrainGenerator()
{
}

float* terrainGenerator::getVerticesPosition()
{
	int postionsSize = vertices.size() * 3;
	const int finalSize = postionsSize;
	float* positions = new float[finalSize];

	int index = 0;
	for (auto vex : vertices) {
		if (index >= finalSize)
			break;

		positions[index] = vex.x;
		positions[index+1] = vex.y;
		positions[index+2] = vex.z;

		index += 3;
	}
	return positions;
}

void terrainGenerator::createTerrain()
{
	this->getZvalues();

	for (int y = 0; y < rows - 1; y++) {
		for (int x = 0; x < cols; x++) {
			vertices.push_back(
				glm::vec3(x * scl, y * scl, zValues[x][y])
			);
			vertices.push_back(
				glm::vec3(x * scl, (y + 1) * scl, zValues[x][y])
			);
			
		}
	}
}

float** terrainGenerator::getZvalues()
{
	const float constantCol = this->cols;
	

	float yoffset = 0;
	for (int y = 0; y < rows; y++) {
		float xoffset = 0;
		for (int x = 0; x < cols; x++) {
			zValues[x][y] = rand() % 100 + -100;
			xoffset += 0.2;
		}
		yoffset += 0.2;
	}
	float a = zValues[0][0];
	return zValues;
}

