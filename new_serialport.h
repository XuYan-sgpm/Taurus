
#include <windows.h>
#include <malloc.h>
#include <assert.h>
using namespace std;


class CSerialPort
{
protected:
    HANDLE m_handle;
    char *m_name;
    DWORD m_inque;
    DWORD m_outque;
    HANDLE m_writeThread;
    HANDLE m_readThread;
    HANDLE m_writeEvent;
    HANDLE m_readEvent;
    bool m_StopSignal;

    typedef DWORD(WINAPI *ThreadFuncPointer)(LPVOID);

    struct OperateParam
    {
        char *buffer;
        DWORD nSize;
        DWORD *lpRealSize;
        CSerialPort *SendPort, *RecvPort;

        void Set(CSerialPort *_SendPort,
                 CSerialPort *_RecvPort,
                 char *_buffer,
                 DWORD _nSize,
                 DWORD *_lpRealSize)
        {
            SendPort = _SendPort;
            RecvPort = _RecvPort;
            buffer = _buffer;
            nSize = _nSize;
            lpRealSize = _lpRealSize;
        }
    };

#ifndef __WINCE_PLATFORM__

    struct AsyncInfo
    {
        LPOVERLAPPED lpOverlapped;
        LPDWORD lpBytesTransmitted;

        void Set(OVERLAPPED *_lpOverlapped = NULL,
                 DWORD *_lpBytesTransmitted = NULL)
        {
            lpOverlapped = _lpOverlapped;
            lpBytesTransmitted = _lpBytesTransmitted;
        }
    };

    AsyncInfo *m_lpReadInfo, *m_lpWriteInfo;

#endif

protected:
    static DWORD WINAPI _M_SyncReliableWriteProc(LPVOID param)
    {
        OperateParam *Op = static_cast<OperateParam *>(param);
        CSerialPort *SendPort = Op->SendPort;
        CSerialPort *RecvPort = Op->RecvPort;
        DWORD nSize = Op->nSize;
        DWORD write_size, written_size;
        char *buffer = Op->buffer, *p = buffer;
        bool state = true;
        DWORD error_flags;
        COMSTAT comstat;
        while (nSize > 0 && !SendPort->m_StopSignal && state)
        {
            RecvPort->ClearError(&error_flags,
                                 &comstat, NULL, NULL);
            DWORD inres = RecvPort->m_inque - comstat.cbInQue;
            if (inres > 0)
            {
                SendPort->ClearError(NULL, NULL, NULL, NULL);
                write_size = min(inres, nSize);
                if (SendPort->m_outque > 0)
                    write_size = min(write_size,
                                     SendPort->m_outque);
                state = WriteFile(
                    SendPort->m_handle,
                    p, write_size,
                    &written_size, NULL);
                if (state)
                {
                    nSize -= written_size;
                    p += written_size;
                }
            }
        }
        *Op->lpRealSize = p - buffer;
        SetEvent(SendPort->m_writeEvent);
        return 0;
    }

    static DWORD WINAPI _M_SyncReliableReadProc(LPVOID param)
    {
        OperateParam *Op = static_cast<OperateParam *>(param);
        CSerialPort *RecvPort = Op->RecvPort;
        DWORD nSize = Op->nSize;
        DWORD read_size, size_read, error_flags;
        COMSTAT comstat;
        char *buffer = Op->buffer, *p = buffer;
        bool state = true;
        while (nSize > 0 && !RecvPort->m_StopSignal && state)
        {
            RecvPort->ClearError(&error_flags,
                                 &comstat, NULL, NULL);
            read_size = min(comstat.cbInQue, nSize);
            if (read_size > 0)
            {
                state = ReadFile(
                    RecvPort->m_handle,
                    p, read_size,
                    &size_read, NULL);
                if (state)
                {
                    p += size_read;
                    nSize -= size_read;
                }
            }
        }
        *p = '\0';
        *Op->lpRealSize = p - buffer;
        SetEvent(RecvPort->m_readEvent);
        return 0;
    }

#ifndef __WINCE_PLATFORM__

