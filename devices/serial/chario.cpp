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

/** Character I/O backend implementations. */

#include <core/timermanager.h>
#include <devices/serial/chario.h>
#include <loguru.hpp>

#include <cinttypes>
#include <cstring>
#include <memory>
#include <thread>

namespace loguru {
    enum : Verbosity {
        Verbosity_RECEIVETHREAD = loguru::Verbosity_INFO,
    };
}

//======================== BASE character I/O backend ========================
CharIoBackEnd::CharIoBackEnd(const std::string &name)
{
    this->name = name;
    LOG_F(INFO, "Created %s", this->get_name().c_str());
}

CharIoBackEnd::~CharIoBackEnd()
{
    LOG_F(INFO, "Deleted %s", this->get_name().c_str());
}

bool CharIoBackEnd::rcv_char_available()
{
    if (this->is_consecutive_char())
        return this->rcv_char_available_now();
    return false;
}

bool CharIoBackEnd::is_consecutive_char()
{
    if (this->consecutivechars >= this->chars_consecutive_max) {
        this->increment_consecutive_chars();
        return false;
    }
    return true;
}

void CharIoBackEnd::increment_consecutive_chars()
{
    this->consecutivechars++;
    if (this->consecutivechars >= chars_consecutive_reset)
        this->reset_consecutive_chars();
}

void CharIoBackEnd::reset_consecutive_chars()
{
    this->consecutivechars = 0;
}

void CharIoBackEnd::set_text_pacing(bool pacing) {
    if (pacing) {
        // 3 = receive fifo size
        // - For pasting into Open Firmware because read-ahead calls
        // scc-read with a buffer length of 4 but the buffer gets overrun
        // because scc-read only checks the buffer length once.
        chars_consecutive_max = 3;

        // Pasting into Open Firmware drops characters if this is too low.
        // 100 is too low for iMac 1.3f2
        chars_consecutive_reset = 110;
    }
    else {
        // max is never reached (because it is greater than reset)
        // so characters are allways accepted
        chars_consecutive_max = 1 << 30;
        chars_consecutive_reset = 1 << 29;
    }
}

//======================== NULL character I/O backend ========================
bool CharIoNull::rcv_char_available_now()
{
    return false;
}

int CharIoNull::xmit_chars(uint8_t *buf, int len)
{
    return 0;
}

int CharIoNull::rcv_chars(uint8_t *buf, int len)
{
    return 0;
}

//======================== STDIO character I/O backend ========================
#ifdef _WIN32

#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <windows.h>

HANDLE hInput  = GetStdHandle(STD_INPUT_HANDLE);
HANDLE hOutput = GetStdHandle(STD_OUTPUT_HANDLE);
DWORD old_in_mode, old_out_mode;
int old_stdin_trans_mode;

void CharIoStdin::mysig_handler(int signum) {
    SetStdHandle(signum, hInput);
    SetStdHandle(signum, hOutput);
}

int CharIoStdin::rcv_enable() {
    if (this->stdio_inited)
        return 0;

    GetConsoleMode(hInput, &old_in_mode);
    GetConsoleMode(hOutput, &old_out_mode);

    DWORD new_in_mode = old_in_mode;
    new_in_mode &= ~ENABLE_ECHO_INPUT;
    new_in_mode &= ~ENABLE_LINE_INPUT;
    new_in_mode &= ~ENABLE_PROCESSED_INPUT;

    new_in_mode |= ENABLE_EXTENDED_FLAGS;
    new_in_mode |= ENABLE_INSERT_MODE;
    new_in_mode |= ENABLE_VIRTUAL_TERMINAL_INPUT;

    SetConsoleMode(hInput, new_in_mode);

    SetConsoleMode(hOutput, old_out_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    // disable automatic CRLF translation
    old_stdin_trans_mode = _setmode(_fileno(stdin), _O_BINARY);

    this->stdio_inited = true;

    LOG_F(INFO, "%s: Winterm: receiver initialized", this->get_name().c_str());

    return 0;
}

void CharIoStdin::rcv_disable() {
    if (!this->stdio_inited)
        return;

    // restore original console mode
    SetConsoleMode(hInput, old_in_mode);
    SetConsoleMode(hOutput, old_out_mode);

    // restore original translation mode
    _setmode(_fileno(stdin), old_stdin_trans_mode);

    this->stdio_inited = false;

    LOG_F(INFO, "%s: Winterm: receiver disabled", this->get_name().c_str());
}

bool CharIoStdin::rcv_char_available_now() {
    DWORD events;
    INPUT_RECORD buffer;

    PeekConsoleInput(hInput, &buffer, 1, &events);
    if (events > 0)
        this->increment_consecutive_chars();
    else
        this->reset_consecutive_chars();
    return events > 0;
}

int CharIoStdin::xmit_chars(uint8_t *buf, int len) {
    return _write(_fileno(stdout), buf, len);
}

int CharIoStdin::rcv_chars(uint8_t *buf, int len) {
    return (int)_read(_fileno(stdin), buf, len);
}

#else // non-Windows OS (Linux, mac OS etc.)

#include <fcntl.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <unistd.h>

static struct sigaction    old_act_sigint, new_act_sigint;
static struct sigaction    old_act_sigterm, new_act_sigterm;
static struct termios      orig_termios;

void CharIoStdin::mysig_handler(int signum)
{
    // restore original terminal state
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);

    // restore original signal handler for SIGINT
    signal(SIGINT, old_act_sigint.sa_handler);
    signal(SIGTERM, old_act_sigterm.sa_handler);

    LOG_F(INFO, "Old terminal state restored, SIG#=%d", signum);

    // re-post signal
    raise(signum);
}

