#include "MySerial.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <iomanip>

std::string str_input;

bool MySerial::s_bExit = false;

const UINT SleepTimeInterval = 5;

MySerial::MySerial()								//INVALID_HANDLE_VALUE = -1表示操作失败或者无效句柄		
{
	m_hComm = INVALID_HANDLE_VALUE;						//存储串口句柄
	m_hListenThread = INVALID_HANDLE_VALUE;				//存储监听句柄
	InitializeCriticalSection(&m_csCommunicationSync);	//初始化临界区对象。临界区对象用于同步对串口通信操作的访问
}

MySerial::~MySerial()
{
	CloseListenThread();
	ClosePort();
	DeleteCriticalSection(&m_csCommunicationSync);
}

bool MySerial::InitPort(UINT portNo, UINT baud, char parity, UINT databits, UINT stopbits)
{//COM5 115200 NONE 8 ONE EV_RXCHAR
	if (!OpenPort(portNo))
		return false;

	EnterCriticalSection(&m_csCommunicationSync);	//进入临界区

	bool bIsSuccess = SetupComm(m_hComm, 10, 10);	//判断串口是否配置成功

	COMMTIMEOUTS CommTimeouts;						//定义一个超时参数COMMTIMEOUTS结构体
	CommTimeouts.ReadIntervalTimeout = 0;			//忽略间隔超时
	CommTimeouts.ReadTotalTimeoutMultiplier = 0;	//读取的字节数乘以的系数
	CommTimeouts.ReadTotalTimeoutConstant = 0;		//常数超时值
	CommTimeouts.WriteTotalTimeoutMultiplier = 0;	//写入的字节数
	CommTimeouts.WriteTotalTimeoutConstant = 0;		//常数超时值
	if (bIsSuccess)
		bIsSuccess = SetCommTimeouts(m_hComm, &CommTimeouts);	//设置串行通信端口的超时参数

	DCB dcb;	//一个包含波特率、数据位、停止位、校验位的结构体
	if (bIsSuccess)
	{
		bIsSuccess = GetCommState(m_hComm, &dcb);	//获取串口句柄关联的DCB设置

		if (bIsSuccess)	//如果获取成功
		{
			dcb.BaudRate = baud;							//波特率
			dcb.ByteSize = (BYTE)databits;					//数据位
			switch (parity)									//校验位
			{
			case 'N':	dcb.Parity = NOPARITY;		break;	//无
			case 'O':	dcb.Parity = ODDPARITY;		break;	//奇
			case 'E':	dcb.Parity = EVENPARITY;	break;	//偶
			default:	dcb.Parity = NOPARITY;				//默认无
			}
			switch (stopbits)								//停止位
			{
			case 1:		dcb.StopBits = ONESTOPBIT;	break;	//one
			case 2:		dcb.StopBits = TWOSTOPBITS;	break;	//two
			}
		}

		dcb.fRtsControl = RTS_CONTROL_ENABLE;

	}

	if (bIsSuccess)
		bIsSuccess = SetCommState(m_hComm, &dcb);			//将更新后的dcb应用到串口上

	PurgeComm(m_hComm, PURGE_RXCLEAR | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_TXABORT);	//清空串口缓冲区  接收缓冲区|发送缓冲区|中止当前接收操作|中止当前发送操作

	LeaveCriticalSection(&m_csCommunicationSync);	//离开临界区
	return bIsSuccess == true;
}

bool MySerial::InitPort(UINT portNo, const LPDCB& plDCB)
{
	if (!OpenPort(portNo))
		return false;

	EnterCriticalSection(&m_csCommunicationSync);

	if (!SetCommState(m_hComm, plDCB))
	{
		LeaveCriticalSection(&m_csCommunicationSync);
		return false;
	}

	PurgeComm(m_hComm, PURGE_RXCLEAR | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_TXABORT);

	LeaveCriticalSection(&m_csCommunicationSync);
	return true;
}

void MySerial::ClosePort()
{
	//如果有串口打开则关闭
	if (m_hComm != INVALID_HANDLE_VALUE)
	{
		CloseHandle(m_hComm);
		m_hComm = INVALID_HANDLE_VALUE;
	}
}

bool MySerial::OpenPort(UINT portNo)
{
	EnterCriticalSection(&m_csCommunicationSync);

	char szPort[50];
	sprintf_s(szPort, "COM%d", portNo);	//写入串口号

	m_hComm = CreateFileA(szPort,		//串口名
		GENERIC_READ | GENERIC_WRITE,	//访问模式，可同时读写
		0,								//不允许子进程继承句柄
		NULL,							//没有安全属性
		OPEN_EXISTING,					//打开已存在的串口，设备必须存在，否则创建失败
		0,								//没有特殊文件属性
		0);								//没有模板文件

	if (m_hComm == INVALID_HANDLE_VALUE)	//如果串口没有打开
	{
		std::cout << "打开失败！" << std::endl;
		LeaveCriticalSection(&m_csCommunicationSync);
		return false;
	}

	LeaveCriticalSection(&m_csCommunicationSync);
	return true;
}