    static DWORD WINAPI _M_AsyncReliableWriteProc(LPVOID param)
    {
        OperateParam *Op = static_cast<OperateParam *>(param);
        CSerialPort *SendPort = Op->SendPort;
        CSerialPort *RecvPort = Op->RecvPort;
        DWORD nSize = Op->nSize;
        DWORD write_size, written_size;
        DWORD error_flags;
        COMSTAT comstat;
        LPOVERLAPPED lpOverlapped = new OVERLAPPED;
        char *buffer = Op->buffer, *p = buffer;
        bool state = true;
        DWORD error_code = 0;
        while (nSize > 0 && !SendPort->m_StopSignal && state)
        {
            RecvPort->ClearError(&error_flags,
                                 &comstat, NULL, NULL);
            if (RecvPort->m_inque > comstat.cbInQue)
            {
                SendPort->ClearError(NULL, NULL, NULL, NULL);
                DWORD inres = RecvPort->m_inque - comstat.cbInQue;
                memset(lpOverlapped, 0, sizeof(*lpOverlapped));
                write_size = min(inres, nSize);
                if (SendPort->m_outque > 0)
                {
                    write_size = min(
                        write_size,
                        SendPort->m_outque);
                }
                state = WriteFile(
                    SendPort->m_handle,
                    p, write_size,
                    &written_size, lpOverlapped);
                if (!state)
                {
                    error_code = GetLastError();
                    switch (error_code)
                    {
                    case ERROR_IO_PENDING:
                        state = GetOverlappedResult(
                            SendPort->m_handle,
                            lpOverlapped,
                            &written_size, true);
                        break;
                    default:
                        break;
                    }
                }
                if (state)
                {
                    p += written_size;
                    nSize -= written_size;
                }
            }
        }
        *Op->lpRealSize = (p - buffer);
        SetEvent(SendPort->m_writeEvent);
        return 0;
    }

    static DWORD WINAPI _M_AsyncReliableReadProc(LPVOID param)
    {
        OperateParam *Op = static_cast<OperateParam *>(param);
        CSerialPort *RecvPort = Op->RecvPort;
        DWORD nSize = Op->nSize;
        DWORD read_size, size_read, error_flags;
        COMSTAT comstat;
        char *buffer = Op->buffer, *p = buffer;
        bool state = true;
        OVERLAPPED ovWait;
        LPOVERLAPPED lpOverlapped = new OVERLAPPED;
        DWORD event_read, wait_event;
        DWORD error_code = 0;
        while (nSize > 0 && !RecvPort->m_StopSignal && state)
        {
            RecvPort->ClearError(&error_flags,
                                 &comstat, NULL, NULL);
            if (comstat.cbInQue == 0)
            {
                SetCommMask(RecvPort->m_handle,
                            EV_RXCHAR | EV_ERR | EV_BREAK);
                memset(&ovWait, 0, sizeof(ovWait));
                ovWait.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
                if (!WaitCommEvent(RecvPort->m_handle,
                                   &wait_event, &ovWait))
                {
                    error_code = GetLastError();
                    switch (error_code)
                    {
                    case 87:
                        state = true;
                        break;
                    case ERROR_IO_PENDING:
                        state = true;
                        break;
                    default:
                        state = false;
                        break;
                    }
                    SetCommMask(RecvPort->m_handle, 0);
                }
                else
                    state = (wait_event & EV_RXCHAR) ? true : false;
                CloseHandle(ovWait.hEvent);
            }
            else
            {
                memset(lpOverlapped, 0, sizeof(*lpOverlapped));
                read_size = min(comstat.cbInQue, nSize);
                state = ReadFile(
                    RecvPort->m_handle,
                    p, read_size,
                    &size_read, lpOverlapped);
                if (!state)
                {
                    error_code = GetLastError();
                    switch (error_code)
                    {
                    case ERROR_IO_PENDING:
                        state = GetOverlappedResult(
                            RecvPort->m_handle,
                            lpOverlapped,
                            &size_read, true);
                        break;
                    default:
                        break;
                    }
                }
                if (state)
                {
                    p += size_read;
                    nSize -= size_read;
                }
            }
        }
        *p = '\0';
        *Op->lpRealSize = p - buffer;
        SetEvent(RecvPort->m_readEvent);
        return 0;
    }

#endif

protected:
    static void _M_CloseHandle(HANDLE &handle)
    {
        if (handle != INVALID_HANDLE_VALUE)
        {
            CloseHandle(handle);
            handle = INVALID_HANDLE_VALUE;
        }
    }

