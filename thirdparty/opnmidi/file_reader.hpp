/*
 * FileAndMemoryReader - a tiny helper to utify file reading from a disk and memory block
 *
 * Copyright (c) 2015-2025 Vitaly Novichkov <admin@wohlnet.ru>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#pragma once
#ifndef FILE_AND_MEM_READER_HHHH
#define FILE_AND_MEM_READER_HHHH

#include <string> // std::string
#include <cstdio> // std::fopen, std::fread, std::fseek, std::ftell, std::fclose, std::feof
#include <stdint.h> // uint*_t
#include <stddef.h> // size_t and friends
#ifdef _WIN32
#define NOMINMAX 1
#include <cstring> // std::strlen
#include <windows.h> // MultiByteToWideChar
#endif

/**
 * @brief A little class gives able to read filedata from disk and also from a memory segment
 */
class FileAndMemReader
{
    //! Currently loaded filename (empty for a memory blocks)
    std::string m_file_name;
    //! File reader descriptor
    std::FILE   *m_fp;

    //! Memory pointer descriptor
    const void  *m_mp;
    //! Size of memory block
    size_t      m_mp_size;
    //! Cursor position in the memory block
    size_t      m_mp_tell;

public:
    /**
     * @brief Relation direction
     */
    enum relTo
    {
        //! At begin position
        SET = SEEK_SET,
        //! At current position
        CUR = SEEK_CUR,
        //! At end position
        END = SEEK_END
    };

    /**
     * @brief C.O.: It's a constructor!
     */
    FileAndMemReader() :
        m_fp(NULL),
        m_mp(NULL),
        m_mp_size(0),
        m_mp_tell(0)
    {}

    /**
     * @brief C.O.: It's a destructor!
     */
    ~FileAndMemReader()
    {
        close();
    }

    /**
     * @brief Open file from a disk
     * @param path Path to the file in UTF-8 (even on Windows!)
     */
    void openFile(const char *path)
    {
        if(m_fp)
            this->close();//Close previously opened file first!
#if !defined(_WIN32) || defined(__WATCOMC__)
        m_fp = std::fopen(path, "rb");
#else
        wchar_t widePath[MAX_PATH];
        int size = MultiByteToWideChar(CP_UTF8, 0, path, static_cast<int>(std::strlen(path)), widePath, MAX_PATH);
        widePath[size] = '\0';
        m_fp = _wfopen(widePath, L"rb");
#endif
        m_file_name = path;
        m_mp = NULL;
        m_mp_size = 0;
        m_mp_tell = 0;
    }

    /**
     * @brief Open file from memory block
     * @param mem Pointer to the memory block
     * @param length Size of given block
     */
    void openData(const void *mem, size_t length)
    {
        if(m_fp)
            this->close();//Close previously opened file first!
        m_fp = NULL;
        m_mp = mem;
        m_mp_size = length;
        m_mp_tell = 0;
    }

    /**
     * @brief Seek to given position
     * @param pos Offset or position
     * @param rel_to Relation (at begin, at current, or at end)
     */
    void seek(long pos, int rel_to)
    {
        if(!this->isValid())
            return;

        if(m_fp)//If a file
        {
            std::fseek(m_fp, pos, rel_to);
        }
        else//If a memory block
        {
            switch(rel_to)
            {
            default:
            case SET:
                m_mp_tell = static_cast<size_t>(pos);
                break;

            case END:
                m_mp_tell = m_mp_size - static_cast<size_t>(pos);
                break;

            case CUR:
                m_mp_tell = m_mp_tell + static_cast<size_t>(pos);
                break;
            }

            if(m_mp_tell > m_mp_size)
                m_mp_tell = m_mp_size;
        }
    }

    /**
     * @brief Seek to given position (unsigned integer 64 as relation. Negative values not supported)
     * @param pos Offset or position
     * @param rel_to Relation (at begin, at current, or at end)
     */
    inline void seeku(uint64_t pos, int rel_to)
    {
        this->seek(static_cast<long>(pos), rel_to);
    }

    /**
     * @brief Read the buffer from a file
     * @param buf Pointer to the destination memory block
     * @param num Number of elements
     * @param size Size of one element
     * @return Size
     */
    size_t read(void *buf, size_t num, size_t size)
    {
        if(!this->isValid())
            return 0;
        if(m_fp)
            return std::fread(buf, num, size, m_fp);
        else
        {
            size_t pos = 0;
            size_t maxSize = static_cast<size_t>(size * num);

            while((pos < maxSize) && (m_mp_tell < m_mp_size))
            {
                reinterpret_cast<uint8_t *>(buf)[pos] = reinterpret_cast<const uint8_t *>(m_mp)[m_mp_tell];
                m_mp_tell++;
                pos++;
            }

            return pos / num;
        }
    }

    /**
     * @brief Get one byte and seek forward
     * @return Readed byte or EOF (a.k.a. -1)
     */
    int getc()
    {
        if(!this->isValid())
            return -1;
        if(m_fp)//If a file
        {
            return std::getc(m_fp);
        }
        else  //If a memory block
        {
            if(m_mp_tell >= m_mp_size)
                return -1;
            int x = reinterpret_cast<const uint8_t *>(m_mp)[m_mp_tell];
            m_mp_tell++;
            return x;
        }
    }

    /**
     * @brief Returns current offset of cursor in a file
     * @return Offset position
     */
    size_t tell()
    {
        if(!this->isValid())
            return 0;
        if(m_fp)//If a file
            return static_cast<size_t>(std::ftell(m_fp));
        else//If a memory block
            return m_mp_tell;
    }

    /**
     * @brief Close the file
     */
    void close()
    {
        if(m_fp)
            std::fclose(m_fp);

        m_fp = NULL;
        m_mp = NULL;
        m_mp_size = 0;
        m_mp_tell = 0;
    }

    /**
     * @brief Is file instance valid
     * @return true if vaild
     */
    bool isValid()
    {
        return (m_fp) || (m_mp);
    }

    /**
     * @brief Is End Of File?
     * @return true if end of file was reached
     */
    bool eof()
    {
        if(!this->isValid())
            return true;
        if(m_fp)
            return (std::feof(m_fp) != 0);
        else
            return m_mp_tell >= m_mp_size;
    }

    /**
     * @brief Get a current file name
     * @return File name of currently loaded file
     */
    const std::string &fileName()
    {
        return m_file_name;
    }

    /**
     * @brief Retrieve file size
     * @return Size of file in bytes
     */
    size_t fileSize()
    {
        if(!this->isValid())
            return 0;
        if(!m_fp)
            return m_mp_size; //Size of memory block is well known
        size_t old_pos = this->tell();
        seek(0l, FileAndMemReader::END);
        size_t file_size = this->tell();
        seek(static_cast<long>(old_pos), FileAndMemReader::SET);
        return file_size;
    }
};

#endif /* FILE_AND_MEM_READER_HHHH */
