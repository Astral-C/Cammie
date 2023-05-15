#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct GLFWwindow;

constexpr glm::vec3 ZERO = glm::vec3(0.0f, 0.0f, 0.0f);
constexpr glm::vec3 UNIT_X = glm::vec3(1.0f, 0.0f, 0.0f);
constexpr glm::vec3 UNIT_Y = glm::vec3(0.0f, 1.0f, 0.0f);
constexpr glm::vec3 UNIT_Z = glm::vec3(0.0f, 0.0f, 1.0f);

constexpr float LOOK_UP_MIN = -glm::half_pi<float>() + glm::epsilon<float>();
constexpr float LOOK_UP_MAX = glm::half_pi<float>() - glm::epsilon<float>();

class USceneCamera {
	glm::vec3 mEye;
	glm::vec3 mCenter;

	float mPitch;
	float mYaw;
	glm::vec3 mForward;
	glm::vec3 mRight;
	glm::vec3 mUp;


	float mMoveSpeed;
	float mMouseSensitivity;

	void Rotate(float deltaTime, glm::vec2 mouseDelta);

public:
	float mTwist;
	float mAspectRatio;
	float mFovy;
	float mNearPlane;
	float mFarPlane;

	USceneCamera();
	~USceneCamera() {}

	void Update(float deltaTime);
	void UpdateSimple();

	glm::vec3 GetPosition() { return mEye; }
	glm::mat4 GetViewMatrix() { return glm::lookAtLH(mEye, mCenter, mUp); }
	glm::mat4 GetProjectionMatrix() { return glm::perspectiveLH(mFovy, mAspectRatio, mNearPlane, mFarPlane); }

	void SetCenter(glm::vec3 v) { mCenter = v; }
	void SetEye(glm::vec3 v) { mEye = v; }
	void ResetView() { mPitch = 0.0f; mYaw = glm::half_pi<float>(); mUp = UNIT_Y; mRight = UNIT_X; mForward = UNIT_Z; mTwist = 0.0f;}

	glm::vec3 GetCenter() { return mCenter; }
	glm::vec3 GetEye() { return mEye; }
};
