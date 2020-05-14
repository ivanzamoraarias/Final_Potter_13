
#include <GL/glew.h>
#include <stb_image.h>
#include <chrono>
#include <iostream>
#include <labhelper.h>
#include <imgui.h>
#include <imgui_impl_sdl_gl3.h>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <Model.h>
#include <string>
#include <algorithm>
#include "Pathtracer.h"
#include "terrainGenerator.h"
#include "ParticleSystem.h"
#include "embree_copy.h"
#include "embree.h"

using namespace glm;
using namespace std;

///////////////////////////////////////////////////////////////////////////////
// Various globals
///////////////////////////////////////////////////////////////////////////////
SDL_Window* g_window = nullptr;
int windowWidth = 0, windowHeight = 0;

static float currentTime = 0.0f;
static float deltaTime = 0.0f;

// Mouse input
ivec2 g_prevMouseCoords = { -1, -1 };
bool g_isMouseDragging = false;

///////////////////////////////////////////////////////////////////////////////
// Shader programs
///////////////////////////////////////////////////////////////////////////////
GLuint shaderProgram;

///////////////////////////////////////////////////////////////////////////////
// GL texture to put pathtracing result into
///////////////////////////////////////////////////////////////////////////////
uint32_t pathtracer_result_txt_id;

///////////////////////////////////////////////////////////////////////////////
// Camera parameters.
///////////////////////////////////////////////////////////////////////////////
vec3 cameraPosition(0.0f, 0.0f, 30.0f);
vec3 cameraDirection = normalize(vec3(0.0f, 0.0f, 0.0f) - cameraPosition);
vec3 worldUp(0.0f, 1.0f, 0.0f);

///////////////////////////////////////////////////////////////////////////////
// Models
///////////////////////////////////////////////////////////////////////////////
vector<pair<labhelper::Model*, mat4>> models;

mat4 terrainModelMatrix;
labhelper::Model* terrainModel = nullptr;

// Clouds
ParticleSystem particle_system = ParticleSystem(100000);
std::vector<glm::vec4> dataParticles;

mat4 cloudsModelMatrix;
GLuint vertexArrayObject, particleBuffer, particleTexture;

///////////////////////////////////////////////////////////////////////////////
// Terrain
///////////////////////////////////////////////////////////////////////////////
terrainGenerator* terrain = new terrainGenerator(300.0f, 300.0f, 10.0f);
labhelper::Model* createTerrainModel() {
	labhelper::Model* model = new labhelper::Model;
	model->m_name = "terrain";
	model->m_filename = "terrain.obj";
	model->m_positions = terrain->getVerticesPosition();
	model->m_normals = terrain->getNormals();
	int sizeOfVertices = model->m_positions.size();
	model->m_texture_coordinates.resize(sizeOfVertices);
	/*for (int i = 0; i < sizeOfVertices; i++) {
		std::cout <<"Y: "<<&model->m_positions[0]<<"\n" ;
	}*/

	labhelper::Mesh m;
	m.m_material_idx = 0;
	m.m_name = "terrain_mesh";
	m.m_number_of_vertices = terrain->vertices.size();
	m.m_start_index = 0;
	model->m_meshes.push_back(m);

	labhelper::loadMaterials(model);
	///////////////////////////////////////////////////////////////////////
	// Upload to GPU
	///////////////////////////////////////////////////////////////////////
	glGenVertexArrays(1, &model->m_vaob);
	glBindVertexArray(model->m_vaob);
	glGenBuffers(1, &model->m_positions_bo);
	glBindBuffer(GL_ARRAY_BUFFER, model->m_positions_bo);
	glBufferData(GL_ARRAY_BUFFER, model->m_positions.size() * sizeof(glm::vec3), &model->m_positions[0].x,
		GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, false, 0, 0);
	glEnableVertexAttribArray(0);
	//glGenBuffers(1, &model->m_normals_bo);
	//glBindBuffer(GL_ARRAY_BUFFER, model->m_normals_bo);
	//glBufferData(GL_ARRAY_BUFFER, model->m_normals.size() * sizeof(glm::vec3), &model->m_normals[0].x,
	//	GL_STATIC_DRAW);
	//glVertexAttribPointer(1, 3, GL_FLOAT, false, 0, 0);
	//glEnableVertexAttribArray(1);
	//glGenBuffers(1, &model->m_texture_coordinates_bo);
	//glBindBuffer(GL_ARRAY_BUFFER, model->m_texture_coordinates_bo);
	//glBufferData(GL_ARRAY_BUFFER, model->m_texture_coordinates.size() * sizeof(glm::vec2),
	//	&model->m_texture_coordinates[0].x, GL_STATIC_DRAW);
	//glVertexAttribPointer(2, 2, GL_FLOAT, false, 0, 0);
	//glEnableVertexAttribArray(2);
	return model;
}