bool MySerial::OpenListenThread()
{
	//如果线程已开启
	if (m_hListenThread != INVALID_HANDLE_VALUE)
		return false;

	s_bExit = false;

	UINT threadID;	//创建线程ID

	m_hListenThread = (HANDLE)_beginthreadex(	//开启串口数据监听线程
		NULL,									//默认安全属性
		0,										//默认的线程堆栈大小
		ListenThread,							//新线程执行的函数
		this,									//传递给线程函数的当前对象的指针
		0,										//无特殊的创建标志
		&threadID);								//新线程ID的地址

	if (!m_hListenThread)	//如果监听线程开启失败
		return false;

	if (!SetThreadPriority(m_hListenThread, THREAD_PRIORITY_ABOVE_NORMAL))	//设置m_hListenThread线程的优先级高于普通线程
		return false;

	return true;
}

bool MySerial::CloseListenThread()
{
	//如果数据监听线程已开启
	if (m_hListenThread != INVALID_HANDLE_VALUE)
	{
		s_bExit = true;
		Sleep(10);									//等待10ms以待线程退出
		CloseHandle(m_hListenThread);				//关闭线程句柄
		m_hListenThread = INVALID_HANDLE_VALUE;		//将句柄置为无效值
	}
	return true;
}

UINT MySerial::GetBytesInCom()
{
	DWORD dwError = 0;									//用于存储ClearCommError可能返回的错误代码
	COMSTAT comstat;									//COMSTAT结构体，包括输入和输出缓冲区的状态
	memset(&comstat, 0, sizeof(COMSTAT));				//将comstat所有字节初始化为0

	UINT BytesInQue = 0;								//缓冲区的字节数

	if (ClearCommError(m_hComm, &dwError, &comstat))	//如果ClearCommError执行成功，返回true并填充comstat
		BytesInQue = comstat.cbInQue;					//获取缓冲区的字节数
	else
		std::cout << "错误代码行数：" << __LINE__ << "\n错误代码：" << dwError << std::endl;

	return BytesInQue;
}

UINT WINAPI MySerial::ListenThread(void* pParam)
{
	MySerial* pSerialPort = reinterpret_cast<MySerial*>(pParam);	//将pParam强制转换为MySerial*类型，并赋给pSerialPort

	str_input = "";

	while (pSerialPort->s_bExit == false)
	{
		UINT BytesInQue = pSerialPort->GetBytesInCom();

		if (BytesInQue == 0)
		{
			Sleep(SleepTimeInterval);
			continue;
		}

		unsigned char cRecved = 0x00;

		do
		{
			cRecved = 0x00;
			if (pSerialPort->ReadChar(cRecved) == true)
			{
				std::stringstream ss;
				int tm = cRecved;
				ss << std::hex << std::setw(2) << std::setfill('0') << tm;
				//ss << " ";
				std::string a = ss.str();
				std::string b;

				std::transform(a.begin(), a.end(), back_inserter(b), ::toupper);	//将a中的字符转为大写并添加在b末尾

				if (b == "2A ")
					b = "AA ";

				str_input.append(b);

				continue;
			}
		} while (--BytesInQue);

		if (BytesInQue == 0)
		{
			//str_input.pop_back();

			//str_input.clear();
		}
	}

	return 0;
}

bool MySerial::ReadChar(unsigned char& cRecved)
{
	bool bResult = true;
	DWORD BytesRead = 0;
	if (m_hComm == INVALID_HANDLE_VALUE)
		return false;

	EnterCriticalSection(&m_csCommunicationSync);

	bResult = ReadFile(		//从缓冲区读取一个字节的数据
		m_hComm,			//串口句柄
		&cRecved,			//读取数据的缓冲区
		1,					//请求的字节数
		&BytesRead,			//实际字节数
		NULL);				//重叠结构指针

	if (!bResult)
	{
		DWORD dwError = GetLastError();
		std::cout << "错误代码行数：" << __LINE__ << "\n错误代码：" << dwError << std::endl;

		PurgeComm(m_hComm, PURGE_RXCLEAR | PURGE_RXABORT);
		LeaveCriticalSection(&m_csCommunicationSync);

		return false;
	}

	LeaveCriticalSection(&m_csCommunicationSync);
	return true;
}

bool MySerial::WriteData(unsigned char* pData, int length)
{
	int* pData1 = new int;
	bool bResult = true;
	DWORD BytesToSend = 0;

	if (m_hComm == INVALID_HANDLE_VALUE)
		return false;

	EnterCriticalSection(&m_csCommunicationSync);

	bResult = WriteFile(	//向缓冲区写入指定量的数据
		m_hComm,			//串口句柄
		pData,				//保存读入数据的缓存区
		length,				//请求读入的字节数
		&BytesToSend,		//实际字节数
		NULL);				//重叠结构指针

	if (!bResult)
	{
		DWORD dwError = GetLastError();
		std::cout << "错误代码行数：" << __LINE__ << "\n错误代码：" << dwError << std::endl;

		PurgeComm(m_hComm, PURGE_TXCLEAR | PURGE_TXABORT);
		LeaveCriticalSection(&m_csCommunicationSync);
		return false;
	}

	LeaveCriticalSection(&m_csCommunicationSync);
	return true;
}

std::string MySerial::GetStrInput()
{
	std::string buffer = str_input;
	str_input.clear();
	return buffer;
}