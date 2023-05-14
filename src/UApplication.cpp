#include "UApplication.hpp"
#include "UTime.hpp"

void UApplication::Run() {
	Clock::time_point lastFrameTime, thisFrameTime;

	while (true) {
		thisFrameTime = UUtil::GetTime();

		if(UUtil::GetDeltaTime(lastFrameTime, thisFrameTime) > 1.0/30.0f){
			if (!Execute(UUtil::GetDeltaTime(lastFrameTime, thisFrameTime)))
				break;
			lastFrameTime = thisFrameTime;
		}
			
	}
}
