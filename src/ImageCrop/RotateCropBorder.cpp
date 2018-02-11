#include "ModifyTime.h"
#include "OpLog.h"

#include <SM_Vector.h>
#include <SM_MinBoundingBox.h>
#include <SM_Calc.h>
#include <pimg/OutlineRaw.h>
#include <pimg/ImageData.h>
#include <s2loader/SymbolFile.h>
#include <s2loader/SpriteFactory.h>
#include <sprite2/SymType.h>
#include <sprite2/Sprite.h>
#include <sprite2/DrawRT.h>
#include <gum/ResPool.h>

#include <boost/filesystem.hpp>

#include <string>
#include <fstream>
#include <iostream>

namespace
{

static const char* TIME_FILEPATH = "rotate_crop_border_time.json";
static const char* LOG_FILEPATH  = "rotate_crop_border_log.json";

bool GetRotateTrimInfo(const uint8_t* pixels, int img_w, int img_h, 
	                   int& width, int& height, sm::vec2& center, float& angle)
{
	pimg::OutlineRaw raw(pixels, img_w, img_h);
	raw.CreateBorderLineAndMerge();
	if (raw.GetBorderLine().empty()) {
		return false;
	}
	raw.CreateBorderConvexHull();

	sm::vec2 bound[4];
	bool is_rotate = sm::MinBoundingBox::Do(raw.GetConvexHull(), bound);

	center = (bound[0] + bound[2]) * 0.5f;
	center.x -= img_w * 0.5f;
	center.y -= img_h * 0.5f;

	center = -center;

	if (is_rotate) {
		float left = FLT_MAX;
		int left_idx;
		for (int i = 0; i < 4; ++i) {
			if (bound[i].x < left) {
				left = bound[i].x;
				left_idx = i;
			}
		}

		const sm::vec2& s = bound[left_idx];
		const sm::vec2& e = bound[left_idx == 3 ? 0 : left_idx + 1];
		sm::vec2 right = s;
		right.x += 1;
		angle = -sm::get_angle(s, e, right);
		center = sm::rotate_vector(center, angle);

		width  = static_cast<int>(std::ceil(sm::dis_pos_to_pos(s, e)));
		height = static_cast<int>(std::ceil(sm::dis_pos_to_pos(e, bound[(left_idx+2)%4])));
	} else {
		angle = 0;
	}

	return true;
}

bool Crop(const std::string& src_filepath, const std::string& dst_filepath)
{
	static const bool PRE_MUL_ALPHA(false);
	auto img = gum::ResPool::Instance().Fetch<pimg::ImageData>(src_filepath, PRE_MUL_ALPHA);

	int width, height;
	sm::vec2 center;
	float angle;
	bool success = GetRotateTrimInfo(img->GetPixelData(), img->GetWidth(),
		img->GetHeight(), width, height, center, angle);
	if (!success || angle == 0) {
		return false;
	}

	auto spr = s2loader::SpriteFactory::Instance()->Create(src_filepath.c_str());
	spr->SetPosition(center);
	spr->SetAngle(angle);

	boost::filesystem::create_directory(
		boost::filesystem::path(dst_filepath).parent_path());

	s2::DrawRT rt;
	rt.Draw(*spr, true, width, height);
	rt.StoreToFile(dst_filepath.c_str(), width, height);

	return true;
}

bool Crop(const std::string& filepath, const std::string& src_dir, 
	      const std::string& dst_filepath, tc::OpLog& log, rapidjson::MemoryPoolAllocator<>& alloc)
{
	if (s2loader::SymbolFile::Instance()->Type(filepath.c_str()) != s2::SYM_IMAGE) {
		return false;
	}

	static const bool PRE_MUL_ALPHA(false);
	auto img = gum::ResPool::Instance().Fetch<pimg::ImageData>(filepath, PRE_MUL_ALPHA);

	int width, height;
	sm::vec2 center;
	float angle;
	bool success = GetRotateTrimInfo(img->GetPixelData(), img->GetWidth(),
		img->GetHeight(), width, height, center, angle);
	if (!success || angle == 0) {
		return false;
	}

	auto spr = s2loader::SpriteFactory::Instance()->Create(filepath.c_str());
	spr->SetPosition(center);
	spr->SetAngle(angle);

	boost::filesystem::create_directory(
		boost::filesystem::path(dst_filepath).parent_path());

	s2::DrawRT rt;
	rt.Draw(*spr, true, width, height);
	rt.StoreToFile(dst_filepath.c_str(), width, height);

	// save log
	rapidjson::Value val;
	val.SetObject();

	auto relative_path = boost::filesystem::relative(filepath, src_dir);
	val.AddMember("filepath", rapidjson::Value(relative_path.string().c_str(), alloc), alloc);

	val.AddMember("center_x", center.x, alloc);
	val.AddMember("center_y", center.y, alloc);

	val.AddMember("angle", angle, alloc);

	log.Insert(relative_path.string(), val, alloc);

	return true;
}

}

namespace tc
{

extern bool InitRender();

}

namespace crop
{

void RotateCropBorder(const std::string& src_path, const std::string& dst_path)
{
	tc::InitRender();

	if (!boost::filesystem::is_directory(src_path)) {
		Crop(src_path, dst_path);
		return;
	}

	boost::filesystem::create_directory(dst_path);

	tc::ModifyTime modify_time;
	auto modify_time_filepath = boost::filesystem::absolute(TIME_FILEPATH, dst_path);
	modify_time.LoadFromFile(modify_time_filepath.string());

	tc::OpLog log;
	auto cfg_filepath = boost::filesystem::absolute(LOG_FILEPATH, dst_path);
	log.LoadFromFile(cfg_filepath.string());

	rapidjson::MemoryPoolAllocator<> alloc;

	boost::filesystem::recursive_directory_iterator itr(src_path), end;
	while (itr != end)
	{
		std::string filepath = itr->path().string();
		if (s2loader::SymbolFile::Instance()->Type(filepath.c_str()) != s2::SYM_IMAGE) {
			++itr;
			continue;
		}

		auto relative_path = boost::filesystem::relative(filepath, src_path);

		log.SetUsed(relative_path.string());

		uint64_t old_time = modify_time.Query(relative_path.string()),
				 new_time = boost::filesystem::last_write_time(filepath);
		if (old_time == new_time) {
			++itr;
			continue;
		}

		auto dst_filepath = boost::filesystem::absolute(relative_path, dst_path);
		Crop(filepath, src_path, dst_filepath.string(), log, alloc);
		modify_time.Insert(relative_path.string(), new_time);

		++itr;
	}

	log.StoreToFile(cfg_filepath.string(), dst_path);

	modify_time.StoreToFile(modify_time_filepath.string());

}

}