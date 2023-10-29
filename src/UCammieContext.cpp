#include "UCammieContext.hpp"

#include "UPointSpriteManager.hpp"
#include "io/KeyframeIO.hpp"
#include "imgui_neo_internal.h"
#include "imgui_neo_sequencer.h"
#include "ImGuizmo.h"

#include "util/UUIUtil.hpp"

#include <J3D/J3DModelLoader.hpp>
#include <J3D/J3DModelData.hpp>
#include <J3D/J3DUniformBufferObject.hpp>
#include <J3D/J3DLight.hpp>
#include <J3D/J3DModelInstance.hpp>

#include <filesystem>
#include <glad/glad.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <bstream.h>
#include <optional>
#include <sys/types.h>
#include "fmt/core.h"
#include "ResUtil.hpp"

static bool wasDraggingFrame = false;
static bool dragEnded = false;

const char* keyframe_types[4] = { "Static", "Normal", "Symmetric Slope", "Non-Symmetric Slope"};

bool RenderTimelineTrack(std::string label, CTrackCommon* track, int* keyframeSelection){
	bool selected = false;
	ImGui::BeginNeoTimelineEx(label.data());
		for(auto&& key : track->mKeys){
			ImGui::NeoKeyframe(&key);

			if(ImGui::IsNeoKeyframeSelected()){
				*keyframeSelection = key;
				selected = true;

				if(ImGui::NeoIsDraggingSelection()){
					wasDraggingFrame = true;
				} else if(!ImGui::NeoIsDraggingSelection() && wasDraggingFrame){
					wasDraggingFrame = false;
					dragEnded = true;
				}
			}
		}

		if(ImGui::IsKeyPressed(ImGuiKey_Delete)){
			*keyframeSelection = -1;
			selected = false;

			uint32_t selected_count = ImGui::GetNeoKeyframeSelectionSize();
			ImGui::FrameIndexType* toRemove = new ImGui::FrameIndexType[selected_count];

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

float hermiteInterpolation(float point0, float tangent0, float point1, float tangent1, float t) {
    float t2 = t * t;
    float t3 = t2 * t;
    float h1 = 2 * t3 - 3 * t2 + 1;
    float h2 = -2 * t3 + 3 * t2;
    float h3 = t3 - 2 * t2 + t;
    float h4 = t3 - t2;

    return h1 * point0 + h2 * point1 + h3 * tangent0 + h4 * tangent1;
}

inline float UpdateCameraAnimationTrack(CTrackCommon track, int currentFrame, bool hermiteInterp){
	if(track.mKeys.size() == 0) return 0.0f;
	if(track.mKeys.size() == 1) {
		if(currentFrame > track.mKeys[0]) return track.mFrames[track.mKeys[0]].value;
		if(hermiteInterp){
			return hermiteInterpolation(0.0f, 1.0f, track.mFrames[track.mKeys[0]].value, track.mFrames[track.mKeys[0]].inslope, currentFrame / track.mFrames[track.mKeys[0]].frame);
		} else {
			return glm::mix(0.0f, track.mFrames[track.mKeys[0]].value, currentFrame / track.mFrames[track.mKeys[0]].frame);			
		}
	}
	//I don't know how this works when there is only one keyframe. I don't want to know how this works with only one keyframe. But it works when there is only one keyframe.
	CKeyframeCommon nextKeyframe, prevKeyframe;
	bool hasNext = false;
	for(auto keyframe : track.mKeys){
		if(currentFrame >= keyframe) prevKeyframe = track.mFrames[keyframe];
		if(currentFrame < keyframe) {
			nextKeyframe = track.mFrames[keyframe];
			hasNext = true;
			break;
		}
	}

	if(!hasNext) return prevKeyframe.value;

	if(hermiteInterp){
		return hermiteInterpolation(prevKeyframe.value, (track.mElementCount == 4 ? prevKeyframe.outslope : (track.mElementCount == 3 ? prevKeyframe.inslope : 1.0f)), nextKeyframe.value, nextKeyframe.inslope, (currentFrame - prevKeyframe.frame) / (nextKeyframe.frame - prevKeyframe.frame));
	} else {
		return glm::mix(prevKeyframe.value, nextKeyframe.value, (currentFrame - prevKeyframe.frame) / (nextKeyframe.frame - prevKeyframe.frame));
	}
}

inline void AddUpdateKeyframe(float value, float delta, uint32_t currentFrame, CTrackCommon* track){
	if(delta != 0.0f){
		if(track->mFrames.contains(currentFrame)){
			track->mFrames.at(currentFrame).value = value + delta;
		} else {
			track->AddKeyframe(currentFrame, value + delta);
		}
	}
}

void UCammieContext::ProcessEndDrag(){
	std::vector<CTrackCommon*> tracks = {&XPositionTrack, &YPositionTrack, &ZPositionTrack, &XTargetTrack, &YTargetTrack, &ZTargetTrack, &FovYTrack, &TwistTrack, &ZNearTrack, &ZFarTrack};
	std::cout << "Stopped Dragging Keyframe, Updating Keyframe Positions..." << std::endl;
	
	for(auto track : tracks){
		std::sort(track->mKeys.begin(), track->mKeys.end());
		for(auto key : track->mKeys){
			if(track->mFrames.find(key) == track->mFrames.end()){ // we couldn't kind the framedata for this keyframe
				std::cout << "Couldn't find framedata for keyframe " << key << std::endl;
				for(auto [unusedKey, frame] : track->mFrames){
					// find framedata that no longer has corresponding keyframe
					if(std::find(track->mKeys.begin(), track->mKeys.end(), unusedKey) == track->mKeys.end()){
						std::cout << "Couldn't find keyframe for framedata w/ keyframe " << unusedKey << std::endl;
						track->mFrames.insert({key, frame});
						track->mFrames.erase(unusedKey);
						break;
					}
				}
			}
		}

		//keyframes must be sorted!
	}

	dragEnded = false;
}

glm::vec3 UCammieContext::ManipulationGizmo(glm::vec3 position){
	glm::mat4 mtx = glm::translate(glm::identity<glm::mat4>(), position);
	glm::mat4 delta;

    ImGuizmo::Manipulate(&mCamera.GetViewMatrix()[0][0], &mCamera.GetProjectionMatrix()[0][0], ImGuizmo::TRANSLATE, ImGuizmo::WORLD, &mtx[0][0], &delta[0][0], NULL);
	return glm::vec3(delta[3]);
}

UCammieContext::UCammieContext(){
	BinModel::InitShaders();
	Options.LoadOptions();
	mGrid.Init();
	mBillboardManager.Init(128, 3);
	mBillboardManager.SetBillboardTexture(std::filesystem::current_path() / "res/camera.png", 0);
	mBillboardManager.SetBillboardTexture(std::filesystem::current_path() / "res/target.png", 1);
	mBillboardManager.SetBillboardTexture(std::filesystem::current_path() / "res/character.png", 2);

	mBillboardManager.mBillboards.push_back(CPointSprite());
	mBillboardManager.mBillboards.push_back(CPointSprite());
	mBillboardManager.mBillboards.push_back(CPointSprite());

	// 51200
	mBillboardManager.mBillboards[0].Texture = 0;
	mBillboardManager.mBillboards[0].SpriteSize = 0;
	mBillboardManager.mBillboards[1].Texture = 1;
	mBillboardManager.mBillboards[1].SpriteSize = 0;
	mBillboardManager.mBillboards[2].Texture = 2;
	mBillboardManager.mBillboards[2].SpriteSize = 0;

	GCResourceManager.Init();

	ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontFromFileTTF((std::filesystem::current_path() / "res/NotoSansJP-Regular.otf").string().c_str(), 16.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	mCurrentFrame = mStartFrame = 0;
	mEndFrame = 10;
}

UCammieContext::~UCammieContext(){
	BinModel::DestroyShaders();
}

bool UCammieContext::Update(float deltaTime) {
	
	if(!(mPlaying && mViewCamera)){
		mCamera.mNearPlane = 0.1f;
		mCamera.mFarPlane = 100000.0f;
		mCamera.Update(deltaTime);
	} else {
		mCamera.UpdateSimple();
	}

	if(ImGui::IsKeyPressed(ImGuiKey_Space)){
		if(ImGui::IsKeyDown(ImGuiKey_LeftShift)){
			if(!TwistTrack.mFrames.contains(mCurrentFrame)){
				TwistTrack.AddKeyframe(mCurrentFrame, 0);
			}
        } else {
			if(!FovYTrack.mFrames.contains(mCurrentFrame)){
				FovYTrack.AddKeyframe(mCurrentFrame, mCamera.mFovy);
			}
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
		ImGui::Text(fmt::format("{2} Animation [{0}/{1}]", mCurrentFrame, mEndFrame, mOpenTrackType == ETrackType::CMN ? "Camera" : "Path").data());
		ImGui::SameLine();
		if(mOpenTrackType == ETrackType::CMN){
			ImGui::Checkbox("Camera Sight", &mViewCamera);
			ImGui::SameLine();
		}
		ImGui::Checkbox("Use Hermite Interp", &mUseHermite);
		ImGui::SameLine();

		if(ImGui::Button("Play")){ mPlaying = true; mCurrentFrame = 0; if(mOpenTrackType == ETrackType::CMN) mCamera.ResetView(); }
		if(mPlaying){ ImGui::SameLine(); if(ImGui::Button("Stop")) mPlaying = false; }
		if(!mShowZones){ ImGui::SameLine(); if(ImGui::Button("Rooms")) mShowZones = true; }
		ImGui::SameLine(ImGui::GetWindowWidth() - 50);
		ImGui::InputInt("End Frame", &mEndFrame);
		ImGui::Separator();

		//This segment of code is insanely messy
		ImGui::BeginNeoSequencer("Sequencer", &mCurrentFrame, &mStartFrame, &mEndFrame, {0, 0}, mPlaying ? 0 : (ImGuiNeoSequencerFlags_EnableSelection | (Options.mDragEnabled ? ImGuiNeoSequencerFlags_Selection_EnableDragging : 0) | ImGuiNeoSequencerFlags_Selection_EnableDeletion));
			if(mOpenTrackType == ETrackType::CMN){
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

				if(RenderTimelineTrack("Z Near", &ZNearTrack, &selectedKeyframe)) selectedTrack = &ZNearTrack;
				if(RenderTimelineTrack("Z Far", &ZFarTrack, &selectedKeyframe)) selectedTrack = &ZFarTrack;

				if(Options.mDragEnabled && dragEnded) ProcessEndDrag();

			} else if (mOpenTrackType == ETrackType::PTH){
				if(ImGui::BeginNeoGroup("Position", &mPositionOpen)){
					if(RenderTimelineTrack("X", &XPositionTrack, &selectedKeyframe)) selectedTrack = &XPositionTrack;
					if(RenderTimelineTrack("Y", &YPositionTrack, &selectedKeyframe)) selectedTrack = &YPositionTrack;
					if(RenderTimelineTrack("Z", &ZPositionTrack, &selectedKeyframe)) selectedTrack = &ZPositionTrack;
					ImGui::EndNeoGroup();
				}
				if(ImGui::BeginNeoGroup("Unknown", &mTargetOpen)){
					if(RenderTimelineTrack("X", &XTargetTrack, &selectedKeyframe)) selectedTrack = &XTargetTrack;
					if(RenderTimelineTrack("Y", &YTargetTrack, &selectedKeyframe)) selectedTrack = &YTargetTrack;
					if(RenderTimelineTrack("Z", &ZTargetTrack, &selectedKeyframe)) selectedTrack = &ZTargetTrack;
					ImGui::EndNeoGroup();
				}
			}
		ImGui::EndNeoSequencer();

	ImGui::End();

	ImGui::SetNextWindowClass(&mainWindowOverride);

	ImGui::Begin("detailWindow", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);
		bool keyframeIsSelected = (selectedKeyframe != -1 && selectedTrack != nullptr && selectedTrack->mFrames.count(selectedKeyframe) != 0);
		bool showTrackSettings = ImGui::TreeNode("Track Settings");
		ImGui::Separator();
		if(showTrackSettings){
			if(keyframeIsSelected){
				int elementCount = selectedTrack->mElementCount;
				for (size_t i = 1; i <= 4; i++){
					ImGui::RadioButton(fmt::format("{0}", keyframe_types[i-1]).c_str(), &elementCount, i);
				}
				selectedTrack->mElementCount = elementCount;
			}
			ImGui::TreePop();
		}

		bool showKeyframeSettings = ImGui::TreeNode("Selected Keyframe");
		ImGui::Separator();
		if(showKeyframeSettings){
			if(keyframeIsSelected){
				ImGui::InputFloat("Value", &selectedTrack->mFrames.at(selectedKeyframe).value);

				if(selectedTrack->mElementCount == 3){
					ImGui::InputFloat("Slope", &selectedTrack->mFrames.at(selectedKeyframe).inslope);
				} else if(selectedTrack->mElementCount == 4) {
					ImGui::InputFloat("In Slope", &selectedTrack->mFrames.at(selectedKeyframe).inslope);
					ImGui::InputFloat("Out Slope", &selectedTrack->mFrames.at(selectedKeyframe).outslope);
				}
				if(mOpenTrackType == ETrackType::CMN && ImGui::Button("Set Camera to Keyframe")){
					mUpdateCameraPosition = true;
					mCurrentFrame = selectedKeyframe;
				}
			}
			ImGui::TreePop();
		}
	ImGui::End();

	if(mShowZones){
		ImGui::SetNextWindowClass(&mainWindowOverride);

		ImGui::Begin("zoneWindow", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);
			ImGui::Text("Rooms");
			ImGui::SameLine(ImGui::GetWindowWidth()-50);
			if(ImGui::Button("Hide")) mShowZones = false;
			ImGui::Separator();
			mMapRenderer.RenderUI();
		ImGui::End();
	}

	glm::mat4 projection, view;
	projection = mCamera.GetProjectionMatrix();
	view = mCamera.GetViewMatrix();

	//J3DUniformBufferObject::SetProjAndViewMatrices(&projection, &view);
	
	//Render Models here

	mGrid.Render(mCamera.GetPosition(), projection, view);
	
	//mGalaxyRenderer.RenderGalaxy(deltaTime);
	mMapRenderer.RenderMap(deltaTime, &projection, &view);
	
	mBillboardManager.Draw(&mCamera);

	glm::vec3 eyePos;// = mCamera.GetEye();
	glm::vec3 centerPos;// = mCamera.GetCenter();

	eyePos.x = UpdateCameraAnimationTrack(XPositionTrack, mCurrentFrame, mUseHermite);
	eyePos.y = UpdateCameraAnimationTrack(YPositionTrack, mCurrentFrame, mUseHermite);
	eyePos.z = UpdateCameraAnimationTrack(ZPositionTrack, mCurrentFrame, mUseHermite);

	centerPos.x = UpdateCameraAnimationTrack(XTargetTrack, mCurrentFrame, mUseHermite);
	centerPos.y = UpdateCameraAnimationTrack(YTargetTrack, mCurrentFrame, mUseHermite);
	centerPos.z = UpdateCameraAnimationTrack(ZTargetTrack, mCurrentFrame, mUseHermite);

	//TODO Add way to do this for fov/twist

	mBillboardManager.mBillboards[0].Position = eyePos;
	mBillboardManager.mBillboards[1].Position = centerPos;
	mBillboardManager.mBillboards[2].Position = eyePos;

	if((mPlaying && (mCurrentFrame != mEndFrame)) || (mOpenTrackType == ETrackType::CMN && mUpdateCameraPosition)){
		if(mOpenTrackType == ETrackType::CMN && mViewCamera){
			mCamera.mFovy = glm::radians(UpdateCameraAnimationTrack(FovYTrack, mCurrentFrame, mUseHermite));

			//mCamera.mTwist = UpdateCameraAnimationTrack(TwistTrack, mCurrentFrame);

			mCamera.mFarPlane = UpdateCameraAnimationTrack(ZFarTrack, mCurrentFrame, mUseHermite);
			mCamera.mNearPlane = UpdateCameraAnimationTrack(ZNearTrack, mCurrentFrame, mUseHermite);

			mCamera.SetCenter(centerPos);
			mCamera.SetEye(eyePos);
		}

		if(!(mOpenTrackType == ETrackType::CMN && mUpdateCameraPosition)) mCurrentFrame++;
		mUpdateCameraPosition = false;
	} else if(mPlaying && mCurrentFrame == mEndFrame){
		mPlaying = false;
    } else if(!mPlaying){
		ImGuizmo::BeginFrame();
		ImGuiIO& io = ImGui::GetIO();
		ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

		// I don't like that this is in the render function but its whatever
		if(!ImGui::IsKeyDown(ImGuiKey_LeftCtrl)){
			glm::vec3 camDelta = ManipulationGizmo(eyePos);
			AddUpdateKeyframe(eyePos.x, camDelta.x, mCurrentFrame, &XPositionTrack);
			AddUpdateKeyframe(eyePos.y, camDelta.y, mCurrentFrame, &YPositionTrack);
			AddUpdateKeyframe(eyePos.z, camDelta.z, mCurrentFrame, &ZPositionTrack);
		} else {
			glm::vec3 targetDelta = ManipulationGizmo(centerPos);
			AddUpdateKeyframe(centerPos.x, targetDelta.x, mCurrentFrame, &XTargetTrack);
			AddUpdateKeyframe(centerPos.y, targetDelta.y, mCurrentFrame, &YTargetTrack);
			AddUpdateKeyframe(centerPos.z, targetDelta.z, mCurrentFrame, &ZTargetTrack);
		}
	}

}

void UCammieContext::RenderMainWindow(float deltaTime) {


}

static bool isGalaxy2 = false;


void UCammieContext::RenderMenuBar() {
	mOptionsOpen = false;
	ImGui::BeginMainMenuBar();

	if (ImGui::BeginMenu("File")) {
		if (ImGui::MenuItem("Open CMN...")) {
			OpenCMN();
		}
		if (ImGui::MenuItem("Save CMN...")) {
			SaveCMN();
		}
		if (ImGui::MenuItem("Open PTH...")) {
			OpenPTH();
		}
		if (ImGui::MenuItem("Save PTH...")) {
			SavePTH();
		}
		if (ImGui::MenuItem("Open Map...")) {
			//TODO: Open zone..
			bIsGalaxyDialogOpen = true;
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

	if (bIsFileDialogCMNOpen) {
		ImGuiFileDialog::Instance()->OpenDialog("OpenFileDialogCMN", "Choose Camera File", "Camera Animation (*.cmn){.cmn}", ".");
	}
	if (bIsSaveDialogCMNOpen) {
		ImGuiFileDialog::Instance()->OpenDialog("SaveFileDialogCMN", "Save Camera File", "Camera Animation (*.cmn){.cmn}", ".", 1, nullptr, ImGuiFileDialogFlags_ConfirmOverwrite);
	}
	if (bIsFileDialogPTHOpen) {
		ImGuiFileDialog::Instance()->OpenDialog("OpenFileDialogPTH", "Choose Path File", "Path Animation (*.pth){.pth}", ".");
	}
	if (bIsSaveDialogPTHOpen) {
		ImGuiFileDialog::Instance()->OpenDialog("SaveFileDialogPTH", "Save Path File", "Path Animation (*.pth){.pth}", ".", 1, nullptr, ImGuiFileDialogFlags_ConfirmOverwrite);
	}
	if (bIsGalaxyDialogOpen){
		//TODO: make this ensure the selected root is a galaxy/2 root!
		ImGuiFileDialog::Instance()->OpenDialog("OpenMapDialog", "Open Map Archive", "Compressed RARC Archive (*.szp){.szp}", (Options.mRootPath / "files" / "Map" / ".").string());
	}

	if (ImGuiFileDialog::Instance()->Display("OpenFileDialogCMN")) {
		if (ImGuiFileDialog::Instance()->IsOk()) {
			std::string FilePath = ImGuiFileDialog::Instance()->GetFilePathName();

			try {
				LoadFromPathCMN(FilePath);
			}
			catch (std::exception e) {
				std::cout << "Failed to load camera file " << FilePath << "! Exception: " << e.what() << "\n";
			}

			bIsFileDialogCMNOpen = false;
		}

		ImGuiFileDialog::Instance()->Close();
	}

	if (ImGuiFileDialog::Instance()->Display("SaveFileDialogCMN")) {
		if (ImGuiFileDialog::Instance()->IsOk()) {
			std::string FilePath = ImGuiFileDialog::Instance()->GetFilePathName();

			try {
				//TODO: Write camera file
				SaveAnimationCMN(FilePath);
			}
			catch (std::exception e) {
				std::cout << "Failed to save model to " << FilePath << "! Exception: " << e.what() << "\n";
			}

			bIsSaveDialogCMNOpen = false;
		}

		ImGuiFileDialog::Instance()->Close();
	}

	if (ImGuiFileDialog::Instance()->Display("OpenFileDialogPTH")) {
		if (ImGuiFileDialog::Instance()->IsOk()) {
			std::string FilePath = ImGuiFileDialog::Instance()->GetFilePathName();

			try {
				LoadFromPathPTH(FilePath);
			}
			catch (std::exception e) {
				std::cout << "Failed to load path file " << FilePath << "! Exception: " << e.what() << "\n";
			}

			bIsFileDialogPTHOpen = false;
		}

		ImGuiFileDialog::Instance()->Close();
	}

	if (ImGuiFileDialog::Instance()->Display("SaveFileDialogPTH")) {
		if (ImGuiFileDialog::Instance()->IsOk()) {
			std::string FilePath = ImGuiFileDialog::Instance()->GetFilePathName();

			try {
				SaveAnimationPTH(FilePath);
			}
			catch (std::exception e) {
				std::cout << "Failed to save path to " << FilePath << "! Exception: " << e.what() << "\n";
			}

			bIsSaveDialogPTHOpen = false;
		}

		ImGuiFileDialog::Instance()->Close();
	}

	if (ImGuiFileDialog::Instance()->Display("OpenMapDialog")) {
		if (ImGuiFileDialog::Instance()->IsOk()) {
			std::string FilePath = ImGuiFileDialog::Instance()->GetFilePathName();

			try {
				mMapRenderer.LoadMap(FilePath);
			}
			catch (std::exception e) {
				std::cout << "Failed to load galaxy " << FilePath << "! Exception: " << e.what() << "\n";
			}

			bIsGalaxyDialogOpen = false;
		}

		ImGuiFileDialog::Instance()->Close();
	}

	if(mOptionsOpen){
		ImGui::OpenPopup("Options");
	}

	Options.RenderOptionMenu();
}

void UCammieContext::OpenCMN() {
	bIsFileDialogCMNOpen = true;
	mBillboardManager.mBillboards[0].SpriteSize = 51200;
	mBillboardManager.mBillboards[1].SpriteSize = 51200;
	mBillboardManager.mBillboards[2].SpriteSize = 0;
	mOpenTrackType = ETrackType::CMN;
}

void UCammieContext::SaveCMN() {
	bIsSaveDialogCMNOpen = true;
}

void UCammieContext::OpenPTH() {
	bIsFileDialogPTHOpen = true;
	mBillboardManager.mBillboards[0].SpriteSize = 0;
	mBillboardManager.mBillboards[1].SpriteSize = 0;
	mBillboardManager.mBillboards[2].SpriteSize = 102400;
	mOpenTrackType = ETrackType::PTH;
}

void UCammieContext::SavePTH() {
	bIsSaveDialogPTHOpen = true;
}

void UCammieContext::SetLights() {

}

void UCammieContext::SaveAnimationCMN(std::filesystem::path savePath){
	bStream::CFileStream camn(savePath.string(), bStream::Endianess::Big, bStream::OpenMode::Out);

	// Write header

	camn.writeUInt16(mEndFrame);
	camn.writeUInt16(0); //padding

	ETrackType type = ETrackType::CMN;

	std::vector<float> FrameData;

	ZPositionTrack.WriteTrack(&camn, FrameData, type);
	YPositionTrack.WriteTrack(&camn, FrameData, type);
	XPositionTrack.WriteTrack(&camn, FrameData, type);

	ZTargetTrack.WriteTrack(&camn, FrameData, type);
	YTargetTrack.WriteTrack(&camn, FrameData, type);
	XTargetTrack.WriteTrack(&camn, FrameData, type);

	TwistTrack.WriteTrack(&camn, FrameData, type);
	FovYTrack.WriteTrack(&camn, FrameData, type);

	ZNearTrack.WriteTrack(&camn, FrameData, type);
	ZFarTrack.WriteTrack(&camn, FrameData, type);

	camn.writeFloat(UnknownValue);
	
	for(auto& flt : FrameData){
		camn.writeFloat(flt);
    }

}

void UCammieContext::LoadFromPathCMN(std::filesystem::path filePath) {
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

	ZNearTrack.mKeys.clear();
	ZNearTrack.mFrames.clear();

	ZFarTrack.mKeys.clear();
	ZFarTrack.mFrames.clear();

	bStream::CFileStream camn(filePath.string(), bStream::Endianess::Big, bStream::OpenMode::In);

	mEndFrame = camn.readUInt16();
	camn.skip(2);

	ZPositionTrack.LoadTrack(&camn, 68, ETrackType::CMN);
	YPositionTrack.LoadTrack(&camn, 68, ETrackType::CMN);
	XPositionTrack.LoadTrack(&camn, 68, ETrackType::CMN);
	
	ZTargetTrack.LoadTrack(&camn, 68, ETrackType::CMN);
	YTargetTrack.LoadTrack(&camn, 68, ETrackType::CMN);
	XTargetTrack.LoadTrack(&camn, 68, ETrackType::CMN);


	TwistTrack.LoadTrack(&camn, 68, ETrackType::CMN);
	FovYTrack.LoadTrack(&camn, 68, ETrackType::CMN);

	ZNearTrack.LoadTrack(&camn, 68, ETrackType::CMN);
	ZFarTrack.LoadTrack(&camn, 68, ETrackType::CMN);
    
	camn.seek(0x40);
	UnknownValue = camn.readFloat();

}

void UCammieContext::SaveAnimationPTH(std::filesystem::path savePath){
	bStream::CFileStream camn(savePath.string(), bStream::Endianess::Big, bStream::OpenMode::Out);

	// Write header

	camn.writeUInt16(mEndFrame);
	camn.writeUInt16(0); //padding

	ETrackType type = ETrackType::PTH;

	std::vector<float> FrameData;
	std::vector<float> UnknownData;

	ZPositionTrack.WriteTrack(&camn, FrameData, type);
	YPositionTrack.WriteTrack(&camn, FrameData, type);
	XPositionTrack.WriteTrack(&camn, FrameData, type);

	ZTargetTrack.WriteTrack(&camn, UnknownData, type);
	YTargetTrack.WriteTrack(&camn, UnknownData, type);
	XTargetTrack.WriteTrack(&camn, UnknownData, type);

	size_t floatOffset = camn.tell() + 8;

	camn.writeUInt32(floatOffset);
	camn.writeUInt32(floatOffset + (4 * FrameData.size()));

	for(auto& flt : FrameData){
		camn.writeFloat(flt);
    }

	for(auto& flt : UnknownData){
		camn.writeFloat(flt);
    }

}

void UCammieContext::LoadFromPathPTH(std::filesystem::path filePath) {
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

	ZNearTrack.mKeys.clear();
	ZNearTrack.mFrames.clear();

	ZFarTrack.mKeys.clear();
	ZFarTrack.mFrames.clear();

	bStream::CFileStream camn(filePath.string(), bStream::Endianess::Big, bStream::OpenMode::In);

	mEndFrame = camn.readUInt16();
	camn.skip(2);

	camn.seek(0x28);
	uint32_t keyframeOffset = camn.readUInt32();
	uint32_t unknownOffset = camn.readUInt32();

	camn.seek(0x04);

	ZPositionTrack.LoadTrack(&camn, keyframeOffset, ETrackType::PTH);
	YPositionTrack.LoadTrack(&camn, keyframeOffset, ETrackType::PTH);
	XPositionTrack.LoadTrack(&camn, keyframeOffset, ETrackType::PTH);

	if(unknownOffset != 0){
		ZTargetTrack.LoadTrack(&camn, unknownOffset, ETrackType::PTH);
		YTargetTrack.LoadTrack(&camn, unknownOffset, ETrackType::PTH);
		XTargetTrack.LoadTrack(&camn, unknownOffset, ETrackType::PTH);
	}

}