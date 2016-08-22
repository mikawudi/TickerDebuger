// TickerDebuger.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "winsock2.h"
#include "thread"
#include "list"
#include "queue"
#include "mutex"
#include "iostream"

#include "assert.h"

using namespace std;
enum OP_TYPE{ Read = 1, Write = 2 };


struct DataPack
{
	char* data;
	int length;
};
class DataResult
{
public:
	DataResult(int count, char* data) :count(count), data(data){}
	int count;
	char* data;
};
class BaseProtocol
{
protected:
	BaseProtocol(){};
protected:
	virtual DataResult GetData(const list<DataPack*>* data) = 0;
	virtual void DeleteData(list<DataPack*>* data, int count) = 0;
public:
	DataResult CheckData(list<DataPack*>* data);
};

DataResult BaseProtocol::CheckData(list<DataPack*>* data)
{
	DataResult result = this->GetData(data);
	if (result.count > 0)
		this->DeleteData(data, result.count);
	return result;
}

class MyProtocol
	: public BaseProtocol
{
public:
	MyProtocol() 
		:BaseProtocol()
	{

	}
protected:
	DataResult GetData(const list<DataPack*>* data) override;
	void DeleteData(list<DataPack*>* data, int count) override;

};

DataResult MyProtocol::GetData(const list<DataPack*>* data)
{
	if (data->size() == 0)
		return DataResult(0, nullptr);
	list<DataPack*>::const_iterator start = data->begin();
	byte length = (*start)->data[0];
	int dataCount = 0;
	while (start != data->end())
	{
		dataCount += (*start)->length;
		++start;
	}
	if (dataCount < length)
		return DataResult(0, nullptr);
	char* dataresult = new char[length];
	dataCount = 0;
	for (auto startD = data->begin(); startD != data->end(); ++startD)
	{
		int t = dataCount + (*startD)->length;
		bool isOk = false;
		int copyLeng = (*startD)->length;
		if (t >= length)
		{
			if (t > length)
				copyLeng = (*startD)->length - (t - length);
			isOk = true;
		}
		dataCount += dataCount;
		memcpy(dataresult + dataCount, (*startD)->data, copyLeng);
		if (isOk)
			break;
	}
	return DataResult(length, dataresult);
}

void MyProtocol::DeleteData(list<DataPack*>* data, int count)
{
	int index = 0;
	int removeCount = 0;
	for (auto start : *data)
	{
		int temp = index + (*start).length;
		if (temp <= count)
		{
			removeCount++;
			delete (*start).data;
			index = temp;
			if (temp == count)
				break;
		}
		else
		{
			int live = temp - count;
			char* newTemp = (char*)malloc(live);
			memcpy(newTemp, (*start).data + ((*start).length - live), live);
			delete (*start).data;
			(*start).data = newTemp;
			(*start).length = live;
			index = count;
			break;
		}
	}
	for (int i = 0; i < removeCount; i++)
	{
		delete data->front();
		data->pop_front();
	}
}

class OperatorObject
{
public:
	static const int RecvCount = 1024;
public:
	WSAOVERLAPPED _overlapped;
	OP_TYPE _op;
	char* _sendData;
	byte* _recvDataBuff;
	WSABUF _recvWSABUF;
	int _recvCount;
	WSABUF _sendWSABUF;
	int _sendCount;
public:
	OperatorObject()
		: _op(OP_TYPE::Read)
	{
		this->_recvDataBuff = new byte[OperatorObject::RecvCount];
		this->_recvWSABUF.buf = (CHAR*)this->_recvDataBuff;
		this->_recvWSABUF.len = OperatorObject::RecvCount;
		this->InitRead();
	}
	OperatorObject(char* data, int dataLen)
		: _op(OP_TYPE::Write)
		, _sendData(data)
	{
		this->_sendWSABUF.buf = data;
		this->_sendWSABUF.len = dataLen;
		this->InitWrite();
	}
	void ReSet();
	void ReSetSend(char* newData, int length);
private:
	void InitRead();
	void InitWrite();
};

