/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-26 The DingusPPC Development Team
          (See CREDITS.MD for more details)

(You may also contact divingkxt or powermax2286 on Discord)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

/** Character I/O definitions. */

#ifndef CHAR_IO_H
#define CHAR_IO_H

#include <cinttypes>
#include <string>
#include <map>

#ifdef _WIN32
#else
#include <termios.h>
#include <signal.h>
#include <sys/select.h>
#endif

enum {
    CHARIO_BE_NULL  = 0, // NULL backend: swallows everything, receives nothing
    CHARIO_BE_STDIO = 1, // STDIO backend: uses STDIN for input and STDOUT for output
    CHARIO_BE_SOCKET = 2, // socket backend: uses a socket for input and output
};

/** Interface for character I/O backends. */
class CharIoBackEnd {
public:
    CharIoBackEnd(const std::string &name);
    virtual ~CharIoBackEnd();

    virtual int rcv_enable() { return 0; }
    virtual void rcv_disable() {}
    virtual bool rcv_char_available();
    virtual bool rcv_char_available_now() = 0;
    virtual int xmit_char(uint8_t c) = 0;
    virtual int rcv_char(uint8_t *c) = 0;
    bool is_consecutive_char();
    void increment_consecutive_chars();
    void reset_consecutive_chars();

private:
    std::string name;
    int     consecutivechars = 0;
    int     chars_consecutive_max = 15;
    int     chars_consecutive_reset = 800;
};

/** Null character I/O backend. */
class CharIoNull : public CharIoBackEnd {
public:
    CharIoNull(const std::string &name) : CharIoBackEnd(name) {}
    ~CharIoNull() = default;

    bool rcv_char_available_now();
    int xmit_char(uint8_t c);
    int rcv_char(uint8_t *c);
};

/** Stdin character I/O backend. */
class CharIoStdin : public CharIoBackEnd  {
public:
    CharIoStdin(const std::string &name) : CharIoBackEnd(name) { this->stdio_inited = false; }
    ~CharIoStdin() = default;

    int rcv_enable();
    void rcv_disable();
    bool rcv_char_available_now();
    int xmit_char(uint8_t c);
    int rcv_char(uint8_t *c);

private:
    static void mysig_handler(int signum);
    bool    stdio_inited;
};

/** Socket character I/O backend. */
class CharIoSocket : public CharIoBackEnd  {
public:
    CharIoSocket(const std::string &name, const std::string &path);
    ~CharIoSocket();

    int rcv_enable();
    void rcv_disable();
    bool rcv_char_available_now();
    int xmit_char(uint8_t c);
    int rcv_char(uint8_t *c);

private:
    void check_all_fds(int &sel_rv, fd_set (&sets)[3]);
    void close_accept_fd();

    bool    socket_inited = false;
    int     sockfd = -1;
    int     acceptfd = -1;
    std::string path;
};

/** Socket cache which servives machine shutdown/restart. */
class SocketCache {
public:
    typedef struct {
        int sockfd;
        int acceptfd;
    } SocketInfo;

    static SocketCache* get_instance() {
        if (!socketcache_obj) {
            socketcache_obj = std::unique_ptr<SocketCache>(new SocketCache());
        }
        return socketcache_obj.get();
    }

    static void delete_instance() {
        delete socketcache_obj.release();
    }

    ~SocketCache();

    std::map<std::string, SocketInfo> sockets;
private:
    inline static std::unique_ptr<SocketCache> socketcache_obj{nullptr};
    explicit SocketCache(); // private constructor to implement a singleton
};

#endif // CHAR_IO_H
