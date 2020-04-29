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

std::vector<glm::vec3> terrainGenerator::getVerticesPosition()
{	
	return vertices;
}

void terrainGenerator::createTerrain()
{
	this->getZvalues();

	std::vector<glm::vec3> tempvex;
	for (int y = 0; y < rows - 1; y++) {
		for (int x = 0; x < cols; x++) {
			tempvex.push_back(
				glm::vec3(x * scl, y * scl, zValues[x][y])
			);
			tempvex.push_back(
				glm::vec3(x * scl, (y + 1) * scl, zValues[x][y])
			);
			
		}
	}

	int currentIndex = 0;


	for (int i = 0; i < tempvex.size(); i++) {
		if (i + 2 >= tempvex.size())
			break;

		vertices.push_back(tempvex[i]);
		vertices.push_back(tempvex[i+1]);
		vertices.push_back(tempvex[i+2]);


	}

}

// creas una textura, generar una imagen(tabla de valores, un valor por cada vertice que vas a tener) con valores aleatorios y usas los valores de esa imagen apra crear el mesh

std::vector<glm::vec3> terrainGenerator::getNormals()
{
	std::vector<glm::vec3> normals;

	for (glm::vec3 v : vertices) {
		normals.push_back(glm::normalize(v));
	}
	
	return normals;
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

