#ifndef TMATE_H
#define TMATE_H

#include <sys/syslog.h>
#include <sys/types.h>
#include <msgpack.h>
#include <libssh/libssh.h>
#include <libssh/callbacks.h>
#include <event.h>
#include <time.h>

#include "tmux.h"
struct tmate_session;

/* log.c */

extern void init_logging(const char *program_name, bool use_syslog, int log_level);
extern void printflike2 tmate_log(int level, const char *msg, ...);

#define tmate_debug(str, ...)	tmate_log(LOG_DEBUG, str, ##__VA_ARGS__)
#define tmate_info(str, ...)	tmate_log(LOG_INFO, str, ##__VA_ARGS__)
#define tmate_notice(str, ...)	tmate_log(LOG_NOTICE, str, ##__VA_ARGS__)
#define tmate_warn(str, ...)	tmate_log(LOG_WARNING, str, ##__VA_ARGS__)
#define tmate_fatal(str, ...)					\
({								\
	tmate_log(LOG_CRIT, "fatal: " str, ##__VA_ARGS__);	\
 	exit(1);						\
})

/* tmate-msgpack.c */

typedef void tmate_encoder_write_cb(void *userdata, struct evbuffer *buffer);

struct tmate_encoder {
	msgpack_packer pk;
	tmate_encoder_write_cb *ready_callback;
	void *userdata;
	struct evbuffer *buffer;
	struct event ev_buffer;
	bool ev_active;
};

extern void tmate_encoder_init(struct tmate_encoder *encoder,
			       tmate_encoder_write_cb *callback,
			       void *userdata);

#define NEW_MSGPACK_API 0

#if NEW_MSG_PACK
#define msgpack_pack_buffer(pk, buf, len) ({		\
	msgpack_pack_bin(pk, len);			\
	msgpack_pack_bin_body(pk, buf, len);		\
})

#define msgpack_pack_string(pk, str) ({			\
	int __strlen = strlen(str);			\
	msgpack_pack_str(pk, __strlen);			\
	msgpack_pack_str_body(pk, str, __strlen);	\
})
#else
/* old msgpack version */
#define msgpack_pack_buffer(pk, buf, len) ({		\
	msgpack_pack_raw(pk, len);			\
	msgpack_pack_raw_body(pk, buf, len);		\
})

#define msgpack_pack_string(pk, str) ({			\
	int __strlen = strlen(str);			\
	msgpack_pack_raw(pk, __strlen);			\
	msgpack_pack_raw_body(pk, str, __strlen);	\
})
#endif

#define _pack(enc, what, ...) msgpack_pack_##what(&(enc)->pk, __VA_ARGS__)

struct tmate_unpacker;
struct tmate_decoder;
typedef void tmate_decoder_reader(void *userdata, struct tmate_unpacker *uk);

struct tmate_decoder {
	struct msgpack_unpacker unpacker;
	tmate_decoder_reader *reader;
	void *userdata;
};

extern void tmate_decoder_init(struct tmate_decoder *decoder, tmate_decoder_reader *reader, void *userdata);
extern void tmate_decoder_get_buffer(struct tmate_decoder *decoder, char **buf, size_t *len);
extern void tmate_decoder_commit(struct tmate_decoder *decoder, size_t len);

struct tmate_unpacker {
	msgpack_object *argv;
	int argc;
};

extern void init_unpacker(struct tmate_unpacker *uk, msgpack_object obj);
extern int64_t unpack_int(struct tmate_unpacker *uk);
extern void unpack_buffer(struct tmate_unpacker *uk, const char **buf, size_t *len);
extern char *unpack_string(struct tmate_unpacker *uk);
extern void unpack_array(struct tmate_unpacker *uk, struct tmate_unpacker *nested);

#define unpack_each(nested_uk, tmp_uk, uk)						\
	for (unpack_array(uk, tmp_uk);							\
	     (tmp_uk)->argc > 0 && (init_unpacker(nested_uk, (tmp_uk)->argv[0]), 1);	\
	     (tmp_uk)->argv++, (tmp_uk)->argc--)

/* tmate-daemon-encoder.c */

#define TMATE_LATEST_VERSION "1.8.10"

extern void printflike1 tmate_notify(const char *fmt, ...);
extern void printflike2 tmate_notify_later(int timeout, const char *fmt, ...);
extern void tmate_notify_client_join(struct client *c);
extern void tmate_notify_client_left(struct client *c);

