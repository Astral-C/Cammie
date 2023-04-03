#pragma once

#include "UCamera.hpp"

#include "io/KeyframeIO.hpp"

#include <vector>
#include <filesystem>
#include <memory>
#include <UGrid.hpp>

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


	int mCurrentFrame, mStartFrame, mEndFrame;

	USceneCamera mCamera;
	UGrid mGrid;

	//std::vector<std::unique_ptr<GalaxyZone>();

	uint32_t mMainDockSpaceID;
	uint32_t mDockNodeBottomID, mDockNodeBottomRightID;
	
	bool bIsDockingSetUp { false };
	bool bIsFileDialogOpen { false };
	bool bIsSaveDialogOpen { false };
	bool mSetLights { false };
	bool mTextEditorActive { false };
	bool mPositionOpen { false };
	bool mTargetOpen { false };
	bool mPlaying { false };
	bool mUpdateCameraPosition { false };

	void RenderMainWindow(float deltaTime);
	void RenderPanels(float deltaTime);
	void RenderMenuBar();

	void OpenModelCB();
	void SaveModelCB();

	void SetLights();
	void LoadFromPath(std::filesystem::path filePath);

	void SaveModel(std::filesystem::path filePath);

public:
	UCammieContext();
	~UCammieContext() {}

	bool Update(float deltaTime);
	void Render(float deltaTime);
};
