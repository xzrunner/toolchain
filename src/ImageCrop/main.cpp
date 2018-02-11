#include <string>
#include <iostream>

//#pragma comment(lib,"user32.lib")
//#pragma comment(lib,"gdi32.lib")
#pragma comment(lib,"Shell32")

namespace crop
{

extern void CropBorder(const std::string& src_path, const std::string& dst_path);
extern void RotateCropBorder(const std::string& src_path, const std::string& dst_path);
extern void StaticCrop(const std::string& src_path, const std::string& dst_path,
	int xmin, int ymin, int xmax, int ymax);

}

int main(int argc, char* argv[])
{
	if (argc < 4)
	{
		std::cout << "Usage: ImageCrop border <src path> <dst path>" << std::endl;
		std::cout << "Usage: ImageCrop border-rotate <src path> <dst path>" << std::endl;
		std::cout << "Usage: ImageCrop static <src path> <dst path> <xmin> <ymin> <xmax> <ymax>" << std::endl;
		std::cout << "Usage: ImageCrop static-grids <src path> <dst path> <min width> <min height>" << std::endl;
		std::cout << "Usage: ImageCrop auto-grids <src path> <dst path>" << std::endl;
		return 1;
	}

	const std::string op_str = argv[1];
	if (op_str == "border")
	{
		crop::CropBorder(argv[2], argv[3]);
	}
	else if (op_str == "border-rotate")
	{
		crop::RotateCropBorder(argv[2], argv[3]);
	} 
	else if (op_str == "static")
	{
		int xmin = std::stoi(argv[4]);
		int ymin = std::stoi(argv[5]);
		int xmax = std::stoi(argv[6]);
		int ymax = std::stoi(argv[7]);
		crop::StaticCrop(argv[2], argv[3], xmin, ymin, xmax, ymax);
	}
	else if (op_str == "static-grids")
	{
		
	}
	else if (op_str == "auto-grids")
	{

	}

	return 0;
}