// TickerDebuger.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "winsock2.h"
#include "thread"
#include "list"
#include "queue"
#include "mutex"
#include "iostream"
#include "map"
#include "WS2tcpip.h"

#include "assert.h"

using namespace std;
enum OP_TYPE{ Read = 1, Write = 2 };
#pragma pack (1)
struct IPTag
{
	//ip类型(v4 or v6)
	byte IP_TYPE;
	//ip值
	INT64 IP_VALUE;
	//tcp端口号
	UINT16 TCP_PORT;
};
struct ServerTag
{
	IPTag IP_INFO;
	//client总数
	int C_COUNT;
	//协议类型
	byte PRO_TYPE;
	//serverName的长度
	byte KEY_COUNT;
};
struct SerInfo
{
	byte tag;
	IPTag IP_INFO;
};
#pragma pack ()
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

class DataList
{
public:
	static DataList* GetInstance();
private:
	static DataList* instance;
	static mutex createMutex;
	mutex* _dataMutex;
	map<string, ServerTag>* _servermap;
	DataList();
	static char* dataBuffer;
	static int dataCount;
public:
	void AddData(ServerTag value, string& key);
	int GetData(char** data);
	void DeleteData(string& key);
};
char* DataList::dataBuffer = nullptr;
int DataList::dataCount = 0;
mutex DataList::createMutex;
DataList* DataList::instance;
DataList::DataList()
{
	this->_dataMutex = new mutex();
	this->_servermap = new map<string, ServerTag>();
}
DataList* DataList::GetInstance()
{
	if (DataList::instance != nullptr)
		return DataList::instance;
	DataList::createMutex.lock();
	if (DataList::instance == nullptr)
		DataList::instance = new DataList();
	DataList::createMutex.unlock();
	return DataList::instance;
}
void DataList::AddData(ServerTag value, string& key)
{
	this->_dataMutex->lock();
	auto findResult = this->_servermap->find(key);
	if (findResult == this->_servermap->end())
	{
		this->_servermap->insert(pair<string, ServerTag>(key, value));
	}
	else
	{
		findResult->second = value;
	}
	char* oldData = DataList::dataBuffer;
	int newCount = 0;
	for (auto data : *this->_servermap)
	{
		newCount += (sizeof(ServerTag) + data.first.size());
	}
	char* newData = new char[newCount];
	int start = 0;
	for (auto data : *this->_servermap)
	{
		*((ServerTag*)(newData + start)) = data.second;
		string str = data.first;
		memcpy(newData + sizeof(ServerTag) + start, str.data(), str.size());
		start += (sizeof(ServerTag) + data.first.size());
	}
	DataList::dataBuffer = newData;
	DataList::dataCount = newCount;
	delete  oldData;
	this->_dataMutex->unlock();
}
int DataList::GetData(char** data)
{
	/*char* result = nullptr;
	int resultCount = 0;
	this->_dataMutex->lock();
	int length = this->_servermap->size();
	if (length != 0)
	{
		result = new char[length * sizeof(ServerTag)];
		for (auto kvp : *this->_servermap)
		{
			*((ServerTag*)(result + resultCount)) = kvp.second;
			resultCount += sizeof(ServerTag);
		}
	}
	this->_dataMutex->unlock();
	*data = result;
	return resultCount;*/
	if (DataList::dataCount == 0)
	{
		return 0;
	}
	char* result = nullptr;
	int count = 0;
	this->_dataMutex->lock();
	result = (char*)malloc(DataList::dataCount + 2);
	memcpy(result + 2, DataList::dataBuffer, DataList::dataCount);
	count = DataList::dataCount + 2;
	*((uint16_t*)result) = count;
	this->_dataMutex->unlock();
	*data = result;
	return count;
}
void DataList::DeleteData(string& key)
{
	this->_dataMutex->lock();
	auto findResult = this->_servermap->find(key);
	if (findResult != this->_servermap->end())
	{
		this->_servermap->erase(findResult);
	}
	this->_dataMutex->unlock();
}

