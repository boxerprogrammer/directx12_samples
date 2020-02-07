#include<iostream>
#include"Application.h"

using namespace std;

//int main() {
int WINAPI WinMain(HINSTANCE ,HINSTANCE ,LPSTR,int){
	auto& app = Application::Instance();
	if (!app.Initialize())return -1;
	app.Run();
	app.Terminate();
	return 0;
}