#include <SM_Vector.h>
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

bool Scale(const std::string& src_path, const std::string& dst_path, float scale)
{
	if (sx::ResFileHelper::Type(src_path.c_str()) != sx::RES_FILE_IMAGE) {
		return false;
	}

	static const bool PRE_MUL_ALPHA(false);
	auto img = facade::ResPool::Instance().Fetch<pimg::ImageData>(src_path, PRE_MUL_ALPHA);
	int w = static_cast<int>(std::ceil(img->GetWidth() * scale));
	int h = static_cast<int>(std::ceil(img->GetHeight() * scale));

	auto casset = ns::NodeFactory::CreateAssetComp(src_path);
	if (!casset) {
		return false;
	}

	float sx = static_cast<float>(w) / img->GetWidth();
	float sy = static_cast<float>(h) / img->GetHeight();

	sm::Matrix2D trans;
	trans.SetTransformation(0, 0, 0, sx, sy, 0, 0, 0, 0);

	pt2::DrawRT rt;
	rt.Draw<n0::CompAsset>(*casset, [&](const n0::CompAsset& casset, const sm::Matrix2D& mt) {
		n2::RenderSystem::Draw(casset, trans * mt);
	}, true, sx, sy);
	rt.StoreToFile(dst_path.c_str(), w, h);

	return true;
}

}

namespace trans
{

void ScaleImage(const std::string& src_path, const std::string& dst_path, float scale)
{
	if (boost::filesystem::is_directory(src_path))
	{
		boost::filesystem::recursive_directory_iterator itr(src_path), end;
		while (itr != end)
		{
			std::string src_filepath = itr->path().string();
			auto relative_path = boost::filesystem::relative(src_filepath, src_path);
			auto dst_filepath = boost::filesystem::absolute(relative_path, dst_path);
			Scale(src_filepath, dst_filepath.string(), scale);
			++itr;
		}
	}
	else
	{
		Scale(src_path, dst_path, scale);
	}
}

}