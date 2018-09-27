#include "conn.h"
#include "uv.h"
#include <stdio.h>
#include <stdlib.h>
#include <cassert>

namespace gx {

	uv_loop_t* Conn::loop;
	int Conn::server_closed;
	stream_type Conn::serverType;
	uv_tcp_t Conn::tcpServer;
	uv_handle_t* Conn::server;

	Conn::Conn() {
		loop = uv_default_loop();

		if (tcp4_echo_start(34567))
			return;

		uv_run(loop, UV_RUN_DEFAULT);
		return;
	}

	Conn::~Conn() {

	}

	void Conn::after_write(uv_write_t* req, int status) {

		write_req_t* wr;

		/* Free the read/write buffer and the request */
		wr = (write_req_t*)req;
		free(wr->buf.base);
		free(wr);

		if (status == 0)
			return;

		fprintf(stderr,
			"uv_write error: %s - %s\n",
			uv_err_name(status),
			uv_strerror(status));
	}


	void Conn::after_shutdown(uv_shutdown_t* req, int status) {

		uv_close((uv_handle_t*)req->handle, on_close);
		free(req);
	}


	void Conn::after_read(uv_stream_t* handle,
		ssize_t nread,
		const uv_buf_t* buf) {

		int i;
		write_req_t *wr;
		uv_shutdown_t* sreq;

		if (nread < 0) {

			/* Error or EOF */
			assert(nread == UV_EOF);

			free(buf->base);
			sreq = (uv_shutdown_t*)malloc(sizeof* sreq);
			assert(0 == uv_shutdown(sreq, handle, after_shutdown));
			return;
		}

		if (nread == 0) {

			/* Everything OK, but nothing read. */
			free(buf->base);
			return;
		}

		/*
		 *    * Scan for the letter Q which signals that we should quit the server.
		 *       * If we get QS it means close the stream.
		 *          */
		if (!server_closed) {

			for (i = 0; i < nread; i++) {
				printf("%c", buf->base[i]);

				if (buf->base[i] == 'Q') {

					if (i + 1 < nread && buf->base[i + 1] == 'S') {

						free(buf->base);
						uv_close((uv_handle_t*)handle, on_close);
						return;
					}
					else {

						uv_close(server, on_server_close);
						server_closed = 1;
					}
				}
			}
			printf("\n");
		}

		wr = (write_req_t*)malloc(sizeof *wr);
		assert(wr != NULL);
		wr->buf = uv_buf_init(buf->base, (int)nread);

		if (uv_write(&wr->req, handle, &wr->buf, 1, after_write)) {
			assert(false);
		}
	}


	void Conn::on_close(uv_handle_t* peer) {

		free(peer);
	}


	void Conn::echo_alloc(uv_handle_t* handle,
		size_t suggested_size,
		uv_buf_t* buf) {

		buf->base = (char*)malloc(suggested_size);
		buf->len = suggested_size;
	}


	void Conn::on_connection(uv_stream_t* server, int status) {

		uv_stream_t* stream;
		int r;

		if (status != 0) {

			fprintf(stderr, "Connect error %s\n", uv_err_name(status));
		}
		assert(status == 0);

		switch (serverType) {

		case TCP:
			stream = (uv_stream_t*)malloc(sizeof(uv_tcp_t));
			assert(stream != NULL);
			r = uv_tcp_init(loop, (uv_tcp_t*)stream);
			assert(r == 0);
			break;

		case PIPE:
			stream = (uv_stream_t*)malloc(sizeof(uv_pipe_t));
			assert(stream != NULL);
			r = uv_pipe_init(loop, (uv_pipe_t*)stream, 0);
			assert(r == 0);
			break;

		default:
			assert(0 && "Bad serverType");
			abort();
		}

		/* associate server with stream */
		stream->data = server;

		r = uv_accept(server, stream);
		assert(r == 0);

		r = uv_read_start(stream, echo_alloc, after_read);
		assert(r == 0);
	}


	void Conn::on_server_close(uv_handle_t* handle) {

		assert(handle == server);
	}


	int Conn::tcp4_echo_start(int port) {
		struct sockaddr_in addr;
		int r;

		assert(0 == uv_ip4_addr("0.0.0.0", port, &addr));

		server = (uv_handle_t*)&tcpServer;
		serverType = TCP;

		r = uv_tcp_init(loop, &tcpServer);
		if (r) {

			/* TODO: Error codes */
			fprintf(stderr, "Socket creation error\n");
			return 1;
		}

		r = uv_tcp_bind(&tcpServer, (const struct sockaddr*) &addr, 0);
		if (r) {

			/* TODO: Error codes */
			fprintf(stderr, "Bind error\n");
			return 1;
		}

		r = uv_listen((uv_stream_t*)&tcpServer, SOMAXCONN, on_connection);
		if (r) {

			/* TODO: Error codes */
			fprintf(stderr, "Listen error %s\n", uv_err_name(r));
			return 1;
		}

		return 0;
	}

}