class OperatorObject
{
public:
	static const int RecvCount = 1024;
public:
	WSAOVERLAPPED _overlapped;
	OP_TYPE _op;
	char* _sendDataBuff;
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
		, _sendDataBuff(data)
	{
		this->_sendWSABUF.buf = data;
		this->_sendWSABUF.len = dataLen;
		this->InitWrite();
	}
	void ReSet();
	void ReSetSend(char* newData, int length);
	~OperatorObject();
private:
	void InitRead();
	void InitWrite();
};
OperatorObject::~OperatorObject()
{
	delete this->_recvDataBuff;
	delete this->_sendDataBuff;
}

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
	delete this->_sendDataBuff;
	this->_sendDataBuff = newData;
	this->_sendWSABUF.buf = this->_sendDataBuff;
	this->_sendWSABUF.len = length;
}
//PreIO object

class Client
{
public:
	SOCKET _socket;
protected:
	queue<OperatorObject*>* _sendQueue;
	mutex* _sendMutex;
	mutex* _dispMutex;
	int _waitEndSend;
	bool _isEndRecv;
	int _waitProcess;
	list<DataPack*>* _recvData;
	OperatorObject* _recvOperatorObject;
private:
	BaseProtocol* _protocolCehcker;
	bool isDisponse = false;
public:
	Client(SOCKET socket)
		: _socket(socket)
		, _isEndRecv(false)
		, _waitEndSend(0)
		, _waitProcess(0)
	{
		_sendQueue = new queue<OperatorObject*>();
		this->_sendMutex = new mutex();
		this->_dispMutex = new mutex();
		this->_recvData = new list<DataPack*>();
		this->_protocolCehcker = new MyProtocol();
		this->_recvOperatorObject = new OperatorObject();
	}
	void StartRecv();
	void StartRecv(OperatorObject* operatorObj);
	void EndRecv(OperatorObject* recvObj, DWORD opCount);
	void EndSend(OperatorObject* sendData, DWORD opCount);
	void SendData(char* data, int length);
	void EndProcess();
	bool Close() { closesocket(this->_socket); return this->CloseClient(); }
	~Client();
private:
	bool CloseClient();
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
	delete this->_protocolCehcker;
	delete this->_recvOperatorObject;
	this->isDisponse = true;
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
	static mutex QueueMutext;
	vector<thread*>* _threadVector;
	ClientProcess();
	void ProcessThread();
protected:
	queue<RequestPack>* _dataQueue;
	HANDLE _semaphore;
};
class NodeClient
{
protected:
	static mutex opLock;
	static mutex updateLock;
	static map<string, map<byte, SerInfo>*> ServerMap;
	static bool AddData(char* name, map<byte, SerInfo>* data);
	static bool Remove(const char* name);
	static void UpdateData();
	static char* data;
protected:
	SOCKET _csock;
	thread* _recvThread;
	byte* _buffer;
	string* _name;
	static const int bufferCount;
	virtual bool Login(int recvCount);
	virtual int GetData(int recvCount, char** data, int* other);
	virtual bool DoSth(char* data);
	void RecvThread();
public:
	NodeClient(SOCKET client);
	static byte* GetOnlineServer();
	void Start();
	~NodeClient();
};
void Client::StartRecv()
{
	this->StartRecv(this->_recvOperatorObject);
}

void Client::StartRecv(OperatorObject* operatorObject)
{
	DWORD Flag = 0;
	WSARecv(this->_socket, &operatorObject->_recvWSABUF, 1, (LPDWORD)&operatorObject->_recvCount, &Flag, &operatorObject->_overlapped, nullptr);
}

