#include "io/KeyframeIO.hpp"
#include "bstream.h"
#include <algorithm>
#include <cmath>

void CTrackCommon::WriteTrack(bStream::CStream* stream, std::vector<float>& frameDataBuffer, ETrackType type){
	if (type == ETrackType::CKAN){
		stream->writeInt32(mKeys.size());
		stream->writeInt32(frameDataBuffer.size());
		stream->writeInt32(mSymmetricSlope);
    } else {
		stream->writeInt32(mKeys.size());
		stream->writeInt32(frameDataBuffer.size());
    }

	for (size_t frame = 0; frame < mKeys.size(); frame++){
		auto framedata = mFrames.at(mKeys.at(frame));
		frameDataBuffer.push_back(framedata.frame);
		frameDataBuffer.push_back(framedata.value);\
		if(type == ETrackType::CKAN){
			frameDataBuffer.push_back(framedata.inslope);
			if(mSymmetricSlope != 0) frameDataBuffer.push_back(framedata.outslope);
		}
	}
}

void CTrackCommon::LoadTrack(bStream::CStream* stream, uint32_t keyframeDataOffset, ETrackType type)
{
    mType = type;
    
    uint16_t keyCount = stream->readInt32();
    uint16_t beginIndex = stream->readInt32();
    uint16_t slopeFlags = 0;

    if(mType == ETrackType::CKAN){
        slopeFlags = stream->readInt32();
		mSymmetricSlope = slopeFlags;
    }

    size_t group = stream->tell();

    stream->seek(keyframeDataOffset + 4 + (4 * beginIndex));
    for (size_t frame = 0; frame < keyCount; frame++)
    {
        
        CKeyframeCommon keyframe;

        keyframe.frame = stream->readFloat();
        keyframe.value = stream->readFloat();

        if(mType == ETrackType::CKAN){
            keyframe.inslope = stream->readFloat();
            if(slopeFlags != 0) keyframe.outslope = stream->readFloat();
        }
        
        mFrames.insert(std::make_pair((uint32_t)keyframe.frame, keyframe));
    }

    stream->seek(group);

    for (auto& frame : mFrames)
    {
        mKeys.push_back(frame.first);
    }

}

void CTrackCommon::AddKeyframe(uint32_t keyframe, float value, float slopeIn, float slopeOut) {
	if(!std::count(mKeys.begin(), mKeys.end(), keyframe)){
		mKeys.insert(std::upper_bound(mKeys.begin(), mKeys.end(), keyframe), keyframe);
		mFrames.insert({(uint32_t)keyframe, {(float)keyframe, value, slopeIn, slopeOut}});
	}
}

void CTrackCommon::DeleteKeyframe(uint32_t keyframe) {
	if(!std::count(mKeys.begin(), mKeys.end(), keyframe)) return; // keyframe doesnt exist
	mKeys.erase(std::remove(mKeys.begin(), mKeys.end(), keyframe), mKeys.end());
	mFrames.erase(keyframe);
}