#pragma once

#include <boost/noncopyable.hpp>

#include <map>

namespace tc
{

class ModifyTime : boost::noncopyable
{
public:
	ModifyTime();

	void LoadFromFile(const std::string& filepath);
	void StoreToFile(const std::string& filepath) const;
	
	void Insert(const std::string& filepath, uint64_t timestamp);
	uint64_t Query(const std::string& filepath) const;

private:
	std::map<std::string, uint64_t> m_path2time;

	bool m_dirty;

}; // ModifyTime

}