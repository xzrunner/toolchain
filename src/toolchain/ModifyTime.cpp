#include "ModifyTime.h"

#include <js/RapidJsonHelper.h>
#include <guard/check.h>

#include <rapidjson/document.h>

namespace tc
{

ModifyTime::ModifyTime()
	: m_dirty(false)
{
}

void ModifyTime::LoadFromFile(const std::string& filepath)
{
	m_dirty = false;

	rapidjson::Document doc;
	if (js::RapidJsonHelper::ReadFromFile(filepath.c_str(), doc))
	{
		for (auto& item : doc.GetArray())
		{
			std::string path = item["path"].GetString();
			uint64_t timestamp = item["timestamp"].GetUint64();
			m_path2time.insert(std::make_pair(path, timestamp));
		}
	}
}

void ModifyTime::StoreToFile(const std::string& filepath) const
{
	if (!m_dirty) {
		return;
	}

	rapidjson::Document doc;
	doc.SetArray();

	auto& alloc = doc.GetAllocator();
	for (auto& itr : m_path2time) 
	{
		rapidjson::Value val;
		val.SetObject();
		val.AddMember("path", rapidjson::Value(itr.first.c_str(), alloc), alloc);
		val.AddMember("timestamp", itr.second, alloc);
		doc.PushBack(val, alloc);
	}

	js::RapidJsonHelper::WriteToFile(filepath.c_str(), doc/*, false*/);
}

void ModifyTime::Insert(const std::string& filepath, uint64_t timestamp)
{
	m_dirty = true;

	auto pair = m_path2time.insert(std::make_pair(filepath, timestamp));
	if (!pair.second) {
		GD_ASSERT(pair.first->second < timestamp, "err timestamp.");
	}
}

uint64_t ModifyTime::Query(const std::string& filepath) const
{
	auto itr = m_path2time.find(filepath);
	if (itr != m_path2time.end()) {
		return itr->second;
	} else {
		return 0;
	}
}

}