void Client::EndRecv(OperatorObject* recvObj, DWORD opCount)
{
	if (opCount != 0)
	{
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
	else
	{
		mutex* t = this->_dispMutex;
		t->lock();
		this->_isEndRecv = true;
		bool isSuc = this->Close();
		t->unlock();
		if (isSuc)
			delete t;
	}
}

void Client::EndSend(OperatorObject* sendObj, DWORD opCount)
{
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
		this->_waitEndSend++;
		WSASend(this->_socket, &frontData->_sendWSABUF, 1, (LPDWORD)&frontData->_sendCount, Flag, &frontData->_overlapped, nullptr);
	}
	else
	{
		delete frontData;
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
	mutex* t = this->_dispMutex;
	t->lock();
	bool isSucc = false;
	this->_waitEndSend--;
	if (this->_isEndRecv)
	{
		isSucc = this->CloseClient();
	}
	t->unlock();
	if (isSucc)
		delete t;
}

void Client::SendData(char* data, int length)
{
	this->_waitEndSend++;
	DWORD Flag = 0;
	auto writeOP = new OperatorObject(data, length);
	this->_sendMutex->lock();
	this->_sendQueue->push(writeOP);
	if (this->_sendQueue->size() == 1)
	{
		WSASend(this->_socket, &writeOP->_sendWSABUF, 1, (LPDWORD)&writeOP->_sendCount, Flag, &writeOP->_overlapped, nullptr);
	}
	else
		this->_waitEndSend--;
	this->_sendMutex->unlock();
}

bool Client::CloseClient()
{
	if (this->_isEndRecv && this->_waitEndSend == 0 && this->_waitProcess == 0)
	{
		/*if (this->isDisponse)
		{
			return;
		}*/
		//this->isDisponse = true;
		delete this;
		return true;
	}
	return false;
}

void Client::EndProcess()
{
	bool isSucc = false;
	mutex* t = this->_dispMutex;
	t->lock();
	this->_waitProcess--;
	if (this->_isEndRecv)
		isSucc = this->CloseClient();
	t->unlock();
	if (isSucc)
		delete t;
}

ClientProcess* ClientProcess::_instance = nullptr;
mutex ClientProcess::CreateMutext;
mutex ClientProcess::QueueMutext;

const int ClientProcess::ProcessCount = 5;
const int ClientProcess::MaxSemaphore = 1000;

void ClientProcess::AddData(Client* client, char* data, int count)
{
	ClientProcess::QueueMutext.lock();
	this->_dataQueue->push(RequestPack{ client, data, count });
	ReleaseSemaphore(this->_semaphore, 1, NULL);
	ClientProcess::QueueMutext.unlock();
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
		ClientProcess::QueueMutext.lock();
		/*if (isLock == false)
		{
			DWORD dd = GetLastError();
			return;
		}*/
		auto data = this->_dataQueue->front();
		this->_dataQueue->pop();
		ClientProcess::QueueMutext.unlock();
		char* command = data._data + 1;
		if (memcmp(command, "getDnsList", data._data[0] - 1) == 0)
		{
			/*char* buf = new char[11];
			memcpy(buf + 1, "10.2.0.182", 10);
			buf[0] = 11;
			data._client->SendData(buf, 11);*/
			char* result = nullptr;
			char* sendBuff = nullptr;
			int sendCount = 0;
			int resultCount = DataList::GetInstance()->GetData(&result);
			if (resultCount > 0)
			{
				/*sendCount = resultCount + 2;
				sendBuff = new char[resultCount + 2];
				memcpy(sendBuff + 2, result, resultCount);
				*((UINT16*)sendBuff) = sendCount;*/
				sendBuff = result;
				sendCount = resultCount;
			}
			else
			{
				sendCount = 6;
				sendBuff = new char[6];
				memcpy(sendBuff + 2, "fail", 4);
				*((UINT16*)sendBuff) = 6;
			}
			data._client->SendData(sendBuff, sendCount);
		}
		if (memcmp(command, "getSerList", data._data[0] - 1) == 0)
		{
			byte* resu = NodeClient::GetOnlineServer();
			if (resu == nullptr)
				resu = new byte[6];
			*((int*)resu) = 6;
			*((uint16_t*)(resu + 4)) = 0;
			data._client->SendData((char*)resu, *((int*)resu));
		}
		delete data._data;
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
#if _DEBUG
int ccc = 0;
int bbb = 0;
int aaa = 0;
#endif
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
#if _DEBUG
			aaa++;
#endif
			client->EndRecv(opObject, opCount);
			continue;
		}
		if (opObject->_op == OP_TYPE::Write)
		{
#if _DEBUG
			bbb++;
#endif
			if (opCount == 0)
			{
#if _DEBUG
				ccc++;
#endif
				continue;
			}
			client->EndSend(opObject, opCount);
			continue;
		}
	}
}

