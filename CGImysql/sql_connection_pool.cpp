#include <mysql/mysql.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <list>
#include <pthread.h>
#include <iostream>
#include "sql_connection_pool.h"

using namespace std;

connection_pool::connection_pool()
{
	m_CurConn = 0;
	m_FreeConn = 0;
}

connection_pool *connection_pool::GetInstance()
{
	static connection_pool connPool;
	return &connPool;
}

//构造初始化
void connection_pool::init(string url, string User, string PassWord, string DBName, int Port, int MaxConn, int close_log)
{
	//初始化数据库信息
	m_url = url;
	m_Port = Port;
	m_User = User;
	m_PassWord = PassWord;
	m_DatabaseName = DBName;
	m_close_log = close_log;

	//创建MaxConn条数据库连接
	for (int i = 0; i < MaxConn; i++)
	{
		//这个函数用来分配或者初始化一个MYSQL对象，用于连接mysql服务端。如果你传入的参数是NULL指针，它将自动为你分配一个MYSQL对象
		MYSQL *con = NULL;
		con = mysql_init(con);//创建一个数据库连接

		if (con == NULL)
		{
			LOG_ERROR("MySQL Error");
			exit(1);
		}
		//建立连接：尝试与运行在主机上的MySQL数据库引擎建立连接
		//c_str()：生成一个const char*指针，指向以空字符终止的数组
		con = mysql_real_connect(con, url.c_str(), User.c_str(), PassWord.c_str(), DBName.c_str(), Port, NULL, 0);

		if (con == NULL)
		{
			LOG_ERROR("MySQL Error");//这里只有format，为"MySQL Error"，没有参数：...；所以只用照搬写入"MySQL Error"就行
			exit(1);
		}
		//更新连接池和空闲连接数量
		connList.push_back(con);
		++m_FreeConn;
	}
	//将信号量初始化为最大连接次数
	reserve = sem(m_FreeConn);

	m_MaxConn = m_FreeConn;
}


//当有请求时，从数据库连接池中返回一个可用连接，更新使用和空闲连接数(同pop()函数)
MYSQL *connection_pool::GetConnection()
{
	MYSQL *con = NULL;

	if (0 == connList.size())
		return NULL;
	//取出连接，信号量原子减1，为0则等待
	reserve.wait();
	
	lock.lock();

	con = connList.front();
	connList.pop_front();

	--m_FreeConn;
	++m_CurConn;

	lock.unlock();
	return con;
}

//释放当前使用的连接(con)(同push()函数)
bool connection_pool::ReleaseConnection(MYSQL *con)
{
	if (NULL == con)
		return false;

	lock.lock();

	connList.push_back(con);
	++m_FreeConn;
	--m_CurConn;

	lock.unlock();

	reserve.post();
	return true;
}

//销毁数据库连接池
void connection_pool::DestroyPool()
{

	lock.lock();
	if (connList.size() > 0)
	{
		list<MYSQL *>::iterator it;
		for (it = connList.begin(); it != connList.end(); ++it)
		{
			MYSQL *con = *it;
			mysql_close(con);//数据库关闭一个连接
		}
		m_CurConn = 0;
		m_FreeConn = 0;
		connList.clear();
	}

	lock.unlock();
}

//当前空闲的连接数
int connection_pool::GetFreeConn()
{
	return this->m_FreeConn;
}

connection_pool::~connection_pool()
{
	DestroyPool();//析构的是销毁整个连接库
}
//不直接调用获取和释放连接的接口，将其封装起来，通过RAII机制进行获取和释放，避免手动释放
//其中数据库连接本身是指针类型，所以参数需要通过双指针才能对其进行修改
connectionRAII::connectionRAII(MYSQL **SQL, connection_pool *connPool){
	*SQL = connPool->GetConnection();
	//即，我们在使用的时候，通过connectionRAII的成员来获取数据库和连接
	conRAII = *SQL;
	poolRAII = connPool;
}

connectionRAII::~connectionRAII(){
	poolRAII->ReleaseConnection(conRAII);
}