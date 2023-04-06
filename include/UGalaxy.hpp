#pragma once

#include <map>
#include <vector>
#include <string>
#include <glm/glm.hpp>

#include <J3D/J3DModelLoader.hpp>
#include <J3D/J3DModelData.hpp>
#include <J3D/J3DUniformBufferObject.hpp>
#include <J3D/J3DLight.hpp>
#include <J3D/J3DModelInstance.hpp>

static std::map<std::string, std::unique_ptr<J3DModelInstance>> ModelCache;

class CGalaxyRenderer {
	std::map<std::string, std::vector<std::pair<std::string, glm::vec3>>> layers;
};