void OperatorObject::InitRead()
{

}
void OperatorObject::InitWrite()
{

}
void OperatorObject::ReSet()
{
	this->_recvCount = 0;
	this->_recvDataBuff = new byte[OperatorObject::RecvCount];
	this->_recvWSABUF.buf = (char*)this->_recvDataBuff;
	this->_recvWSABUF.len = OperatorObject::RecvCount;
}
void OperatorObject::ReSetSend(char* newData, int length)
{

}
//PreIO object

class Client
{
public:
	SOCKET _socket;
protected:
	queue<OperatorObject*>* _sendQueue;
	mutex* _sendMutex;
	bool _isClose;
	int _waitEndSend;
	int _waitEndRecv;
	int _waitProcess;
	list<DataPack*>* _recvData;
private:
	BaseProtocol* _protocolCehcker;
public:
	Client(SOCKET socket)
		: _socket(socket)
		, _isClose(false)
		, _waitEndRecv(0)
		, _waitEndSend(0)
		, _waitProcess(0)
	{
		_sendQueue = new queue<OperatorObject*>();
		this->_sendMutex = new mutex();
		this->_recvData = new list<DataPack*>();
		this->_protocolCehcker = new MyProtocol();
	}
	void StartRecv();
	void StartRecv(OperatorObject* operatorObj);
	void EndRecv(OperatorObject* recvObj, DWORD opCount);
	void EndSend(OperatorObject* sendData, DWORD opCount);
	void SendData(char* data, int length);
	void EndProcess();
	void Close(){ this->_isClose = true; this->CloseClient(); }
	~Client();
private:
	void CloseClient();
};
Client::~Client()
{
	while (!this->_sendQueue->empty())
	{
		auto d = this->_sendQueue->front();
		this->_sendQueue->pop(); 
		delete d;
	}
	delete _sendQueue;
	while (!this->_recvData->empty())
	{
		auto d = this->_recvData->front();
		this->_recvData->pop_front();
		delete d;
	}
	delete _recvData;
	delete _sendMutex;
}
struct RequestPack
{
	Client* _client;
	char* _data;
	int _count;
};
class ClientProcess
{
public:
	static ClientProcess* GetInstance();
	static const int ProcessCount;
	static const int MaxSemaphore;
	void AddData(Client* client, char* data, int count);
private:
	static ClientProcess* _instance;
	static mutex CreateMutext;
	static mutex* AddMutext;
	static mutex* GetDataMutex;
	vector<thread*>* _threadVector;
	ClientProcess();
	void ProcessThread();
protected:
	queue<RequestPack>* _dataQueue;
	HANDLE _semaphore;
};

void Client::StartRecv()
{
	this->StartRecv(new OperatorObject());
}

void Client::StartRecv(OperatorObject* operatorObject)
{
	if (this->_isClose)
		return;
	DWORD Flag = 0;
	this->_waitEndRecv++;
	WSARecv(this->_socket, &operatorObject->_recvWSABUF, 1, (LPDWORD)&operatorObject->_recvCount, &Flag, &operatorObject->_overlapped, nullptr);
}

void Client::EndRecv(OperatorObject* recvObj, DWORD opCount)
{
	this->_waitEndRecv--;
	if (this->_isClose)
	{
		this->CloseClient();
		return;
	}
	if (opCount == 0)
	{
		this->_isClose = true;
		this->CloseClient();
		return;
	}
	//op
	DataPack* pack = new DataPack();
	pack->data = (char*)recvObj->_recvDataBuff;
	pack->length = opCount;
	this->_recvData->push_back(pack);
	auto result = this->_protocolCehcker->CheckData(this->_recvData);
	while (result.count > 0)
	{
		this->_waitProcess++;
		ClientProcess::GetInstance()->AddData(this, result.data, result.count);
		result = this->_protocolCehcker->CheckData(this->_recvData);
	}


	recvObj->ReSet();
	this->StartRecv(recvObj);
}