    virtual void _M_StartUp()
    {
#ifndef __WINCE_PLATFORM__

        m_lpReadInfo = NULL;
        m_lpWriteInfo = NULL;

#endif
        m_writeThread = INVALID_HANDLE_VALUE;
        m_readThread = INVALID_HANDLE_VALUE;
        m_readEvent = CreateEvent(NULL, true, false, NULL);
        m_writeEvent = CreateEvent(NULL, true, false, NULL);
        m_StopSignal = false;
        SetEvent(m_readEvent);
        SetEvent(m_writeEvent);
    }

    virtual void _M_CheckOpenStatus()
    {
        assert(isOpened());
    }

    virtual void _M_CreateThread(bool isWrite,
                                 bool isAsync,
                                 char *&buffer,
                                 DWORD &n,
                                 DWORD *lpRealSize,
                                 CSerialPort *SendPort,
                                 CSerialPort *RecvPort)
    {
        OperateParam *Op = new OperateParam;
        Op->Set(SendPort, RecvPort,
                buffer, n, lpRealSize);
        HANDLE thread_handle;
        ThreadFuncPointer fp;
        if (isWrite)
        {
#ifndef __WINCE_PLATFORM__

            fp = isAsync
                     ? &CSerialPort::_M_AsyncReliableWriteProc
                     : &CSerialPort::_M_SyncReliableWriteProc;

#else

            fp = &CSerialPort::_M_SyncReliableWriteProc;

#endif

            thread_handle =
                CreateThread(NULL, 0, *fp, Op, 0, NULL);
            if (thread_handle != NULL)
                SendPort->m_writeThread = thread_handle;
        }
        else
        {
#ifndef __WINCE_PLATFORM__

            fp = isAsync
                     ? &CSerialPort::_M_AsyncReliableReadProc
                     : &CSerialPort::_M_SyncReliableReadProc;

#else

            fp = &CSerialPort::_M_SyncReliableReadProc;

#endif

            thread_handle =
                CreateThread(NULL, 0, *fp, Op, 0, NULL);
            if (thread_handle != NULL)
                RecvPort->m_readThread = thread_handle;
        }
    }

    virtual void _M_WriteEx(bool isAsync,
                            char *buffer,
                            DWORD n,
                            DWORD *lpWrittenSize,
                            CSerialPort *RecvPort)
    {
        _M_CheckOpenStatus();
        ClearError(NULL, NULL, NULL, NULL);
        _M_CloseHandle(m_writeThread);
        _M_CreateThread(true, isAsync,
                        buffer, n,
                        lpWrittenSize,
                        this, RecvPort);
    }

    virtual void _M_ReadEx(bool isAsync,
                           DWORD n,
                           DWORD *lpReadSize,
                           char *&buffer,
                           CSerialPort *SendPort)
    {
        _M_CheckOpenStatus();
        ClearError(NULL, NULL, NULL, NULL);
        _M_CloseHandle(m_readThread);
        _M_CreateThread(false, isAsync,
                        buffer, n,
                        lpReadSize,
                        SendPort, this);
    }

    virtual DWORD _M_GetReadTimeout(DWORD read_chars)
    {
        COMMTIMEOUTS timeouts;
        if (GetCommTimeouts(m_handle, &timeouts))
        {
            DWORD read_total_timeout_multiplier =
                timeouts.ReadTotalTimeoutMultiplier;
            DWORD read_total_timeout_constant =
                timeouts.ReadTotalTimeoutConstant;
            if (read_total_timeout_multiplier == 0 &&
                read_total_timeout_constant == 0)
                return 0;
            return read_total_timeout_constant +
                   read_total_timeout_multiplier * read_chars;
        }
        return 1000;
    }