class Worker
{
public:
	Worker();
	void SendData(int data);
protected:
	list<thread*>* threadList;
	HANDLE semaphore;
	queue<int>* dataQueue;
	static mutex* GetMutex;
	static mutex* AddMutex;
private:
	void ThreadWork();
};

mutex* Worker::GetMutex = new mutex();

Worker::Worker()
{
	this->threadList = new list<thread*>();
	this->semaphore = CreateSemaphore(NULL, 0, 100000, nullptr);
	this->dataQueue = new queue<int>();
	for (int i = 0; i < 10; i++)
	{
		this->threadList->push_back(new thread(mem_fn(&Worker::ThreadWork), this));
	}
}

void Worker::ThreadWork()
{
	while (true)
	{
		WaitForSingleObject(this->semaphore, INFINITE);
		Worker::GetMutex->lock();
		if (dataQueue->empty())
		{
			Worker::GetMutex->unlock();
			return;
		}
		dataQueue->front();
		dataQueue->pop();
		Worker::GetMutex->unlock();
	}
}

void Worker::SendData(int data)
{
	Worker::GetMutex->lock();
	this->dataQueue->push(data);
	ReleaseSemaphore(this->semaphore, 1, NULL);
	Worker::GetMutex->unlock();
}

enum IP_TYPE{ IPV4, IPV6 };

struct Command
{
	char* commandValue;
	char* key;
	byte keyCount;
	IP_TYPE type;
	USHORT port;
	union IP
	{
		UINT32 v4;
		UINT64 v6;
	} IPValue;
	int value;
};

bool ParseValue(char* comValue, Command* commandValue)
{
	if (commandValue == nullptr)
		return false;
	char* valueData = nullptr;
	char* command = strtok_s(comValue, " ", &valueData);
	if (valueData[0] == 0x00)
	{
		return false;;
	}
	if (strcmp(command, "add") == 0)
	{
		char* count = nullptr;
		char* other = nullptr;
		char* key = strtok_s(valueData, " ", &other);
		int keyLength = strnlen_s(key, 256);
		if (keyLength <= 0)
		{
			throw 1;
		}
		char* ipValue = strtok_s(other, " ", &count);
		int countvalue = 0;
		if (sscanf_s(count, "%d", &countvalue) == -1)
		{
			return false;
		}
		char* iptype = nullptr;
		char* temp = nullptr;
		iptype = strtok_s(other, ":", &temp);
		if (strcmp(iptype, "ipv4") == 0)
		{
			char* port = nullptr;
			char* ipvalue = strtok_s(temp, ":", &port);
			int portvalue = 0;
			sscanf_s(port, "%d", &portvalue);
			struct in_addr addr;
			if (inet_pton(AF_INET, ipvalue, &addr) < 0)
			{
				return false;
			}
			else
			{
				commandValue->commandValue = command;
				commandValue->type = IP_TYPE::IPV4;
				commandValue->value = countvalue;
				commandValue->port = portvalue;
				commandValue->IPValue.v4 = addr.S_un.S_addr;
				commandValue->key = key;
				commandValue->keyCount = keyLength;
				return true;
			}
		}
		if (strcmp(iptype, "ipv6") == 0)
		{
			return false;
		}
	}
	if (strcmp(command, "delete") == 0)
	{

	}
	return false;
}