extern void tmate_client_resize(u_int sx, u_int sy);
extern void tmate_client_pane_key(int pane_id, int key);
extern void tmate_client_cmd(int client_id, const char *cmd);
extern void tmate_client_set_active_pane(int client_id, int win_idx, int pane_id);
extern int tmate_should_exec_cmd_locally(const struct cmd_entry *cmd);
extern void tmate_send_env(const char *name, const char *value);
extern void tmate_send_client_ready(void);

/* tmate-daemon-decoder.c */

#define TMATE_HLIMIT 2000
#define TMATE_PANE_ACTIVE 1

extern char *tmate_left_status, *tmate_right_status;
extern void tmate_dispatch_daemon_message(struct tmate_session *session,
					  struct tmate_unpacker *uk);

/* tmate-ssh-client.c */

#define TMATE_KEYFRAME_INTERVAL_SEC 10
#define TMATE_KEYFRAME_MAX_SIZE 1024*1024

extern void tmate_daemon_init(struct tmate_session *session);

/* tmate-ssh-client-pty.c */

extern void tmate_client_pty_init(struct tmate_session *session);
extern void tmate_flush_pty(struct tmate_session *session);

/* tmate-ssh-server.c */

#define TMATE_SSH_BANNER "tmate"
#define TMATE_SSH_KEEPALIVE 60

#define TMATE_ROLE_DAEMON	1
#define TMATE_ROLE_PTY_CLIENT	2

struct tmate_ssh_client {
	char ip_address[64];

	ssh_session session;
	ssh_channel channel;
	/*
	 * We need to store the entire callback struct because
	 * libssh stores the userdata within the cb struct...
	 */
	struct ssh_channel_callbacks_struct channel_cb;

	int role;

	char *username;
	char *pubkey;

	struct winsize winsize_pty;

	struct event ev_ssh;
	struct event ev_keepalive_timer;
};

extern void tmate_ssh_server_main(struct tmate_session *session,
				  const char *keys_dir, int port);

/* tmate-slave.c */

#ifdef DEVENV
#define TMATE_SSH_DEFAULT_PORT 2200
#else
#define TMATE_SSH_DEFAULT_PORT 22
#endif

#define TMATE_SSH_GRACE_PERIOD 60

#define TMATE_SSH_DEFAULT_KEYS_DIR "keys"

#define TMATE_DEFAULT_MASTER_PORT 7000

#define TMATE_TOKEN_LEN 25
#define TMATE_WORKDIR "/tmp/tmate"
#define TMATE_JAIL_USER "nobody"

struct tmate_settings {
	const char *keys_dir;
	int ssh_port;
	const char *master_hostname;
	int master_port;
	const char *tmate_host;
	int log_level;
	bool use_syslog;
};
extern struct tmate_settings *tmate_settings;

struct tmate_session {
	struct tmate_ssh_client ssh_client;
	int tmux_socket_fd;

	/* only for deamon */
	const char *session_token;
	const char *session_token_ro;

	struct tmate_encoder daemon_encoder;
	struct tmate_decoder daemon_decoder;
	int client_protocol_version;
	struct event ev_notify_timer;

	int master_fd;
	struct bufferevent *bev_master;
	struct tmate_encoder master_encoder;
	struct tmate_decoder master_decoder;

	struct timespec session_start_time;
	struct timespec keyframe_start_time;
	unsigned int keyframe_cnt;
	size_t keyframe_size;

	/* only for client-pty */
	int pty;
	struct event ev_pty;
	bool readonly;
};

extern struct tmate_session *tmate_session;
extern void tmate_get_random_bytes(void *buffer, ssize_t len);
extern long tmate_get_random_long(void);
extern void request_server_termination(void);
extern void tmate_spawn_slave(struct tmate_session *session);

/* tmate-master.c */

extern void tmate_send_master_keyframe(struct tmate_session *session);
extern void tmate_send_master_daemon_msg(struct tmate_session *session,
					 struct tmate_unpacker *uk);
extern void tmate_send_master_header(struct tmate_session *session);
extern void tmate_init_master_session(struct tmate_session *session);

extern int tmate_connect_to_master(void);
static inline bool tmate_has_master(void)
{
	return !!tmate_settings->master_hostname;
}


extern void timespec_subtract(struct timespec *result,
			      struct timespec *x, struct timespec *y);
extern unsigned long long timespec_to_millisec(struct timespec *ts);

/* tmate-debug.c */

extern void tmate_preload_trace_lib(void);
extern void tmate_print_stack_trace(void);

/* tmux-bare.c */

extern void tmux_server_init(int flags);

#endif