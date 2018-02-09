#include <guard/check.h>
#include <gimg_typedef.h>
#include <gimg_export.h>
#include <pimg/Rect.h>
#include <pimg/ImageData.h>
#include <pimg/Condense.h>
#include <sprite2/SymType.h>
#include <s2loader/SymbolFile.h>

#include <gum/ResPool.h>
#include <gum/Config.h>

#include <json/json.h>

#include <boost/filesystem.hpp>

#include <string>
#include <map>
#include <memory>
#include <iostream>
#include <fstream>

FILE _iob[] = { *stdin, *stdout, *stderr };
extern "C" FILE * __cdecl __iob_func(void) { return _iob; }

namespace
{

static const char* OUTPUT_FILE = "trim";

class JsonConfig
{
public:	
	void LoadFromFile(const std::string& filepath)
	{
		Json::Value value;
		Json::Reader reader;
		std::locale::global(std::locale(""));
		std::ifstream fin(filepath.c_str());
		std::locale::global(std::locale("C"));
		reader.parse(fin, value);
		fin.close();

		int idx = 0;
		Json::Value val = value[idx++];
		while (!val.isNull()) 
		{
			std::string filepath = val["filepath"].asString();
			auto item = std::make_unique<Item>();
			item->val  = val;
			item->time = std::stoi(val["time"].asString());
			m_map_items.insert(std::make_pair(filepath, std::move(item)));
			val = value[idx++];
		}
	}

	void Insert(const std::string& filepath, const Json::Value& val, int64_t time)
	{
		auto itr = m_map_items.find(filepath);
		if (itr != m_map_items.end()) 
		{
			GD_ASSERT(itr->second->time != time, "err time");
			itr->second->time = time;
			itr->second->val  = val;
		} 
		else 
		{
			Item* item = new Item;
			item->val = val;
			item->time = time;
			item->used = true;
			m_map_items.insert(std::make_pair(filepath, item));
		}
	}

	void OutputToFile(const std::string& filepath, const std::string& dst_dir) const
	{
		Json::Value value;
		auto itr = m_map_items.begin();
		for (int i = 0; itr != m_map_items.end(); ++itr) 
		{
			if (itr->second->used) {
				value[i++] = itr->second->val;
			} else {
				boost::filesystem::remove(dst_dir + "\\" + itr->first);
			}
		}

		Json::StyledStreamWriter writer;
		std::locale::global(std::locale(""));
		std::ofstream fout(filepath.c_str());
		std::locale::global(std::locale("C"));	
		writer.write(fout, value);
		fout.close();
	}

	int64_t QueryTime(const std::string& filepath) const
	{
		auto itr = m_map_items.find(filepath);
		if (itr != m_map_items.end()) {
			itr->second->used = true;
			return itr->second->time;
		} else {
			return 0;
		}
	}

private:
	struct Item
	{
		Item() : time(0), used(false) {}

		Json::Value val;
		int64_t time;

		mutable bool used;
	};

private:
	std::map<std::string, std::unique_ptr<Item>> m_map_items;

}; // JsonConfig

bool IsTransparent(const pimg::ImageData& img, int x, int y)
{
	if (img.GetFormat() != GPF_RGBA8) {
		return false;
	} else {
		return img.GetPixelData()[(img.GetWidth() * y + x) * 4 + 3] == 0;
	}
}

void StoreBoundInfo(const pimg::ImageData& img, const pimg::Rect& r, Json::Value& val)
{
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
		val["bound"]["left"]["tot"] = tot;
		val["bound"]["left"]["max"] = max;
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
		val["bound"]["right"]["tot"] = tot;
		val["bound"]["right"]["max"] = max;
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
		val["bound"]["bottom"]["tot"] = tot;
		val["bound"]["bottom"]["max"] = max;
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
		val["bound"]["up"]["tot"] = tot;
		val["bound"]["up"]["max"] = max;
	}
}

void Trim(const std::string& filepath, const std::string& src_dir, const std::string& dst_dir, JsonConfig& cfg)
{
	auto& img = gum::ResPool::Instance().Fetch<pimg::ImageData>(filepath, gum::Config::Instance()->GetPreMulAlpha());

	uint8_t* condense = NULL;
	pimg::Rect r;
	if (img.GetFormat() == GPF_RGBA8)
	{
		pimg::Condense cd(img.GetPixelData(), img.GetWidth(), img.GetHeight());
		condense = cd.GetPixels(r);
	}
	if (!condense) {
		r.xmin = r.ymin = 0;
		r.xmax = img.GetWidth();
		r.ymax = img.GetHeight();
	}

	// save info
	Json::Value spr_val;
	std::string relative_path = boost::filesystem::relative(filepath, src_dir).string();
	spr_val["filepath"] = relative_path;
	spr_val["source size"]["w"] = img.GetWidth();
	spr_val["source size"]["h"] = img.GetHeight();
	spr_val["position"]["x"] = r.xmin;
	spr_val["position"]["y"] = img.GetHeight() - r.ymax;
	spr_val["position"]["w"] = r.Width();
	spr_val["position"]["h"] = r.Height();
	int64_t time = boost::filesystem::last_write_time(filepath);
	spr_val["time"] = std::to_string(time);
	StoreBoundInfo(img, pimg::Rect(r.xmin, r.ymin, r.xmax, r.ymax), spr_val);
	cfg.Insert(relative_path, spr_val, time);

	std::string out_filepath = dst_dir + "\\" + relative_path,
		out_dir = boost::filesystem::path(out_filepath).parent_path().string();
	boost::filesystem::create_directory(out_dir);

	if (condense) {
		gimg_export(out_filepath.c_str(), condense, r.Width(), r.Height(), img.GetFormat(), true);
		delete[] condense;
	} else {
		gimg_export(out_filepath.c_str(), img.GetPixelData(), img.GetWidth(), img.GetHeight(), img.GetFormat(), true);
	}
}

void ImageCropBorder(const std::string& src_dir, const std::string& dst_dir)
{
	JsonConfig cfg;

	boost::filesystem::create_directory(dst_dir);
	std::string out_json_filepath = dst_dir + "\\" + OUTPUT_FILE + ".json";
	if (boost::filesystem::exists(out_json_filepath)) {
		cfg.LoadFromFile(out_json_filepath);
	}

	boost::filesystem::recursive_directory_iterator itr(src_dir), end;
	while (itr != end) 
	{
		std::string filepath = itr->path().string();

		if (s2loader::SymbolFile::Instance()->Type(filepath.c_str()) != s2::SYM_IMAGE) {
			continue;
		}

		int64_t img_ori_time = cfg.QueryTime(boost::filesystem::relative(filepath, src_dir).string()),
			    img_new_time = boost::filesystem::last_write_time(filepath);

		if (img_new_time != img_ori_time) {
			Trim(filepath, src_dir, dst_dir, cfg);
		}		

		++itr;
	}

	cfg.OutputToFile(out_json_filepath, dst_dir);
}

}

int main(int argc, char* argv[])
{
	if (argc < 3)
	{
		std::cout << "Usage: ImageCropBorder <src dir> <dst dir>" << std::endl;
		return 1;
	}

	gum::Config* cfg = gum::Config::Instance();
	bool old = cfg->GetPreMulAlpha();
	cfg->SetPreMulAlpha(false);
	ImageCropBorder(argv[2], argv[3]);
	cfg->SetPreMulAlpha(old);

	return 0;
}