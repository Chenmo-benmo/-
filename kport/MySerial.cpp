#include "MySerial.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <iomanip>

std::string str_input;

bool MySerial::s_bExit = false;

const UINT SleepTimeInterval = 5;

MySerial::MySerial()								//INVALID_HANDLE_VALUE = -1��ʾ����ʧ�ܻ�����Ч���		
{
	m_hComm = INVALID_HANDLE_VALUE;						//�洢���ھ��
	m_hListenThread = INVALID_HANDLE_VALUE;				//�洢�������
	InitializeCriticalSection(&m_csCommunicationSync);	//��ʼ���ٽ��������ٽ�����������ͬ���Դ���ͨ�Ų����ķ���
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

	EnterCriticalSection(&m_csCommunicationSync);	//�����ٽ���

	bool bIsSuccess = SetupComm(m_hComm, 10, 10);	//�жϴ����Ƿ����óɹ�

	COMMTIMEOUTS CommTimeouts;						//����һ����ʱ����COMMTIMEOUTS�ṹ��
	CommTimeouts.ReadIntervalTimeout = 0;			//���Լ����ʱ
	CommTimeouts.ReadTotalTimeoutMultiplier = 0;	//��ȡ���ֽ������Ե�ϵ��
	CommTimeouts.ReadTotalTimeoutConstant = 0;		//������ʱֵ
	CommTimeouts.WriteTotalTimeoutMultiplier = 0;	//д����ֽ���
	CommTimeouts.WriteTotalTimeoutConstant = 0;		//������ʱֵ
	if (bIsSuccess)
		bIsSuccess = SetCommTimeouts(m_hComm, &CommTimeouts);	//���ô���ͨ�Ŷ˿ڵĳ�ʱ����

	DCB dcb;	//һ�����������ʡ�����λ��ֹͣλ��У��λ�Ľṹ��
	if (bIsSuccess)
	{
		bIsSuccess = GetCommState(m_hComm, &dcb);	//��ȡ���ھ��������DCB����

		if (bIsSuccess)	//�����ȡ�ɹ�
		{
			dcb.BaudRate = baud;							//������
			dcb.ByteSize = (BYTE)databits;					//����λ
			switch (parity)									//У��λ
			{
			case 'N':	dcb.Parity = NOPARITY;		break;	//��
			case 'O':	dcb.Parity = ODDPARITY;		break;	//��
			case 'E':	dcb.Parity = EVENPARITY;	break;	//ż
			default:	dcb.Parity = NOPARITY;				//Ĭ����
			}
			switch (stopbits)								//ֹͣλ
			{
			case 1:		dcb.StopBits = ONESTOPBIT;	break;	//one
			case 2:		dcb.StopBits = TWOSTOPBITS;	break;	//two
			}
		}

		dcb.fRtsControl = RTS_CONTROL_ENABLE;

	}

	if (bIsSuccess)
		bIsSuccess = SetCommState(m_hComm, &dcb);			//�����º��dcbӦ�õ�������

	PurgeComm(m_hComm, PURGE_RXCLEAR | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_TXABORT);	//��մ��ڻ�����  ���ջ�����|���ͻ�����|��ֹ��ǰ���ղ���|��ֹ��ǰ���Ͳ���

	LeaveCriticalSection(&m_csCommunicationSync);	//�뿪�ٽ���
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
	//����д��ڴ���ر�
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
	sprintf_s(szPort, "COM%d", portNo);	//д�봮�ں�

	m_hComm = CreateFileA(szPort,		//������
		GENERIC_READ | GENERIC_WRITE,	//����ģʽ����ͬʱ��д
		0,								//�������ӽ��̼̳о��
		NULL,							//û�а�ȫ����
		OPEN_EXISTING,					//���Ѵ��ڵĴ��ڣ��豸������ڣ����򴴽�ʧ��
		0,								//û�������ļ�����
		0);								//û��ģ���ļ�

	if (m_hComm == INVALID_HANDLE_VALUE)	//�������û�д�
	{
		std::cout << "��ʧ�ܣ�" << std::endl;
		LeaveCriticalSection(&m_csCommunicationSync);
		return false;
	}

	LeaveCriticalSection(&m_csCommunicationSync);
	return true;
}

