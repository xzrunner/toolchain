#include <string>
#include <iostream>

namespace quake
{

extern void ExtractPak(const std::string& src_path, const std::string& dst_path);

}

namespace tc
{
extern bool InitRender();
extern bool InitSubmodule();
}

int main(int argc, char* argv[])
{
	if (argc < 4)
	{
		std::cout << "Usage: Quake extract-pack <src path> <dst path>" << std::endl;
		return 1;
	}

	const std::string op_str = argv[1];
	if (op_str == "extract-pack")
	{
		quake::ExtractPak(argv[2], argv[3]);
	}

	return 0;
}