#pragma once

#include <map>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <filesystem>

#include <J3D/J3DModelLoader.hpp>
#include <J3D/J3DModelData.hpp>
#include <J3D/J3DUniformBufferObject.hpp>
#include <J3D/J3DLight.hpp>
#include <J3D/J3DModelInstance.hpp>
#include "io/BcsvIO.hpp"
#include "ResUtil.hpp"

class CGalaxyRenderer {
	std::map<std::string, std::map<std::string, std::pair<std::vector<std::pair<std::string, glm::mat4>>, bool>>> mZones;
	std::map<std::string, glm::mat4> mZoneTransforms;

	std::vector<std::pair<std::string, glm::mat4>> LoadZoneLayer(GCarchive* zoneArchive, GCarcfile* layerDir, bool isMainGalaxyZone);
	void LoadModel(std::string modelName);

public:
	void RenderUI();
	void RenderGalaxy(float dt);
	void LoadGalaxy(std::filesystem::path galaxy_path, bool isGalaxy2);

	~CGalaxyRenderer();
};