#pragma once
#include "../lib/bStream/bstream.h"
#include <map>
#include <vector>

enum class ETrackType
{
	CKAN,
    CANM
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
    ETrackType mType;
    

public:
    std::vector<int32_t> mKeys;
    std::map<uint32_t, CKeyframeCommon> mFrames;

    void LoadTrack(bStream::CStream* stream, uint32_t keyframeDataOffset, ETrackType type);
    void SaveTrack(bStream::CStream* stream,  bStream::CMemoryStream& framesOut, uint32_t keyframeDataOffset, ETrackType type);

    CTrackCommon(){}
    ~CTrackCommon(){}
};