///////////////////////////////////////////////////////////////////////////////
// Clouds
///////////////////////////////////////////////////////////////////////////////

void loadParticleIntoOpenGl()
{
	int w, h, comp;
	unsigned char* image = stbi_load("../scenes/cloudImageTest.png", &w, &h, &comp, STBI_rgb_alpha);
	glGenTextures(0, &particleTexture);
	glBindTexture(GL_TEXTURE_2D, particleTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
	free(image);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glGenerateMipmap(GL_TEXTURE_2D);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	///////////////////////////////////////////////////////////////////////////
	// Create the vertex array object
	///////////////////////////////////////////////////////////////////////////
	// Create a handle for the vertex array object
	glGenVertexArrays(1, &vertexArrayObject);
	// Set it as current, i.e., related calls will affect this object
	glBindVertexArray(vertexArrayObject);
	/////////////////////////
	/// Buffer
	////////////
	glGenBuffers(1, &particleBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, particleBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec4) * 100000, nullptr, GL_STATIC_DRAW);
	//glBufferSubData(GL_ARRAY_BUFFER, 64 ,sizeof(data), &data);
	glVertexAttribPointer(0, 4, GL_FLOAT, false /*normalized*/, 0 /*stride*/, 0 /*offset*/);
	glEnableVertexAttribArray(0);
}

void drawParticles(const mat4& viewMatrix, const mat4& projMatrix) {
	/* Code for extracting data goes here */
	dataParticles.clear();
	dataParticles.reserve(int(particle_system.particles.size() * 1.5) + 1);
	// populate with vector 4 
	for (Particle p : particle_system.particles) {
		vec3 pos = vec3(viewMatrix * vec4(p.pos, 1.0f));
		dataParticles.push_back(vec4(pos, p.lifetime));
	}
	// sort particles with sort from c++ standard library
	std::sort(dataParticles.begin(), std::next(dataParticles.begin(), dataParticles.size()),
		[](const vec4& lhs, const vec4& rhs) { return lhs.z < rhs.z; });
	glBindVertexArray(vertexArrayObject);
	glBindBuffer(GL_ARRAY_BUFFER, particleBuffer);
	glBufferSubData(GL_ARRAY_BUFFER, 0, dataParticles.size() * sizeof(vec4), dataParticles.data());
	// Particles
	/*glUseProgram(particleProgram);
	labhelper::setUniformSlow(particleProgram, "screen_x", float(windowWidth));
	labhelper::setUniformSlow(particleProgram, "screen_y", float(windowHeight));
	labhelper::setUniformSlow(particleProgram, "P",
		projMatrix);*/
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, particleTexture);
	glEnable(GL_PROGRAM_POINT_SIZE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDrawArrays(GL_POINTS, 0, dataParticles.size());
	glDisable(GL_BLEND);
	particle_system.process_particles(deltaTime, cloudsModelMatrix);
}

