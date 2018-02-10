#include <SM_Vector.h>
#include <SM_MinBoundingBox.h>
#include <SM_Calc.h>
#include <pimg/OutlineRaw.h>
#include <pimg/ImageData.h>
#include <s2loader/SymbolFile.h>
#include <s2loader/SpriteFactory.h>
#include <sprite2/SymType.h>
#include <sprite2/Sprite.h>
#include <sprite2/DrawRT.h>
#include <gum/ResPool.h>
#include <gum/RenderContext.h>

#include <gl/glew.h>
#include <glfw3.h>

#include <boost/filesystem.hpp>

#include <string>
#include <fstream>
#include <iostream>

namespace
{

static const char* OUTPUT_FILE = "rotate_crop";

void error_callback(int error, const char* description)
{
	fputs(description, stderr);
}

bool InitGL()
{
	glfwSetErrorCallback(error_callback);
	if (!glfwInit()) {
		exit(EXIT_FAILURE);
		return false;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWwindow* window = glfwCreateWindow(100, 100, "rotate-crop", nullptr, nullptr);
	if (!window)
	{
		glfwTerminate();
		return false;
	}
	glfwMakeContextCurrent(window);

	// Set this to true so GLEW knows to use a modern approach to retrieving function pointers and extensions
	glewExperimental = GL_TRUE;
	//// Initialize GLEW to setup the OpenGL Function pointers
	//if (glewInit() != GLEW_OK) {
	//	return -1;
	//}	

	return true;
}

void InitRender()
{
	if (!InitGL()) {
		return;
	}

	gum::RenderContext::Instance();

//	gum::ShaderLab::Instance()->Init();
	//{
	//	sl::ShaderMgr* mgr = sl::ShaderMgr::Instance();
	//	ur::RenderContext* rc = gum::RenderContext::Instance()->GetImpl();
	//	mgr->SetContext(rc);

	//	mgr->CreateShader(sl::SHAPE2, new sl::Shape2Shader(rc));
	//	mgr->CreateShader(sl::SHAPE3, new sl::Shape3Shader(rc));
	//	mgr->CreateShader(sl::SPRITE2, new sl::Sprite2Shader(rc));
	//	mgr->CreateShader(sl::SPRITE3, new sl::Sprite3Shader(rc));
	//	mgr->CreateShader(sl::BLEND, new sl::BlendShader(rc));
	//	mgr->CreateShader(sl::FILTER, new sl::FilterShader(rc));
	//	mgr->CreateShader(sl::MASK, new sl::MaskShader(rc));
	//	mgr->CreateShader(sl::MODEL3, new sl::Model3Shader(rc));
	//}

	//ee::DTex::Init();
	//ee::GTxt::Init();
}

bool GetRotateTrimInfo(const uint8_t* pixels, int img_w, int img_h, 
	                   int& width, int& height, sm::vec2& center, float& angle)
{
	pimg::OutlineRaw raw(pixels, img_w, img_h);
	raw.CreateBorderLineAndMerge();
	if (raw.GetBorderLine().empty()) {
		return false;
	}
	raw.CreateBorderConvexHull();

	sm::vec2 bound[4];
	bool is_rotate = sm::MinBoundingBox::Do(raw.GetConvexHull(), bound);

	center = (bound[0] + bound[2]) * 0.5f;
	center.x -= img_w * 0.5f;
	center.y -= img_h * 0.5f;

	center = -center;

	if (is_rotate) {
		float left = FLT_MAX;
		int left_idx;
		for (int i = 0; i < 4; ++i) {
			if (bound[i].x < left) {
				left = bound[i].x;
				left_idx = i;
			}
		}

		const sm::vec2& s = bound[left_idx];
		const sm::vec2& e = bound[left_idx == 3 ? 0 : left_idx + 1];
		sm::vec2 right = s;
		right.x += 1;
		angle = -sm::get_angle(s, e, right);
		center = sm::rotate_vector(center, angle);

		width = std::ceil(sm::dis_pos_to_pos(s, e));
		height = std::ceil(sm::dis_pos_to_pos(e, bound[(left_idx+2)%4]));
	} else {
		angle = 0;
	}

	return true;
}

bool Crop(const std::string& src_filepath, const std::string& dst_filepath)
{
	static const bool PRE_MUL_ALPHA(false);
	auto img = gum::ResPool::Instance().Fetch<pimg::ImageData>(src_filepath, PRE_MUL_ALPHA);

	int width, height;
	sm::vec2 center;
	float angle;
	bool success = GetRotateTrimInfo(img->GetPixelData(), img->GetWidth(),
		img->GetHeight(), width, height, center, angle);
	if (!success || angle == 0) {
		return false;
	}

	auto spr = s2loader::SpriteFactory::Instance()->Create(src_filepath.c_str());
	spr->SetPosition(center);
	spr->SetAngle(angle);

	boost::filesystem::create_directory(
		boost::filesystem::path(dst_filepath).parent_path());

	s2::DrawRT rt;
	rt.Draw(*spr, true, width, height);
	rt.StoreToFile(dst_filepath.c_str(), width, height);
}

bool Crop(const std::string& filepath, const std::string& src_dir, 
	      const std::string& dst_filepath, std::ofstream& fout)
{
	static const bool PRE_MUL_ALPHA(false);
	auto img = gum::ResPool::Instance().Fetch<pimg::ImageData>(filepath, PRE_MUL_ALPHA);

	int width, height;
	sm::vec2 center;
	float angle;
	bool success = GetRotateTrimInfo(img->GetPixelData(), img->GetWidth(),
		img->GetHeight(), width, height, center, angle);
	if (!success || angle == 0) {
		return false;
	}

	auto spr = s2loader::SpriteFactory::Instance()->Create(filepath.c_str());
	spr->SetPosition(center);
	spr->SetAngle(angle);

	boost::filesystem::create_directory(
		boost::filesystem::path(dst_filepath).parent_path());

	s2::DrawRT rt;
	rt.Draw(*spr, true, width, height);
	rt.StoreToFile(dst_filepath.c_str(), width, height);

	auto relative_path = boost::filesystem::relative(filepath, src_dir);
	fout << relative_path.string() << " " << center.x << " " << center.y << " " << angle << "\n";
}

}

namespace crop
{

void RotateCropBorder(const std::string& src_path, const std::string& dst_path)
{
	InitRender();

	if (boost::filesystem::is_directory(src_path))
	{
		auto output_file = boost::filesystem::absolute(OUTPUT_FILE, dst_path);
		std::locale::global(std::locale(""));	
		std::ofstream fout(output_file.c_str(), std::ios::binary);
		std::locale::global(std::locale("C"));	
		if (fout.fail()) {
			std::cout << "Can't open output file. \n";
			return;
		}

		boost::filesystem::recursive_directory_iterator itr(src_path), end;
		while (itr != end)
		{
			std::string filepath = itr->path().string();

			if (s2loader::SymbolFile::Instance()->Type(filepath.c_str()) != s2::SYM_IMAGE) {
				++itr;
				continue;
			}

			auto relative_path = boost::filesystem::relative(filepath, src_path);
			auto dst_filepath = boost::filesystem::absolute(relative_path, dst_path);
			Crop(filepath, src_path, dst_filepath.string(), fout);

			++itr;
		}

		fout.close();
	}
	else
	{
		Crop(src_path, dst_path);
	}

}

}