#include "OpLog.h"

#include <js/RapidJsonHelper.h>

namespace tc
{

void OpLog::LoadFromFile(const std::string& filepath)
{
	if (js::RapidJsonHelper::ReadFromFile(filepath.c_str(), m_src_doc))
	{
		for (auto& val : m_src_doc.GetArray())
		{
			std::string filepath = val["filepath"].GetString();
			auto item = std::make_unique<Item>(val, false, m_src_doc.GetAllocator());
			m_map_items.insert(std::make_pair(filepath, std::move(item)));
		}
	}
}

void OpLog::StoreToFile(const std::string& filepath, const std::string& dst_dir) const
{
	rapidjson::Document dst_doc;
	dst_doc.SetArray();

	for (auto& item : m_map_items) 
	{
		if (item.second->used) {
			dst_doc.PushBack(item.second->val, dst_doc.GetAllocator());
		} else {
			// ?
			//boost::filesystem::remove(
			//	boost::filesystem::absolute(item.first, dst_dir));
		}
	}

	js::RapidJsonHelper::WriteToFile(filepath.c_str(), dst_doc);
}

void OpLog::Insert(const std::string& filepath, rapidjson::Value& val, rapidjson::MemoryPoolAllocator<>& alloc)
{
	auto itr = m_map_items.find(filepath);
	if (itr != m_map_items.end()) {
		itr->second->val.Swap(val);
	} else {
		auto item = std::make_unique<Item>(val, true, alloc);
		m_map_items.insert(std::make_pair(filepath, std::move(item)));
	}
}

void OpLog::SetUsed(const std::string& filepath)
{
	auto itr = m_map_items.find(filepath);
	if (itr != m_map_items.end()) {
		itr->second->used = true;
	}
}

}