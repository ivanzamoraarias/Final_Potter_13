#pragma once
#include <glm/glm.hpp>
#include <vector>
class terrainGenerator
{
public:
	terrainGenerator(float w, float h, float scl);
	~terrainGenerator();

	std::vector <glm::vec3> vertices;
	float* getVerticesPosition();
	void createTerrain();
private:
	int cols, rows;
	float scl;
	float** zValues;

	float** getZvalues();



};

