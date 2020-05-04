#pragma once
#include <glm/glm.hpp>
#include <vector>
class terrainGenerator
{
public:
	terrainGenerator(float w, float h, float scl);
	~terrainGenerator();

	std::vector <glm::vec3> vertices;
	std::vector<glm::vec3> getVerticesPosition();
	std::vector<glm::vec3> getVerticesPositionTest();
	void createTerrain();
	std::vector<glm::vec3> getNormals();
private:
	int cols, rows;
	float scl;
	float** zValues;

	float** getZvalues();



};