bool ReadData()
{
	//char ddd[] = "add ipv4:127.0.0.1:1234 1000";
	char* input = new char[50];
	cin.getline(input, 50);
	Command com;
	bool resu = ParseValue(input, &com);
	if (resu && strcmp(com.commandValue, "add") == 0)
	{
		ServerTag tag;
		string key(com.key);
		tag.IP_INFO.IP_TYPE = (com.type == IP_TYPE::IPV4 ? 0x04 : 0x06);
		tag.IP_INFO.TCP_PORT = com.port;
		tag.IP_INFO.IP_VALUE = com.IPValue.v4;
		tag.C_COUNT = com.value;
		tag.KEY_COUNT = key.size();
		tag.PRO_TYPE = 0x00;
		DataList::GetInstance()->AddData(tag, key);
	}
	if (resu && strcmp(com.commandValue, "delete") == 0)
	{

	}
	delete input;
	return resu;
}

mutex NodeClient::opLock;
mutex NodeClient::updateLock;
map<string, map<byte, SerInfo>*> NodeClient::ServerMap;

char* NodeClient::data = nullptr;
void NodeClient::UpdateData()
{
	if (NodeClient::ServerMap.size() == 0)
	{
		delete NodeClient::data;
		NodeClient::data = nullptr;
		return;
	}
	//leng(4):count(2)
	int resultSize = 6;
	for (auto& tag : NodeClient::ServerMap)
	{
		//strlen(2):str(n):IPTag:(iptype(1):ip(8):port(2)):
		resultSize += (tag.first.length() + 2 + sizeof(IPTag));
	}
	char* result = (char*)malloc(resultSize);
	*(INT32*)result = resultSize;
	*((uint16_t*)(result + 4)) = (uint16_t)NodeClient::ServerMap.size();
	char* tempStart = result + 6;
	for (auto& tag : NodeClient::ServerMap)
	{
		const string& key = tag.first;
		*(uint16_t*)tempStart = key.length();
		tempStart += 2;
		memcpy(tempStart, key.c_str(), key.length());
		tempStart += key.length();
		SerInfo& info = tag.second->at(0x02);
		*(IPTag*)tempStart = info.IP_INFO;
		tempStart += sizeof(IPTag);
	}
	NodeClient::updateLock.lock();
	delete NodeClient::data;
	NodeClient::data = result;
	NodeClient::updateLock.unlock();
}

bool NodeClient::AddData(char* name, map<byte, SerInfo>* data)
{
	string key(name);
	bool isSuccess = false;
	NodeClient::opLock.lock();
	if (NodeClient::ServerMap.find(key) == NodeClient::ServerMap.end())
	{
		NodeClient::ServerMap.insert(pair<string, map<byte, SerInfo>*>(key, data));
		NodeClient::UpdateData();
		isSuccess = true;
	}
	NodeClient::opLock.unlock();
	return isSuccess;
}

bool NodeClient::Remove(const char* name)
{
	string key(name);
	bool isSuccess = true;
	NodeClient::opLock.lock();
	auto& item = NodeClient::ServerMap.find(key);
	if (item != NodeClient::ServerMap.end())
	{
		delete item->second;
		NodeClient::ServerMap.erase(key);
		NodeClient::UpdateData();
	}
	NodeClient::opLock.unlock();
	return isSuccess;
}

