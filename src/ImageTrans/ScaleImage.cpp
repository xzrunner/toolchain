#include <SM_Vector.h>
#include <sprite2/SymType.h>
#include <sprite2/Sprite.h>
#include <sprite2/DrawRT.h>
#include <s2loader/SymbolFile.h>
#include <s2loader/SpriteFactory.h>
#include <pimg/ImageData.h>
#include <gum/ResPool.h>

#include <boost/filesystem.hpp>

#include <string>

namespace
{

bool Scale(const std::string& src_path, const std::string& dst_path, float scale)
{
	if (s2loader::SymbolFile::Instance()->Type(src_path.c_str()) != s2::SYM_IMAGE) {
		return false;
	}

	static const bool PRE_MUL_ALPHA(false);
	auto img = gum::ResPool::Instance().Fetch<pimg::ImageData>(src_path, PRE_MUL_ALPHA);
	int w = static_cast<int>(std::ceil(img->GetWidth() * scale));
	int h = static_cast<int>(std::ceil(img->GetHeight() * scale));

	auto spr = s2loader::SpriteFactory::Instance()->Create(src_path.c_str());
	sm::vec2 spr_scale;
	spr_scale.x = static_cast<float>(w) / img->GetWidth();
	spr_scale.y = static_cast<float>(h) / img->GetHeight();
	spr->SetScale(spr_scale);

	s2::DrawRT rt;
	rt.Draw(*spr, true, w, h);
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