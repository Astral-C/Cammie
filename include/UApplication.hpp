#pragma once
#define IMGUI_DEFINE_MATH_OPERATORS

class UApplication {

	virtual bool Execute(float deltaTime) = 0;
public:
	UApplication() {}
	virtual ~UApplication() {}

	void Run();

	virtual bool Setup() = 0;
	virtual bool Teardown() = 0;
};
