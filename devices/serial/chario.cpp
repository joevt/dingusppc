/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-25 divingkatae and maximum
                      (theweirdo)     spatium

(Contact divingkatae#1017 or powermax#2286 on Discord for more info)

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

#include <devices/serial/chario.h>
#include <loguru.hpp>

#include <cinttypes>
#include <cstring>
#include <memory>

//======================== BASE character I/O backend ========================
CharIoBackEnd::CharIoBackEnd(const std::string &name)
{
    this->name = name;
    LOG_F(INFO, "Created %s", this->name.c_str());
}

CharIoBackEnd::~CharIoBackEnd()
{
    LOG_F(INFO, "Deleted %s", this->name.c_str());
}

//======================== NULL character I/O backend ========================
bool CharIoNull::rcv_char_available()
{
    return false;
}

bool CharIoNull::rcv_char_available_now()
{
    return false;
}

int CharIoNull::xmit_char(uint8_t /*c*/)
{
    return 0;
}

int CharIoNull::rcv_char(uint8_t *c)
{
    *c = 0xFF;
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

    LOG_F(INFO, "Winterm: receiver initialized");

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

    LOG_F(INFO, "Winterm: receiver disabled");
}

bool CharIoStdin::rcv_char_available()
{
    return this->rcv_char_available_now();
}

bool CharIoStdin::rcv_char_available_now() {
    DWORD events;
    INPUT_RECORD buffer;

    PeekConsoleInput(hInput, &buffer, 1, &events);
    return !!(events > 0);
}

int CharIoStdin::xmit_char(uint8_t c) {
    _write(_fileno(stdout), &c, 1);
    return 0;
}

int CharIoStdin::rcv_char(uint8_t* c) {
    _read(_fileno(stdin), c, 1);
    return 0;
}

#else // non-Windows OS (Linux, mac OS etc.)

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

bool CharIoStdin::rcv_char_available()
{
    if (consecutivechars >= 15) {
        consecutivechars++;
        if (consecutivechars >= 400)
            consecutivechars = 0;
        return 0;
    }
    return this->rcv_char_available_now();
}

bool CharIoStdin::rcv_char_available_now()
{
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    int sel_rv = select(1, &readfds, NULL, NULL, &timeout);
    if (sel_rv > 0)
        consecutivechars++;
    else
        consecutivechars = 0;
    return sel_rv > 0;
}

int CharIoStdin::xmit_char(uint8_t c)
{
    write(STDOUT_FILENO, &c, 1);
    return 0;
}

int CharIoStdin::rcv_char(uint8_t *c)
{
    read(STDIN_FILENO, c, 1);
    return 0;
}

#endif

//======================== SOCKET character I/O backend ========================
#ifdef _WIN32

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
        this->sockfd = sockinfo.sockfd;
        this->acceptfd = sockinfo.acceptfd;
        LOG_F(INFO, "using existing socket \"%s\"", path.c_str());
    } else do {
        rc = unlink(path.c_str());
        if (rc == 0) {
            LOG_F(INFO, "socket \"%s\" unlinked", path.c_str());
        }
        else if (errno != ENOENT) {
            int err = errno;
            LOG_F(INFO, "socket \"%s\" unlink result:%d err:%d = %s", this->path.c_str(), rc, err, strerror(err));
            break;
        }

        sockaddr_un address;
        memset(&address, 0, sizeof(address));
        address.sun_family = AF_UNIX;
        strcpy(address.sun_path, path.c_str());

        this->sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (this->sockfd == -1) {
            LOG_F(INFO, "socket \"%s\" create err: %s", this->path.c_str(), strerror(errno));
            break;
        }

        rc = bind(this->sockfd, (sockaddr*)(&address), sizeof(address));
        if (rc == -1) {
            LOG_F(INFO, "socket \"%s\" bind err: %s", this->path.c_str(), strerror(errno));
            close(this->sockfd);
            this->sockfd = -1;
            break;
        }

        rc = listen(this->sockfd, 100);
        if (rc == -1) {
            LOG_F(INFO, "socket \"%s\" listen err: %s", this->path.c_str(), strerror(errno));
            close(this->sockfd);
            this->sockfd = -1;
            break;
        }

        LOG_F(INFO, "socket \"%s\" listen %d", this->path.c_str(), this->sockfd);

        SocketCache::get_instance()->sockets[path].sockfd = this->sockfd;
        SocketCache::get_instance()->sockets[path].acceptfd = -1;
    } while (0);
}


