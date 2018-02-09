#include <guard/check.h>
#include <gimg_typedef.h>
#include <gimg_export.h>
#include <pimg/Rect.h>
#include <pimg/ImageData.h>
#include <pimg/Condense.h>
#include <sprite2/SymType.h>
#include <s2loader/SymbolFile.h>
#include <js/RapidJsonHelper.h>

#include <gum/ResPool.h>
#include <gum/Config.h>

#include <rapidjson/document.h>

#include <boost/filesystem.hpp>

#include <string>
#include <map>
#include <memory>
#include <iostream>
#include <fstream>

namespace
{

static const char* OUTPUT_FILE = "trim";

class JsonConfig
{
public:
	void LoadFromFile(const std::string& filepath)
	{
		rapidjson::Document doc;
		js::RapidJsonHelper::ReadFromFile(filepath.c_str(), doc);
		for (auto& val : doc.GetArray())
		{
			std::string filepath = val["filepath"].GetString();
			uint64_t timestamp = std::stoi(val["timestamp"].GetString());
			auto item = std::make_unique<Item>(val, timestamp, false, doc.GetAllocator());
			m_map_items.insert(std::make_pair(filepath, std::move(item)));
		}
	}

	void Insert(const std::string& filepath, const rapidjson::Value& val, uint64_t timestamp,
		        rapidjson::MemoryPoolAllocator<>& alloc)
	{
		auto itr = m_map_items.find(filepath);
		if (itr != m_map_items.end()) 
		{
			GD_ASSERT(itr->second->timestamp != timestamp, "err timestamp");
			itr->second->timestamp = timestamp;
			itr->second->val ;
		} 
		else 
		{
			auto item = std::make_unique<Item>(val, timestamp, true, alloc);
			m_map_items.insert(std::make_pair(filepath, std::move(item)));
		}
	}

	void OutputToFile(const std::string& filepath, const std::string& dst_dir) const
	{
		rapidjson::Document doc;
		doc.SetArray();

		auto itr = m_map_items.begin();
		for (int i = 0; itr != m_map_items.end(); ++itr) 
		{
			if (itr->second->used) {
				doc.PushBack(itr->second->val, doc.GetAllocator());
			} else {
				boost::filesystem::remove(dst_dir + "\\" + itr->first);
			}
		}

		js::RapidJsonHelper::WriteToFile(filepath.c_str(), doc);
	}

	uint64_t QueryTime(const std::string& filepath) const
	{
		auto itr = m_map_items.find(filepath);
		if (itr != m_map_items.end()) {
			itr->second->used = true;
			return itr->second->timestamp;
		} else {
			return 0;
		}
	}

private:
	struct Item
	{
		Item(const rapidjson::Value& val, uint64_t timestamp, bool used, 
			 rapidjson::MemoryPoolAllocator<>& alloc)
			: val(val, alloc), timestamp(timestamp), used(used) {}

		rapidjson::Value val;
		uint64_t         timestamp;

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

void Trim(const std::string& filepath, const std::string& src_dir, const std::string& dst_dir, JsonConfig& cfg)
{
	auto img = gum::ResPool::Instance().Fetch<pimg::ImageData>(filepath, gum::Config::Instance()->GetPreMulAlpha());

	uint8_t* condense = nullptr;
	pimg::Rect r;
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

	rapidjson::MemoryPoolAllocator<> alloc;

	// save info
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

	uint64_t timestamp = boost::filesystem::last_write_time(filepath);
	val.AddMember("timestamp", rapidjson::Value(std::to_string(timestamp).c_str(), alloc), alloc);
	StoreBoundInfo(*img, pimg::Rect(r.xmin, r.ymin, r.xmax, r.ymax), val, alloc);

	cfg.Insert(relative_path, val, timestamp, alloc);

	std::string out_filepath = dst_dir + "\\" + relative_path,
		out_dir = boost::filesystem::path(out_filepath).parent_path().string();
	boost::filesystem::create_directory(out_dir);

	if (condense) {
		gimg_export(out_filepath.c_str(), condense, r.Width(), r.Height(), img->GetFormat(), true);
		delete[] condense;
	} else {
		gimg_export(out_filepath.c_str(), img->GetPixelData(), img->GetWidth(), img->GetHeight(), img->GetFormat(), true);
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

		uint64_t img_ori_time = cfg.QueryTime(boost::filesystem::relative(filepath, src_dir).string()),
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
	ImageCropBorder(argv[1], argv[2]);
	cfg->SetPreMulAlpha(old);

	return 0;
}