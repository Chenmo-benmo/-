#pragma once

#include <iostream>
#include <Windows.h>
#include <string>
#include <process.h>
#include <sstream>
#include <vector>

#include "MySerial.h"

struct XYFMessage		//注意是string类型
{
	std::string X;
	std::string Y;
	std::string T;
	std::string F;
	std::string c;
	std::string In;
	std::string Out;
};

class KPort
{
public:
	KPort(int portNo);										//初始化（打开）串口，输入串口号
	~KPort();

	void RelativeMove(float X, float Y, int Speed);			//相对运动
	void CoordMove(float X, float Y, int Speed);			//坐标运动
	void SuspendMove();										//暂停
	void ContinueMove();									//继续

	XYFMessage GetXYF();									//获取运动信息

private:
	std::unique_ptr<MySerial> myserial;
	unsigned char* temp;									//存储指令的指针
};