bool NodeClient::Login(int recvCount)
{
	if (recvCount < 2)
	{
		return false;
	}
	const int16_t* leng = (int16_t*)this->_buffer;
	if (*leng <= 0 || *leng > 1024)
	{
		return false;
	}
	const char* start = (char*)this->_buffer + 2;
	if (memcmp(start, "logins", 6) != 0)
		return false;
	start += 8;
	const int16_t* nameLeng = (int16_t*)start;
	if (*nameLeng > 30)
		return false;
	start += 2;
	char* name = (char*)malloc((*nameLeng) + 1);
	name[*nameLeng] = 0x00;
	memcpy(name, start, *nameLeng);
	start += (*nameLeng);
	byte nodeLength = *start;
	start++;
	map<byte, SerInfo>* infoMap = new map<byte, SerInfo>();
	for (int i = 0; i < nodeLength; i++)
	{
		const SerInfo* iptag = (SerInfo*)start;
		if (infoMap->find(iptag->tag) != infoMap->end())
			throw 1;
		infoMap->insert(pair<byte, SerInfo>(iptag->tag, *iptag));
		start += sizeof(SerInfo);
	}
	bool result = false;
	//必需要有的信息
	if (infoMap->find(0x02) == infoMap->end())
	{
		delete infoMap;
	}
	else
	{
		_name = new string(name);
		result = NodeClient::AddData(name, infoMap);
	}
	delete name;
	return result;
}

int NodeClient::GetData(int recvCount, char** data, int* other)
{
	if (recvCount < 2)
	{
		*other = recvCount;
		return 0;
	}
	const int16_t* dataLeng = (int16_t*)this->_buffer;
	if (*dataLeng > 1024)
		return -1;
	if (*dataLeng < recvCount)
	{
		*other = (*dataLeng);
		return 0;
	}
	char* result = (char*)malloc((*dataLeng) + 1);
	result[(*dataLeng)] = 0x00;
	memcpy(result, this->_buffer, *dataLeng);
	*data = result;
	int rather = (*dataLeng) - recvCount;
	*other = rather;
	if (rather > 0)
		memcpy(this->_buffer, this->_buffer + (*dataLeng), rather);
	return 1;
}

bool NodeClient::DoSth(char* data)
{
	const int16_t* length = (int16_t*)data;
	char* str = data + 2;
	Command com;
	bool resu = ::ParseValue(str, &com);
	if (resu && memcmp(com.key, this->_name->c_str(), com.keyCount) != 0)
	{
		throw 1;
	}
	if (resu && strcmp(com.commandValue, "add") == 0)
	{
		ServerTag tag;
		string key(com.key);
		tag.IP_INFO.IP_TYPE = (com.type == IP_TYPE::IPV4 ? 0x04 : 0x06);
		tag.IP_INFO.TCP_PORT = com.port;
		tag.IP_INFO.IP_VALUE = com.IPValue.v4;
		tag.C_COUNT = com.value;
		tag.KEY_COUNT = key.size();
		tag.PRO_TYPE = 0x00;
		DataList::GetInstance()->AddData(tag, key);
	}
	if (resu && strcmp(com.commandValue, "delete") == 0)
	{

	}
	return resu;
}

NodeClient::~NodeClient()
{
	delete this->_buffer;
	delete this->_recvThread;
	delete this->_name;
}

const int NodeClient::bufferCount = 1024;