int CharIoStdin::rcv_enable()
{
    if (this->stdio_inited)
        return 0;

    // save original terminal state
    tcgetattr(STDIN_FILENO, &orig_termios);

    struct termios new_termios = orig_termios;

    new_termios.c_cflag &= ~(CSIZE | PARENB);
    new_termios.c_cflag |= CS8;
    new_termios.c_lflag &= ~(ECHO | ICANON | ISIG);
    new_termios.c_iflag &= ~(ICRNL);

    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);

    // save original signal handler for SIGINT
    // then redirect SIGINT to new handler
    memset(&new_act_sigint, 0, sizeof(new_act_sigint));
    new_act_sigint.sa_handler = mysig_handler;
    sigaction(SIGINT, &new_act_sigint, &old_act_sigint);

    // save original signal handler for SIGTERM
    // then redirect SIGTERM to new handler
    memset(&new_act_sigterm, 0, sizeof(new_act_sigterm));
    new_act_sigterm.sa_handler = mysig_handler;
    sigaction(SIGTERM, &new_act_sigterm, &old_act_sigterm);

    this->stdio_inited = true;

    return 0;
}

void CharIoStdin::rcv_disable()
{
    if (!this->stdio_inited)
        return;

    // restore original terminal state
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);

    // restore original signal handler for SIGINT
    signal(SIGINT, old_act_sigint.sa_handler);

    // restore original signal handler for SIGTERM
    signal(SIGTERM, old_act_sigterm.sa_handler);

    this->stdio_inited = false;
}

bool CharIoStdin::rcv_char_available_now()
{
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    int sel_rv = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout);
    if (sel_rv > 0)
        this->increment_consecutive_chars();
    else
        this->reset_consecutive_chars();
    return sel_rv > 0;
}

int CharIoStdin::xmit_chars(uint8_t *buf, int len)
{
    return (int)write(STDOUT_FILENO, buf, len);
}

int CharIoStdin::rcv_chars(uint8_t *buf, int len)
{
    return (int)read(STDIN_FILENO, buf, len);
}

#endif

//======================== SOCKET character I/O backend ========================
#ifdef _WIN32

SocketCache::~SocketCache() {}

#else // non-Windows OS (Linux, mac OS etc.)

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/errno.h>


