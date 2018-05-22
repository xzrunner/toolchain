#include <pimg/ImageData.h>
#include <sm_const.h>
#include <SM_Calc.h>
#include <pimg/ImageData.h>
#include <unirender/Blackboard.h>
#include <painting2/DrawRT.h>
#include <node2/RenderSystem.h>
#include <sx/ResFileHelper.h>
#include <ns/NodeFactory.h>
#include <facade/ResPool.h>

#include <boost/filesystem.hpp>

#include <string>

namespace
{

bool Rotate(const std::string& src_path, const std::string& dst_path, float angle)
{
	if (sx::ResFileHelper::Type(src_path.c_str()) != sx::RES_FILE_IMAGE) {
		return false;
	}

	static const bool PRE_MUL_ALPHA(false);
	auto img = facade::ResPool::Instance().Fetch<pimg::ImageData>(src_path, PRE_MUL_ALPHA);
	float hw = img->GetWidth() * 0.5f;
	float hh = img->GetHeight() * 0.5f;

	float rad = angle * SM_DEG_TO_RAD;
	int w = static_cast<int>(std::ceil(sm::rotate_vector(sm::vec2(hw, hh), -rad).x * 2));
	int h = static_cast<int>(std::ceil(sm::rotate_vector(sm::vec2(-hw, hh), -rad).x * 2));

	auto casset = ns::NodeFactory::CreateAssetComp(src_path);
	if (!casset) {
		return false;
	}

	sm::Matrix2D trans;
	trans.SetTransformation(0, 0, rad, 1, 1, 0, 0, 0, 0);

	pt2::DrawRT rt;
	rt.Draw<n0::CompAsset>(*casset, [&](const n0::CompAsset& casset, const sm::Matrix2D& mt) {
		n2::RenderSystem::Draw(casset, trans * mt);
	}, true);
	rt.StoreToFile(dst_path.c_str(), w, h);

	return true;
}

}

namespace trans
{

void RotateImage(const std::string& src_path, const std::string& dst_path, float angle)
{
	if (boost::filesystem::is_directory(src_path))
	{
		boost::filesystem::recursive_directory_iterator itr(src_path), end;
		while (itr != end)
		{
			std::string src_filepath = itr->path().string();
			auto relative_path = boost::filesystem::relative(src_filepath, src_path);
			auto dst_filepath = boost::filesystem::absolute(relative_path, dst_path);
			Rotate(src_filepath, dst_filepath.string(), angle);
			++itr;
		}
	}
	else
	{
		Rotate(src_path, dst_path, angle);
	}
}

}