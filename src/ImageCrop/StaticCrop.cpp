#include <gimg_typedef.h>
#include <gimg_export.h>
#include <pimg/ImageData.h>
#include <pimg/Cropping.h>
#include <facade/ResPool.h>

#include <string>

namespace crop
{

void StaticCrop(const std::string& src_path, const std::string& dst_path,
	            int xmin, int ymin, int xmax, int ymax)
{
	static const bool PRE_MUL_ALPHA(false);
	auto img = facade::ResPool::Instance().Fetch<pimg::ImageData>(src_path, PRE_MUL_ALPHA);

	int hw = static_cast<int>(img->GetWidth() * 0.5f);
	int hh = static_cast<int>(img->GetHeight() * 0.5f);
	xmin += hw;
	xmax += hw;
	ymin += hh;
	ymax += hh;

	int channels = img->GetFormat() == GPF_RGB ? 3 : 4;
	pimg::Cropping crop(img->GetPixelData(), img->GetWidth(), img->GetHeight(), channels, true);

	const uint8_t* pixels = crop.Crop(xmin, ymin, xmax, ymax);
	gimg_export(dst_path.c_str(), pixels, xmax - xmin, ymax - ymin, GPF_RGBA8, true);
	delete[] pixels;
}

}