///////////////////////////////////////////////////////////////////////////////
// Load shaders, environment maps, models and so on
///////////////////////////////////////////////////////////////////////////////
void initialize()
{
	///////////////////////////////////////////////////////////////////////////
	// Load shader program
	///////////////////////////////////////////////////////////////////////////
	shaderProgram = labhelper::loadShaderProgram("../pathtracer/simple.vert", "../pathtracer/simple.frag");

	///////////////////////////////////////////////////////////////////////////
	// Initial path-tracer settings
	///////////////////////////////////////////////////////////////////////////
	pathtracer::settings.max_bounces = 8;
	pathtracer::settings.max_paths_per_pixel = 0; // 0 = Infinite
#ifdef _DEBUG
	pathtracer::settings.subsampling = 16;
#else
	pathtracer::settings.subsampling = 4;
#endif

	///////////////////////////////////////////////////////////////////////////
	// Set up light
	///////////////////////////////////////////////////////////////////////////
	
	// Point Light
	
	pathtracer::point_light.intensity_multiplier = 0.0f;
	pathtracer::point_light.color = vec3(1.f, 1.f, 1.f);
	pathtracer::point_light.position = vec3(10.0f, 40.0f, 10.0f);
	

	// Disk Light
	pathtracer::disk_lights[0].intensity_multiplier = 1000.0f;
	pathtracer::disk_lights[0].color = vec3(1.f, 1.f, 1.f);
	pathtracer::disk_lights[0].position = vec3(0.0f, 5.0f, 0.0f);
	pathtracer::disk_lights[0].radius = 3.0f;
	pathtracer::disk_lights[0].normal = normalize(vec3(0.0f, 0.0f, 0.0f) - pathtracer::disk_lights[0].position);

	pathtracer::disk_lights[1].intensity_multiplier = 0.0f;
	pathtracer::disk_lights[1].color = vec3(1.f, 1.f, 1.f);
	pathtracer::disk_lights[1].position = vec3(-30.0f, 30.0f, 0.0f);
	pathtracer::disk_lights[1].radius = 10.0f;
	pathtracer::disk_lights[1].normal = normalize(vec3(0.0f, 0.0f, 0.0f) - pathtracer::disk_lights[1].position);

	///////////////////////////////////////////////////////////////////////////
	// Load environment map
	///////////////////////////////////////////////////////////////////////////
	pathtracer::environment.map.load("../scenes/envmaps/001.hdr");
	pathtracer::environment.multiplier = 1.0f;

	///////////////////////////////////////////////////////////////////////////
	// Load .obj models to scene
	///////////////////////////////////////////////////////////////////////////
	
	//models.push_back(make_pair(labhelper::loadModelFromOBJ("../scenes/ffxiii/FF13_360_CHARACTER_Claire_Farron_Default.obj"), scale(vec3(15.0f, 15.0f, 15.0f))));
	//models.push_back(make_pair(labhelper::loadModelFromOBJ("../scenes/ffxiii/Lightning.obj"), translate(vec3(15.0f, 0.0f, 0.0f)) * scale(vec3(15.0f, 15.0f, 15.0f))));
	//models.push_back(make_pair(labhelper::loadModelFromOBJ("../scenes/harry_potter/harry/skharrymesh.obj"), translate(vec3(15.0f, 0.0f, 0.0f)) * scale(vec3(1.0f, 1.0f, 1.0f))));

	//models.push_back(make_pair(labhelper::loadModelFromOBJ("../scenes/NewShip.obj"), translate(vec3(0.0f, 10.0f, 0.0f))));
	//models.push_back(make_pair(labhelper::loadModelFromOBJ("../scenes/landingpad2.obj"), mat4(1.0f)));
	//models.push_back(make_pair(labhelper::loadModelFromOBJ("../scenes/tetra_balls.obj"), translate(vec3(10.f, 0.f, 0.f))));
	models.push_back(make_pair(labhelper::loadModelFromOBJ("../scenes/BigSphere.obj"), mat4(1.0f)));
	//models.push_back(make_pair(labhelper::loadModelFromOBJ("../scenes/ground_plane.obj"), mat4(1.0f)));
	//models.push_back(make_pair(labhelper::loadModelFromOBJ("../scenes/Cube.obj"), translate(vec3(0.f, 2.f, 0.f))));
	//models.push_back(make_pair(labhelper::loadModelFromOBJ("../scenes/Cornell.obj"), scale(vec3(2.0f, 2.0f, 2.0f))));

	//terrainModel = createTerrainModel();

	//models.push_back(make_pair(terrainModel, scale(vec3(20.0f, 1.0f, 20.0f))));
	
	//models.push_back(make_pair(labhelper::loadModelFromOBJ("../scenes/BigSphere.obj"), translate(vec3(-15.0f, 5.0f, 0.0f))));

	/*int x = 0;

	while (x < 20) {
		particle_system.process_particles(deltaTime, cloudsModelMatrix);
		x++;
	}

	for (Particle p : particle_system.particles) {
		models.push_back(make_pair(labhelper::loadModelFromOBJ("../scenes/sphere.obj"), translate(p.pos)));
	}*/

	///////////////////////////////////////////////////////////////////////////
	// Add models to pathtracer scene
	///////////////////////////////////////////////////////////////////////////
	for(auto m : models)
	{
		pathtracer::addModel(m.first, m.second);
	}
	pathtracer::buildBVH();

	///////////////////////////////////////////////////////////////////////////
	// Generate result texture
	///////////////////////////////////////////////////////////////////////////
	glGenTextures(1, &pathtracer_result_txt_id);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, pathtracer_result_txt_id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	///////////////////////////////////////////////////////////////////////////
	// This is INCORRECT! But an easy way to get us a brighter image that
	// just looks a little better...
	///////////////////////////////////////////////////////////////////////////
	//glEnable(GL_FRAMEBUFFER_SRGB);
}

