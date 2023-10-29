#include "io/JmpIO.hpp"

LJmpIO::LJmpIO()
{

}

bool LJmpIO::Load(bStream::CMemoryStream* stream)
{
	mEntryCount = stream->readInt32();
	mFieldCount = stream->readInt32();
	mEntryStartOffset = stream->readUInt32();
	mEntrySize = stream->readUInt32();

	if (mEntrySize == 0 || mEntryStartOffset + mEntrySize * mEntryCount > stream->getSize())
		return false;

	mData = new uint8_t[mEntryCount * mEntrySize];

	// Make our own copy of the entry data, since the stream will be
	// destroyed when we're done reading.
	memcpy(mData, stream->getBuffer() + mEntryStartOffset, mEntrySize * mEntryCount);

	mFields.clear();
	mFields.reserve(mFieldCount);

	for (int32_t i = 0; i < mFieldCount; i++)
	{
		LJmpFieldInfo newField;
		newField.Hash = stream->readUInt32();
		newField.Bitmask = stream->readUInt32();
		newField.Start = stream->readUInt16();
		newField.Shift = stream->readUInt8();
		newField.Type = (EJmpFieldType)stream->readUInt8();

		mFields.push_back(newField);
	}

	return true;
}

bool LJmpIO::Load(nlohmann::json jsonTemplate, size_t count)
{
	nlohmann::json fieldsArray = jsonTemplate["fields"];

	mFields.clear();
	mFields.reserve(fieldsArray.size());

	for (auto field : fieldsArray)
	{
		LJmpFieldInfo newField;
		newField.Hash = field["hash"];
		newField.Bitmask = field["mask"];
		newField.Start = field["start"];
		newField.Shift = field["shift"];
		newField.Type = (EJmpFieldType)field["type"];

		mFields.push_back(newField);

		mFieldCount++;
	}

	mEntryCount = count;
	mFieldCount = static_cast<int32_t>(mFields.size());
	mEntryStartOffset = JMP_HEADER_SIZE + (mFieldCount * JMP_FIELD_DEF_SIZE);
	mEntrySize = CalculateNewEntrySize();

	if (mEntrySize == 0)
		return false;

	mData = nullptr;

	return true;
}

uint32_t LJmpIO::HashFieldName(std::string name) const
{
	uint32_t hash = 0;

	for (char c : name)
	{
		hash = ((hash << 8) + c) % JMP_HASH_PRIME;
	}

	return hash;
}

const LJmpFieldInfo* LJmpIO::FetchJmpFieldInfo(std::string name)
{
	const LJmpFieldInfo* field = nullptr;
	uint32_t nameHash = HashFieldName(name);

	for (const LJmpFieldInfo& f : mFields)
	{
		if (nameHash == f.Hash)
		{
			field = &f;
			break;
		}
	}

	return field;
}

const LJmpFieldInfo* LJmpIO::FetchJmpFieldInfo(uint32_t hash)
{
	const LJmpFieldInfo* field = nullptr;

	for (const LJmpFieldInfo& f : mFields)
	{
		if (hash == f.Hash)
		{
			field = &f;
			break;
		}
	}

	return field;
}

uint32_t LJmpIO::PeekU32(uint32_t offset)
{
	if (offset >= mEntryCount * mEntrySize)
		return 0;

	return (
		mData[offset]     << 24 |
		mData[offset + 1] << 16 |
		mData[offset + 2] << 8 |
		mData[offset + 3]
	);
}

int32_t LJmpIO::PeekS32(uint32_t offset)
{
	return (int32_t)PeekU32(offset);
}

float LJmpIO::PeekF32(uint32_t offset)
{
	union {
		uint32_t u32;
		float f32;
	} converter;

	converter.u32 = PeekU32(offset);
	return converter.f32;
}

bool LJmpIO::PokeU32(uint32_t offset, uint32_t value)
{
	if (offset >= mEntryCount * mEntrySize)
		return false;

	mData[offset] = (uint8_t)(value >> 24);
	mData[offset + 1] = (uint8_t)(value >> 16);
	mData[offset + 2] = (uint8_t)(value >> 8);
	mData[offset + 3] = (uint8_t)(value);

	return true;
}

bool LJmpIO::PokeS32(uint32_t offset, int32_t value)
{
	return PokeU32(offset, (uint32_t)value);
}

bool LJmpIO::PokeF32(uint32_t offset, float value)
{
	union {
		uint32_t u32;
		float f32;
	} converter;

	converter.f32 = value;
	return PokeU32(offset, converter.u32);
}

uint32_t LJmpIO::GetUnsignedInt(uint32_t entry_index, std::string field_name)
{
	const LJmpFieldInfo* field = FetchJmpFieldInfo(field_name);

	// If field is still nullptr, we failed to find a field matching the given name.
	if (field == nullptr)
		return 0;

	uint32_t fieldOffset = entry_index * mEntrySize + field->Start;
	uint32_t rawFieldValue = PeekU32(fieldOffset);

	return (rawFieldValue & field->Bitmask) >> field->Shift;
}

