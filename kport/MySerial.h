#pragma once

#include <iostream>
#include <Windows.h>
#include <string>
#include <process.h>
#include <vector>

class MySerial
{
public:
	MySerial();
	~MySerial();

	bool InitPort(UINT portNo, UINT baud, char parity, UINT bytebits, UINT stopbits);	//初始化
	bool InitPort(UINT portNo, const LPDCB& plDCB);										//初始化
	bool OpenListenThread();															//打开监听
	bool CloseListenThread();															//关闭监听

	bool WriteData(unsigned char* pData, int length);	//向串口写数据
	UINT GetBytesInCom();								//获取串口缓冲区中的字节数
	bool ReadChar(unsigned char& cRecved);				//读取串口接收缓冲区中一个字节的数据

	bool OpenPort(UINT portNo);						//打开串口
	void ClosePort();								//关闭串口
	static UINT WINAPI ListenThread(void* pParam);	//串口监听线程

private:
	HANDLE m_hComm;							//一个存储句柄的变量
	static bool s_bExit;					//监听线程退出的标志
	volatile HANDLE m_hListenThread;		//volatile该变量可以在程序的控制之外被改变
	CRITICAL_SECTION m_csCommunicationSync;	//临界区对象

public:
	std::string GetStrInput();
};

