#pragma once

#include "UCamera.hpp"

#include "io/KeyframeIO.hpp"
#include "io/BinIO.hpp"

#include <vector>
#include <filesystem>
#include <memory>
#include <UGrid.hpp>
#include <UPointSpriteManager.hpp>
#include <UMapRenderer.hpp>

#include <ImGuiFileDialog.h>

namespace bStream { class CStream; }
class J3DModelData;
class J3DMaterial;
class J3DModelInstance;


class UCammieContext {
	
	// mRoot;
	CTrackCommon XPositionTrack;
	CTrackCommon YPositionTrack;
	CTrackCommon ZPositionTrack;


	CTrackCommon XTargetTrack;
	CTrackCommon YTargetTrack;
	CTrackCommon ZTargetTrack;

	CTrackCommon TwistTrack;
	CTrackCommon FovYTrack;

	CTrackCommon ZNearTrack;
	CTrackCommon ZFarTrack;

	float UnknownValue;
	
	int mCurrentFrame, mStartFrame, mEndFrame;

	USceneCamera mCamera;
	UGrid mGrid;
	CPointSpriteManager mBillboardManager;
	//CGalaxyRenderer mGalaxyRenderer;
	//std::shared_ptr<BinModel> mModel { nullptr };
	CMapRenderer mMapRenderer;


	//std::vector<std::unique_ptr<GalaxyZone>();

	uint32_t mMainDockSpaceID;
	uint32_t mDockNodeBottomID, mDockNodeRightID, mDockNodeBottomRightID;
	
	bool bIsDockingSetUp { false };
	bool bIsFileDialogOpen { false };
	bool bIsGalaxyDialogOpen { false };
	bool bIsSaveDialogOpen { false };

	bool mSetLights { false };

	bool mPositionOpen { false };
	bool mTargetOpen { false };

	bool mPlaying { false };
	bool mUpdateCameraPosition { false };
	bool mViewCamera { false };
	bool mOptionsOpen { false };
	bool mShowZones { false };
	bool mGizmoTarget { true };

	void RenderMainWindow(float deltaTime);
	void RenderPanels(float deltaTime);
	void RenderMenuBar();

	void OpenModelCB();
	void SaveModelCB();

	void SetLights();
	void LoadFromPath(std::filesystem::path filePath);
	void SaveAnimation(std::filesystem::path savePath);

	glm::vec3 ManipulationGizmo(glm::vec3 position);

public:
	UCammieContext();
	~UCammieContext();

	bool Update(float deltaTime);
	void Render(float deltaTime);
};
