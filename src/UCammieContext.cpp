#include "UCammieContext.hpp"

#include "UPointSpriteManager.hpp"
#include "io/KeyframeIO.hpp"
#include "imgui_neo_internal.h"
#include "imgui_neo_sequencer.h"
#include "ResUtil.hpp"

#include "util/UUIUtil.hpp"

#include <J3D/J3DModelLoader.hpp>
#include <J3D/J3DModelData.hpp>
#include <J3D/J3DUniformBufferObject.hpp>
#include <J3D/J3DLight.hpp>
#include <J3D/J3DModelInstance.hpp>

#include <bits/fs_path.h>
#include <glad/glad.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <bstream.h>
#include <optional>
#include <sys/types.h>
#include "fmt/core.h"
#include "ResUtil.hpp"


bool RenderTimelineTrack(std::string label, CTrackCommon* track, int* keyframeSelection){
	bool selected;
	ImGui::BeginNeoTimelineEx(label.data());
		for(auto&& key : track->mKeys){
			ImGui::NeoKeyframe(&key);

			if(ImGui::IsNeoKeyframeSelected()){
				*keyframeSelection = key;
				selected = true;
			}
		}

		if(ImGui::IsKeyPressed(ImGuiKey_Delete)){
			*keyframeSelection = -1;
			selected = false;

			uint32_t selected_count = ImGui::GetNeoKeyframeSelectionSize();
			ImGui::FrameIndexType * toRemove = new ImGui::FrameIndexType[selected_count];

			ImGui::GetNeoKeyframeSelection(toRemove);

			for(int i = 0; i < selected_count; i++){
				if(toRemove[i] <= 0) continue;
				track->DeleteKeyframe((uint32_t)toRemove[i]);
			}

			delete toRemove;
		}

	ImGui::EndNeoTimeLine();

	return selected;
}

inline float UpdateCameraAnimationTrack(CTrackCommon track, int currentFrame){
	CKeyframeCommon nextKeyframe, prevKeyframe;
	for(auto keyframe : track.mKeys){
		if(currentFrame >= keyframe) prevKeyframe = track.mFrames[keyframe];
		if(currentFrame < keyframe) {
			nextKeyframe = track.mFrames[keyframe];
			break;
		}
	}

	return glm::mix(prevKeyframe.value, nextKeyframe.value, (currentFrame - prevKeyframe.frame) / (nextKeyframe.frame - prevKeyframe.frame));
}

