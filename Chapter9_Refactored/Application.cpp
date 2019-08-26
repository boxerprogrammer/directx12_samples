#include "Application.h"



Application::Application()
{
}


Application::~Application()
{
}

Application& 
Application::Instance() {
	static Application instance;
	return instance;
}