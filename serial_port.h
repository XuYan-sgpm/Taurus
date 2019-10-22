#include <windows.h>
#include <assert.h>
using namespace std;

class CSerialPort
{
protected:
    HANDLE m_handle;
    char *m_name;
    DWORD m_inque;
    DWORD m_outque;
    HANDLE m_writeEvent;
    HANDLE m_readEvent;
    HANDLE m_writeThread;
    HANDLE m_readThread;
    bool m_StopSignal;

    typedef DWORD(WINAPI *ThreadFuncPointer)(LPVOID);

    struct OperateParam
    {
        bool isWrite;
        char *buffer;
        DWORD nSize;
        DWORD *lpRealSize;
        CSerialPort *SendPort, *RecvPort;

        void Set(bool _isWrite,
                 CSerialPort *_SendPort,
                 CSerialPort *_RecvPort,
                 char *_buffer,
                 DWORD _nSize,
                 DWORD *_lpRealSize)
        {
            isWrite = _isWrite;
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
    static bool _M_SyncOperate(bool isWrite,
                               CSerialPort *SendPort,
                               CSerialPort *RecvPort,
                               char *buffer,
                               DWORD nSize,
                               LPDWORD lpTotalBytes)
    {
        DWORD real_size;
        LPDWORD lpBytesTransmitted = new DWORD;
        bool state = true;
        bool stopSignal;
        DWORD error_flags;
        COMSTAT comstat;
        char *p = buffer;
        while (state && nSize > 0)
        {
            stopSignal = isWrite ? SendPort->m_StopSignal
                                 : RecvPort->m_StopSignal;
            if (stopSignal)
            {
                state = false;
                break;
            }
            RecvPort->ClearError(&error_flags,
                                 &comstat,
                                 NULL, NULL);
            real_size = isWrite ? RecvPort->m_inque - comstat.cbInQue
                                : comstat.cbInQue;
            if (real_size == 0)
            {
                continue;
            }
            real_size = min(real_size, nSize);
            if (isWrite)
            {
                SendPort->ClearError(NULL, NULL, NULL, NULL);
                if (SendPort->m_outque > 0)
                {
                    real_size = min(
                        real_size,
                        SendPort->m_outque);
                }
            }
            state = isWrite ? WriteFile(
                                  SendPort->m_handle,
                                  p,
                                  real_size,
                                  lpBytesTransmitted,
                                  NULL)
                            : ReadFile(
                                  RecvPort->m_handle,
                                  p,
                                  real_size,
                                  lpBytesTransmitted,
                                  NULL);
            if (state)
            {
                nSize -= *lpBytesTransmitted;
                p += *lpBytesTransmitted;
            }
        }
        if (!isWrite)
            *p = '\0';
        *lpTotalBytes = p - buffer;
        isWrite ? SetEvent(SendPort->m_writeEvent)
                : SetEvent(RecvPort->m_readEvent);
        return state;
    }

#ifndef __WINCE_PLATFORM__

    static bool _M_AsyncOperate(bool isWrite,
                                CSerialPort *SendPort,
                                CSerialPort *RecvPort,
                                char *buffer,
                                DWORD nSize,
                                LPDWORD lpTotalBytes)
    {
        DWORD real_size;
        LPDWORD lpBytesTransmitted = new DWORD;
        bool state = true;
        bool stopSignal;
        DWORD error_flags;
        COMSTAT comstat;
        char *p = buffer;
        LPOVERLAPPED lpOverlapped = new OVERLAPPED;
        OVERLAPPED ovWait;
        DWORD wait_event;
        while (state && nSize > 0)
        {
            stopSignal = isWrite ? SendPort->m_StopSignal
                                 : RecvPort->m_StopSignal;
            if (stopSignal)
            {
                state = false;
                break;
            }
            RecvPort->ClearError(&error_flags,
                                 &comstat,
                                 NULL, NULL);
            real_size = isWrite ? RecvPort->m_inque - comstat.cbInQue
                                : comstat.cbInQue;
            if (real_size == 0 && !isWrite)
            {
                SetCommMask(RecvPort->m_handle,
                            EV_RXCHAR | EV_ERR | EV_BREAK);
                memset(&ovWait, 0, sizeof(ovWait));
                ovWait.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
                if (!WaitCommEvent(RecvPort->m_handle, &wait_event, &ovWait))
                {
                    DWORD error_code = GetLastError();
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
                {
                    state = (wait_event & EV_RXCHAR) ? true : false;
                }
                CloseHandle(ovWait.hEvent);
            }
            else if (real_size > 0)
            {
                real_size = min(real_size, nSize);
                if (isWrite)
                {
                    SendPort->ClearError(NULL, NULL, NULL, NULL);
                    if (SendPort->m_outque > 0)
                    {
                        real_size = min(
                            SendPort->m_outque,
                            real_size);
                    }
                }
                memset(lpOverlapped, 0, sizeof(*lpOverlapped));
                bool state =
                    isWrite ? WriteFile(
                                  SendPort->m_handle,
                                  p,
                                  real_size,
                                  lpBytesTransmitted,
                                  lpOverlapped)
                            : ReadFile(
                                  RecvPort->m_handle,
                                  p,
                                  real_size,
                                  lpBytesTransmitted,
                                  lpOverlapped);
                if (!state)
                {
                    DWORD error_code = GetLastError();
                    if (error_code == ERROR_IO_PENDING)
                    {
                        state = GetOverlappedResult(
                            isWrite ? SendPort->m_handle
                                    : RecvPort->m_handle,
                            lpOverlapped,
                            lpBytesTransmitted, true);
                    }
                }
                if (state)
                {
                    p += *lpBytesTransmitted;
                    nSize -= *lpBytesTransmitted;
                }
            }
        }
        if (!isWrite)
            *p = '\0';
        *lpTotalBytes = p - buffer;
        isWrite ? SetEvent(SendPort->m_writeEvent)
                : SetEvent(RecvPort->m_readEvent);
        return state;
    }

#endif

protected:
    static DWORD WINAPI _M_SyncProc(LPVOID param)
    {
        OperateParam *Op = static_cast<OperateParam *>(param);
        return !_M_SyncOperate(Op->isWrite,
                               Op->SendPort,
                               Op->RecvPort,
                               Op->buffer,
                               Op->nSize,
                               Op->lpRealSize);
    }

    static DWORD WINAPI _M_AsyncProc(LPVOID param)
    {
        OperateParam *Op = static_cast<OperateParam *>(param);
        return !_M_AsyncOperate(Op->isWrite,
                                Op->SendPort,
                                Op->RecvPort,
                                Op->buffer,
                                Op->nSize,
                                Op->lpRealSize);
    }

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
                                 char *buffer,
                                 DWORD nSize,
                                 DWORD *lpRealSize,
                                 CSerialPort *SendPort,
                                 CSerialPort *RecvPort)
    {
        OperateParam *Op = new OperateParam;
        Op->Set(isWrite, SendPort, RecvPort,
                buffer, nSize, lpRealSize);
        HANDLE thread_handle;
        ThreadFuncPointer fp;
#ifndef __WINCE_PLATFORM__
        fp = isAsync ? &CSerialPort::_M_AsyncProc
                     : &CSerialPort::_M_SyncProc;
#else
        fp = &CSerialPort::_M_SyncProc;
#endif
        thread_handle = CreateThread(NULL, 0, *fp, Op, 0, NULL);
        if (thread_handle != NULL)
        {
            isWrite ? (SendPort->m_writeThread = thread_handle)
                    : (RecvPort->m_readThread = thread_handle);
        }
    }

    virtual void _M_WriteEx(bool isAsync,
                            char *buffer,
                            DWORD nSize,
                            DWORD *lpWrittenSize,
                            CSerialPort *RecvPort)
    {
        _M_CheckOpenStatus();
        ClearError(NULL, NULL, NULL, NULL);
        _M_CloseHandle(m_writeThread);
        _M_CreateThread(true, isAsync, buffer, nSize,
                        lpWrittenSize, this, RecvPort);
    }

    virtual void _M_ReadEx(bool isAsync,
                           DWORD nSize,
                           DWORD *lpReadSize,
                           char *buffer,
                           CSerialPort *SendPort)
    {
        _M_CheckOpenStatus();
        ClearError(NULL, NULL, NULL, NULL);
        _M_CloseHandle(m_readThread);
        _M_CreateThread(false, isAsync, buffer, nSize,
                        lpReadSize, SendPort, this);
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
                              char *buffer, DWORD n,
                              DWORD *lpWrittenSize)
    {
        _M_CheckOpenStatus();
        WaitWritePermission();
        _M_WriteEx(true, buffer, n, lpWrittenSize, RecvPort);
    }

    virtual void AsyncReadEx(CSerialPort *SendPort,
                             DWORD n, LPDWORD lpReadSize,
                             char *buffer)
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