void display(void)
{
	{ ///////////////////////////////////////////////////////////////////////
		// If first frame, or window resized, or subsampling changes,
		// inform the pathtracer
		///////////////////////////////////////////////////////////////////////
		int w, h;
		SDL_GetWindowSize(g_window, &w, &h);
		static int old_subsampling;
		if(windowWidth != w || windowHeight != h || old_subsampling != pathtracer::settings.subsampling)
		{
			pathtracer::resize(w, h);
			windowWidth = w;
			windowWidth = h;
			old_subsampling = pathtracer::settings.subsampling;
		}
	}

	///////////////////////////////////////////////////////////////////////////
	// Trace one path per pixel
	///////////////////////////////////////////////////////////////////////////
	mat4 viewMatrix = lookAt(cameraPosition, cameraPosition + cameraDirection, worldUp);
	mat4 projMatrix = perspective(radians(45.0f),
	                              float(pathtracer::rendered_image.width)
	                                  / float(pathtracer::rendered_image.height),
	                              0.1f, 100.0f);
	pathtracer::tracePaths(viewMatrix, projMatrix);

	///////////////////////////////////////////////////////////////////////////
	// Copy pathtraced image to texture for display
	///////////////////////////////////////////////////////////////////////////
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, pathtracer::rendered_image.width,
	             pathtracer::rendered_image.height, 0, GL_RGB, GL_FLOAT, pathtracer::rendered_image.getPtr());

	///////////////////////////////////////////////////////////////////////////
	// Render a fullscreen quad, textured with our pathtraced image.
	///////////////////////////////////////////////////////////////////////////
	glViewport(0, 0, windowWidth, windowHeight);
	glClearColor(0.1f, 0.1f, 0.6f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	SDL_GetWindowSize(g_window, &windowWidth, &windowHeight);
	glUseProgram(shaderProgram);
	labhelper::drawFullScreenQuad();
}

bool handleEvents(void)
{
	// check events (keyboard among other)
	SDL_Event event;
	bool quitEvent = false;

	// Allow ImGui to capture events.
	ImGuiIO& io = ImGui::GetIO();

	while(SDL_PollEvent(&event))
	{
		ImGui_ImplSdlGL3_ProcessEvent(&event);

		if(event.type == SDL_QUIT || (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_ESCAPE))
		{
			quitEvent = true;
		}
		else if(event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT
		        && !ImGui::GetIO().WantCaptureMouse)
		{
			g_isMouseDragging = true;
			int x;
			int y;
			SDL_GetMouseState(&x, &y);
			g_prevMouseCoords.x = x;
			g_prevMouseCoords.y = y;
		}

		if(!(SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_LEFT)))
		{
			g_isMouseDragging = false;
		}

		if(event.type == SDL_MOUSEMOTION && g_isMouseDragging)
		{
			// More info at https://wiki.libsdl.org/SDL_MouseMotionEvent
			int delta_x = event.motion.x - g_prevMouseCoords.x;
			int delta_y = event.motion.y - g_prevMouseCoords.y;
			float rotationSpeed = 0.1f;
			mat4 yaw = rotate(rotationSpeed * deltaTime * -delta_x, worldUp);
			mat4 pitch = rotate(rotationSpeed * deltaTime * -delta_y,
			                    normalize(cross(cameraDirection, worldUp)));
			cameraDirection = vec3(pitch * yaw * vec4(cameraDirection, 0.0f));
			g_prevMouseCoords.x = event.motion.x;
			g_prevMouseCoords.y = event.motion.y;
			pathtracer::restart();
		}
	}

	if(!io.WantCaptureKeyboard)
	{
		// check keyboard state (which keys are still pressed)
		const uint8_t* state = SDL_GetKeyboardState(nullptr);
		vec3 cameraRight = cross(cameraDirection, worldUp);
		const float speed = 10.f;
		if(state[SDL_SCANCODE_W])
		{
			cameraPosition += deltaTime * speed * cameraDirection;
			pathtracer::restart();
		}
		if(state[SDL_SCANCODE_S])
		{
			cameraPosition -= deltaTime * speed * cameraDirection;
			pathtracer::restart();
		}
		if(state[SDL_SCANCODE_A])
		{
			cameraPosition -= deltaTime * speed * cameraRight;
			pathtracer::restart();
		}
		if(state[SDL_SCANCODE_D])
		{
			cameraPosition += deltaTime * speed * cameraRight;
			pathtracer::restart();
		}
		if(state[SDL_SCANCODE_Q])
		{
			cameraPosition -= deltaTime * speed * worldUp;
			pathtracer::restart();
		}
		if(state[SDL_SCANCODE_E])
		{
			cameraPosition += deltaTime * speed * worldUp;
			pathtracer::restart();
		}
	}

	return quitEvent;
}

