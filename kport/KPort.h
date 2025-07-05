#pragma once

#include <iostream>
#include <Windows.h>
#include <string>
#include <process.h>
#include <sstream>
#include <vector>

#include "MySerial.h"

struct XYFMessage		//ע����string����
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
	KPort(int portNo);										//��ʼ�����򿪣����ڣ����봮�ں�
	~KPort();

	void RelativeMove(float X, float Y, int Speed);			//����˶�
	void CoordMove(float X, float Y, int Speed);			//�����˶�
	void SuspendMove();										//��ͣ
	void ContinueMove();									//����

	XYFMessage GetXYF();									//��ȡ�˶���Ϣ

private:
	std::unique_ptr<MySerial> myserial;
	unsigned char* temp;									//�洢ָ���ָ��
};