    virtual DWORD _M_GetWriteTimeout(DWORD write_chars)
    {
        COMMTIMEOUTS timeouts;
        if (GetCommTimeouts(m_handle, &timeouts))
        {
            DWORD write_total_timeout_multiplier =
                timeouts.WriteTotalTimeoutMultiplier;
            DWORD write_total_timeout_constant =
                timeouts.WriteTotalTimeoutConstant;
            if (write_total_timeout_constant == 0 &&
                write_total_timeout_multiplier == 0)
                return 0;
            return write_total_timeout_multiplier * write_chars +
                   write_total_timeout_constant;
        }
        return 1000;
    }

public:
    CSerialPort(const char *name)
    {
        char *no_const_name = const_cast<char *>(name);
        m_name = no_const_name;
        m_handle = INVALID_HANDLE_VALUE;
        m_inque = m_outque = 0;
    }

    ~CSerialPort()
    {
        Close();
        free(m_name);
        m_name = NULL;
    }

#ifndef __WINCE_PLATFORM__

    virtual bool SyncOpen()
    {
        _M_StartUp();
        m_handle = CreateFile(m_name,
                              GENERIC_WRITE | GENERIC_READ,
                              0,
                              NULL,
                              OPEN_EXISTING,
                              0,
                              NULL);
        if (m_handle != INVALID_HANDLE_VALUE)
        {
            SetTimeouts(0, 0, 3000, 0, 3000);
            GetBufSizes(&m_inque, &m_outque, NULL, NULL);
            return true;
        }
        return false;
    }

    virtual bool AsyncOpen()
    {
        _M_StartUp();
        m_handle = CreateFile(m_name,
                              GENERIC_READ | GENERIC_WRITE,
                              0,
                              NULL,
                              OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                              NULL);
        if (m_handle != INVALID_HANDLE_VALUE)
        {
            SetTimeouts(0, 0, 3000, 0, 3000);
            GetBufSizes(&m_inque, &m_outque, NULL, NULL);
            return true;
        }
        return false;
    }

#else

    virtual bool SyncOpen()
    {
        _M_StartUp();
        m_handle = CreateFile(m_name,
                              GENERIC_WRITE | GENERIC_READ,
                              0,
                              NULL,
                              OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL,
                              NULL);
        if (m_handle != INVALID_HANDLE_VALUE)
        {
            SetTimeouts(0, 0, 3000, 0, 3000);
            GetBufSizes(&m_inque, &m_outque, NULL, NULL);
            return true;
        }
        return false;
    }

#endif

    virtual void Close()
    {
        if (isOpened())
        {
            m_StopSignal = true;
            WaitWriteEx();
            WaitReadEx();

#ifndef __WINCE_PLATFORM__

            WaitWrite();
            WaitRead();
            free(m_lpWriteInfo);
            free(m_lpReadInfo);
            m_lpWriteInfo = NULL;
            m_lpReadInfo = NULL;

#endif

            _M_CloseHandle(m_readEvent);
            _M_CloseHandle(m_writeEvent);
            _M_CloseHandle(m_handle);
            m_inque = m_outque = 0;
        }
    }

    virtual bool isOpened()
    {
        return m_handle != INVALID_HANDLE_VALUE;
    }

    virtual bool Init(DWORD baudrate, DWORD bytesize, DWORD parity, DWORD stopbits)
    {
        _M_CheckOpenStatus();
        DCB dcb;
        if (GetCommState(m_handle, &dcb))
        {
            dcb.BaudRate = baudrate;
            dcb.ByteSize = bytesize;
            dcb.Parity = parity;
            dcb.fParity = dcb.Parity != NOPARITY;
            dcb.StopBits = stopbits;
            ClearBuffer();
            return SetCommState(m_handle, &dcb);
        }
        return false;
    }

