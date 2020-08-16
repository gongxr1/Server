#include <stdio.h>
#include <iostream>
#include <vector>
#include <algorithm>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <WinSock2.h>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

// 要考虑字节对齐问题（32位和64位，平台和系统）
enum CMDTYPE
{
	CMD_LOGIN,			// 登录
	CMD_LOGIN_RESULT,	// 登录返回结果
	CMD_LOGOUT,			// 登出
	CMD_LOGOUT_RESULT,	// 登出返回结果
	CMD_NEW_USER_JOIN,	// 新用户加入
	CMD_ERROR			// 错误
};

// 消息头
struct DataHeader
{
	int	cmd;			// 命令类型
	int dataLength;		// 消息体的数据长度
};

// 消息体
// DataPackage
// 登录
struct Login : public DataHeader
{
	Login()
	{
		dataLength = sizeof(Login);
		cmd = CMD_LOGIN;
	}
	char userName[32];
	char passWord[32];
};

// 登录结果
struct LoginResult : public DataHeader
{
	LoginResult()
	{
		dataLength = sizeof(Login);
		cmd = CMD_LOGIN_RESULT;
		result = 0;
	}
	int result;
};

// 登出
struct Logout : public DataHeader
{
	Logout()
	{
		dataLength = sizeof(Logout);
		cmd = CMD_LOGOUT;
	}
	char userName[32];
};

// 登出结果
struct LogoutResult : public DataHeader
{
	LogoutResult()
	{
		dataLength = sizeof(LogoutResult);
		cmd = CMD_LOGOUT_RESULT;
		result = 0;
	}
	int result;
};

// 新用户加入
// 登出
struct NewUserJoin : public DataHeader
{
	NewUserJoin()
	{
		dataLength = sizeof(NewUserJoin);
		cmd = CMD_NEW_USER_JOIN;
		sock = 0;
	}
	int sock;
};

std::vector<SOCKET> g_clientList;	// 客户端套接字列表

int processor(SOCKET sock)
{
	// 缓冲区(4096字节)
	char szRecv[4096] = {};
	// 5、接收客户端的请求
	// 先接收消息头
	int recvLen = recv(sock, szRecv, sizeof(DataHeader), 0);
	DataHeader *pHeader = (DataHeader*)szRecv;
	if (recvLen <= 0)
	{
		printf("客户端<Socket=%d>已退出，任务结束...", sock);
		return -1;
	}

	// 6、处理请求
	switch (pHeader->cmd)
	{
	case CMD_LOGIN:
	{
		Login *login = (Login*)szRecv;

		recv(sock, szRecv + sizeof(DataHeader), pHeader->dataLength - sizeof(DataHeader), 0);
		printf("收到客户端<Socket=%d>请求：CMD_LOGIN, 数据长度：%d, userName：%s Password： %s\n",
			sock, login->dataLength, login->userName, login->passWord);
		// 忽略判断用户名和密码是否正确的过程
		LoginResult ret;
		send(sock, (char*)&ret, sizeof(LoginResult), 0);
	}
	break;
	case CMD_LOGOUT:
	{
		Logout *logout = (Logout*)szRecv;

		recv(sock, szRecv + sizeof(DataHeader), pHeader->dataLength - sizeof(DataHeader), 0);
		printf("收到客户端<Socket=%d>请求：CMD_LOGOUT, 数据长度：%d, userName：%s\n",
			sock, logout->dataLength, logout->userName);
		LogoutResult ret;
		send(sock, (char*)&ret, sizeof(LogoutResult), 0);
	}
	break;
	default:
	{
		DataHeader header = { 0, CMD_ERROR };
		send(sock, (char*)&header, sizeof(header), 0);
		break;
	}
	}

	return 0;
}

