#include "UGalaxy.hpp"
#include "imgui.h"

static std::map<std::string, std::shared_ptr<J3DModelInstance>> ModelCache;

CGalaxyRenderer::~CGalaxyRenderer(){
	ModelCache.clear();
}

void CGalaxyRenderer::LoadModel(std::string modelName){
	std::filesystem::path modelPath = std::filesystem::path(Options.mRootPath) / "DATA" / "files" / "ObjectData" / (modelName + ".arc");
	
	if(std::filesystem::exists(modelPath)){
		GCarchive modelArc;
		GCResourceManager.LoadArchive(modelPath.c_str(), &modelArc);
		for (GCarcfile* file = modelArc.files; file < modelArc.files + modelArc.filenum; file++){
			if(std::filesystem::path(file->name).extension() == ".bdl"){
				J3DModelLoader Loader;
				bStream::CMemoryStream modelStream((uint8_t*)file->data, file->size, bStream::Endianess::Big, bStream::OpenMode::In);
				
				auto data = std::make_shared<J3DModelData>();
				data = Loader.Load(&modelStream, NULL);
				std::shared_ptr<J3DModelInstance> instance = data->GetInstance();
				ModelCache.insert({modelName, instance});
			}
		}
	}
}

std::vector<std::pair<std::string, glm::vec3>> CGalaxyRenderer::LoadZoneLayer(GCarchive* zoneArchive, GCarcfile* layerDir, bool isMainGalaxyZone){
	std::vector<std::pair<std::string, glm::vec3>> objects;
	for (GCarcfile* layer_file = &zoneArchive->files[zoneArchive->dirs[layerDir->size].fileoff]; layer_file < &zoneArchive->files[zoneArchive->dirs[layerDir->size].fileoff] + zoneArchive->dirs[layerDir->size].filenum; layer_file++){
		if((strcmp(layer_file->name, "stageobjinfo") == 0 || strcmp(layer_file->name, "StageObjInfo") == 0) && isMainGalaxyZone){
			// TODO: Load this for this zone
			//std::cout << "This should only happen once!" << std::endl;
			
			SBcsvIO StageObjInfo;
			bStream::CMemoryStream StageObjInfoStream((uint8_t*)layer_file->data, (size_t)layer_file->size, bStream::Endianess::Big, bStream::OpenMode::In);
			StageObjInfo.Load(&StageObjInfoStream);
			for(size_t stageObjEntry = 0; stageObjEntry < StageObjInfo.GetEntryCount(); stageObjEntry++){
				std::string zoneName = StageObjInfo.GetString(stageObjEntry, "name");
				std::cout << "Loading StageObjInfo Entry " << zoneName << std::endl;
				glm::vec3 position = {StageObjInfo.GetFloat(stageObjEntry, "pos_x"), StageObjInfo.GetFloat(stageObjEntry, "pos_y"), StageObjInfo.GetFloat(stageObjEntry, "pos_z")};
				glm::vec3 rotation = {StageObjInfo.GetFloat(stageObjEntry, "dir_x"), StageObjInfo.GetFloat(stageObjEntry, "dir_y"), StageObjInfo.GetFloat(stageObjEntry, "dir_z")};
				mZoneTransforms.insert({zoneName, {position, rotation}});
			}
		}
		if((strcmp(layer_file->name, "objinfo") == 0 || strcmp(layer_file->name, "ObjInfo") == 0) && layer_file->data != nullptr){
			SBcsvIO ObjInfo;
			bStream::CMemoryStream ObjInfoStream((uint8_t*)layer_file->data, (size_t)layer_file->size, bStream::Endianess::Big, bStream::OpenMode::In);
			ObjInfo.Load(&ObjInfoStream);
			for(size_t objEntry = 0; objEntry < ObjInfo.GetEntryCount(); objEntry++){
				std::string modelName = ObjInfo.GetString(objEntry, "name");
				glm::vec3 position = {ObjInfo.GetFloat(objEntry, "pos_x"), ObjInfo.GetFloat(objEntry, "pos_y"), ObjInfo.GetFloat(objEntry, "pos_z")};
				if(Options.mRootPath != "" && !ModelCache.contains(modelName)){
					LoadModel(modelName);
				}
				objects.push_back({modelName, position});

			}
		}
	}
	return objects;
}

