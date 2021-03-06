/* Copyright 2016-2020 the Age of Empires Free Software Remake authors. See LEGAL for legal info */

#pragma once

#include "../os_macros.hpp"
#include "types.hpp"

#include <cstdint>

#include <vector>
#include <atomic>
#include <string>
#include <set>
#include <map>
#include <queue>
#include <mutex>

#if windows
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>

typedef SOCKET sockfd; /**< wrapper for low-level socket descriptor */
typedef WSAPOLLFD pollev; /**< wrapper for low-level incoming slave network events. */

static inline SOCKET pollfd(const pollev &ev) { return ev.fd; }
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>

#include <arpa/inet.h>
#include <netinet/in.h>
typedef int sockfd; /**< wrapper for low-level socket descriptor */
typedef epoll_event pollev; /**< wrapper for low-level incoming slave network events. */

// windows already provides one in winsock2, so define this on posix to make it more consistent
#define INVALID_SOCKET ((int)-1)

static inline int pollfd(const epoll_event &ev) { return ev.data.fd; }
static inline int pollfd(int fd) { return fd; }
#endif

namespace genie {

int net_get_error();
bool str_to_ip(const std::string &str, uint32_t &addr);

class Net final {
public:
	Net();
	~Net();
};

static constexpr unsigned MAX_SLAVES = 64; /**< Maximum concurrent amount of slaves that may connect. */
static constexpr unsigned NAME_LIMIT = 24;
static constexpr unsigned TEXT_LIMIT = 32;

/**
 * Low-level event to indicate a new user has joined the server.
 * The id is guaranteed to be unique.
 */
struct JoinUser final {
	user_id id;
	char name[NAME_LIMIT];

	JoinUser() = default;
	JoinUser(user_id id, const std::string &str);

	std::string nick();
};

struct StartMatch final {
	/*
	scenario: random map/death match/custom scenario
	seed: (used when not custom scenario for) initial state for random number generator
	map size: width x height
	victory condition: enum
	map type: enum
	starting age: enum
	difficulty level: enum
	XXX path finding: do we really want to specify this? the servers handles this anyway...
	options: fixed positions (y/n), full tech tree (y/n), reveal map (y/n), cheating (y/n)
	...
	*/
	uint8_t scenario_type, options;
	uint16_t map_w, map_h;
	uint32_t seed;
	uint8_t map_type, difficulty, starting_age, victory;
	uint16_t slave_count; /**< number of connected clients/users to server */

	static StartMatch random(unsigned slave_count, unsigned player_count);

	void dump() const;
};

struct TextMsg final {
	user_id from;
	char text[TEXT_LIMIT];

	std::string str() const;
};

struct Ready final {
	uint16_t slave_count;

	friend bool operator==(const Ready &lhs, const Ready &rhs) {
		return lhs.slave_count == rhs.slave_count;
	}

	friend bool operator!=(const Ready &lhs, const Ready &rhs) {
		return !(lhs == rhs);
	}
};

struct CreatePlayer final {
	player_id id;
	char name[NAME_LIMIT];

	CreatePlayer() = default;
	CreatePlayer(player_id id, const std::string &str);

	std::string str() const;
};

struct AssignSlave final {
	user_id from;
	player_id to;
};

union CmdData final {
	TextMsg text;
	JoinUser join;
	user_id leave;
	StartMatch start;
	Ready ready;
	CreatePlayer create;
	AssignSlave assign;
	uint8_t gamestate;

	void hton(uint16_t type);
	void ntoh(uint16_t type);
};

static constexpr unsigned CMD_HDRSZ = 4; /**< The network packet header in bytes. */

class CmdBuf;

enum class CmdType {
	text,
	join,
	leave,
	start,
	ready,
	create,
	assign,
	gamestate,
	max,
};

/** Mid-level wrapper for low-level network data and simple interface for high-level network game events. */
class Command final {
	friend CmdBuf;

public:
	uint16_t type, length;
	union CmdData data;
	TextMsg text();
	JoinUser join();
	Ready ready();
	uint8_t gamestate();