UCammieContext::UCammieContext(){
	Options.LoadOptions();
	mGrid.Init();
	mBillboardManager.Init(128, 2);
	mBillboardManager.SetBillboardTexture(std::filesystem::current_path() / "res/camera.png", 0);
	mBillboardManager.SetBillboardTexture(std::filesystem::current_path() / "res/target.png", 1);

	mBillboardManager.mBillboards.push_back(CPointSprite());
	mBillboardManager.mBillboards.push_back(CPointSprite());

	mBillboardManager.mBillboards[0].Texture = 0;
	mBillboardManager.mBillboards[0].SpriteSize = 204800;
	mBillboardManager.mBillboards[1].Texture = 1;
	mBillboardManager.mBillboards[1].SpriteSize = 204800;

	GCResourceManager.Init();

	ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontFromFileTTF((std::filesystem::current_path() / "res/NotoSansJP-Regular.otf").c_str(), 16.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	mCurrentFrame = mStartFrame = 0;
	mEndFrame = 10;
}

bool UCammieContext::Update(float deltaTime) {
	
	if(!(mPlaying && mViewCamera)) mCamera.Update(deltaTime);

	if(ImGui::IsKeyPressed(ImGuiKey_Space)){
		if(!std::count(XPositionTrack.mKeys.begin(), XPositionTrack.mKeys.end(), mCurrentFrame)){
			XPositionTrack.mKeys.insert(std::upper_bound(XPositionTrack.mKeys.begin(), XPositionTrack.mKeys.end(), mCurrentFrame), mCurrentFrame);
			XPositionTrack.mFrames.insert({(uint32_t)mCurrentFrame, {(float)mCurrentFrame, mCamera.GetPosition().x, 0, 0}});
		}

		if(!std::count(YPositionTrack.mKeys.begin(), YPositionTrack.mKeys.end(), mCurrentFrame)){
			YPositionTrack.mKeys.insert(std::upper_bound(YPositionTrack.mKeys.begin(), YPositionTrack.mKeys.end(), mCurrentFrame), mCurrentFrame);
			YPositionTrack.mFrames.insert({(uint32_t)mCurrentFrame, {(float)mCurrentFrame, mCamera.GetPosition().y, 0, 0}});
		}

		if(!std::count(ZPositionTrack.mKeys.begin(), ZPositionTrack.mKeys.end(), mCurrentFrame)){
			ZPositionTrack.mKeys.insert(std::upper_bound(ZPositionTrack.mKeys.begin(), ZPositionTrack.mKeys.end(), mCurrentFrame), mCurrentFrame);
			ZPositionTrack.mFrames.insert({(uint32_t)mCurrentFrame, {(float)mCurrentFrame, mCamera.GetPosition().z, 0, 0}});
		}
    }

	return true;
}

void UCammieContext::Render(float deltaTime) {
	int selectedKeyframe = -1;
	CTrackCommon* selectedTrack = nullptr;

	RenderMenuBar();
	
	const ImGuiViewport* mainViewport = ImGui::GetMainViewport();

	ImGuiDockNodeFlags dockFlags = ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_AutoHideTabBar | ImGuiDockNodeFlags_NoDockingInCentralNode;
	mMainDockSpaceID = ImGui::DockSpaceOverViewport(mainViewport, dockFlags);
	
	if(!bIsDockingSetUp){
		ImGui::DockBuilderRemoveNode(mMainDockSpaceID); // clear any previous layout
		ImGui::DockBuilderAddNode(mMainDockSpaceID, dockFlags | ImGuiDockNodeFlags_DockSpace);
		ImGui::DockBuilderSetNodeSize(mMainDockSpaceID, mainViewport->Size);


		mDockNodeBottomID = ImGui::DockBuilderSplitNode(mMainDockSpaceID, ImGuiDir_Down, 0.257f, nullptr, &mMainDockSpaceID);
		mDockNodeRightID = ImGui::DockBuilderSplitNode(mMainDockSpaceID, ImGuiDir_Left, 0.2f, nullptr, &mMainDockSpaceID);
		mDockNodeBottomRightID = ImGui::DockBuilderSplitNode(mDockNodeBottomID, ImGuiDir_Right, 0.2f, nullptr, &mDockNodeBottomID);

		ImGui::DockBuilderDockWindow("mainWindow", mDockNodeBottomID);
		ImGui::DockBuilderDockWindow("zoneWindow", mDockNodeRightID);
		ImGui::DockBuilderDockWindow("detailWindow", mDockNodeBottomRightID);

		ImGui::DockBuilderFinish(mMainDockSpaceID);
		bIsDockingSetUp = true;
	}


	ImGuiWindowClass mainWindowOverride;
	mainWindowOverride.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoTabBar;
	ImGui::SetNextWindowClass(&mainWindowOverride);
	
	ImGui::Begin("mainWindow", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);
		ImGui::Text(fmt::format("Camera Animation [{0}/{1}]", mCurrentFrame, mEndFrame).data());
		ImGui::SameLine();
		ImGui::Checkbox("Camera Sight", &mViewCamera);
		ImGui::SameLine();
		if(ImGui::Button("Play")){ mPlaying = true; mCurrentFrame = 0; mCamera.ResetView(); }
		if(mPlaying){ ImGui::SameLine(); if(ImGui::Button("Stop")) mPlaying = false; }
		if(!mShowZones){ ImGui::SameLine(); if(ImGui::Button("Zones")) mShowZones = true; }
		ImGui::SameLine(ImGui::GetWindowWidth() - 50);
		ImGui::InputInt("End Frame", &mEndFrame);
		ImGui::Separator();

		//This segment of code is insanely messy

		ImGui::BeginNeoSequencer("Sequencer", &mCurrentFrame, &mStartFrame, &mEndFrame, {0, 0}, mPlaying ? 0 : (ImGuiNeoSequencerFlags_EnableSelection | ImGuiNeoSequencerFlags_Selection_EnableDeletion));
			if(ImGui::BeginNeoGroup("Position", &mPositionOpen)){
				if(RenderTimelineTrack("X", &XPositionTrack, &selectedKeyframe)) selectedTrack = &XPositionTrack;
				if(RenderTimelineTrack("Y", &YPositionTrack, &selectedKeyframe)) selectedTrack = &YPositionTrack;
				if(RenderTimelineTrack("Z", &ZPositionTrack, &selectedKeyframe)) selectedTrack = &ZPositionTrack;
				ImGui::EndNeoGroup();
			}
			if(ImGui::BeginNeoGroup("Target", &mTargetOpen)){
				if(RenderTimelineTrack("X", &XTargetTrack, &selectedKeyframe)) selectedTrack = &XTargetTrack;
				if(RenderTimelineTrack("Y", &YTargetTrack, &selectedKeyframe)) selectedTrack = &YTargetTrack;
				if(RenderTimelineTrack("Z", &ZTargetTrack, &selectedKeyframe)) selectedTrack = &ZTargetTrack;
				ImGui::EndNeoGroup();
			}

			if(RenderTimelineTrack("Fov", &FovYTrack, &selectedKeyframe)) selectedTrack = &FovYTrack;
			if(RenderTimelineTrack("Twist", &TwistTrack, &selectedKeyframe)) selectedTrack = &TwistTrack;

		ImGui::EndNeoSequencer();

	ImGui::End();

	ImGui::SetNextWindowClass(&mainWindowOverride);

	ImGui::Begin("detailWindow", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);
		ImGui::Text("Selected Keyframe");
		ImGui::Separator();
		if(selectedKeyframe != -1 && selectedTrack != nullptr && selectedTrack->mFrames.count(selectedKeyframe) != 0){
			ImGui::InputFloat("Value", &selectedTrack->mFrames.at(selectedKeyframe).value);

			if(selectedTrack->mType == ETrackType::CKAN){
				if(selectedTrack->mSymmetricSlope){
					ImGui::InputFloat("Slope", &selectedTrack->mFrames.at(selectedKeyframe).inslope);
				} else {
					ImGui::InputFloat("In Slope", &selectedTrack->mFrames.at(selectedKeyframe).inslope);
					ImGui::InputFloat("Out Slope", &selectedTrack->mFrames.at(selectedKeyframe).outslope);
	            }
            }
			if(ImGui::Button("Set Camera to Keyframe")){
				mUpdateCameraPosition = true;
				mCurrentFrame = selectedKeyframe;
			}
		}
	ImGui::End();

	if(mShowZones){
		ImGui::SetNextWindowClass(&mainWindowOverride);

		ImGui::Begin("zoneWindow", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);
			ImGui::Text("Zones");
			ImGui::SameLine(ImGui::GetWindowWidth()-40);
			if(ImGui::Button("Hide")) mShowZones = false;
			ImGui::Separator();
			mGalaxyRenderer.RenderUI();
		ImGui::End();
	}

	glm::mat4 projection, view;
	projection = mCamera.GetProjectionMatrix();
	view = mCamera.GetViewMatrix();

	if(!mSetLights) SetLights();
	J3DUniformBufferObject::SetProjAndViewMatrices(&projection, &view);
	
	//Render Models here

	mGrid.Render(mCamera.GetPosition(), mCamera.GetProjectionMatrix(), mCamera.GetViewMatrix());
	mGalaxyRenderer.RenderGalaxy(deltaTime);
	mBillboardManager.Draw(&mCamera);

	glm::vec3 eyePos;// = mCamera.GetEye();
	glm::vec3 centerPos;// = mCamera.GetCenter();

	eyePos.x = UpdateCameraAnimationTrack(XPositionTrack, mCurrentFrame);
	eyePos.y = UpdateCameraAnimationTrack(YPositionTrack, mCurrentFrame);
	eyePos.z = UpdateCameraAnimationTrack(ZPositionTrack, mCurrentFrame);

	centerPos.x = UpdateCameraAnimationTrack(XTargetTrack, mCurrentFrame);
	centerPos.y = UpdateCameraAnimationTrack(YTargetTrack, mCurrentFrame);
	centerPos.z = UpdateCameraAnimationTrack(ZTargetTrack, mCurrentFrame);

	mBillboardManager.mBillboards[1].Position = centerPos;
	mBillboardManager.mBillboards[0].Position = eyePos;

	float twist = UpdateCameraAnimationTrack(TwistTrack, mCurrentFrame);

	if((mPlaying && (mCurrentFrame != mEndFrame)) || mUpdateCameraPosition){
		if(mViewCamera){
			mCamera.mFovy = UpdateCameraAnimationTrack(FovYTrack, mCurrentFrame);

			mCamera.SetCenter(centerPos);
			mCamera.SetEye(eyePos);
		}

		if(!mUpdateCameraPosition) mCurrentFrame++;
		mUpdateCameraPosition = false;
	} else if(mPlaying && mCurrentFrame == mEndFrame){
		mPlaying = false;
    }
}

void UCammieContext::RenderMainWindow(float deltaTime) {


}

static bool isGalaxy2 = false;

void GalaxySelectPane(const char *vFilter, IGFDUserDatas vUserDatas, bool *vCantContinue) // if vCantContinue is false, the user cant validate the dialog
{
    ImGui::Checkbox("Is Galaxy 2", &isGalaxy2);
}


void UCammieContext::RenderMenuBar() {
	mOptionsOpen = false;
	ImGui::BeginMainMenuBar();

	if (ImGui::BeginMenu("File")) {
		if (ImGui::MenuItem("Open...")) {
			OpenModelCB();
		}
		if (ImGui::MenuItem("Open Galaxy...")) {
			//TODO: Open zone..
			bIsGalaxyDialogOpen = true;
		}
		if (ImGui::MenuItem("Save...")) {
			SaveModelCB();
		}

		ImGui::Separator();
		ImGui::MenuItem("Close");

		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Edit")) {
		if(ImGui::MenuItem("Settings")){
			mOptionsOpen = true;
		}
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("About")) {
		ImGui::EndMenu();
	}

	ImGui::EndMainMenuBar();

	if (bIsFileDialogOpen) {
		ImGuiFileDialog::Instance()->OpenDialog("OpenFileDialog", "Choose Camera File", "Camera Animation (*.canm){.canm}", ".");
	}
	if (bIsGalaxyDialogOpen){
		//TODO: make this ensure the selected root is a galaxy/2 root!
		ImGuiFileDialog::Instance()->OpenDialog("OpenGalaxyDialog", "Choose Stage Directory", nullptr, Options.mRootPath == "" ? "." : Options.mRootPath / "DATA" / "files" / "StageData" / ".", "", std::bind(&GalaxySelectPane, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	}
	if (bIsSaveDialogOpen) {
		ImGuiFileDialog::Instance()->OpenDialog("SaveFileDialog", "Save Camera File", "Camera Animation (*.canm){.canm}", ".", 1, nullptr, ImGuiFileDialogFlags_ConfirmOverwrite);
	}

	if (ImGuiFileDialog::Instance()->Display("OpenFileDialog")) {
		if (ImGuiFileDialog::Instance()->IsOk()) {
			std::string FilePath = ImGuiFileDialog::Instance()->GetFilePathName();

			try {
				LoadFromPath(FilePath);
			}
			catch (std::exception e) {
				std::cout << "Failed to load camera file " << FilePath << "! Exception: " << e.what() << "\n";
			}

			bIsFileDialogOpen = false;
		}

		ImGuiFileDialog::Instance()->Close();
	}

	if (ImGuiFileDialog::Instance()->Display("OpenGalaxyDialog")) {
		if (ImGuiFileDialog::Instance()->IsOk()) {
			std::string FilePath = ImGuiFileDialog::Instance()->GetFilePathName();

			try {
				mGalaxyRenderer.LoadGalaxy(FilePath, isGalaxy2);
			}
			catch (std::exception e) {
				std::cout << "Failed to load galaxy " << FilePath << "! Exception: " << e.what() << "\n";
			}

			bIsGalaxyDialogOpen = false;
		}

		ImGuiFileDialog::Instance()->Close();
	}
	

	if (ImGuiFileDialog::Instance()->Display("SaveFileDialog")) {
		if (ImGuiFileDialog::Instance()->IsOk()) {
			std::string FilePath = ImGuiFileDialog::Instance()->GetFilePathName();

			try {
				//TODO: Write camera file
				SaveAnimation(FilePath);
			}
			catch (std::exception e) {
				std::cout << "Failed to save model to " << FilePath << "! Exception: " << e.what() << "\n";
			}

			bIsSaveDialogOpen = false;
		}

		ImGuiFileDialog::Instance()->Close();
	}

	if(mOptionsOpen){
		ImGui::OpenPopup("Options");
	}

	Options.RenderOptionMenu();
}

void UCammieContext::OpenModelCB() {
	bIsFileDialogOpen = true;
}

void UCammieContext::SaveModelCB() {
	bIsSaveDialogOpen = true;
}

void UCammieContext::SetLights() {

	J3DLight lights[8];

	lights[0].Position = glm::vec4(00000, 00000, 00000, 1);
	lights[0].AngleAtten = glm::vec4(1.0, 0.0, 0.0, 1);
	lights[0].DistAtten = glm::vec4(1.0, 0.0, 0.0, 1);

	lights[1].Position = glm::vec4(1.0, 0.0, 0.0, 1);
	lights[1].AngleAtten = glm::vec4(1.0, 0.0, 0.0, 1);
	lights[1].DistAtten = glm::vec4(1.0, 0., 0.0, 1);

	lights[2].Position = glm::vec4(0.0, 0.0, 0.0, 1);
	lights[2].AngleAtten = glm::vec4(0, 0, 1, 1);
	lights[2].DistAtten = glm::vec4(25.0, 0.0, -24.0, 1);
	lights[2].Direction = glm::vec4(1.0, -0.868448, 0.239316, 1);

	for (int i = 0; i < 8; i++)
		lights[i].Color = glm::vec4(1, 1, 1, 1);

	J3DUniformBufferObject::SetLights(lights);
}

void UCammieContext::SaveAnimation(std::filesystem::path savePath){
	bStream::CFileStream camn(savePath.string(), bStream::Endianess::Big, bStream::OpenMode::Out);

	// Write header

	camn.writeString("ANDO");
	camn.writeString(mFrameType);

	camn.writeUInt32(mCamUnkData[0]);
	camn.writeUInt32(mCamUnkData[1]);
	camn.writeUInt32(mCamUnkData[2]);
	camn.writeUInt32(mCamUnkData[3]);

	camn.writeInt32(mEndFrame);
	camn.writeUInt32(mFrameType == "CANM" ? 64 : 96);
	std::cout << "Should be 0x20!" << std::hex << camn.tell() << std::endl;

	ETrackType type = (mFrameType == "CANM" ? ETrackType::CANM : ETrackType::CKAN);

	std::vector<float> FrameData;

	XPositionTrack.WriteTrack(&camn, FrameData, type);
	YPositionTrack.WriteTrack(&camn, FrameData, type);
	ZPositionTrack.WriteTrack(&camn, FrameData, type);

	XTargetTrack.WriteTrack(&camn, FrameData, type);
	YTargetTrack.WriteTrack(&camn, FrameData, type);
	ZTargetTrack.WriteTrack(&camn, FrameData, type);

	TwistTrack.WriteTrack(&camn, FrameData, type);
	FovYTrack.WriteTrack(&camn, FrameData, type);

	camn.writeUInt32(FrameData.size() + 2);
	for(auto& flt : FrameData){
		camn.writeFloat(flt);
    }

	camn.writeUInt32(0x3DCCCCCD);
	camn.writeUInt32(0x4E6E6B28);
	camn.writeUInt32(0xFFFFFFFF);
}

void UCammieContext::LoadFromPath(std::filesystem::path filePath) {
	//TODO: Make game a setting

	XPositionTrack.mKeys.clear();
	XPositionTrack.mFrames.clear();

	YPositionTrack.mKeys.clear();
	YPositionTrack.mFrames.clear();

	ZPositionTrack.mKeys.clear();
	ZPositionTrack.mFrames.clear();


	XTargetTrack.mKeys.clear();
	XTargetTrack.mFrames.clear();

	YTargetTrack.mKeys.clear();
	YTargetTrack.mFrames.clear();

	ZTargetTrack.mKeys.clear();
	ZTargetTrack.mFrames.clear();


	FovYTrack.mKeys.clear();
	FovYTrack.mFrames.clear();

	TwistTrack.mKeys.clear();
	TwistTrack.mFrames.clear();

	bStream::CFileStream camn(filePath.string(), bStream::Endianess::Big, bStream::OpenMode::In);

	camn.readString(4);

	mFrameType = camn.readString(4);

	mCamUnkData[0] = camn.readUInt32();
	mCamUnkData[1] = camn.readUInt32();
	mCamUnkData[2] = camn.readUInt32();
	mCamUnkData[3] = camn.readUInt32();

	camn.seek(0x18);
	mEndFrame = camn.readInt32();
	mTrackSize = camn.readUInt32();

	XPositionTrack.LoadTrack(&camn, 0x20 + mTrackSize, (mFrameType == "CANM" ? ETrackType::CANM : ETrackType::CKAN));
	YPositionTrack.LoadTrack(&camn, 0x20 + mTrackSize, (mFrameType == "CANM" ? ETrackType::CANM : ETrackType::CKAN));
	ZPositionTrack.LoadTrack(&camn, 0x20 + mTrackSize, (mFrameType == "CANM" ? ETrackType::CANM : ETrackType::CKAN));
	
	XTargetTrack.LoadTrack(&camn, 0x20 + mTrackSize, (mFrameType == "CANM" ? ETrackType::CANM : ETrackType::CKAN));
	YTargetTrack.LoadTrack(&camn, 0x20 + mTrackSize, (mFrameType == "CANM" ? ETrackType::CANM : ETrackType::CKAN));
	ZTargetTrack.LoadTrack(&camn, 0x20 + mTrackSize, (mFrameType == "CANM" ? ETrackType::CANM : ETrackType::CKAN));

	TwistTrack.LoadTrack(&camn, 0x20 + mTrackSize, (mFrameType == "CANM" ? ETrackType::CANM : ETrackType::CKAN));
	FovYTrack.LoadTrack(&camn, 0x20 + mTrackSize, (mFrameType == "CANM" ? ETrackType::CANM : ETrackType::CKAN));
    
}