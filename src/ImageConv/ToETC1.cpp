#include <gimg_typedef.h>
#include <pimg/ImageData.h>
#include <pimg/TransToETC1.h>
#include <sx/ResFileHelper.h>
#include <facade/ResPool.h>

#include <boost/filesystem.hpp>

#include <string>

namespace
{

bool Conv(const std::string& src_path, const std::string& dst_path)
{
	if (sx::ResFileHelper::Type(src_path.c_str()) != sx::RES_FILE_IMAGE) {
		return false;
	}

	static const bool PRE_MUL_ALPHA(false);
	auto img = facade::ResPool::Instance().Fetch<pimg::ImageData>(src_path, PRE_MUL_ALPHA);

	int c = img->GetFormat() == GPF_RGB ? 3 : 4;
	pimg::TransToETC1 trans(img->GetPixelData(), img->GetWidth(), img->GetHeight(), c, false, true);
	trans.OutputFile(dst_path);

	return true;
}

}

namespace conv
{

void ToETC1(const std::string& src_path, const std::string& dst_path)
{
	if (boost::filesystem::is_directory(src_path))
	{
		boost::filesystem::recursive_directory_iterator itr(src_path), end;
		while (itr != end)
		{
			auto relative_path = boost::filesystem::relative(*itr, src_path);
			auto absolute_path = boost::filesystem::absolute(relative_path, dst_path);
			boost::filesystem::create_directory(absolute_path.parent_path());
			Conv(itr->path().string(), absolute_path.string());
			++itr;
		}
	}
	else
	{
		Conv(src_path, dst_path);
	}
}

}