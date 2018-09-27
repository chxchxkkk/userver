#ifndef __TCP_CONN_H__
#define __TCP_CONN_H__

#include <string>
#include <memory>
#include "uv.h"
#include "platform.h"
#include "buffer.h"

namespace gx {

	class Buffer;
	class TCPServer;
	struct ISerial;

	enum class ConnState {
		connecting,
		connected,
		closed,
	};
	class TCPConn : public std::enable_shared_from_this<TCPConn>
	{

		typedef struct {
			uv_write_t req;
			uv_buf_t buf;
		} write_req_t;

		typedef std::function<void()> cb_handler;

	public:
		TCPConn(uint32_t, TCPServer*, uv_stream_t*);
		TCPConn(uv_loop_t* loop = uv_default_loop());
		~TCPConn();

		TCPConn(const TCPConn&) = delete;
		TCPConn& operator=(const TCPConn&) = delete;

		static Slice pack(Buffer& buf, ISerial* req, uint32_t seq = 0);

		void State(ConnState cs) {
			_state = cs;
		}
		bool is_connect() {
			return _state == ConnState::connected;
		}

		uint32_t id() {
			return _id;
		}

		bool send(const char *buf, size_t nbuf);
		bool send(uint32_t seq, ISerial* req);
		bool send(ISerial* req) {
			return send(_genseq++, req);
		}
		void connect(const char* ip, int port);
		void close();
		
		void connect_ok_handler(cb_handler handler) {
			_connect_ok_handler = handler;
		}
		void connect_fail_handler(cb_handler handler) {
			_connect_fail_handler = handler;
		}
		void disconnect_handler(cb_handler handler) {
			_disconnect_handler = handler;
		}

	private:
		static void alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);
		static void after_read(uv_stream_t* handle, ssize_t nread, const uv_buf_t* buf);
		static void after_write(uv_write_t* req, int status);
		static void after_connect(uv_connect_t* handle, int status);
		static void after_close(uv_handle_t *handle);

		void init(uv_loop_t *loop);
		void on_recv(char* buf, size_t nread);
		int on_request(uint16_t msgid, uint32_t req, Buffer* buf);
		int on_response(uint16_t msgid, uint32_t req, Buffer* buf);

	private:
		static const int READ_BUF_SIZE;
		static const int WRITE_BUF_SIZE;
		static Buffer _send_tmp_buf;
		static uint32_t _genseq;
		Buffer _read_msg_buf;
		uv_tcp_t _socket;
		uv_buf_t _read_buf;			//�������ݵ�buf
		uv_connect_t _connect_req;	//����ʱ����
		uv_write_t _write_req;
		TCPServer* _server{nullptr};
		ConnState _state{ConnState::closed};
		void *_user_data{nullptr};
		uint32_t _seq{0};
		uint32_t _id{0};

		cb_handler _connect_ok_handler{nullptr};
		cb_handler _connect_fail_handler{nullptr};
		cb_handler _disconnect_handler{nullptr};
	};

}

#endif
