#include <boost/filesystem.hpp>

#include <string>
#include <iostream>
#include <fstream>

#include <assert.h>

namespace
{

struct PakHeader
{
	char id[4];
	int dir_offset;
	int dir_length;
};

struct PakEntry
{
	char name[56];
	int offset;
	int length;
};

}

namespace quake
{

void ExtractPak(const std::string& src_path, const std::string& dst_path)
{
	assert(boost::filesystem::is_regular_file(src_path));

	std::ifstream fin(src_path.c_str(), std::ios::binary);
	if (fin.fail()) {
		return;
	}

	PakHeader header;

	fin.read(reinterpret_cast<char*>(&header), sizeof(header));
	if (strncmp(header.id, "PACK", 4) != 0) {
		std::cerr << "invalid header id\n";
		return;
    }

	if (header.dir_length % sizeof(PakEntry)) {
		std::cerr << "invalid header length\n";
		return;
	}

	char* buf = new char[header.dir_offset];

	int num = header.dir_length / sizeof(PakEntry);
	PakEntry* entries = new PakEntry[num];
	fin.seekg(header.dir_offset);
	fin.read(reinterpret_cast<char*>(entries), header.dir_length);
	for (int i = 0; i < num; ++i)
	{
		auto& entry = entries[i];

		auto filepath = boost::filesystem::absolute(entry.name, dst_path);
		boost::filesystem::create_directories(filepath.parent_path());

		std::ofstream fout(filepath.c_str(), std::ios::binary);
		fin.seekg(entry.offset);
		fin.read(buf, entry.length);
		fout.write(buf, entry.length);
		fout.close();
	}

	delete[] entries;
	delete[] buf;

	fin.close();
}

}