	void hton();
	void ntoh();

	static Command text(user_id id, const std::string &str);
	static Command join(user_id id, const std::string &str);
	static Command leave(user_id id);
	static Command start(StartMatch &match);
	static Command ready(uint16_t slave_count, uint16_t prng_next);
	static Command create(player_id id, const std::string &str);
	static Command assign(user_id id, player_id pid);
	static Command gamestate(uint8_t type);
};

class ServerCallback {
public:
	virtual void incoming(pollev &ev) = 0;
	virtual void removepeer(sockfd fd) = 0;
	virtual void shutdown() = 0;
	virtual void event_process(sockfd fd, Command &cmd) = 0;
};

/** ServerSocket errors */
enum class SSErr {
	OK,
	BADFD,
	PENDING,
	WRITE,
};

class CmdBuf final {
	/** Total size in bytes and number of bytes read/written with the underlying socket. */
	unsigned size, transmitted;
	/** Communication device. */
	sockfd endpoint;
	/** The command to be read or sent in *network* byte endian order. */
	Command cmd;
public:
	CmdBuf(sockfd fd);
	CmdBuf(sockfd fd, const Command &cmd, bool net_order=false);

	int read(ServerCallback &cb, char *buf, unsigned len);
	/** Try to send the command completely. Zero is returned if the all data has been sent. */
	SSErr write();

	friend bool operator<(const CmdBuf &lhs, const CmdBuf &rhs);
};

class ServerSocket;

class Socket final {
	friend ServerSocket;

	sockfd fd;
	uint16_t port;
public:
	/** Construct server accepted socket. If you want to specify the port (for e.g. bind, connect), you have to use the second ctor. */
	Socket();
	Socket(uint16_t port);
	~Socket();

	void block(bool enabled=true);
	void reuse(bool enabled=true);

	void bind();
	void listen();
	int connect();
	int connect(uint32_t addr, bool netorder=false);

	void close();

	int send(const void *buf, unsigned size);
	int recv(void *buf, unsigned size);

	/**
	 * Block until all data has been fully send.
	 * It is UB to call this if the socket is in non-blocking mode.
	 */
	void sendFully(const void *buf, unsigned len);
	void recvFully(void *buf, unsigned len);

	template<typename T> int send(const T &t) {
		return send((const void*)&t, sizeof t);
	}

	int recv(Command &cmd);

	void send(Command &cmd, bool net_order=false);
};

class ServerSocket final {
	Socket sock;
#if linux
	int efd;
	std::set<int> peers;
#elif windows
	std::vector<pollev> peers, keep;
	bool poke_peers;
#endif
	/** Cache for any pending read operations. */
	std::set<CmdBuf> rbuf;
	/** Cache for any pending write operations. */
	std::map<sockfd, std::queue<CmdBuf>> wbuf;
	std::atomic<bool> activated, accepting;
	std::recursive_mutex mut; /**< Makes all sockets manipulations thread safe. */
public:
	ServerSocket(uint16_t port);
	~ServerSocket();

	bool accept() const { return accepting.load(); }
	/** Control whether we accept incoming clients. It is disabled when a game is running. */
	void accept(bool b) { accepting.store(b); }

	void close();

	SSErr push(sockfd fd, const Command &cmd, bool net_order=false);
	void broadcast(ServerCallback &cb, Command &cmd, bool net_order=false, bool ignore_bad=false);
	void broadcast(ServerCallback &cb, Command &cmd, sockfd fd, bool net_order=false);

private:
	SSErr push_unsafe(sockfd fd, const Command &cmd, bool net_order=false);
	void removepeer(ServerCallback&, sockfd fd);
#if linux
	void incoming(ServerCallback&);
	int event_process(ServerCallback&, pollev &ev);
#endif
public:
	void eventloop(ServerCallback&);
};

}
