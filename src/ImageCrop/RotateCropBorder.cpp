#include "Application.h"
#include "OpLog.h"

#include <SM_Vector.h>
#include <SM_MinBoundingBox.h>
#include <SM_Calc.h>
#include <pimg/OutlineRaw.h>
#include <pimg/ImageData.h>
#include <painting2/DrawRT.h>
#include <sx/ResFileHelper.h>
#include <ns/NodeFactory.h>
#include <node2/RenderSystem.h>
#include <facade/ResPool.h>

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

bool Crop(const std::string& src_filepath, const std::string& dst_filepath,
	      sm::vec2& center, float& angle)
{
	if (sx::ResFileHelper::Type(src_filepath.c_str()) != sx::RES_FILE_IMAGE) {
		return false;
	}

	static const bool PRE_MUL_ALPHA(false);
	auto img = facade::ResPool::Instance().Fetch<pimg::ImageData>(src_filepath, PRE_MUL_ALPHA);

	int width, height;
	bool success = GetRotateTrimInfo(img->GetPixelData(), img->GetWidth(),
		img->GetHeight(), width, height, center, angle);
	if (!success || angle == 0) {
		return false;
	}

	auto casset = ns::NodeFactory::CreateAssetComp(src_filepath);
	if (!casset) {
		return false;
	}

	boost::filesystem::create_directory(
		boost::filesystem::path(dst_filepath).parent_path());

	sm::Matrix2D trans;
	trans.SetTransformation(center.x, center.y, angle, 1, 1, 0, 0, 0, 0);

	pt2::DrawRT rt;
	rt.Draw<n0::CompAsset>(*casset, [&](const n0::CompAsset& casset, const sm::Matrix2D& mt) {
		n2::RenderSystem::Draw(casset, trans * mt);
	}, true);
	rt.StoreToFile(dst_filepath.c_str(), width, height);

	return true;
}

bool CropSingle(const std::string& src_filepath, const std::string& dst_filepath)
{
	sm::vec2 center;
	float angle;
	return Crop(src_filepath, dst_filepath, center, angle);
}

bool CropMulti(const std::string& filepath, const std::string& src_dir, const std::string& dst_filepath,
	           std::unique_ptr<tc::OpLog>& op_log, rapidjson::MemoryPoolAllocator<>& alloc)
{
	sm::vec2 center;
	float angle;
	if (!Crop(filepath, dst_filepath, center, angle)) {
		return false;
	}

	if (op_log)
	{
		rapidjson::Value val;
		val.SetObject();

		auto relative_path = boost::filesystem::relative(filepath, src_dir);
		val.AddMember("filepath", rapidjson::Value(relative_path.string().c_str(), alloc), alloc);

		val.AddMember("center_x", center.x, alloc);
		val.AddMember("center_y", center.y, alloc);

		val.AddMember("angle", angle, alloc);

		op_log->Insert(relative_path.string(), val, alloc);
	}

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

	auto modify_time_filepath = boost::filesystem::absolute(TIME_FILEPATH, dst_path);
	auto cfg_filepath = boost::filesystem::absolute(LOG_FILEPATH, dst_path);
	tc::Application app(modify_time_filepath.string(), cfg_filepath.string());
	app.Do(src_path, dst_path, CropSingle, CropMulti, sx::RES_FILE_IMAGE);
}

}