void Client::EndSend(OperatorObject* sendObj, DWORD opCount)
{
	this->_waitEndSend--;
	if (this->_isClose)
	{
		this->CloseClient();
		return;
	}
	bool success = true;
	this->_sendMutex->lock();
	OperatorObject* frontData = this->_sendQueue->front();
	if (frontData != sendObj)
		success = false;
	int Flag = 0;
	if (frontData->_sendWSABUF.len != opCount)
	{
		int reSendCount = frontData->_sendWSABUF.len - opCount;
		char* newData = (char*)malloc(reSendCount);
		memcpy((void*)newData, (frontData->_sendWSABUF.buf + opCount), reSendCount);
		frontData->ReSetSend(newData, reSendCount);
		WSASend(this->_socket, &frontData->_sendWSABUF, 1, (LPDWORD)&frontData->_sendCount, Flag, &frontData->_overlapped, nullptr);
	}
	else
	{
		//delete frontData;
		this->_sendQueue->pop();
		if (this->_sendQueue->size() > 0)
		{
			frontData = this->_sendQueue->front();
			this->_waitEndSend++;
			WSASend(this->_socket, &frontData->_sendWSABUF, 1, (LPDWORD)&frontData->_sendCount, Flag, &frontData->_overlapped, nullptr);
		}
	}
	this->_sendMutex->unlock();
	if (!success)
		throw 1;
}

void Client::SendData(char* data, int length)
{
	if (this->_isClose)
		return;
	DWORD Flag = 0;
	auto writeOP = new OperatorObject(data, length);
	this->_sendMutex->lock();
	this->_sendQueue->push(writeOP);
	if (this->_sendQueue->size() == 1)
	{
		this->_waitEndSend++;
		WSASend(this->_socket, &writeOP->_sendWSABUF, 1, (LPDWORD)&writeOP->_sendCount, Flag, &writeOP->_overlapped, nullptr);
	}
	this->_sendMutex->unlock();
}

void Client::CloseClient()
{
	//if (this->_isClose && this->_waitEndRecv == 0 && this->_waitEndSend == 0 && this->_waitProcess == 0)
		//delete this;
}

void Client::EndProcess()
{
	this->_waitProcess--;
	if (this->_isClose)
		this->CloseClient();
}

ClientProcess* ClientProcess::_instance = nullptr;
mutex ClientProcess::CreateMutext;
mutex* ClientProcess::AddMutext = new mutex();
mutex* ClientProcess::GetDataMutex = new mutex();

const int ClientProcess::ProcessCount = 5;
const int ClientProcess::MaxSemaphore = 1000;

void ClientProcess::AddData(Client* client, char* data, int count)
{
	ClientProcess::AddMutext->lock();
	this->_dataQueue->push(RequestPack{ client, data, count });
	ReleaseSemaphore(this->_semaphore, 1, NULL);
	ClientProcess::AddMutext->unlock();
}

ClientProcess::ClientProcess()
{
	this->_dataQueue = new queue<RequestPack>();
	this->_semaphore = CreateSemaphore(NULL, 0, ClientProcess::MaxSemaphore, NULL);
	if (this->_semaphore == 0)
		throw 1;
	this->_threadVector = new vector<thread*>();
	for (int i = 0; i < ClientProcess::ProcessCount; i++)
	{
		this->_threadVector->push_back(new thread(mem_fn(&ClientProcess::ProcessThread), this));
	}
}

ClientProcess* ClientProcess::GetInstance()
{
	if (ClientProcess::_instance != nullptr)
		return ClientProcess::_instance;
	ClientProcess::CreateMutext.lock();
	ClientProcess::_instance = new ClientProcess();
	ClientProcess::CreateMutext.unlock();
	return ClientProcess::_instance;
}

