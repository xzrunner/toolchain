#include <string>
#include <iostream>

namespace packer
{

extern void PackNode(const std::string& src_dir, const std::string& dst_dir);

}

int main(int argc, char* argv[])
{
	if (argc < 4)
	{
		std::cout << "Usage: ResPacker pack-node <src path> <dst dir> <dst dir>" << std::endl;
		return 1;
	}

	const std::string op_str = argv[1];
	if (op_str == "pack-node")
	{
		packer::PackNode(argv[2], argv[3]);
	}

	return 0;
}