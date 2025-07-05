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

	bool InitPort(UINT portNo, UINT baud, char parity, UINT bytebits, UINT stopbits);	//��ʼ��
	bool InitPort(UINT portNo, const LPDCB& plDCB);										//��ʼ��
	bool OpenListenThread();															//�򿪼���
	bool CloseListenThread();															//�رռ���

	bool WriteData(unsigned char* pData, int length);	//�򴮿�д����
	UINT GetBytesInCom();								//��ȡ���ڻ������е��ֽ���
	bool ReadChar(unsigned char& cRecved);				//��ȡ���ڽ��ջ�������һ���ֽڵ�����

	bool OpenPort(UINT portNo);						//�򿪴���
	void ClosePort();								//�رմ���
	static UINT WINAPI ListenThread(void* pParam);	//���ڼ����߳�

private:
	HANDLE m_hComm;							//һ���洢����ı���
	static bool s_bExit;					//�����߳��˳��ı�־
	volatile HANDLE m_hListenThread;		//volatile�ñ��������ڳ���Ŀ���֮�ⱻ�ı�
	CRITICAL_SECTION m_csCommunicationSync;	//�ٽ�������

public:
	std::string GetStrInput();
};

