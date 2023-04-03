#include "UCammieContext.hpp"

#include "io/KeyframeIO.hpp"
#include "imgui_neo_internal.h"
#include "imgui_neo_sequencer.h"

#include "util/UUIUtil.hpp"

#include <J3D/J3DModelLoader.hpp>
#include <J3D/J3DModelData.hpp>
#include <J3D/J3DUniformBufferObject.hpp>
#include <J3D/J3DLight.hpp>
#include <J3D/J3DModelInstance.hpp>

#include <ImGuiFileDialog.h>
#include <glad/glad.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <bstream.h>
#include <optional>
#include "fmt/core.h"


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
	mGrid.Init();

	ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontFromFileTTF("res/NotoSansJP-Regular.otf", 16.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	mCurrentFrame = mStartFrame = 0;
	mEndFrame = 10;
}

bool UCammieContext::Update(float deltaTime) {
	mCamera.Update(deltaTime);

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
		mDockNodeBottomRightID = ImGui::DockBuilderSplitNode(mDockNodeBottomID, ImGuiDir_Right, 0.2f, nullptr, &mDockNodeBottomID);


		ImGui::DockBuilderDockWindow("mainWindow", mDockNodeBottomID);
		ImGui::DockBuilderDockWindow("detailWindow", mDockNodeBottomRightID);

		ImGui::DockBuilderFinish(mMainDockSpaceID);
		bIsDockingSetUp = true;
	}


	ImGuiWindowClass mainWindowOverride;
	mainWindowOverride.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoTabBar;
	ImGui::SetNextWindowClass(&mainWindowOverride);
	
	ImGui::Begin("mainWindow", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);
		ImGui::Text("Camera Animation");
		ImGui::SameLine();
		if(ImGui::Button("Play")){ mPlaying = true; mCurrentFrame = 0; }
		if(mPlaying){ ImGui::SameLine(); if(ImGui::Button("Stop")) mPlaying = false; }
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
		ImGui::Text("Keyframe Settings");
		ImGui::Separator();
		if(selectedKeyframe != -1 && selectedTrack != nullptr && selectedTrack->mFrames.count(selectedKeyframe) != 0){
			ImGui::InputFloat("Value", &selectedTrack->mFrames.at(selectedKeyframe).value);
		}
	ImGui::End();

	glm::mat4 projection, view;
	projection = mCamera.GetProjectionMatrix();
	view = mCamera.GetViewMatrix();

	if(!mSetLights) SetLights();
	J3DUniformBufferObject::SetProjAndViewMatrices(&projection, &view);
	
	//Render Models here

	mGrid.Render(mCamera.GetPosition(), mCamera.GetProjectionMatrix(), mCamera.GetViewMatrix());

	if(mPlaying && mCurrentFrame < mEndFrame){
		CKeyframeCommon nextKeyframePosX, prevKeyframePosX;
		CKeyframeCommon nextKeyframePosY, prevKeyframePosY;
		CKeyframeCommon nextKeyframePosZ, prevKeyframePosZ;

		CKeyframeCommon nextKeyframeTargetX, prevKeyframeTargetX;
		CKeyframeCommon nextKeyframeTargetY, prevKeyframeTargetY;
		CKeyframeCommon nextKeyframeTargetZ, prevKeyframeTargetZ;

		CKeyframeCommon nextKeyframeTwist, prevKeyframeTwist;
		CKeyframeCommon nextKeyframeFov, prevKeyframeFov;

		for(auto keyframe : XPositionTrack.mKeys){
			if(mCurrentFrame >= keyframe) prevKeyframePosX = XPositionTrack.mFrames[keyframe];
			if(mCurrentFrame < keyframe) {
				nextKeyframePosX = XPositionTrack.mFrames[keyframe];
				break;
			}
		}

		for(auto keyframe : YPositionTrack.mKeys){
			if(mCurrentFrame >= keyframe) prevKeyframePosY = YPositionTrack.mFrames[keyframe];
			if(mCurrentFrame < keyframe) {
				nextKeyframePosY = YPositionTrack.mFrames[keyframe];
				break;
			}
		}

		for(auto keyframe : ZPositionTrack.mKeys){
			if(mCurrentFrame >= keyframe) prevKeyframePosZ = ZPositionTrack.mFrames[keyframe];
			if(mCurrentFrame < keyframe) {
				nextKeyframePosZ = ZPositionTrack.mFrames[keyframe];
				break;
			}
		}

		//Get nearest Target Keyframes

		for(auto keyframe : XTargetTrack.mKeys){
			if(mCurrentFrame >= keyframe) prevKeyframeTargetX = XTargetTrack.mFrames[keyframe];
			if(mCurrentFrame < keyframe) {
				nextKeyframeTargetX = XTargetTrack.mFrames[keyframe];
				break;
			}
		}

		for(auto keyframe : YTargetTrack.mKeys){
			if(mCurrentFrame >= keyframe) prevKeyframeTargetY = YTargetTrack.mFrames[keyframe];
			if(mCurrentFrame < keyframe) {
				nextKeyframeTargetY = YTargetTrack.mFrames[keyframe];
				break;
			}
		}

		for(auto keyframe : ZTargetTrack.mKeys){
			if(mCurrentFrame >= keyframe) prevKeyframeTargetZ = ZTargetTrack.mFrames[keyframe];
			if(mCurrentFrame < keyframe) {
				nextKeyframeTargetZ = ZTargetTrack.mFrames[keyframe];
				break;
			}
		}

		// Get nearest FOV and Twist Keyframes

		for(auto keyframe : FovYTrack.mKeys){
			if(mCurrentFrame >= keyframe) prevKeyframeFov = FovYTrack.mFrames[keyframe];
			if(mCurrentFrame < keyframe) {
				nextKeyframeFov = FovYTrack.mFrames[keyframe];
				break;
			}
		}

		for(auto keyframe : TwistTrack.mKeys){
			if(mCurrentFrame >= keyframe) prevKeyframeTwist = TwistTrack.mFrames[keyframe];
			if(mCurrentFrame < keyframe) {
				nextKeyframeTwist = TwistTrack.mFrames[keyframe];
				break;
			}
		}

		glm::vec3 eyePos = mCamera.GetEye();
		glm::vec3 centerPos = mCamera.GetCenter();

		eyePos.x = glm::mix(prevKeyframePosX.value, nextKeyframePosX.value, (mCurrentFrame - prevKeyframePosX.frame) / (nextKeyframePosX.frame - prevKeyframePosX.frame));
		eyePos.y = glm::mix(prevKeyframePosY.value, nextKeyframePosY.value, (mCurrentFrame - prevKeyframePosY.frame) / (nextKeyframePosY.frame - prevKeyframePosY.frame));
		eyePos.z = glm::mix(prevKeyframePosZ.value, nextKeyframePosZ.value, (mCurrentFrame - prevKeyframePosZ.frame) / (nextKeyframePosZ.frame - prevKeyframePosZ.frame));

		centerPos.x = glm::mix(prevKeyframeTargetX.value, nextKeyframeTargetX.value, (mCurrentFrame - prevKeyframeTargetX.frame) / (nextKeyframeTargetX.frame - prevKeyframeTargetX.frame));
		centerPos.y = glm::mix(prevKeyframeTargetY.value, nextKeyframeTargetY.value, (mCurrentFrame - prevKeyframeTargetY.frame) / (nextKeyframeTargetY.frame - prevKeyframeTargetY.frame));
		centerPos.z = glm::mix(prevKeyframeTargetZ.value, nextKeyframeTargetZ.value, (mCurrentFrame - prevKeyframeTargetZ.frame) / (nextKeyframeTargetZ.frame - prevKeyframeTargetZ.frame));

		float twist = glm::mix(prevKeyframeTwist.value, nextKeyframeTwist.value, (mCurrentFrame - prevKeyframeTwist.frame) / (nextKeyframeTwist.frame - prevKeyframeTwist.frame));
		mCamera.mFovy = glm::mix(prevKeyframeFov.value, nextKeyframeFov.value, (mCurrentFrame - prevKeyframeFov.frame) / (nextKeyframeFov.frame - prevKeyframeFov.frame));


		mCamera.SetCenter(centerPos);
		mCamera.SetEye(eyePos);

		mCurrentFrame++;
	} else if(mPlaying && mCurrentFrame == mEndFrame){
		mPlaying = false;
    }
}

