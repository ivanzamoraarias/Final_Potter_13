#include "terrainGenerator.h"
#include "Noise.h"

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

std::vector<glm::vec3> terrainGenerator::getVerticesPositionTest()
{
	/*vertices = { {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f} };
	return vertices; */

	int m_meshResolution = 20;

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
			x1 = startX + c * this->scl;
			x2 = startX + (c + 1) * this->scl;
			z1 = startZ - r * this->scl;
			z2 = startZ - (r + 1) * this->scl;
			//Generate one triangle
			//float y1 = noise->getNoise(x1, 0, z1);
			//float y2 = noise->getNoise(x2, 0, z2);

			vertices.push_back(glm::vec3(x1, 0, z1));
			vertices.push_back(glm::vec3(x2, 0, z2));
			vertices.push_back(glm::vec3(x2, 0, z1));

			//Generate other triangle
			vertices.push_back(glm::vec3(x1, 0, z1));
			vertices.push_back(glm::vec3(x1, 0, z2));
			vertices.push_back(glm::vec3(x2, 0, z2));

		}
	}

	std::reverse(vertices.begin(), vertices.end());

	return vertices;
}

void terrainGenerator::createTerrain()
{
	//this->getZvalues();

	this->getVerticesPositionTest();
	this->setHeighWithNoise();

	/*vertices.push_back(glm::vec3(1,1,0));
	vertices.push_back(glm::vec3(4,2,0));
	vertices.push_back(glm::vec3(1,3,0));
	vertices.push_back(glm::vec3(1,3,0));
	vertices.push_back(glm::vec3(4,2,0));
	vertices.push_back(glm::vec3(4,4,0));*/

	//std::vector<glm::vec3> tempvex;
	//for (int y = 0; y < rows - 1; y++) {
	//	for (int x = 0; x < cols; x++) {
	//		tempvex.push_back(
	//			glm::vec3(x * scl, y * scl, zValues[x][y])
	//		);
	//		tempvex.push_back(
	//			glm::vec3(x * scl, (y + 1) * scl, zValues[x][y])
	//		);
	//		
	//	}
	//}

	//int currentIndex = 0;


	//for (int i = 0; i < tempvex.size(); i++) {
	//	if (i + 2 >= tempvex.size())
	//		break;

	//	vertices.push_back(tempvex[i]);
	//	vertices.push_back(tempvex[i+1]);
	//	vertices.push_back(tempvex[i+2]);


	//}

}

// creas una textura, generar una imagen(tabla de valores, un valor por cada vertice que vas a tener) con valores aleatorios y usas los valores de esa imagen apra crear el mesh

std::vector<glm::vec3> terrainGenerator::getNormals()
{
	std::vector<glm::vec3> normals;

	for (glm::vec3 v : vertices) {
		normals.push_back(glm::normalize(v));
		//normals.push_back(glm::normalize(v)*glm::vec3(1.0f, 1.0f, 1.0f));
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

void terrainGenerator::setHeighWithNoise()
{
	Noise* noise = new Noise(867);
	
	for (int i = 0; i < 2000; i++) {
		
		float newZ = 100 * noise->getNoise(
			this->vertices[i].x, 
			0.8, 
			this->vertices[i].z
		);
		newZ = newZ - floor(newZ);

		for (int j = 0; j < 2000; j++) {
			if (this->vertices[j].x == this->vertices[i].x && 
				this->vertices[j].z == this->vertices[i].z) {
				this->vertices[j].y = floor(255 * newZ);
			}
		}
	};

	
}

