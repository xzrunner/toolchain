#include "Application.h"
#include "OpLog.h"

#include <guard/check.h>
#include <pimg/ImageData.h>
#include <pimg/RegularRectCut.h>
#include <pimg/Cropping.h>
#include <gimg_export.h>
#include <gimg_typedef.h>
#include <sx/ResFileHelper.h>
#include <facade/ResPool.h>

#include <boost/filesystem.hpp>

#include <string>
#include <regex>

namespace
{

static const char* TIME_FILEPATH = "auto_crop_grids_time.json";
static const char* LOG_FILEPATH  = "auto_crop_grids_log.json";

template<typename ... Args>
std::string string_format(const std::string& format, Args ... args)
{
	size_t size = std::snprintf(nullptr, 0, format.c_str(), args ...) + 1; // Extra space for '\0'
	std::unique_ptr<char[]> buf(new char[size]);
	std::snprintf(buf.get(), size, format.c_str(), args ...);
	return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
}

bool Crop(const std::string& src_filepath, const std::string& dst_filepath, const std::string& src_dir)
{
	if (sx::ResFileHelper::Type(src_filepath.c_str()) != sx::RES_FILE_IMAGE) {
		return false;
	}

	auto dst_dir = boost::filesystem::path(dst_filepath).parent_path().string();

	static const bool PRE_MUL_ALPHA(false);
	auto img = facade::ResPool::Instance().Fetch<pimg::ImageData>(src_filepath, PRE_MUL_ALPHA);

	pimg::RegularRectCut cut(img->GetPixelData(), img->GetWidth(), img->GetHeight());
	cut.AutoCut();

//	std::cout << ee::StringHelper::Format("File: %s, Left: %d, Used: %d", filepath.c_str(), cut.GetLeftArea(), cut.GetUseArea()) << std::endl;

	std::string filename = boost::filesystem::relative(src_filepath, src_dir).string();
	filename = filename.substr(0, filename.find_last_of('.'));
	filename = std::regex_replace(filename, std::regex("\\\\"), "%");

	GD_ASSERT(img->GetFormat() == GPF_RGB || img->GetFormat() == GPF_RGBA8, "err img");
	int channels = img->GetFormat() == GPF_RGB ? 3 : 4;
	pimg::Cropping crop(img->GetPixelData(), img->GetWidth(), img->GetHeight(), channels, true);

	auto& result = cut.GetResult();
	for (int i = 0, n = result.size(); i < n; ++i)
	{
		auto& r = result[i];
		const uint8_t* pixels = crop.Crop(r.x, r.y, r.x + r.w, r.y + r.h);

		std::string out_path = string_format("%s\\%s#%d#%d#%d#%d#", dst_dir.c_str(), filename.c_str(), r.x, r.y, r.w, r.h) + ".png";
		gimg_export(out_path.c_str(), pixels, r.w, r.h, GPF_RGBA8, true);
		delete[] pixels;
	}
}

bool CropSingle(const std::string& src_filepath, const std::string& dst_filepath)
{
	auto src_dir = boost::filesystem::path(src_filepath).parent_path().string();
	return Crop(src_filepath, dst_filepath, src_dir);
}

bool CropMulti(const std::string& filepath, const std::string& src_dir, const std::string& dst_filepath,
	           std::unique_ptr<tc::OpLog>& op_log, rapidjson::MemoryPoolAllocator<>& alloc)
{
	if (!Crop(filepath, dst_filepath, src_dir)) {
		return false;
	}

	// todo log

	return true;
}

}
namespace tc
{
extern bool InitRender();
}

namespace crop
{

void AutoCropGrids(const std::string& src_path, const std::string& dst_path)
{
	tc::InitRender();

	auto modify_time_filepath = boost::filesystem::absolute(TIME_FILEPATH, dst_path);
	auto cfg_filepath = boost::filesystem::absolute(LOG_FILEPATH, dst_path);
	tc::Application app(modify_time_filepath.string(), cfg_filepath.string());
	app.Do(src_path, dst_path, CropSingle, CropMulti, sx::RES_FILE_IMAGE);
}

}