void UCammieContext::RenderMainWindow(float deltaTime) {


}

void UCammieContext::RenderMenuBar() {
	ImGui::BeginMainMenuBar();

	if (ImGui::BeginMenu("File")) {
		if (ImGui::MenuItem("Open...")) {
			OpenModelCB();
		}
		if (ImGui::MenuItem("Open Zone...")) {
			//TODO: Open zone..
		}
		if (ImGui::MenuItem("Save...")) {
			SaveModelCB();
		}

		ImGui::Separator();
		ImGui::MenuItem("Close");

		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Edit")) {
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("About")) {
		ImGui::EndMenu();
	}

	ImGui::EndMainMenuBar();

	if (bIsFileDialogOpen) {
		ImGuiFileDialog::Instance()->OpenDialog("OpenFileDialog", "Choose Camera File", "Camera Animation (*.canm){.canm}", ".");
	}
	if (bIsSaveDialogOpen) {
		ImGuiFileDialog::Instance()->OpenDialog("SaveFileDialog", "Choose File", "J3D Models (*.bmd *.bdl){.bmd,.bdl}", ".", 1, nullptr, ImGuiFileDialogFlags_ConfirmOverwrite);
	}

	if (ImGuiFileDialog::Instance()->Display("OpenFileDialog")) {
		if (ImGuiFileDialog::Instance()->IsOk()) {
			std::string FilePath = ImGuiFileDialog::Instance()->GetFilePathName();

			try {
				LoadFromPath(FilePath);
			}
			catch (std::exception e) {
				std::cout << "Failed to load galaxy " << FilePath << "! Exception: " << e.what() << "\n";
			}

			bIsFileDialogOpen = false;
		}

		ImGuiFileDialog::Instance()->Close();
	}

	if (ImGuiFileDialog::Instance()->Display("SaveFileDialog")) {
		if (ImGuiFileDialog::Instance()->IsOk()) {
			std::string FilePath = ImGuiFileDialog::Instance()->GetFilePathName();

			try {
			}
			catch (std::exception e) {
				std::cout << "Failed to save model to " << FilePath << "! Exception: " << e.what() << "\n";
			}

			bIsSaveDialogOpen = false;
		}

		ImGuiFileDialog::Instance()->Close();
	}
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

	std::string type = camn.readString(4);

	camn.seek(0x18);
	mEndFrame = camn.readInt32();
	uint32_t track_size = camn.readUInt32();


	XPositionTrack.LoadTrack(&camn, 0x20 + track_size, (type == "CANM" ? ETrackType::CANM : ETrackType::CKAN));
	YPositionTrack.LoadTrack(&camn, 0x20 + track_size, (type == "CANM" ? ETrackType::CANM : ETrackType::CKAN));
	ZPositionTrack.LoadTrack(&camn, 0x20 + track_size, (type == "CANM" ? ETrackType::CANM : ETrackType::CKAN));
	
	XTargetTrack.LoadTrack(&camn, 0x20 + track_size, (type == "CANM" ? ETrackType::CANM : ETrackType::CKAN));
	YTargetTrack.LoadTrack(&camn, 0x20 + track_size, (type == "CANM" ? ETrackType::CANM : ETrackType::CKAN));
	ZTargetTrack.LoadTrack(&camn, 0x20 + track_size, (type == "CANM" ? ETrackType::CANM : ETrackType::CKAN));

	TwistTrack.LoadTrack(&camn, 0x20 + track_size, (type == "CANM" ? ETrackType::CANM : ETrackType::CKAN));
	FovYTrack.LoadTrack(&camn, 0x20 + track_size, (type == "CANM" ? ETrackType::CANM : ETrackType::CKAN));
    
}