void gui()
{
	// Inform imgui of new frame
	ImGui_ImplSdlGL3_NewFrame(g_window);

	///////////////////////////////////////////////////////////////////////////
	// Helpers for getting lists of materials and meshes into widgets
	///////////////////////////////////////////////////////////////////////////
	static auto model_getter = [](void* vec, int idx, const char** text) {
		auto& v = *(static_cast<std::vector<pair<labhelper::Model*, mat4>>*>(vec));
		if(idx < 0 || idx >= static_cast<int>(v.size()))
		{
			return false;
		}
		*text = v[idx].first->m_name.c_str();
		return true;
	};


	static auto mesh_getter = [](void* vec, int idx, const char** text) {
		auto& vector = *static_cast<std::vector<labhelper::Mesh>*>(vec);
		if(idx < 0 || idx >= static_cast<int>(vector.size()))
		{
			return false;
		}
		*text = vector[idx].m_name.c_str();
		return true;
	};

	static auto material_getter = [](void* vec, int idx, const char** text) {
		auto& vector = *static_cast<std::vector<labhelper::Material>*>(vec);
		if(idx < 0 || idx >= static_cast<int>(vector.size()))
		{
			return false;
		}
		*text = vector[idx].m_name.c_str();
		return true;
	};

	ImGui::Begin("Control Panel", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

	///////////////////////////////////////////////////////////////////////////
	// Pathtracer settings
	///////////////////////////////////////////////////////////////////////////
	if(ImGui::CollapsingHeader("Pathtracer", "pathtracer_ch", true, true))
	{
		ImGui::SliderInt("Subsampling", &pathtracer::settings.subsampling, 1, 16);
		ImGui::SliderInt("Max Bounces", &pathtracer::settings.max_bounces, 0, 16);
		ImGui::SliderInt("Max Paths Per Pixel", &pathtracer::settings.max_paths_per_pixel, 0, 1024);
		if(ImGui::Button("Restart Pathtracing"))
		{
			pathtracer::restart();
		}
	}

	///////////////////////////////////////////////////////////////////////////
	// Choose a model to modify
	///////////////////////////////////////////////////////////////////////////
	static int model_index = 0;
	static labhelper::Model* model = models[0].first;
	static int mesh_index = 0;
	static int material_index = model->m_meshes[mesh_index].m_material_idx;

	if(ImGui::CollapsingHeader("Models", "meshes_ch", true, true))
	{
		if(ImGui::Combo("Model", &model_index, model_getter, (void*)&models, int(models.size())))
		{
			model = models[model_index].first;
			mesh_index = 0;
			material_index = model->m_meshes[mesh_index].m_material_idx;
		}

		///////////////////////////////////////////////////////////////////////////
		// List all meshes in the model and show properties for the selected
		///////////////////////////////////////////////////////////////////////////

		if(ImGui::CollapsingHeader("Meshes", "meshes_ch", true, true))
		{
			if(ImGui::ListBox("Meshes", &mesh_index, mesh_getter, (void*)&model->m_meshes,
			                  int(model->m_meshes.size()), 8))
			{
				material_index = model->m_meshes[mesh_index].m_material_idx;
			}

			labhelper::Mesh& mesh = model->m_meshes[mesh_index];
			char name[256];
			strcpy(name, mesh.m_name.c_str());
			if(ImGui::InputText("Mesh Name", name, 256))
			{
				mesh.m_name = name;
			}
			labhelper::Material& selected_material = model->m_materials[material_index];
			if(ImGui::Combo("Material", &material_index, material_getter, (void*)&model->m_materials,
			                int(model->m_materials.size())))
			{
				mesh.m_material_idx = material_index;
			}
		}

		///////////////////////////////////////////////////////////////////////////
		// List all materials in the model and show properties for the selected
		///////////////////////////////////////////////////////////////////////////
		if(ImGui::CollapsingHeader("Materials", "materials_ch", true, true))
		{
			ImGui::ListBox("Materials", &material_index, material_getter, (void*)&model->m_materials,
			               uint32_t(model->m_materials.size()), 8);
			labhelper::Material& material = model->m_materials[material_index];
			char name[256];
			strcpy(name, material.m_name.c_str());
			if(ImGui::InputText("Material Name", name, 256))
			{
				material.m_name = name;
			}
			ImGui::ColorEdit3("Color", &material.m_color.x);
			ImGui::SliderFloat("Reflectivity", &material.m_reflectivity, 0.0f, 1.0f);
			ImGui::SliderFloat("Metalness", &material.m_metalness, 0.0f, 1.0f);
			ImGui::SliderFloat("Fresnel", &material.m_fresnel, 0.0f, 1.0f);
			ImGui::SliderFloat("shininess", &material.m_shininess, 0.0f, 2500.0f);
			ImGui::SliderFloat("Emission", &material.m_emission, 0.0f, 10.0f);
			ImGui::SliderFloat("Transparency", &material.m_transparency, 0.0f, 1.0f);

			///////////////////////////////////////////////////////////////////////////
			// A button for saving your results
			///////////////////////////////////////////////////////////////////////////
			if(ImGui::Button("Save Materials"))
			{
				labhelper::saveModelToOBJ(model, model->m_filename);
			}
		}
	}

	///////////////////////////////////////////////////////////////////////////
	// Light and environment map
	///////////////////////////////////////////////////////////////////////////
	if(ImGui::CollapsingHeader("Light sources", "lights_ch", true, true))
	{
		
		ImGui::SliderFloat("Environment multiplier", &pathtracer::environment.multiplier, 0.0f, 10.0f);
		/*
		ImGui::ColorEdit3("Point light color", &pathtracer::point_light.color.x);
		ImGui::SliderFloat("Point light intensity multiplier", &pathtracer::point_light.intensity_multiplier,
		                   0.0f, 10000.0f);
						   */
						   

		ImGui::ColorEdit3("Disk Light 1 color", &pathtracer::disk_lights[0].color.x);
		ImGui::SliderFloat("Disk Light 1 intensity multiplier", &pathtracer::disk_lights[0].intensity_multiplier,
			0.0f, 10000.0f);
		ImGui::SliderFloat("Disk Light 1 radius", &pathtracer::disk_lights[0].radius,
			0.0f, 100.0f);

		ImGui::ColorEdit3("Disk Light 2 color", &pathtracer::disk_lights[1].color.x);
		ImGui::SliderFloat("Disk Light 2 intensity multiplier", &pathtracer::disk_lights[1].intensity_multiplier,
			0.0f, 10000.0f);
		ImGui::SliderFloat("Disk Light 2 radius", &pathtracer::disk_lights[1].radius,
			0.0f, 100.0f);
	}

	ImGui::End(); // Control Panel

	// Render the GUI.
	ImGui::Render();
}

int main(int argc, char* argv[])
{
	g_window = labhelper::init_window_SDL("Pathtracer", 1280, 720);

	initialize();

	bool stopRendering = false;
	auto startTime = std::chrono::system_clock::now();

	while(!stopRendering)
	{
		//update currentTime
		std::chrono::duration<float> timeSinceStart = std::chrono::system_clock::now() - startTime;
		deltaTime = timeSinceStart.count() - currentTime;
		currentTime = timeSinceStart.count();

		// render to window
		display();

		// Then render overlay GUI.
		gui();

		// Swap front and back buffer. This frame will now be displayed.
		SDL_GL_SwapWindow(g_window);

		// check events (keyboard among other)
		stopRendering = handleEvents();
	}

	// Delete Models
	for(auto& m : models)
	{
		labhelper::freeModel(m.first);
	}
	// Shut down everything. This includes the window and all other subsystems.
	labhelper::shutDown(g_window);
	return 0;
}
