#pragma once

#include <rapidjson/document.h>

#include <boost\noncopyable.hpp>

#include <string>
#include <functional>
#include <memory>

namespace tc
{

class ModifyTime;
class OpLog;

class Application : boost::noncopyable
{
public:
	using SingleFunc = std::function<bool(const std::string& src_filepath, const std::string& dst_filepath)>;
	using MultiFunc  = std::function<bool(const std::string& filepath, const std::string& src_dir, 
		const std::string& dst_filepath, std::unique_ptr<OpLog>& op_log, rapidjson::MemoryPoolAllocator<>& alloc)>;

public:
	Application(const std::string& modify_time_filepath = "", 
		const std::string& op_log_filepath = "");
	~Application();

	void Do(const std::string& src_path, const std::string& dst_path, 
		SingleFunc sfunc, MultiFunc mfunc, int sym_type);

private:
	const std::string m_modify_time_filepath;
	const std::string m_op_log_filepath;

	std::unique_ptr<ModifyTime> m_modify_time = nullptr;
	std::unique_ptr<OpLog>      m_op_log = nullptr;

	rapidjson::MemoryPoolAllocator<> m_alloc;

}; // Application

}