CharIoSocket::~CharIoSocket() {
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

bool CharIoSocket::rcv_char_available()
{
    if (consecutivechars >= 15) {
        consecutivechars++;
        if (consecutivechars >= 800)
            consecutivechars = 0;
        return 0;
    }
    return this->rcv_char_available_now();
}

bool CharIoSocket::rcv_char_available_now()
{
    int sel_rv = 0;
    bool havechars = false;
    fd_set readfds;
    fd_set writefds;
    fd_set errorfds;

    int sockmax = 0;
    if (this->sockfd != -1) {
        FD_ZERO(&readfds);
        FD_SET(this->sockfd, &readfds);
        if (this->sockfd > sockmax) sockmax = this->sockfd;
        if (this->acceptfd != -1) {
            FD_SET(this->acceptfd, &readfds);
            if (this->acceptfd > sockmax) sockmax = this->acceptfd;
        }
        writefds = readfds;
        errorfds = readfds;

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 0;

        sel_rv = select(sockmax + 1, &readfds, &writefds, &errorfds, &timeout);
        if (sel_rv == -1) {
            LOG_F(INFO, "socket \"%s\" select err: %s", this->path.c_str(), strerror(errno));
        }
    }

    if (sel_rv > 0) {
        if (this->sockfd != -1) {
             if (FD_ISSET(this->sockfd, &readfds)) {
                uint8_t c;
                int received = (int)recv(this->sockfd, &c, 1, 0);
                if (received == -1) {
                    if (this->acceptfd == -1) {
                        #if 0
                            LOG_F(INFO, "socket \"%s\" sock read (not accepted yet) err: %s",
                                this->path.c_str(), strerror(errno)); // this happens once before accept
                        #endif
                    }
                    else {
                        LOG_F(INFO, "socket \"%s\" sock read err: %s",
                            this->path.c_str(), strerror(errno)); // should never happen
                    }
                }
                else if (received == 1) {
                    LOG_F(INFO, "socket \"%s\" sock read '%c'", this->path.c_str(), c); // should never happen
                }
                else {
                    LOG_F(INFO, "socket \"%s\" sock read %d", this->path.c_str(), received); // should never happen
                }

                if (this->acceptfd == -1) {
                    sockaddr_un acceptfdaddr;
                    memset(&acceptfdaddr, 0, sizeof(acceptfdaddr));
                    socklen_t len = sizeof(acceptfdaddr);
                    this->acceptfd = accept(this->sockfd, (struct sockaddr *) &acceptfdaddr, &len);
                    if (this->acceptfd == -1) {
                        LOG_F(INFO, "socket \"%s\" accept err: %s", this->path.c_str(), strerror(errno));
                    }
                    else {
                        LOG_F(INFO, "socket \"%s\" accept %d", this->path.c_str(), this->acceptfd);
                        SocketCache::get_instance()->sockets[this->path].acceptfd = this->acceptfd;
                    }
                }
            } // if read

            if (FD_ISSET(this->sockfd, &writefds)) {
                LOG_F(INFO, "socket \"%s\" sock write", this->path.c_str());
            }

            if (FD_ISSET(this->sockfd, &errorfds)) {
                LOG_F(INFO, "socket \"%s\" sock error", this->path.c_str());
            }
        } // if this->sockfd

        if (this->acceptfd != -1) {
            if (FD_ISSET(this->acceptfd, &readfds)) {
                // LOG_F(INFO, "socket \"%s\" accept read havechars", this->path.c_str());
                havechars = true;
                consecutivechars++;
            } // if read

            if (FD_ISSET(this->acceptfd, &writefds)) {
                // LOG_F(INFO, "socket \"%s\" accept write", this->path.c_str()); // this is usually always true
            }

            if (FD_ISSET(this->acceptfd, &errorfds)) {
                LOG_F(INFO, "socket \"%s\" accept error", this->path.c_str());
            }
        } // if this->acceptfd
    }
    else
        consecutivechars = 0;
    return havechars;
}

int CharIoSocket::xmit_char(uint8_t c)
{
    write(STDOUT_FILENO, &c, 1);

    if (this->acceptfd == -1)
        this->rcv_char_available_now();

    if (this->acceptfd != -1) {
        int sent = (int)send(this->acceptfd, &c, 1, 0);
        if (sent == -1) {
            LOG_F(INFO, "socket \"%s\" accept write err: %s", this->path.c_str(), strerror(errno));
        }
        if (sent == 1) {
            /*
            if (c < ' ') {
                LOG_F(INFO, "socket \"%s\" accept write '\\x%02X'", this->path.c_str(), c);
            } else {
                LOG_F(INFO, "socket \"%s\" accept write '%c'", this->path.c_str(), c);
            }
            */
        }
        else {
            LOG_F(INFO, "socket \"%s\" accept write %d", this->path.c_str(), sent);
        }
    }
    return 0;
}

int CharIoSocket::rcv_char(uint8_t *c)
{
    if (this->acceptfd == -1)
        this->rcv_char_available_now();

    if (this->acceptfd != -1) {
        int received = (int)recv(this->acceptfd, c, 1, 0);
        if (received == -1) {
            LOG_F(INFO, "socket \"%s\" accept read err: %s", this->path.c_str(), strerror(errno));
        }
        else if (received == 1) {
            /*
            if (c) {
                if (*c < ' ') {
                    LOG_F(INFO, "socket \"%s\" accept write '\\x%02X'", this->path.c_str(), *c);
                } else {
                    LOG_F(INFO, "socket \"%s\" accept read '%c'", this->path.c_str(), *c);
                }
            } else {
                LOG_F(INFO, "socket \"%s\" accept read %d", this->path.c_str(), received);
            }
            */
        }
        else {
            LOG_F(INFO, "socket \"%s\" accept read %d", this->path.c_str(), received);
        }
    }
    return 0;
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
        if (it.second.sockfd != -1) {
            close(it.second.sockfd);
            it.second.sockfd = -1;
        }
    }
    LOG_F(INFO, "Deleted SocketCache");
}

#endif