void ClientProcess::ProcessThread()
{
	while (true)
	{
		WaitForSingleObject(this->_semaphore, INFINITE);
		ClientProcess::GetDataMutex->lock();
		/*if (isLock == false)
		{
			DWORD dd = GetLastError();
			return;
		}*/
		auto data = this->_dataQueue->front();
		this->_dataQueue->pop();
		ClientProcess::GetDataMutex->unlock();
		char* command = data._data + 1;
		if (memcmp(command, "getDnsList", data._data[0] - 1) == 0)
		{
			char* buf = new char[11];
			memcpy(buf + 1, "10.2.0.182", 10);
			buf[0] = 11;
			data._client->SendData(buf, 11);
		}
		//delete data._data;
		data._client->EndProcess();
	}
}

class Server
{
	//配置区域
protected:
	static const int backlog = 100;
	static const int workThreadCount = 4;
protected:
	sockaddr_in _sockaddr_in;
	int _port;
	SOCKET _listenSocket;
	thread* _acceptThread;
	HANDLE _iocpObjc;
	list<thread>* _workThreadList;
public:
	Server(char* host, int port)
		: _port(port)
	{
		sockaddr_in addr_in;
		ZeroMemory(&addr_in, sizeof(sockaddr_in));
		addr_in.sin_family = AF_INET;
		addr_in.sin_port = htons(_port);
		addr_in.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
		this->_sockaddr_in = addr_in;

		this->_workThreadList = new list<thread>();
	}
	void Start();
	void Stop();
private:
	void AcceptThread();
	void WorkThread();
};
void Server::Start()
{
	this->_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	::bind(this->_listenSocket, (sockaddr*)&this->_sockaddr_in, sizeof(SOCKADDR_IN));
	listen(this->_listenSocket, Server::backlog);
	_iocpObjc = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	for (int i = 0; i < Server::workThreadCount; i++)
	{
		_workThreadList->push_back(thread(mem_fn(&Server::WorkThread), this));
	}
	this->_acceptThread = new thread(mem_fn(&Server::AcceptThread), this);
}

void Server::Stop()
{
	if (this->_acceptThread != nullptr)
	{
		//TerminateThread()
		TerminateThread((HANDLE)this->_acceptThread->native_handle(), 0);
	}
	for (auto& t : *this->_workThreadList)
	{
		TerminateThread((HANDLE)this->_acceptThread->native_handle(), 0);
	}
}

void Server::AcceptThread()
{
	while (true)
	{
		int addrLeng = sizeof(SOCKADDR);
		sockaddr addrResult;
		SOCKET clientSock = accept(this->_listenSocket, &addrResult, &addrLeng);
		Client* client = new Client(clientSock);
		CreateIoCompletionPort((HANDLE)client->_socket, this->_iocpObjc, (ULONG_PTR)client, 0);
		client->StartRecv();
	}
}
void Server::WorkThread()
{
	while (true)
	{
		DWORD opCount;
		Client* client = nullptr;
		OperatorObject* opObject = nullptr;
		BOOL result = GetQueuedCompletionStatus(this->_iocpObjc, &opCount, (PULONG_PTR)&client, (LPOVERLAPPED*)&opObject, INFINITE);
		if (opObject->_op == OP_TYPE::Read)
		{
			client->EndRecv(opObject, opCount);
		}
		if (opObject->_op == OP_TYPE::Write)
		{
			client->EndSend(opObject, opCount);
		}
	}
}

int _tmain(int argc, _TCHAR* argv[])
{

	WSADATA wsaData;
	int nRet;
	if ((nRet = WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0)
	{
		exit(0);
	}

	int returnCode = _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_WNDW);
	ClientProcess::GetInstance();

	Server* server = new Server("10.2.0.182", 1525);
	server->Start();
	int rec = 0;
	cin >> rec;
	server->Stop();
	return 0;
}

