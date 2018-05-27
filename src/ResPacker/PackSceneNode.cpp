#include <guard/check.h>
#include <sx/ResFileHelper.h>
#include <bs/ExportStream.h>
#include <ns/CompFactory.h>
#include <ns/CompSerializer.h>
#include <ns/RegistCallback.h>
#include <ns/Blackboard.h>
#include <node0/CompAsset.h>

#include <boost/filesystem.hpp>

#include <string>
#include <unordered_map>

namespace
{

static const char* TIMESTAMP_FILENAME = "timestamp.bin";

class Timestamp
{
public:
	Timestamp(const std::string& filepath)
		: m_filepath(filepath)
	{
		std::locale::global(std::locale(""));
		std::ifstream fin(filepath, std::ios::binary);
		std::locale::global(std::locale("C"));

		size_t n = 0;
		fin >> n;
		for (size_t i = 0; i < n; ++i)
		{
			std::string path;
			uint32_t time;
			fin >> path >> time;
			m_path2time.insert({ path, time });
		}

		fin.close();
	}
	~Timestamp()
	{
		std::locale::global(std::locale(""));
		std::ofstream fout(m_filepath, std::ios::binary);
		std::locale::global(std::locale("C"));

		fout << m_path2time.size() << "\n";
		for (auto& itr : m_path2time) {
			fout << itr.first << " " << itr.second << "\n";
		}

		fout.close();
	}

	uint64_t QueryTime(const std::string& filepath) const
	{
		auto itr = m_path2time.find(filepath);
		if (itr == m_path2time.end()) {
			return 0;
		} else {
			return itr->second;
		}
	}

	void SetTime(const std::string& filepath, uint64_t time)
	{
		m_path2time[filepath] = time;
	}

private:
	std::string m_filepath;

	std::unordered_map<std::string, uint64_t> m_path2time;

}; // Timestamp

bool Pack(const std::string& src_dir, const std::string& src_path, const std::string& dst_path)
{
	auto asset = ns::CompFactory::Instance()->CreateAsset(src_path);

	size_t sz = ns::CompSerializer::Instance()->GetBinSize(*asset, src_dir);
	uint8_t* buf = new uint8_t[sz];
	bs::ExportStream es(buf, sz);

	ns::CompSerializer::Instance()->ToBin(*asset, src_dir, es);

	GD_ASSERT(es.Size() == 0, "err serialize");

	std::ofstream fout(dst_path, std::ofstream::binary);
	fout.write(reinterpret_cast<const char*>(buf), sz);
	fout.close();

	delete[] buf;

	return true;
}

void InitCallback()
{
	static uint32_t next_obj_id = 0;
	ns::RegistCallback::Init();
	ns::Blackboard::Instance()->SetGenNodeIdFunc([]()->uint32_t {
		return next_obj_id++;
	});
}

}

namespace tc
{
extern bool InitRender();
extern bool InitSubmodule();
}

namespace packer
{

void PackNode(const std::string& src_dir, const std::string& dst_dir)
{
	tc::InitRender();
	tc::InitSubmodule();
	InitCallback();

	GD_ASSERT(boost::filesystem::is_directory(src_dir), "not dir");

	Timestamp time(dst_dir + "/" + TIMESTAMP_FILENAME);

	boost::filesystem::recursive_directory_iterator itr(src_dir), end;
	while (itr != end)
	{
		// skip dir
		if (boost::filesystem::is_directory(itr->path())) {
			++itr;
			continue;
		}

		std::string src_filepath = itr->path().string();

		auto relative_path = boost::filesystem::relative(src_filepath, src_dir);
		auto dst_filepath = boost::filesystem::absolute(relative_path, dst_dir).replace_extension(".bin");

		boost::filesystem::create_directories(dst_filepath.parent_path());

		auto curr_time = boost::filesystem::last_write_time(src_filepath);
		auto last_time = time.QueryTime(src_filepath);

		// not changed
		if (curr_time == last_time) {
			++itr;
			continue;
		}

		auto type = sx::ResFileHelper::Type(src_filepath);
		switch (type)
		{
		case sx::RES_FILE_IMAGE:
			// tdoo: compress img
			boost::filesystem::copy_file(
				src_filepath,
				boost::filesystem::absolute(relative_path, dst_dir),
				boost::filesystem::copy_option::overwrite_if_exists
			);
			if (Pack(src_dir, src_filepath, dst_filepath.string())) {
				time.SetTime(src_filepath, curr_time);
			}
			break;
		case sx::RES_FILE_JSON:
			if (Pack(src_dir, src_filepath, dst_filepath.string())) {
				time.SetTime(src_filepath, curr_time);
			}
			break;
		default:
			GD_REPORT_ASSERT("unsupport type.");
		}

		++itr;
	}
}

}