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

#include <rapidjson/document.h>

#include <boost/filesystem.hpp>

#include <string>
#include <map>
#include <memory>
#include <iostream>
#include <fstream>

namespace
{

static const char* OUTPUT_FILE = "crop-border.json";

class JsonConfig
{
public:
	void LoadFromFile(const std::string& filepath)
	{
		js::RapidJsonHelper::ReadFromFile(filepath.c_str(), m_src_doc);
		for (auto& val : m_src_doc.GetArray())
		{
			std::string filepath = val["filepath"].GetString();
			uint64_t timestamp = std::stoi(val["timestamp"].GetString());
			auto item = std::make_unique<Item>(val, timestamp, false, m_src_doc.GetAllocator());
			m_map_items.insert(std::make_pair(filepath, std::move(item)));
		}
	}

	void Insert(const std::string& filepath, rapidjson::Value& val, uint64_t timestamp,
		        rapidjson::MemoryPoolAllocator<>& alloc)
	{
		auto itr = m_map_items.find(filepath);
		if (itr != m_map_items.end()) 
		{
			GD_ASSERT(itr->second->timestamp != timestamp, "err timestamp");
			itr->second->timestamp = timestamp;
			itr->second->val.Swap(val);
		} 
		else 
		{
			auto item = std::make_unique<Item>(val, timestamp, true, alloc);
			m_map_items.insert(std::make_pair(filepath, std::move(item)));
		}
	}

	void OutputToFile(const std::string& filepath, const std::string& dst_dir) const
	{
		rapidjson::Document dst_doc;
		dst_doc.SetArray();

		auto itr = m_map_items.begin();
		for (int i = 0; itr != m_map_items.end(); ++itr) 
		{
			if (itr->second->used) {
				dst_doc.PushBack(itr->second->val, dst_doc.GetAllocator());
			} else {
				boost::filesystem::remove(
					boost::filesystem::absolute(itr->first, dst_dir));
			}
		}

		js::RapidJsonHelper::WriteToFile(filepath.c_str(), dst_doc);
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
		Item(rapidjson::Value& val, uint64_t timestamp, bool used, 
			 rapidjson::MemoryPoolAllocator<>& alloc)
			: timestamp(timestamp), used(used) 
		{
			this->val.Swap(val);
		}

		rapidjson::Value val;
		uint64_t         timestamp;

		mutable bool used;
	};

private:
	rapidjson::Document m_src_doc;

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

void Crop(const std::string& src_filepath, const std::string& dst_filepath)
{
	static const bool PRE_MUL_ALPHA(false);
	auto img = gum::ResPool::Instance().Fetch<pimg::ImageData>(src_filepath, PRE_MUL_ALPHA);

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

	boost::filesystem::create_directory(boost::filesystem::path(dst_filepath).parent_path());
	if (condense) {
		gimg_export(dst_filepath.c_str(), condense, r.Width(), r.Height(), img->GetFormat(), true);
		delete[] condense;
	} else {
		gimg_export(dst_filepath.c_str(), img->GetPixelData(), img->GetWidth(), img->GetHeight(), img->GetFormat(), true);
	}
}

void Crop(const std::string& filepath, const std::string& src_dir, const std::string& dst_filepath,
	      JsonConfig& cfg, rapidjson::MemoryPoolAllocator<>& alloc)
{
	static const bool PRE_MUL_ALPHA(false);
	auto img = gum::ResPool::Instance().Fetch<pimg::ImageData>(filepath, PRE_MUL_ALPHA);

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

	boost::filesystem::create_directory(boost::filesystem::path(dst_filepath).parent_path());
	if (condense) {
		gimg_export(dst_filepath.c_str(), condense, r.Width(), r.Height(), img->GetFormat(), true);
		delete[] condense;
	} else {
		gimg_export(dst_filepath.c_str(), img->GetPixelData(), img->GetWidth(), img->GetHeight(), img->GetFormat(), true);
	}
}

}

namespace crop
{

void CropBorder(const std::string& src_path, const std::string& dst_path)
{
	if (boost::filesystem::is_directory(src_path))
	{
		JsonConfig cfg;

		boost::filesystem::create_directory(dst_path);
		auto out_json_filepath = boost::filesystem::absolute(OUTPUT_FILE, dst_path);
		if (boost::filesystem::exists(out_json_filepath)) {
			cfg.LoadFromFile(out_json_filepath.string());
		}

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
			uint64_t img_ori_time = cfg.QueryTime(relative_path.string()),
					 img_new_time = boost::filesystem::last_write_time(filepath);
			if (img_new_time != img_ori_time) {
				auto dst_filepath = boost::filesystem::absolute(relative_path, dst_path);
				Crop(filepath, src_path, dst_filepath.string(), cfg, alloc);
			}

			++itr;
		}

		cfg.OutputToFile(out_json_filepath.string(), dst_path);
	}
	else
	{
		Crop(src_path, dst_path);
	}
}

}