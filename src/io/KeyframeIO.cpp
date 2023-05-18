#include "io/KeyframeIO.hpp"
#include "bstream.h"
#include <algorithm>
#include <cmath>

void CTrackCommon::WriteTrack(bStream::CStream* stream, std::vector<float>& frameDataBuffer, ETrackType type){

	stream->writeUInt16(mKeys.size());
	stream->writeUInt16(frameDataBuffer.size());
    stream->writeUInt16(mElementCount);

	for (size_t frame = 0; frame < mKeys.size(); frame++){
		auto framedata = mFrames.at(mKeys.at(frame));

        if(mElementCount == 1){
            frameDataBuffer.push_back(framedata.value);
        } else if(mElementCount == 2) {
            frameDataBuffer.push_back(framedata.frame);
            frameDataBuffer.push_back(framedata.value);
        } else if(mElementCount == 3) {
            frameDataBuffer.push_back(framedata.frame);
            frameDataBuffer.push_back(framedata.value);
            frameDataBuffer.push_back(framedata.inslope);
        } else if(mElementCount == 4){
            frameDataBuffer.push_back(framedata.frame);
            frameDataBuffer.push_back(framedata.value);
            frameDataBuffer.push_back(framedata.inslope);
            frameDataBuffer.push_back(framedata.outslope);
        }

	}
}

void CTrackCommon::LoadTrack(bStream::CStream* stream, uint32_t keyframeDataOffset, ETrackType type)
{
    mType = type;
    
    uint16_t keyCount = stream->readUInt16();
    uint16_t beginIndex = stream->readUInt16();
    mElementCount = stream->readUInt16();

    if(mType != ETrackType::ANM){
        mSymmetricSlope = (mElementCount == 3);
    } else {
        mSymmetricSlope = (mElementCount == 0x80);
    }

    size_t group = stream->tell();

    stream->seek(keyframeDataOffset + (4 * beginIndex));
    for (size_t frame = 0; frame < keyCount; frame++)
    {
        
        CKeyframeCommon keyframe;

        if(mElementCount == 1){
            keyframe.frame = 0;
            keyframe.value = stream->readFloat();
        } else if(mElementCount == 2) {
            keyframe.frame = stream->readFloat();
            keyframe.value = stream->readFloat();
        } else if(mElementCount == 3) {
            keyframe.frame = stream->readFloat();
            keyframe.value = stream->readFloat();
            keyframe.inslope = stream->readFloat();
            keyframe.outslope = keyframe.inslope;
        } else if(mElementCount == 4){
            keyframe.frame = stream->readFloat();
            keyframe.value = stream->readFloat();
            keyframe.inslope = stream->readFloat();
            keyframe.outslope = stream->readFloat();
        }

        //write anm stuff at some point
         
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