    virtual void ClearBuffer()
    {
        ClearSndBuffer();
        ClearRcvBuffer();
    }

    virtual void ClearSndBuffer()
    {
        PurgeComm(m_handle, PURGE_TXABORT | PURGE_TXCLEAR);
    }

    virtual void ClearRcvBuffer()
    {
        PurgeComm(m_handle, PURGE_RXABORT | PURGE_RXCLEAR);
    }

    virtual bool SetupBuffer(DWORD inbuf_size, DWORD outbuf_size)
    {
        if (!isOpened())
        {
            bool state = SetupComm(m_handle, inbuf_size, outbuf_size);
            GetBufSizes(&m_inque, &m_outque, NULL, NULL);
            return state;
        }
        return false;
    }

    virtual void SetTimeouts(DWORD read_interval_timeout,
                             DWORD read_total_timeout_multiplier,
                             DWORD read_total_timeout_constant,
                             DWORD write_total_timeout_multiplier,
                             DWORD write_total_timeout_constant)
    {
        _M_CheckOpenStatus();
        COMMTIMEOUTS timeouts;
        timeouts.ReadIntervalTimeout = read_interval_timeout;
        timeouts.ReadTotalTimeoutMultiplier = read_total_timeout_multiplier;
        timeouts.ReadTotalTimeoutConstant = read_total_timeout_constant;
        timeouts.WriteTotalTimeoutMultiplier = write_total_timeout_multiplier;
        timeouts.WriteTotalTimeoutConstant = write_total_timeout_constant;
        SetCommTimeouts(m_handle, &timeouts);
    }

    virtual void ClearError(DWORD *pErrorFlags,
                            COMSTAT *pComstat,
                            DWORD *pRcvData,
                            DWORD *pSndData)
    {
        _M_CheckOpenStatus();
        ClearCommError(m_handle, pErrorFlags, pComstat);
        if (pRcvData)
            *pRcvData = pComstat->cbInQue;
        if (pSndData)
            *pSndData = pComstat->cbOutQue;
    }

    virtual void GetBufSizes(DWORD *pRcvBufSize,
                             DWORD *pSndBufSize,
                             DWORD *pRcvMaxBufSize,
                             DWORD *pSndMaxBufSize)
    {
        _M_CheckOpenStatus();
        COMMPROP comprop;
        if (GetCommProperties(m_handle, &comprop))
        {
            *pSndBufSize = comprop.dwCurrentTxQueue;
            *pRcvBufSize = comprop.dwCurrentRxQueue;
            if (pRcvMaxBufSize)
                *pRcvMaxBufSize = comprop.dwMaxRxQueue;
            if (pSndMaxBufSize)
                *pSndMaxBufSize = comprop.dwMaxTxQueue;
        }
    }

    virtual void SyncWrite(char *buffer, DWORD n, DWORD *lpWrittenSize)
    {
        _M_CheckOpenStatus();
        WaitWritePermission();
        ClearError(NULL, NULL, NULL, NULL);
        WriteFile(m_handle, buffer, n, lpWrittenSize, NULL);
        SetEvent(m_writeEvent);
    }

    virtual void SyncRead(DWORD n, DWORD *lpReadSize, char *buffer)
    {
        _M_CheckOpenStatus();
        WaitReadPermission();
        ClearError(NULL, NULL, NULL, NULL);
        ReadFile(m_handle, buffer, n, lpReadSize, NULL);
        SetEvent(m_readEvent);
    }

    virtual void SyncWriteEx(CSerialPort *RecvPort,
                             char *buffer, DWORD n, DWORD *lpWrittenSize)
    {
        _M_CheckOpenStatus();
        WaitWritePermission();
        _M_WriteEx(false, buffer, n, lpWrittenSize, RecvPort);
    }

