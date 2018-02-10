#include <string>
#include <iostream>

namespace conv
{
	extern void ToETC1(const std::string& src_path, const std::string& dst_path);
	extern void ToETC2(const std::string& src_path, const std::string& dst_path);
	extern void ToPVR(const std::string& src_path, const std::string& dst_path);
}

int main(int argc, char* argv[])
{
	if (argc < 4)
	{
		std::cout << "Usage: ImageConv etc1 <src path> <dst path>" << std::endl;
		std::cout << "Usage: ImageConv etc2 <src path> <dst path>" << std::endl;
		std::cout << "Usage: ImageConv pvr <src path> <dst path>" << std::endl;
		return 1;
	}

	const std::string op_str = argv[1];
	if (op_str == "etc1")
	{
		conv::ToETC1(argv[2], argv[3]);
	}
	else if (op_str == "etc2")
	{
		conv::ToETC2(argv[2], argv[3]);
	} 
	else if (op_str == "pvr")
	{
		conv::ToPVR(argv[2], argv[3]);
	}

	return 0;
}