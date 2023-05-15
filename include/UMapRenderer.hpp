#pragma once

#include <map>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <filesystem>

#include "io/BinIO.hpp"
#include "io/JmpIO.hpp"
#include "ResUtil.hpp"

class CMapRenderer {
	std::map<uint32_t, std::vector<std::pair<std::string, glm::mat4>>> mMapRooms;
	std::map<uint32_t, bool> mEnabledRooms;

	void LoadModels(std::string mapNumber, std::string roomNumber);

public:
	void RenderUI();
	void RenderMap(float dt, glm::mat4* proj, glm::mat4* view);
	void LoadMap(std::filesystem::path map_path);

	~CMapRenderer();
};