int main(int argc, char *agrv[])
{
	// 加载套接字库
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;

	wVersionRequested = MAKEWORD(2, 2);
	// 启动Windows Socket 2.x环境
	err = WSAStartup(wVersionRequested, &wsaData);

	// 使用Socket API建立简易的TCP服务端
	if (err != 0) {
		/* Tell the user that we could not find a usable */
		/* WinSock DLL.                                  */
		return 1;
	}

	/* Confirm that the WinSock DLL supports 2.2.*/
	/* Note that if the DLL supports versions greater    */
	/* than 2.2 in addition to 2.2, it will still return */
	/* 2.2 in wVersion since that is the version we      */
	/* requested.                                        */

	if (LOBYTE(wsaData.wVersion) != 2 ||
		HIBYTE(wsaData.wVersion) != 2) {
		/* Tell the user that we could not find a usable */
		/* WinSock DLL.                                  */
		WSACleanup();
		return 1;
	}

	/* The WinSock DLL is acceptable. Proceed. */
	 //----------------------
   // Create a SOCKET for listening for
   // incoming connection requests.
	SOCKET ListenSocket;
	ListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (ListenSocket == INVALID_SOCKET) {
		printf("Error at socket(): %ld\n", WSAGetLastError());
		WSACleanup();
		return 1;
	}

	//----------------------
	// The sockaddr_in structure specifies the address family,
	// IP address, and port for the socket that is being bound.
	sockaddr_in service;
	service.sin_family = AF_INET;
	//service.sin_addr.s_addr = inet_addr("127.0.0.1");
	service.sin_addr.s_addr = INADDR_ANY;
	service.sin_port = htons(27015);

	if (bind(ListenSocket,
		(SOCKADDR*)&service,
		sizeof(service)) == SOCKET_ERROR) {
		printf("bind() failed.绑定网络端口失败\n");
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}
	else
	{
		printf("绑定网络端口成功...\n");
	}

	//----------------------
	// Listen for incoming connection requests.
	// on the created socket
	if (listen(ListenSocket, 5) == SOCKET_ERROR) {
		printf("错误，监听网络端口失败...\n");
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}
	else
	{
		printf("监听网络端口成功...\n");
	}

	printf("等待客户端连接...\n");

	while (true)
	{
		// Berkeley sockets
		fd_set readfds;			// 描述符(socket)集合
		fd_set writefds;
		fd_set exceptfds;

		// 清理集合
		FD_ZERO(&readfds);
		FD_ZERO(&writefds);
		FD_ZERO(&exceptfds);

		// 将描述符(socket)加入集合
		FD_SET(ListenSocket, &readfds);
		FD_SET(ListenSocket, &writefds);
		FD_SET(ListenSocket, &exceptfds);

		for (int n = (int)g_clientList.size() - 1; n >= 0; n--)
		{
			FD_SET(g_clientList[n], &readfds);
		}

		// 设置超时时间 select 非阻塞
		timeval timeout = { 1, 0 };

		// nfds是一个整数值，是指fd_set集合中所有描述符(socket)的范围，而不是数量
		// 即是所有文件描述符最大值+1 在Windows中这个参数可以写0
		//int ret = select(ListenSocket + 1, &readfds, &writefds, &exceptfds, NULL);
		int ret = select(ListenSocket + 1, &readfds, &writefds, &exceptfds, &timeout);
		if (ret < 0)
		{
			printf("select任务结束,called failed:%d!\n", WSAGetLastError());
			break;
		}

		// 是否有数据可读
		// 判断描述符(socket)是否在集合中
		if (FD_ISSET(ListenSocket, &readfds))
		{
			//FD_CLR(ListenSocket, &readfds);

			// Create a SOCKET for accepting incoming requests.
			// 4. accept 等待接受客户端连接
			SOCKADDR_IN clientAddr = {};
			int nAddrLen = sizeof(SOCKADDR_IN);
			SOCKET ClientSocket = INVALID_SOCKET;
			ClientSocket = accept(ListenSocket, (SOCKADDR*)&clientAddr, &nAddrLen);
			if (INVALID_SOCKET == ClientSocket) {
				printf("accept() failed: %d,接收到无效客户端Socket\n", WSAGetLastError());
				return 1;
			}
			else
			{
				// 有新的客户端加入，向之前的所有客户端群发消息
				for (int n = (int)g_clientList.size() - 1; n >= 0; n--)
				{
					NewUserJoin userJoin;
					send(g_clientList[n], (const char*)&userJoin, sizeof(NewUserJoin), 0);
				}

				g_clientList.push_back(ClientSocket);
				// 客户端连接成功，则显示客户端连接的IP地址和端口号
				printf("新客户端<Sokcet=%d>加入,Ip地址：%s,端口号：%d\n", ClientSocket, inet_ntoa(clientAddr.sin_addr),
					ntohs(clientAddr.sin_port));
			}
		}

		for (int i = 0; i < (int)readfds.fd_count - 1; ++i)
		{
			if (-1 == processor(readfds.fd_array[i]))
			{
				auto iter = std::find(g_clientList.begin(), g_clientList.end(),
					readfds.fd_array[i]);
				if (iter != g_clientList.end())
				{
					g_clientList.erase(iter);
				}
			}
		}

		//printf("空闲时间处理其他业务...\n");
	}

	for (int n = (int)g_clientList.size() - 1; n >= 0; n--)
	{
		closesocket(g_clientList[n]);
	}

	// 8.关闭套接字
	closesocket(ListenSocket);
	// 9.清除Windows Socket环境
	WSACleanup();

	printf("服务端已退出，任务结束\n");

	getchar();

	return 0;
}