int32_t LJmpIO::GetSignedInt(uint32_t entry_index, std::string field_name)
{
	const LJmpFieldInfo* field = FetchJmpFieldInfo(field_name);

	// If field is still nullptr, we failed to find a field matching the given name.
	if (field == nullptr)
		return 0;

	uint32_t fieldOffset = entry_index * mEntrySize + field->Start;

	return PeekS32(fieldOffset);
}

int32_t LJmpIO::GetSignedInt(uint32_t entry_index, uint32_t field_hash)
{
	const LJmpFieldInfo* field = FetchJmpFieldInfo(field_hash);

	// If field is still nullptr, we failed to find a field matching the given name.
	if (field == nullptr)
		return 0;

	uint32_t fieldOffset = entry_index * mEntrySize + field->Start;

	return PeekS32(fieldOffset);
}

float LJmpIO::GetFloat(uint32_t entry_index, std::string field_name)
{
	const LJmpFieldInfo* field = FetchJmpFieldInfo(field_name);

	// If field is still nullptr, we failed to find a field matching the given name.
	if (field == nullptr)
		return 0.0f;

	uint32_t fieldOffset = entry_index * mEntrySize + field->Start;

	return PeekF32(fieldOffset);
}

bool LJmpIO::GetBoolean(uint32_t entry_index, std::string field_name)
{
	return GetUnsignedInt(entry_index, field_name) != 0;
}

std::string LJmpIO::GetString(uint32_t entry_index, std::string field_name)
{
	const LJmpFieldInfo* field = FetchJmpFieldInfo(field_name);

	// If field is still nullptr, we failed to find a field matching the given name.
	if (field == nullptr)
		return "";

	uint32_t fieldOffset = entry_index * mEntrySize + field->Start;

	char strBuffer[33];
	memcpy(strBuffer, mData + fieldOffset, mStringSize);
	strBuffer[32] = 0;

	return std::string(strBuffer);
}

uint32_t LJmpIO::CalculateNewEntrySize()
{
	uint32_t newSize = 0;

	for (const LJmpFieldInfo f : mFields)
	{
		uint32_t tempNewSize = f.Start;

		if (f.Type == EJmpFieldType::String)
			tempNewSize += mStringSize;
		else
			tempNewSize += sizeof(uint32_t);

		newSize = std::max(newSize, tempNewSize);
	}

	return newSize;
}

bool LJmpIO::SetUnsignedInt(uint32_t entry_index, std::string field_name, uint32_t value)
{
	const LJmpFieldInfo* field = FetchJmpFieldInfo(field_name);

	// If field is still nullptr, we failed to find a field matching the given name.
	if (field == nullptr)
		return false;

	uint32_t fieldOffset = entry_index * mEntrySize + field->Start;

	uint32_t curField = PeekU32(fieldOffset);
	uint32_t packedValue = (value << field->Shift) & field->Bitmask;

	return PokeU32(fieldOffset, (curField & ~field->Bitmask) | packedValue);
}

bool LJmpIO::SetSignedInt(uint32_t entry_index, std::string field_name, int32_t value)
{
	const LJmpFieldInfo* field = FetchJmpFieldInfo(field_name);

	// If field is still nullptr, we failed to find a field matching the given name.
	if (field == nullptr)
		return false;

	uint32_t fieldOffset = entry_index * mEntrySize + field->Start;

	return PokeS32(fieldOffset, value);
}

bool LJmpIO::SetSignedInt(uint32_t entry_index, uint32_t field_hash, int32_t value)
{
	const LJmpFieldInfo* field = FetchJmpFieldInfo(field_hash);

	// If field is still nullptr, we failed to find a field matching the given name.
	if (field == nullptr)
		return false;

	uint32_t fieldOffset = entry_index * mEntrySize + field->Start;

	return PokeS32(fieldOffset, value);
}

bool LJmpIO::SetFloat(uint32_t entry_index, std::string field_name, float value)
{
	const LJmpFieldInfo* field = FetchJmpFieldInfo(field_name);

	// If field is still nullptr, we failed to find a field matching the given name.
	if (field == nullptr)
		return false;

	uint32_t fieldOffset = entry_index * mEntrySize + field->Start;

	return PokeF32(fieldOffset, value);
}

bool LJmpIO::SetBoolean(uint32_t entry_index, std::string field_name, bool value)
{
	return SetUnsignedInt(entry_index, field_name, (uint32_t)value);
}

bool LJmpIO::SetString(uint32_t entry_index, std::string field_name, std::string value)
{
	const LJmpFieldInfo* field = FetchJmpFieldInfo(field_name);

	// If field is still nullptr, we failed to find a field matching the given name.
	if (field == nullptr)
		return false;

	uint32_t fieldOffset = entry_index * mEntrySize + field->Start;
	memcpy(mData + fieldOffset, value.data(), std::min(mStringSize - 1, value.length()));

	return true;
}
