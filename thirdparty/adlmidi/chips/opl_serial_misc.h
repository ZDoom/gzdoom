/*
 * Interfaces over Yamaha OPL3 (YMF262) chip emulators
 *
 * Copyright (c) 2017-2025 Vitaly Novichkov (Wohlstand)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef OPL_SERIAL_MISC_H
#define OPL_SERIAL_MISC_H

#if defined( __unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <errno.h>
#include <sys/ioctl.h>
#endif

#ifdef __APPLE__
#include <IOKit/serial/ioss.h>
#endif

#ifdef _WIN32
#include <windows.h>
#endif

#include <string>


class ChipSerialPortBase
{
public:
    ChipSerialPortBase() {}
    virtual ~ChipSerialPortBase() {}

    virtual bool isOpen()
    {
        return false;
    }

    virtual void close() {}

    virtual bool open(const std::string &/*portName*/, unsigned /*baudRate*/)
    {
        return false;
    }

    virtual int write(uint8_t * /*data*/, size_t /*size*/)
    {
        return 0;
    }
};



#if defined( __unix__) || defined(__APPLE__)
class ChipSerialPort : public ChipSerialPortBase
{
    int m_port;
    struct termios m_portSetup;

    static unsigned int baud2enum(unsigned int baud)
    {
        if(baud == 0)
            return B0;
        else if(baud <= 50)
            return B50;
        else if(baud <= 75)
            return B75;
        else if(baud <= 110)
            return B110;
        else if(baud <= 134)
            return B134;
        else if(baud <= 150)
            return B150;
        else if(baud <= 200)
            return B200;
        else if(baud <= 300)
            return B300;
        else if(baud <= 600)
            return B600;
        else if(baud <= 1200)
            return B1200;
        else if(baud <= 1800)
            return B1800;
        else if(baud <= 2400)
            return B2400;
        else if(baud <= 4800)
            return B4800;
        else if(baud <= 9600)
            return B9600;
        else if(baud <= 19200)
            return B19200;
        else if(baud <= 38400)
            return B38400;
        else if(baud <= 57600)
            return B57600;
        else if(baud <= 115200)
            return B115200;
        else
            return B230400;
    }

public:
    ChipSerialPort() :
        ChipSerialPortBase()
    {
        m_port = 0;
        std::memset(&m_portSetup, 0, sizeof(struct termios));
    }

    virtual ~ChipSerialPort()
    {
        close();
    }

    bool isOpen()
    {
        return m_port != 0;
    }

    void close()
    {
        if(m_port)
            ::close(m_port);

        m_port = 0;
    }

    bool open(const std::string &portName, unsigned baudRate)
    {
        if(m_port)
            this->close();

        std::string portPath = "/dev/" + portName;
        m_port = ::open(portPath.c_str(), O_WRONLY);

        if(m_port < 0)
        {
            std::fprintf(stderr, "-- OPL Serial ERROR: failed to open tty device `%s': %s\n", portPath.c_str(), strerror(errno));
            std::fflush(stderr);
            m_port = 0;
            return false;
        }

        if(tcgetattr(m_port, &m_portSetup) != 0)
        {
            std::fprintf(stderr, "-- OPL Serial ERROR: failed to retrieve setup `%s': %s\n", portPath.c_str(), strerror(errno));
            std::fflush(stderr);
            close();
            return false;
        }

        m_portSetup.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
        m_portSetup.c_oflag &= ~OPOST;
        m_portSetup.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
        m_portSetup.c_cflag &= ~(CSIZE | PARENB);
        m_portSetup.c_cflag |= CS8;

#if defined (__linux__) || defined (__CYGWIN__)
        m_portSetup.c_cflag &= ~CBAUD;
#endif

#if defined (BSD) || defined(__FreeBSD__)
        cfsetispeed(&m_portSetup, baudRate);
        cfsetospeed(&m_portSetup, baudRate);
#elif !defined(__APPLE__)
        cfsetospeed(&m_portSetup, baud2enum(baudRate));
#endif

        if(tcsetattr(m_port, TCSANOW, &m_portSetup) != 0)
        {
            std::fprintf(stderr, "-- OPL Serial ERROR: failed to apply setup `%s': %s\n", portPath.c_str(), strerror(errno));
            std::fflush(stderr);
            close();
            return false;
        }

#ifdef __APPLE__
        if(ioctl(m_port, IOSSIOSPEED, &baudRate) == -1)
        {
            std::fprintf(stderr, "-- OPL Serial ERROR: Failed to set MacOS specific tty attributes for `%s': %s", portPath.c_str(), strerror(errno));
            std::fflush(stderr);
            close();
            return false;
        }
#endif

        return true;
    }

    int write(uint8_t *data, size_t size)
    {
        if(!m_port)
            return 0;

        return ::write(m_port, data, size);
    }
};

#endif // __unix__




#ifdef _WIN32

class ChipSerialPort : public ChipSerialPortBase
{
    HANDLE m_port;

    static unsigned int baud2enum(unsigned int baud)
    {
        if(baud <= 110)
            return CBR_110;
        else if(baud <= 300)
            return CBR_300;
        else if(baud <= 600)
            return CBR_600;
        else if(baud <= 1200)
            return CBR_1200;
        else if(baud <= 2400)
            return CBR_2400;
        else if(baud <= 4800)
            return CBR_4800;
        else if(baud <= 9600)
            return CBR_9600;
        else if(baud <= 19200)
            return CBR_19200;
        else if(baud <= 38400)
            return CBR_38400;
        else if(baud <= 57600)
            return CBR_57600;
        else if(baud <= 115200)
            return CBR_115200;
        else
            return CBR_115200;
    }

public:
    ChipSerialPort() : ChipSerialPortBase()
    {
        m_port = NULL;
    }

    virtual ~ChipSerialPort()
    {
        close();
    }

    bool isOpen()
    {
        return m_port != NULL;
    }

    void close()
    {
        if(m_port)
            CloseHandle(m_port);

        m_port = NULL;
    }

    bool open(const std::string &portName, unsigned baudRate)
    {
        if(m_port)
            this->close();

        std::string portPath = "\\\\.\\" + portName;
#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8)
        std::wstring wportPath;
        wportPath.resize(portPath.size());
        int newSize = MultiByteToWideChar(CP_UTF8,
                                          0,
                                          portPath.c_str(),
                                          (int)portPath.size(),
                                          (wchar_t *)wportPath.c_str(),
                                          (int)wportPath.size());
        m_port = CreateFile2(wportPath.c_str(), GENERIC_WRITE, 0, OPEN_EXISTING, NULL);
#else
        m_port = CreateFileA(portPath.c_str(), GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
#endif

        if(m_port == INVALID_HANDLE_VALUE)
        {
            m_port = NULL;
            return false;
        }

        DCB dcb;
        BOOL succ;

        SecureZeroMemory(&dcb, sizeof(DCB));
        dcb.DCBlength = sizeof(DCB);

        succ = GetCommState(m_port, &dcb);
        if(!succ)
        {
            this->close();
            return false;
        }

        dcb.BaudRate = baud2enum(baudRate);
        dcb.ByteSize = 8;
        dcb.Parity = NOPARITY;
        dcb.StopBits = ONESTOPBIT;

        succ = SetCommState(m_port, &dcb);

        if(!succ)
        {
            this->close();
            return false;
        }

        return true;
    }

    int write(uint8_t *data, size_t size)
    {
        if(!m_port)
            return 0;

        DWORD written = 0;

        if(!WriteFile(m_port, data, size, &written, 0))
            return 0;

        return written;
    }
};

#endif // _WIN32

#endif // OPL_SERIAL_MISC_H