CharIoSocket::CharIoSocket(const std::string &name, const std::string &path) : CharIoBackEnd(name)
{
    int rc;

    this->path = path;

    if (SocketCache::get_instance()->sockets.count(path)) {
        auto& sockinfo = SocketCache::get_instance()->sockets[path];
        this->pipefd[0] = sockinfo.pipefd[0];
        this->pipefd[1] = sockinfo.pipefd[1];
        this->sockfd = sockinfo.sockfd;
        this->acceptfd = sockinfo.acceptfd;
        LOG_F(INFO, "%s: using existing socket \"%s\"", this->get_name().c_str(), path.c_str());
    } else do {
        rc = unlink(path.c_str());
        if (rc == 0) {
            LOG_F(INFO, "%s: socket \"%s\" unlinked", this->get_name().c_str(), path.c_str());
        }
        else if (errno != ENOENT) {
            int err = errno;
            LOG_F(INFO, "%s: socket \"%s\" unlink result:%d err:%d = %s",
                this->get_name().c_str(), this->path.c_str(), rc, err, strerror(err));
            break;
        }

#if defined(__APPLE__)
        signal(SIGPIPE, SIG_IGN);
#endif
        if (pipe(this->pipefd) == -1) {
            LOG_F(INFO, "%s: pipe create err: %s", this->get_name().c_str(), strerror(errno));
            break;
        }
        fcntl(this->pipefd[0], F_SETFL, O_NONBLOCK);
        fcntl(this->pipefd[1], F_SETFL, O_NONBLOCK);

        sockaddr_un address;
        memset(&address, 0, sizeof(address));
        address.sun_family = AF_UNIX;
        strcpy(address.sun_path, path.c_str());

        this->sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (this->sockfd == -1) {
            LOG_F(INFO, "%s: socket \"%s\" create err: %s", this->get_name().c_str(), this->path.c_str(), strerror(errno));
            break;
        }

        rc = bind(this->sockfd, (sockaddr*)(&address), sizeof(address));
        if (rc == -1) {
            LOG_F(INFO, "%s: socket \"%s\" bind err: %s", this->get_name().c_str(), this->path.c_str(), strerror(errno));
            close(this->sockfd);
            this->sockfd = -1;
            break;
        }

        rc = listen(this->sockfd, 1);
        if (rc == -1) {
            LOG_F(INFO, "%s: socket \"%s\" listen err: %s", this->get_name().c_str(), this->path.c_str(), strerror(errno));
            close(this->sockfd);
            this->sockfd = -1;
            break;
        }

        LOG_F(INFO, "%s: socket \"%s\" listen %d", this->get_name().c_str(), this->path.c_str(), this->sockfd);

        auto& sockinfo = SocketCache::get_instance()->sockets[path];
        sockinfo.pipefd[0] = this->pipefd[0];
        sockinfo.pipefd[1] = this->pipefd[1];
        sockinfo.sockfd = this->sockfd;
        sockinfo.acceptfd = this->acceptfd;
    } while (0);
    this->start_receive_thread();
}


CharIoSocket::~CharIoSocket() {
    // sockfd is closed by SocketCache destructor
    stop_receive_thread();
    this->sockfd = -1;
}


int CharIoSocket::rcv_enable()
{
    if (this->socket_inited)
        return 0;

    this->socket_inited = true;

    return 0;
}

void CharIoSocket::rcv_disable()
{
    if (!this->socket_inited)
        return;

    this->socket_inited = false;
}

bool CharIoSocket::rcv_char_available_now()
{
    bool has_char = false;

    fd_set read_fds;
    int fd_max;
    int sel_rv;

    if (acceptfd != -1) {
        fd_max = -1;
        FD_ZERO(&read_fds);
        FD_SET(this->acceptfd, &read_fds); if (this->acceptfd > fd_max) fd_max = this->acceptfd;

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 0;

        sel_rv = select(fd_max + 1, &read_fds, nullptr, nullptr, &timeout);
        if (sel_rv == -1) {
            LOG_F(ERROR, "%s: socket \"%s\" select acceptfd err: %s",
                this->get_name().c_str(), this->path.c_str(), strerror(errno));
        }
        else if (sel_rv > 0 && FD_ISSET(this->acceptfd, &read_fds)) {
            has_char = true;
        }
    }
    if (has_char)
        this->increment_consecutive_chars();
    else
        this->reset_consecutive_chars();
    return has_char;
}

int CharIoSocket::xmit_chars(uint8_t *buf, int len)
{
    if (this->get_echo_to_stdout())
        write(STDOUT_FILENO, buf, len);

    int sent = 0;
    if (this->acceptfd != -1) {
        sent = (int)send(this->acceptfd, buf, len, 0);
        if (sent < len) {
            if (sent == -1) {
                LOG_F(INFO, "%s: socket \"%s\" accept write err: %s",
                    this->get_name().c_str(), this->path.c_str(), strerror(errno));
            }
            else {
                LOG_F(INFO, "%s: socket \"%s\" accept write %d", this->get_name().c_str(), this->path.c_str(), sent);
            }
            this->close_accept_fd();
        }
    }
    return sent;
}

enum RECEIVE_THREAD_STATE : uint8_t {
    RECEIVE_THREAD_ACK,
    RECEIVE_THREAD_WAITING,
    RECEIVE_THREAD_STOP,
};

