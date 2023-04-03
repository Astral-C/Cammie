#pragma once

#include "UApplication.hpp"

class UCammieApplication : public UApplication {
	struct GLFWwindow* mWindow;
	class UCammieContext* mContext;

	virtual bool Execute(float deltaTime) override;

public:
	UCammieApplication();
	virtual ~UCammieApplication() {}

	virtual bool Setup() override;
	virtual bool Teardown() override;
};