void CGalaxyRenderer::LoadGalaxy(std::filesystem::path galaxy_path, bool isGalaxy2){

	mZones.clear();
	mZoneTransforms.clear();

	GCarchive scenarioArchive;

	std::string name = (galaxy_path / std::string(".")).parent_path().filename();

    //Get scenario bcsv (its the only file in galaxy_path)

	if(!std::filesystem::exists(galaxy_path / (name + "Scenario.arc"))){
		std::cout << "Couldn't open scenario archive " << galaxy_path / (name + "Scenario.arc") << std::endl;
		return;
	}

    GCResourceManager.LoadArchive((galaxy_path / (name + "Scenario.arc")).c_str(), &scenarioArchive);

    for(GCarcfile* file = scenarioArchive.files; file < scenarioArchive.files + scenarioArchive.filenum; file++){
        
        // Load Scenarios and cameras. Todo!

		/*
        if(strcmp(file->name, "scenariodata.bcsv") == 0){
            SBcsvIO ScenarioData;
            bStream::CMemoryStream ScenarioDataStream((uint8_t*)file->data, (size_t)file->size, bStream::Endianess::Big, bStream::OpenMode::In);
            ScenarioData.Load(&ScenarioDataStream);
            for(size_t entry = 0; entry < ScenarioData.GetEntryCount(); entry++){

            }
        }
		*/

        // Load all zones and all zone layers

        if(strcmp(file->name, "zonelist.bcsv") == 0 || strcmp(file->name, "ZoneList.bcsv") == 0){
            SBcsvIO ZoneData;
            bStream::CMemoryStream ZoneDataStream((uint8_t*)file->data, (size_t)file->size, bStream::Endianess::Big, bStream::OpenMode::In);
            ZoneData.Load(&ZoneDataStream);
            for(size_t entry = 0; entry < ZoneData.GetEntryCount(); entry++){
				std::filesystem::path zonePath = (galaxy_path.parent_path() / (ZoneData.GetString(entry, "ZoneName") + ".arc"));

				if(isGalaxy2){
					zonePath = (galaxy_path.parent_path() / ZoneData.GetString(entry, "ZoneName") / (ZoneData.GetString(entry, "ZoneName") + "Map.arc"));
				}
				
				if(!std::filesystem::exists(zonePath)){
					std::cout << "Couldn't open zone archive " << zonePath << std::endl;
					gcFreeArchive(&scenarioArchive);
					return;
				} else {
					std::cout << "Loading zone archive " << zonePath << std::endl;
                }

				GCarchive zoneArchive;
				GCResourceManager.LoadArchive(zonePath.c_str(), &zoneArchive);
				
				std::map<std::string, std::pair<std::vector<std::pair<std::string, glm::vec3>>, bool>> zone;

				for (GCarcfile* file = zoneArchive.files; file < zoneArchive.files + zoneArchive.filenum; file++){
					if(file->parent != nullptr && (strcmp(file->parent->name, "placement") == 0 || strcmp(file->parent->name, "Placement") == 0) && (file->attr & 0x02) && strcmp(file->name, ".") != 0 && strcmp(file->name, "..") != 0){
						std::cout << "Loading zone " << ZoneData.GetString(entry, "ZoneName") << " layer " << file->name << std::endl;
						auto layer = LoadZoneLayer(&zoneArchive, file, (ZoneData.GetString(entry, "ZoneName") == name));
						zone.insert({file->name, {layer, true}});
					}
				}
				
				mZones.insert({ZoneData.GetString(entry, "ZoneName"), zone});

				gcFreeArchive(&zoneArchive);
            }
        }
    }

	gcFreeArchive(&scenarioArchive);
}

void CGalaxyRenderer::RenderUI() {
	for(auto& [zoneName, zone] : mZones){
		if (ImGui::TreeNode(zoneName.c_str())){
			for(auto& [layerName, layer] : zone){
				ImGui::Checkbox(layerName.c_str(), &layer.second);
			}

			ImGui::TreePop();
		}
	}
}

void CGalaxyRenderer::RenderGalaxy(float dt){
	for(auto& [zoneName, zone] : mZones){
		for(auto& [layerName, layer] : zone){
			if(!layer.second) continue; //layer not set to visible
			for(auto& object : layer.first){
				if(ModelCache.count(object.first) == 0) continue; //TODO: Render placeholder
				if(mZoneTransforms.count(zoneName) != 0){
					ModelCache.at(object.first)->SetRotation(mZoneTransforms.at(zoneName).second);
					ModelCache.at(object.first)->SetTranslation(object.second + mZoneTransforms.at(zoneName).first);
				} else {
					ModelCache.at(object.first)->SetTranslation(object.second);
				}
				ModelCache.at(object.first)->Render(dt);
			}
		}
	}
}