int CharIoSocket::rcv_chars(uint8_t *buf, int len)
{
    if (this->acceptfd != -1) {
        // The emulator's CPU thread must NEVER block so use MSG_DONTWAIT.
        int received = (int)recv(this->acceptfd, buf, len, MSG_DONTWAIT);
        if (received <= 0) {
            if (received == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    LOG_F(INFO, "%s: socket \"%s\" accept read err: %s",
                        this->get_name().c_str(), this->path.c_str(), strerror(errno));
                }
                else {
                    LOG_F(ERROR, "%s: socket \"%s\" accept read err: %s",
                        this->get_name().c_str(), this->path.c_str(), strerror(errno));
                }
            }
            else {
                LOG_F(INFO, "%s: socket \"%s\" accept read %d", this->get_name().c_str(), this->path.c_str(), received);
            }
            this->close_accept_fd();
        }
        RECEIVE_THREAD_STATE pipe_message = RECEIVE_THREAD_ACK;
        write(this->pipefd[1], &pipe_message, 1);
    }
    return len;
}

void CharIoSocket::close_accept_fd()
{
    LOG_F(INFO, "%s: socket \"%s\" accept close", this->get_name().c_str(), this->path.c_str());
    close(acceptfd);
    acceptfd = -1;
    SocketCache::get_instance()->sockets[this->path].acceptfd = acceptfd;
}

#define LOG_PIPE_MESSAGE(when) \
    LOG_F(RECEIVETHREAD, "%s: socket \"%s\" pipe received %s while waiting for " when, \
        this->get_name().c_str(), \
        this->path.c_str(), \
        pipe_message == RECEIVE_THREAD_ACK     ? "ACK"     : \
        pipe_message == RECEIVE_THREAD_WAITING ? "WAITING" : \
        pipe_message == RECEIVE_THREAD_STOP    ? "STOP"    : \
                                                 "UNKNOWN" \
    )

void CharIoSocket::receive_thread_fn()
{
    loguru::set_thread_name(this->get_name().c_str());

    if (this->sockfd == -1) {
        LOG_F(ERROR, "%s: no socket for receive_thread", get_name().c_str());
        return;
    }
    LOG_F(INFO, "%s: receive_thread start", get_name().c_str());

    fd_set read_fds;
    int fd_max;
    int sel_rv;
    RECEIVE_THREAD_STATE pipe_message = RECEIVE_THREAD_ACK;

    while (pipe_message != RECEIVE_THREAD_STOP) {

        while (acceptfd == -1 && pipe_message != RECEIVE_THREAD_STOP) {
            LOG_F(RECEIVETHREAD, "%s: receive_thread waiting for connection", get_name().c_str());

            fd_max = -1;
            FD_ZERO(&read_fds);
            FD_SET(this->sockfd, &read_fds); if (this->sockfd > fd_max) fd_max = this->sockfd;
            FD_SET(this->pipefd[0], &read_fds); if (this->pipefd[0] > fd_max) fd_max = this->pipefd[0];

            sel_rv = select(fd_max + 1, &read_fds, nullptr, nullptr, nullptr);
            if (sel_rv == -1) {
                LOG_F(INFO, "%s: socket \"%s\" select sockfd err: %s", get_name().c_str(), this->path.c_str(), strerror(errno));
            }
            else if (sel_rv == 0) {
                LOG_F(ERROR, "%s: socket \"%s\" select sockfd 0", get_name().c_str(), this->path.c_str());
            }
            else {
                if (FD_ISSET(this->pipefd[0], &read_fds)) {
                    read(this->pipefd[0], &pipe_message, 1); // Drain the pipe
                    LOG_PIPE_MESSAGE("connection");
                    if (pipe_message == RECEIVE_THREAD_STOP)
                        break;
                }
                if (FD_ISSET(this->sockfd, &read_fds)) {
                    sockaddr_un acceptfdaddr;
                    memset(&acceptfdaddr, 0, sizeof(acceptfdaddr));
                    socklen_t len = sizeof(acceptfdaddr);
                    this->acceptfd = accept(this->sockfd, (struct sockaddr *) &acceptfdaddr, &len);
                    if (this->acceptfd == -1) {
                        LOG_F(ERROR, "%s: socket \"%s\" accept err: %s", get_name().c_str(), this->path.c_str(), strerror(errno));
                    }
                    else {
                        LOG_F(INFO, "%s: socket \"%s\" accept %d", get_name().c_str(), this->path.c_str(), this->acceptfd);
                        SocketCache::get_instance()->sockets[this->path].acceptfd = this->acceptfd;
                    }
                }
            }
        }

        while (acceptfd != -1 && pipe_message != RECEIVE_THREAD_STOP) {
            LOG_F(RECEIVETHREAD, "%s: receive_thread waiting for character", get_name().c_str());

            fd_max = -1;
            FD_ZERO(&read_fds);
            FD_SET(this->acceptfd, &read_fds); if (this->acceptfd > fd_max) fd_max = this->acceptfd;
            FD_SET(this->pipefd[0], &read_fds); if (this->pipefd[0] > fd_max) fd_max = this->pipefd[0];

            sel_rv = select(fd_max + 1, &read_fds, nullptr, nullptr, nullptr);
            if (sel_rv == -1) {
                LOG_F(ERROR, "%s: socket \"%s\" select acceptfd err: %s",
                    get_name().c_str(), this->path.c_str(), strerror(errno));
            }
            else if (sel_rv == 0) {
                LOG_F(ERROR, "%s: socket \"%s\" select acceptfd 0", get_name().c_str(), this->path.c_str());
            }
            else {
                if (FD_ISSET(this->pipefd[0], &read_fds)) {
                    read(this->pipefd[0], &pipe_message, 1); // Drain the pipe
                    LOG_PIPE_MESSAGE("character");
                    if (pipe_message == RECEIVE_THREAD_STOP)
                        break;
                }
                if (FD_ISSET(this->acceptfd, &read_fds)) {
                    pipe_message = RECEIVE_THREAD_WAITING;
                    if (this->receive_cb)
                        this->receive_cb();
                    break;
                }
            }
        }

        while (acceptfd != -1 && pipe_message == RECEIVE_THREAD_WAITING) {
            LOG_F(RECEIVETHREAD, "%s: receive_thread waiting for acknowledge", get_name().c_str());

            fd_max = -1;
            FD_ZERO(&read_fds);
            FD_SET(this->pipefd[0], &read_fds); if (this->pipefd[0] > fd_max) fd_max = this->pipefd[0];

            sel_rv = select(fd_max + 1, &read_fds, nullptr, nullptr, nullptr);
            if (sel_rv == -1) {
                LOG_F(ERROR, "%s: socket \"%s\" select acceptfd err: %s",
                    get_name().c_str(), this->path.c_str(), strerror(errno));
            }
            else if (sel_rv == 0) {
                LOG_F(ERROR, "%s: socket \"%s\" select acceptfd 0", get_name().c_str(), this->path.c_str());
            }
            else {
                if (FD_ISSET(this->pipefd[0], &read_fds)) {
                    read(this->pipefd[0], &pipe_message, 1); // Drain the pipe
                    LOG_PIPE_MESSAGE("acknowledge");
                }
            }
        }

    } // while pipe_message != RECEIVE_THREAD_STOP

    LOG_F(INFO, "%s: stopped receive_thread", get_name().c_str());
}