NodeClient::NodeClient(SOCKET sock)
	: _csock(sock)
{
	this->_buffer = new byte[NodeClient::bufferCount];
}
void NodeClient::Start()
{
	this->_recvThread = new thread(mem_fn(&NodeClient::RecvThread), this);
}
void NodeClient::RecvThread()
{
	while (this->_recvThread == nullptr);
	this->_recvThread->detach();
	//waitLog
	int recvCount = ::recv(this->_csock, (char*)this->_buffer, NodeClient::bufferCount, 0);
	
	if (!this->Login(recvCount))
	{
		::closesocket(this->_csock);
		delete this;
		return;
	}
	if (SOCKET_ERROR == ::send(this->_csock, "ok", 2, 0))
	{
		::closesocket(this->_csock);
		delete this;
		return;
	}
	int rather = 0;
	char* data;
	int result = 0;
	while (true)
	{
		int recvCount = ::recv(this->_csock, (char*)this->_buffer + rather, NodeClient::bufferCount - rather, 0);
		//被远端close
		if (recvCount <= 0)
		{
			break;
		}
		while ((result = this->GetData(recvCount, &data, &rather)) > 0)
		{
			recvCount = rather;
			bool resu = false;
			try
			{
				resu = this->DoSth(data);
			}
			catch (int expID)
			{
				delete data;
				break;
			}
			char success = resu ? 0x01 : 0x02;
			if (SOCKET_ERROR == ::send(this->_csock, &success, 1, 0))
			{
				break;
			}
			delete data;
		}
		if (result == -1)
		{
			break;
		}
	}
	::closesocket(this->_csock);
	NodeClient::Remove(this->_name->c_str());
	DataList::GetInstance()->DeleteData(*this->_name);
	delete this;
}
byte* NodeClient::GetOnlineServer()
{
	byte* result = nullptr;
	NodeClient::updateLock.lock();
	if (NodeClient::data != nullptr)
	{
		uint16_t datalength = *((uint16_t*)NodeClient::data);
		result = (byte*)malloc(datalength);
		memcpy(result, NodeClient::data, datalength);
	}
	NodeClient::updateLock.unlock();
	return result;
}

class NodeServer
{
private:
	static const int backlog;
protected:
	sockaddr_in _listenAddr;
	SOCKET _listener;
	bool _isStop;
	thread* _acceptThread;
public:
	NodeServer(string& host, int port);
protected:
	void AcceptThread();
};

const int NodeServer::backlog = 100;

NodeServer::NodeServer(string& host, int port)
	:_isStop(false)
{
	ZeroMemory(&this->_listenAddr, sizeof(sockaddr_in));
	this->_listenAddr.sin_family = AF_INET;
	this->_listenAddr.sin_port = htons(port);
	this->_listenAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	this->_listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	::bind(this->_listener, (sockaddr*)&this->_listenAddr, sizeof(SOCKADDR_IN));
	::listen(this->_listener, NodeServer::backlog);
	this->_acceptThread = new thread(mem_fn(&NodeServer::AcceptThread), this);
}

void NodeServer::AcceptThread()
{
	sockaddr remoteAddr;
	int addrLeng = sizeof(SOCKADDR);
	while (true)
	{
		if (this->_isStop)
			return;
		SOCKET remoteClient = ::accept(this->_listener, &remoteAddr, &addrLeng);
		auto client = new NodeClient(remoteClient);
		client->Start();
	}
}

int _tmain(int argc, _TCHAR* argv[])
{
	/*auto worker = new Worker();
	for (int i = 0; i < 100000; i++)
	{
		worker->SendData(i);
	}
	int aa = 0;
	cin >> aa;*/

	/*map<UINT32, UINT32>* mapss = new map<UINT32, UINT32>();
	mapss->insert(pair<UINT32, UINT32>(11111111, 1111));
	mapss->insert(pair<UINT32, UINT32>(11111112, 1112));
	mapss->insert(pair<UINT32, UINT32>(11111113, 1113));
	char* temp = new char[mapss->size() * 9];
	int start = 0;
	for (auto kvp : *mapss)
	{
		*(temp + start) = 4;
		*((UINT32*)(temp + start + 1)) = kvp.first;
		*((UINT32*)(temp + start + 5)) = kvp.second;
		start += 9;
	}*/
	WSADATA wsaData;
	int nRet;
	if ((nRet = WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0)
	{
		exit(0);
	}

	ClientProcess::GetInstance();

	int returnCode = _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE | _CRTDBG_MODE_WNDW);
	Server* server = new Server("10.2.0.182", 1525);
	server->Start();
	NodeServer* nodeServer = new NodeServer(string("10.2.0.182"), 5656);
	int rec = 0;
	while (true) 
	{
		if (ReadData())
		{
			cout << "解析执行成功" << endl;
		}
		else
		{
			cout << "解析执行失败!" << endl;
		}
	};
	server->Stop();
	return 0;
}

