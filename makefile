# server: main.cpp ./threadpool/threadpool.h ./http/http_conn.cpp ./http/http_conn.h ./locker/locker.h ./log/log.cpp ./log/log.h ./log/block_queue.h ./mysql/sql_connection_pool.cpp ./mysql/sql_connection_pool.h
# 	g++ -o server main.cpp ./threadpool/threadpool.h ./http/http_conn.cpp ./http/http_conn.h ./locker/locker.h ./log/log.cpp ./log/log.h ./mysql/sql_connection_pool.cpp ./mysql/sql_connection_pool.h -lpthread -lmysqlclient

# check.cgi:./mysql/sign.cpp ./mysql/sql_connection_pool.cpp ./mysql/sql_connection_pool.h
# 	g++ -o check.cgi ./mysql/sign.cpp ./mysql/sql_connection_pool.cpp ./mysql/sql_connection_pool.h -lmysqlclient

server: main.cpp ./threadpool/threadpool.h ./http/http_conn.cpp ./http/http_conn.h ./locker/locker.h ./log/log.cpp ./log/log.h ./log/block_queue.h ./mysql/sql_connection_pool.cpp ./mysql/sql_connection_pool.h
	g++ -o server main.cpp ./threadpool/threadpool.h ./http/http_conn.cpp ./http/http_conn.h ./locker/locker.h ./log/log.cpp ./log/log.h ./mysql/sql_connection_pool.cpp ./mysql/sql_connection_pool.h -lpthread -lmysqlclient

# check.cgi:./mysql/sign.cpp ./mysql/sql_connection_pool.cpp ./mysql/sql_connection_pool.h
# 	g++ -o check.cgi ./mysql/sign.cpp ./mysql/sql_connection_pool.cpp ./mysql/sql_connection_pool.h -lmysqlclient


clean:
	rm  -r server
	rm  -r check.cgi
