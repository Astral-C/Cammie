#pragma once
#include "../lib/bStream/bstream.h"
#include <map>
#include <vector>

enum class ETrackType
{
	CMN,
    PTH,
    LIG,
    ANM
};

struct CKeyframeCommon
{
    float frame;
    float value;
    float inslope;
    float outslope;
};

class CTrackCommon
{
public:
    uint16_t mElementCount;
	uint32_t mSymmetricSlope;
    ETrackType mType;
    std::vector<int32_t> mKeys;
    std::map<uint32_t, CKeyframeCommon> mFrames;

    void LoadTrack(bStream::CStream* stream, uint32_t keyframeDataOffset, ETrackType type);
    void WriteTrack(bStream::CStream* stream, std::vector<float>& frameDataBuffer, ETrackType type);

	void AddKeyframe(uint32_t keyframe, float value, float slopeIn=0.0f, float slopeOut=0.0f);
	void DeleteKeyframe(uint32_t keyframe);

    CTrackCommon(){}
    ~CTrackCommon(){}
};