bool MySerial::OpenListenThread()
{
	//����߳��ѿ���
	if (m_hListenThread != INVALID_HANDLE_VALUE)
		return false;

	s_bExit = false;

	UINT threadID;	//�����߳�ID

	m_hListenThread = (HANDLE)_beginthreadex(	//�����������ݼ����߳�
		NULL,									//Ĭ�ϰ�ȫ����
		0,										//Ĭ�ϵ��̶߳�ջ��С
		ListenThread,							//���߳�ִ�еĺ���
		this,									//���ݸ��̺߳����ĵ�ǰ�����ָ��
		0,										//������Ĵ�����־
		&threadID);								//���߳�ID�ĵ�ַ

	if (!m_hListenThread)	//��������߳̿���ʧ��
		return false;

	if (!SetThreadPriority(m_hListenThread, THREAD_PRIORITY_ABOVE_NORMAL))	//����m_hListenThread�̵߳����ȼ�������ͨ�߳�
		return false;

	return true;
}

bool MySerial::CloseListenThread()
{
	//������ݼ����߳��ѿ���
	if (m_hListenThread != INVALID_HANDLE_VALUE)
	{
		s_bExit = true;
		Sleep(10);									//�ȴ�10ms�Դ��߳��˳�
		CloseHandle(m_hListenThread);				//�ر��߳̾��
		m_hListenThread = INVALID_HANDLE_VALUE;		//�������Ϊ��Чֵ
	}
	return true;
}

UINT MySerial::GetBytesInCom()
{
	DWORD dwError = 0;									//���ڴ洢ClearCommError���ܷ��صĴ������
	COMSTAT comstat;									//COMSTAT�ṹ�壬��������������������״̬
	memset(&comstat, 0, sizeof(COMSTAT));				//��comstat�����ֽڳ�ʼ��Ϊ0

	UINT BytesInQue = 0;								//���������ֽ���

	if (ClearCommError(m_hComm, &dwError, &comstat))	//���ClearCommErrorִ�гɹ�������true�����comstat
		BytesInQue = comstat.cbInQue;					//��ȡ���������ֽ���
	else
		std::cout << "�������������" << __LINE__ << "\n������룺" << dwError << std::endl;

	return BytesInQue;
}

UINT WINAPI MySerial::ListenThread(void* pParam)
{
	MySerial* pSerialPort = reinterpret_cast<MySerial*>(pParam);	//��pParamǿ��ת��ΪMySerial*���ͣ�������pSerialPort

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

				std::transform(a.begin(), a.end(), back_inserter(b), ::toupper);	//��a�е��ַ�תΪ��д�������bĩβ

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

	bResult = ReadFile(		//�ӻ�������ȡһ���ֽڵ�����
		m_hComm,			//���ھ��
		&cRecved,			//��ȡ���ݵĻ�����
		1,					//������ֽ���
		&BytesRead,			//ʵ���ֽ���
		NULL);				//�ص��ṹָ��

	if (!bResult)
	{
		DWORD dwError = GetLastError();
		std::cout << "�������������" << __LINE__ << "\n������룺" << dwError << std::endl;

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

	bResult = WriteFile(	//�򻺳���д��ָ����������
		m_hComm,			//���ھ��
		pData,				//����������ݵĻ�����
		length,				//���������ֽ���
		&BytesToSend,		//ʵ���ֽ���
		NULL);				//�ص��ṹָ��

	if (!bResult)
	{
		DWORD dwError = GetLastError();
		std::cout << "�������������" << __LINE__ << "\n������룺" << dwError << std::endl;

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