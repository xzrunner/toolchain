#include "Application.h"
#include "ModifyTime.h"
#include "OpLog.h"

#include <sx/ResFileHelper.h>

#include <boost/filesystem.hpp>

namespace tc
{

Application::Application(const std::string& modify_time_filepath,
	                     const std::string& op_log_filepath)
	: m_modify_time_filepath(modify_time_filepath)
	, m_op_log_filepath(op_log_filepath)
{
	if (!modify_time_filepath.empty()) {
		m_modify_time = std::make_unique<ModifyTime>();
		m_modify_time->LoadFromFile(modify_time_filepath);
	}
	if (!op_log_filepath.empty()) {
		m_op_log = std::make_unique<OpLog>();
		m_op_log->LoadFromFile(op_log_filepath);
	}
}

Application::~Application()
{
	if (m_modify_time) {
		m_modify_time->StoreToFile(m_modify_time_filepath);
	}
	if (m_op_log) {
		m_op_log->StoreToFile(m_op_log_filepath);
	}
}

void Application::Do(const std::string& src_path, const std::string& dst_path,
	                 SingleFunc sfunc, MultiFunc mfunc, int sym_type)
{
	if (!boost::filesystem::is_directory(src_path)) {
		sfunc(src_path, dst_path);
		return;
	}

	boost::filesystem::create_directory(dst_path);

	boost::filesystem::recursive_directory_iterator itr(src_path), end;
	while (itr != end)
	{
		std::string filepath = itr->path().string();
		if (sx::ResFileHelper::Type(filepath.c_str()) != sym_type) {
			++itr;
			continue;
		}

		auto relative_path = boost::filesystem::relative(filepath, src_path);

		if (m_op_log) {
			m_op_log->SetUsed(relative_path.string());
		}

		if (m_modify_time)
		{
			uint64_t old_time = m_modify_time->Query(relative_path.string()),
				     new_time = boost::filesystem::last_write_time(filepath);
			if (old_time == new_time) {
				++itr;
				continue;
			} else {
				m_modify_time->Insert(relative_path.string(), new_time);
			}
		}

		auto dst_filepath = boost::filesystem::absolute(relative_path, dst_path);
		boost::filesystem::create_directory(dst_filepath.parent_path());
		mfunc(filepath, src_path, dst_filepath.string(), m_op_log, m_alloc);

		++itr;
	}
}

}