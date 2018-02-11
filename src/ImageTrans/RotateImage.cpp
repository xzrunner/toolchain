#include <sprite2/SymType.h>
#include <sprite2/Sprite.h>
#include <sprite2/DrawRT.h>
#include <s2loader/SymbolFile.h>
#include <s2loader/SpriteFactory.h>
#include <pimg/ImageData.h>
#include <sm_const.h>
#include <SM_Calc.h>
#include <gum/ResPool.h>

#include <boost/filesystem.hpp>

#include <string>

namespace
{

bool Rotate(const std::string& src_path, const std::string& dst_path, float angle)
{
	if (s2loader::SymbolFile::Instance()->Type(src_path.c_str()) != s2::SYM_IMAGE) {
		return false;
	}

	static const bool PRE_MUL_ALPHA(false);
	auto img = gum::ResPool::Instance().Fetch<pimg::ImageData>(src_path, PRE_MUL_ALPHA);
	float hw = img->GetWidth() * 0.5f;
	float hh = img->GetHeight() * 0.5f;

	float rad = angle * SM_DEG_TO_RAD;
	int w = static_cast<int>(std::ceil(sm::rotate_vector(sm::vec2(hw, hh), -rad).x * 2));
	int h = static_cast<int>(std::ceil(sm::rotate_vector(sm::vec2(-hw, hh), -rad).x * 2));

	auto spr = s2loader::SpriteFactory::Instance()->Create(src_path.c_str());
	spr->SetAngle(rad);

	s2::DrawRT rt;
	rt.Draw(*spr, true, w, h);
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