#include "Application.h"
#include "OpLog.h"

#include <guard/check.h>
#include <gimg_typedef.h>
#include <gimg_export.h>
#include <pimg/Rect.h>
#include <pimg/ImageData.h>
#include <pimg/Condense.h>
#include <js/RapidJsonHelper.h>
#include <sx/ResFileHelper.h>
#include <facade/ResPool.h>

#include <rapidjson/document.h>

#include <boost/filesystem.hpp>

#include <string>
#include <map>
#include <memory>
#include <iostream>
#include <fstream>

namespace
{

static const char* TIME_FILEPATH = "crop_border_time.json";
static const char* LOG_FILEPATH  = "crop_border_log.json";

bool IsTransparent(const pimg::ImageData& img, int x, int y)
{
	if (img.GetFormat() != GPF_RGBA8) {
		return false;
	} else {
		return img.GetPixelData()[(img.GetWidth() * y + x) * 4 + 3] == 0;
	}
}

void StoreBoundInfo(const pimg::ImageData& img, const pimg::Rect& r,
	                rapidjson::Value& val, rapidjson::MemoryPoolAllocator<>& alloc)
{
	rapidjson::Value bound_val;
	bound_val.SetObject();

	// left
	{
		int tot = 0, max = 0;
		int curr = 0;
		for (int y = 0; y < img.GetHeight(); ++y) {
			if (!IsTransparent(img, r.xmin, y)) {
				++curr;
				++tot;
				if (curr > max) {
					max = curr;
				} else {
					curr = 0;
				}
			}
		}

		rapidjson::Value left_val;
		left_val.SetObject();
		left_val.AddMember("tot", tot, alloc);
		left_val.AddMember("max", max, alloc);
		bound_val.AddMember("left", left_val, alloc);
	}
	// right
	{
		int tot = 0, max = 0;
		int curr = 0;
		for (int y = 0; y < img.GetHeight(); ++y) {
			if (!IsTransparent(img, r.xmax - 1, y)) {
				++curr;
				++tot;
				if (curr > max) {
					max = curr;
				} else {
					curr = 0;
				}
			}
		}

		rapidjson::Value right_val;
		right_val.SetObject();
		right_val.AddMember("tot", tot, alloc);
		right_val.AddMember("max", max, alloc);
		bound_val.AddMember("right", right_val, alloc);
	}
	// bottom
	{
		int tot = 0, max = 0;
		int curr = 0;
		for (int x = 0; x < img.GetWidth(); ++x) {
			if (!IsTransparent(img, x, r.ymin)) {
				++curr;
				++tot;
				if (curr > max) {
					max = curr;
				} else {
					curr = 0;
				}
			}
		}

		rapidjson::Value bottom_val;
		bottom_val.SetObject();
		bottom_val.AddMember("tot", tot, alloc);
		bottom_val.AddMember("max", max, alloc);
		bound_val.AddMember("bottom", bottom_val, alloc);
	}
	// up
	{
		int tot = 0, max = 0;
		int curr = 0;
		for (int x = 0; x < img.GetWidth(); ++x) {
			if (!IsTransparent(img, x, r.ymax - 1)) {
				++curr;
				++tot;
				if (curr > max) {
					max = curr;
				} else {
					curr = 0;
				}
			}
		}

		rapidjson::Value up_val;
		up_val.SetObject();
		up_val.AddMember("tot", tot, alloc);
		up_val.AddMember("max", max, alloc);
		bound_val.AddMember("up", up_val, alloc);
	}

	val.AddMember("bound", bound_val, alloc);
}

bool Crop(const std::string& src_filepath, const std::string& dst_filepath,
	      std::shared_ptr<pimg::ImageData>& img, pimg::Rect& r)
{
	if (sx::ResFileHelper::Type(src_filepath.c_str()) != sx::RES_FILE_IMAGE) {
		return false;
	}

	static const bool PRE_MUL_ALPHA(false);
	img = facade::ResPool::Instance().Fetch<pimg::ImageData>(src_filepath, PRE_MUL_ALPHA);

	uint8_t* condense = nullptr;
	if (img->GetFormat() == GPF_RGBA8)
	{
		pimg::Condense cd(img->GetPixelData(), img->GetWidth(), img->GetHeight());
		condense = cd.GetPixels(r);
	}
	if (!condense) {
		r.xmin = r.ymin = 0;
		r.xmax = img->GetWidth();
		r.ymax = img->GetHeight();
	}

	boost::filesystem::create_directory(boost::filesystem::path(dst_filepath).parent_path());
	if (condense) {
		gimg_export(dst_filepath.c_str(), condense, r.Width(), r.Height(), img->GetFormat(), true);
		delete[] condense;
	} else {
		gimg_export(dst_filepath.c_str(), img->GetPixelData(), img->GetWidth(), img->GetHeight(), img->GetFormat(), true);
	}

	return true;
}

bool CropSingle(const std::string& src_filepath, const std::string& dst_filepath)
{
	std::shared_ptr<pimg::ImageData> img;
	pimg::Rect r;
	return Crop(src_filepath, dst_filepath, img, r);
}

bool CropMulti(const std::string& filepath, const std::string& src_dir, const std::string& dst_filepath,
	           std::unique_ptr<tc::OpLog>& op_log, rapidjson::MemoryPoolAllocator<>& alloc)
{
	std::shared_ptr<pimg::ImageData> img;
	pimg::Rect r;
	if (!Crop(filepath, dst_filepath, img, r)) {
		return false;
	}

	if (op_log)
	{
		// save log
		rapidjson::Value val;
		val.SetObject();

		std::string relative_path = boost::filesystem::relative(filepath, src_dir).string();
		val.AddMember("filepath", rapidjson::Value(relative_path.c_str(), alloc), alloc);

		rapidjson::Value sz_val;
		sz_val.SetObject();
		sz_val.AddMember("w", img->GetWidth(), alloc);
		sz_val.AddMember("h", img->GetHeight(), alloc);
		val.AddMember("source_size", sz_val, alloc);

		rapidjson::Value pos_val;
		pos_val.SetObject();
		pos_val.AddMember("x", r.xmin, alloc);
		pos_val.AddMember("y", img->GetHeight() - r.ymax, alloc);
		pos_val.AddMember("w", r.Width(), alloc);
		pos_val.AddMember("h", r.Height(), alloc);
		val.AddMember("position", pos_val, alloc);

		StoreBoundInfo(*img, pimg::Rect(r.xmin, r.ymin, r.xmax, r.ymax), val, alloc);

		op_log->Insert(relative_path, val, alloc);
	}

	return true;
}

}

namespace crop
{

void CropBorder(const std::string& src_path, const std::string& dst_path)
{
	auto modify_time_filepath = boost::filesystem::absolute(TIME_FILEPATH, dst_path);
	auto cfg_filepath = boost::filesystem::absolute(LOG_FILEPATH, dst_path);
	tc::Application app(modify_time_filepath.string(), cfg_filepath.string());
	app.Do(src_path, dst_path, CropSingle, CropMulti, sx::RES_FILE_IMAGE);
}

}