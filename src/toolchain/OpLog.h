#pragma once

#include <rapidjson/document.h>

#include <memory>
#include <string>
#include <map>

namespace tc
{

class OpLog
{
public:
	void LoadFromFile(const std::string& filepath);
	void StoreToFile(const std::string& filepath/*, const std::string& dst_dir*/) const;

	void Insert(const std::string& filepath, rapidjson::Value& val, rapidjson::MemoryPoolAllocator<>& alloc);

	void SetUsed(const std::string& filepath);

private:
	struct Item
	{
		Item(rapidjson::Value& val, bool used, 
			 rapidjson::MemoryPoolAllocator<>& alloc)
			: used(used) {
			this->val.Swap(val);
		}

		rapidjson::Value val;
		mutable bool     used;
	};

private:
	rapidjson::Document m_src_doc;

	std::map<std::string, std::unique_ptr<Item>> m_map_items;

}; // OpLog

}