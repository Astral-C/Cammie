#include "UMapRenderer.hpp"
#include <glm/gtc/type_ptr.hpp>
#include "imgui.h"
#include "fmt/format.h"
#include <glad/glad.h>

static std::map<std::string, std::shared_ptr<BinModel>> ModelCache;

glm::mat4 computeBinTransform(glm::vec3 scale, glm::vec3 dir, glm::vec3 pos){
    glm::mat4 transform = glm::identity<glm::mat4>();
    transform = glm::scale(transform, scale); 
    transform = glm::rotate(transform, glm::radians(dir.x), glm::vec3(1,0,0));
    transform = glm::rotate(transform, glm::radians(dir.y), glm::vec3(0,1,0));
    transform = glm::rotate(transform, glm::radians(dir.z), glm::vec3(0,0,1));
    transform = glm::translate(transform, pos);
	return transform;
}

CMapRenderer::~CMapRenderer(){
	ModelCache.clear();
}

void CMapRenderer::LoadModels(std::string mapNumber, std::string roomNumber){
	std::filesystem::path modelPath = std::filesystem::path(Options.mRootPath) / "files" / "Iwamoto" / mapNumber /  fmt::format("{0}.arc", roomNumber);
	
	if(std::filesystem::exists(modelPath)){
		GCarchive modelArc;
		GCResourceManager.LoadArchive(modelPath.c_str(), &modelArc);
		for (GCarcfile* file = modelArc.files; file < modelArc.files + modelArc.filenum; file++){
			if(std::filesystem::path(file->name).extension() == ".bin"){
				bStream::CMemoryStream modelStream((uint8_t*)file->data, file->size, bStream::Endianess::Big, bStream::OpenMode::In);
				
				auto data = std::make_shared<BinModel>(&modelStream);
				if(!(std::filesystem::path(file->name).stem().string() == "room")){
					ModelCache.insert({std::filesystem::path(file->name).stem().string(), data});
				} else {
					ModelCache.insert({roomNumber, data});
				}

			}
		}
		gcFreeArchive(&modelArc);
	} else {
		std::cout << "Couldn't find archive " << modelPath << std::endl;
	}
}

void CMapRenderer::LoadMap(std::filesystem::path map_path){

	mMapRooms.clear();
	ModelCache.clear();

	GCarchive mapArchive;

    //Get scenario bcsv (its the only file in galaxy_path)

	if(!std::filesystem::exists(map_path)){
		std::cout << "Couldn't open map archive " << map_path << std::endl;
		return;
	}

    GCResourceManager.LoadArchive(map_path.string().c_str(), &mapArchive);

    for(GCarcfile* file = mapArchive.files; file < mapArchive.files + mapArchive.filenum; file++){
        if(strcmp(file->name, "furnitureinfo") == 0){
            LJmpIO FurnitureInfo;
            bStream::CMemoryStream FurnitureInfoStream((uint8_t*)file->data, (size_t)file->size, bStream::Endianess::Big, bStream::OpenMode::In);
            FurnitureInfo.Load(&FurnitureInfoStream);
            for(size_t entry = 0; entry < FurnitureInfo.GetEntryCount(); entry++){
				//TODO: Load models
				uint32_t room_no = FurnitureInfo.GetUnsignedInt(entry, "room_no");
				LoadModels(map_path.stem().string(), fmt::format("room_{:02}", room_no));

				glm::vec3 pos = {FurnitureInfo.GetFloat(entry, "pos_z"), FurnitureInfo.GetFloat(entry, "pos_y"), FurnitureInfo.GetFloat(entry, "pos_x")};
				glm::vec3 dir = {FurnitureInfo.GetFloat(entry, "dir_z"), FurnitureInfo.GetFloat(entry, "dir_y"), FurnitureInfo.GetFloat(entry, "dir_x")};
				glm::vec3 scale = {FurnitureInfo.GetFloat(entry, "scale_z"), FurnitureInfo.GetFloat(entry, "scale_y"), FurnitureInfo.GetFloat(entry, "scale_x")};

				if(mMapRooms.contains(room_no)){
					mMapRooms.at(room_no).insert({FurnitureInfo.GetString(entry, "dmd_name"), computeBinTransform(scale, dir, pos)});
				} else {
					std::map<std::string, glm::mat4> roomFurniture;
					roomFurniture.insert({FurnitureInfo.GetString(entry, "dmd_name"), computeBinTransform(scale, dir, pos)});
					mMapRooms.insert({room_no, roomFurniture});
					mEnabledRooms.insert({room_no, false});
				}

            }
        }
    }

	gcFreeArchive(&mapArchive);
}

void CMapRenderer::RenderUI() {
	for(auto& [room, enabled] : mEnabledRooms){
		ImGui::Text(fmt::format("Room {0}", room).c_str());
		ImGui::SameLine();
		ImGui::Checkbox(fmt::format("##RoomNo{0}", room).c_str(), &enabled);
	}
}

void CMapRenderer::RenderMap(float dt, glm::mat4* proj, glm::mat4* view){
    glFrontFace(GL_CW);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(mProgramID);

	glUniformMatrix4fv(glGetUniformLocation(mProgramID, "proj"), 1, 0, &((*proj)[0][0]));
	glUniformMatrix4fv(glGetUniformLocation(mProgramID, "view"), 1, 0, &((*view)[0][0]));

	//todo: loop through furniture and render
	for(auto& [room_no, room_models] : mMapRooms){
		if(mEnabledRooms.contains(room_no) && mEnabledRooms.at(room_no) && mMapRooms.contains(room_no)){
			for(auto& [model_name, model_transform] : room_models){
				if(ModelCache.contains(model_name)){
					ModelCache.at(model_name)->Draw(model_transform);
				} else {
					//std::cout << "no model " << model_name << " in cache?" << std::endl;
				}
			}

			std::string roomNo = fmt::format("room_{:02}", room_no);
			if(ModelCache.contains(roomNo)){
				glm::mat4 ident = glm::identity<glm::mat4>();
				ModelCache.at(roomNo)->Draw(ident);
			}
		}
	}
}