    virtual void SyncReadEx(CSerialPort *SendPort,
                            DWORD n, DWORD *lpReadSize, char *buffer)
    {
        _M_CheckOpenStatus();
        WaitReadPermission();
        _M_ReadEx(false, n, lpReadSize, buffer, SendPort);
    }

#ifndef __WINCE_PLATFORM__

    virtual void AsyncWrite(char *buffer, DWORD n, DWORD *lpWrittenSize)
    {
        _M_CheckOpenStatus();
        WaitWritePermission();
        m_lpWriteInfo = new AsyncInfo;
        LPOVERLAPPED lpOverlapped = new OVERLAPPED;
        memset(lpOverlapped,
               0, sizeof(*lpOverlapped));
        m_lpWriteInfo->Set(lpOverlapped, lpWrittenSize);
        WriteFile(m_handle, buffer, n,
                  lpWrittenSize, lpOverlapped);
    }

    virtual void AsyncRead(DWORD n, DWORD *lpReadSize, char *buffer)
    {
        _M_CheckOpenStatus();
        WaitReadPermission();
        m_lpReadInfo = new AsyncInfo;
        LPOVERLAPPED lpOverlapped = new OVERLAPPED;
        memset(lpOverlapped,
               0, sizeof(*lpOverlapped));
        m_lpReadInfo->Set(lpOverlapped, lpReadSize);
        ReadFile(m_handle, buffer, n,
                 lpReadSize, lpOverlapped);
    }

    virtual void AsyncWriteEx(CSerialPort *RecvPort,
                              char *buffer, DWORD n, DWORD *lpWrittenSize)
    {
        _M_CheckOpenStatus();
        WaitWritePermission();
        _M_WriteEx(true, buffer, n, lpWrittenSize, RecvPort);
    }

    virtual void AsyncReadEx(CSerialPort *SendPort,
                             DWORD n, LPDWORD lpReadSize, char *buffer)
    {
        _M_CheckOpenStatus();
        WaitReadPermission();
        _M_ReadEx(true, n, lpReadSize, buffer, SendPort);
    }

#endif

    virtual bool WaitWriteEx()
    {
        _M_CheckOpenStatus();
        WaitForSingleObject(m_writeEvent, INFINITE);
        _M_CloseHandle(m_writeThread);
        assert(m_writeThread == INVALID_HANDLE_VALUE);
        return true;
    }

    virtual bool WaitReadEx()
    {
        _M_CheckOpenStatus();
        WaitForSingleObject(m_readEvent, INFINITE);
        _M_CloseHandle(m_readThread);
        assert(m_readThread == INVALID_HANDLE_VALUE);
        return true;
    }

#ifndef __WINCE_PLATFORM__

    virtual bool WaitWrite()
    {
        if (m_lpWriteInfo)
        {
            bool state = GetOverlappedResult(
                m_handle,
                m_lpWriteInfo->lpOverlapped,
                m_lpWriteInfo->lpBytesTransmitted,
                true);
            free(m_lpWriteInfo);
            m_lpWriteInfo = NULL;
            SetEvent(m_writeEvent);
            return state;
        }
        return true;
    }

    virtual bool WaitRead()
    {
        if (m_lpReadInfo)
        {
            bool state = GetOverlappedResult(
                m_handle,
                m_lpReadInfo->lpOverlapped,
                m_lpReadInfo->lpBytesTransmitted,
                true);
            free(m_lpReadInfo);
            m_lpReadInfo = NULL;
            SetEvent(m_readEvent);
            return state;
        }
        return true;
    }

#endif

    virtual bool isWriting()
    {
        return WaitForSingleObject(m_writeEvent, 0) != WAIT_OBJECT_0;
    }

    virtual bool isReading()
    {
        return WaitForSingleObject(m_readEvent, 0) != WAIT_OBJECT_0;
    }

    virtual void WaitWritePermission()
    {
        WaitForSingleObject(m_writeEvent, INFINITE);
        ResetEvent(m_writeEvent);
    }

    virtual void WaitReadPermission()
    {
        WaitForSingleObject(m_readEvent, INFINITE);
        ResetEvent(m_readEvent);
    }
};