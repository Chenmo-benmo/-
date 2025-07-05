#include "KPort.h"

std::string hexToString(const std::string& hexStr)
{//16进制转string
	if (hexStr.length() % 2 != 0) {
		throw std::invalid_argument("Hex string must have an even number of characters");
	}

	std::stringstream ss;
	for (size_t i = 0; i < hexStr.length(); i += 2) {
		std::string byteStr = hexStr.substr(i, 2);
		unsigned char byte = static_cast<unsigned char>(std::stoul(byteStr, nullptr, 16));
		ss << byte;
	}

	return ss.str();
}

KPort::KPort(int portNo)
	: myserial(std::make_unique<MySerial>()), temp(new unsigned char)
{
	if (!myserial->InitPort(portNo, CBR_115200, 'N', 8, 1))
		std::cout << "InitPort Fail !!!" << std::endl;
	else
		std::cout << "InitPort Success !!!" << std::endl;

	if (!myserial->OpenListenThread())
		std::cout << "OpenListenThread Fail !!!" << std::endl;
	else
		std::cout << "OpenListenThread Success !!!" << std::endl;
}

KPort::~KPort()
{
	myserial->~MySerial();
}

void KPort::RelativeMove(float X, float Y, int Speed)
{//CJXCGX100.123Y20.333F20000$
	std::stringstream ss;
	ss << "CJXCGX" << X << "Y" << Y << "F" << Speed << "$";
	std::string str = ss.str();
	temp = (unsigned char*)(str.c_str());
	myserial->WriteData(temp, str.length());

	Sleep(50);	//等待数据录入
}

void KPort::CoordMove(float X, float Y, int Speed)
{//CJXCgX100.123Y20.333F20000$
	std::stringstream ss;
	ss << "CJXCgX" << X << "Y" << Y << "F" << Speed << "$";
	std::string str = ss.str();
	temp = (unsigned char*)(str.c_str());
	myserial->WriteData(temp, str.length());

	Sleep(50);	//等待数据录入
}

void KPort::SuspendMove()
{//CJXRp
	std::string str = "CJXRp";
	temp = (unsigned char*)(str.c_str());
	myserial->WriteData(temp, str.length());

	Sleep(50);	//等待数据录入
}

void KPort::ContinueMove()
{//CJXRr
	std::string str = "CJXRr";
	temp = (unsigned char*)(str.c_str());
	myserial->WriteData(temp, str.length());

	Sleep(50);	//等待数据录入
}

XYFMessage KPort::GetXYF()
{//CJXSA
	std::string str = "CJXSA";
	temp = (unsigned char*)(str.c_str());
	myserial->WriteData(temp, str.length());

	Sleep(50);	//等待数据录入

	std::stringstream buffer;
	std::string mod = myserial->GetStrInput();
	mod = hexToString(mod);			//mod由16进制转string
	buffer << mod;

	enum MessageType
	{
		NONE = -1, X = 0, Y = 1, T = 2, F = 3, c = 4, In = 5, Out = 6
	};
	std::string line;
	std::stringstream ss[7];
	MessageType type = MessageType::NONE;

	while (getline(buffer, line))
	{
		if (line.find("X: ") != std::string::npos)
			type = MessageType::X;
		if (line.find("Y: ") != std::string::npos)
			type = MessageType::Y;
		if (line.find("T: ") != std::string::npos)
			type = MessageType::T;
		if (line.find("F: ") != std::string::npos)
			type = MessageType::F;
		if (line.find("c: ") != std::string::npos)
			type = MessageType::c;
		if (line.find("In:") != std::string::npos)
			type = MessageType::In;
		if (line.find("Out:") != std::string::npos)
			type = MessageType::Out;

		if (type != MessageType::NONE)
		{
			if (type != MessageType::Out)
				line.erase(0, 3);		//去掉前缀
			else
				line.erase(0, 4);
			line.pop_back();			//去掉末尾多余数据
			ss[(int)type] << line;
		}
	}

	return { ss[0].str(), ss[1].str(), ss[2].str(), ss[3].str(), ss[4].str(), ss[5].str(), ss[6].str() };
}