void CharIoSocket::start_receive_thread()
{
    if (this->sockfd == -1) {
        LOG_F(ERROR, "%s: no socket for receive_thread", get_name().c_str());
        return;
    }
    LOG_F(INFO, "%s: starting receive_thread", get_name().c_str());
    this->receive_thread = std::thread(&CharIoSocket::receive_thread_fn, this);
}

void CharIoSocket::stop_receive_thread()
{
    if (this->receive_thread.joinable()) {
        VLOG_SCOPE_F(loguru::Verbosity_INFO, "%s: stopping receive_thread", get_name().c_str());
        RECEIVE_THREAD_STATE msg = RECEIVE_THREAD_STOP;
        write(this->pipefd[1], &msg, 1);
        this->receive_thread.join();
    }
}

SocketCache::SocketCache()
{
    LOG_F(INFO, "Created SocketCache");
}

SocketCache::~SocketCache()
{
    for (auto& it : this->sockets) {
        unlink(it.first.c_str());
        if (errno != ENOENT) {
            LOG_F(INFO, "socket \"%s\" unlink err: %s", it.first.c_str(), strerror(errno));
        } else {
            LOG_F(INFO, "socket \"%s\" unlink", it.first.c_str());
        }
        auto& sockinfo = it.second;
        if (sockinfo.sockfd != -1) {
            close(sockinfo.sockfd);
            sockinfo.sockfd = -1;
        }
        if (sockinfo.pipefd[0] != -1) {
            close(sockinfo.pipefd[0]);
            sockinfo.pipefd[0] = -1;
        }
        if (sockinfo.pipefd[1] != -1) {
            close(sockinfo.pipefd[1]);
            sockinfo.pipefd[1] = -1;
        }
    }
    LOG_F(INFO, "Deleted SocketCache");
}

#endif
