#include "io/KeyframeIO.hpp"
#include <algorithm>


void CTrackCommon::LoadTrack(bStream::CStream* stream, uint32_t keyframeDataOffset, ETrackType type)
{
    mType = type;
    
    uint16_t keyCount = stream->readInt32();
    uint16_t beginIndex = stream->readInt32();
    uint16_t slopeFlags = -1;

    if(mType == ETrackType::CKAN){
        slopeFlags = stream->readInt32();
		mSymmetricSlope = slopeFlags == 0;
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

void CTrackCommon::SaveTrack(bStream::CStream* stream, bStream::CMemoryStream& framesOut, uint32_t keyframeDataOffset, ETrackType type){

}

void CTrackCommon::DeleteKeyframe(uint32_t keyframe) {
	if(!std::count(mKeys.begin(), mKeys.end(), keyframe)) return; // keyframe doesnt exist
	mKeys.erase(std::remove(mKeys.begin(), mKeys.end(), keyframe), mKeys.end());
	mFrames.erase(keyframe);
}