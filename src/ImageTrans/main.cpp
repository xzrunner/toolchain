#include <string>
#include <iostream>

namespace trans
{

extern void RotateImage(const std::string& src_path, const std::string& dst_path, float angle);
extern void ScaleImage(const std::string& src_path, const std::string& dst_path, float scale);

}

int main(int argc, char* argv[])
{
	if (argc < 5)
	{
		std::cout << "Usage: ImageTrans rotate <src path> <dst path> <angle>" << std::endl;
		std::cout << "Usage: ImageTrans scale <src path> <dst path> <scale>" << std::endl;
		return 1;
	}

	const std::string op_str = argv[1];
	if (op_str == "rotate")
	{
		trans::RotateImage(argv[2], argv[3], std::stof(argv[4]));
	}
	else if (op_str == "scale")
	{
		trans::ScaleImage(argv[2], argv[3], std::stof(argv[4]));
	}

	return 0;
}