///////////////////////////////////////////////////////////////////////////////
///
/// Copyright (c) 2010 - Oliver Schneider (assarbad.net)
///
/// Original filename: regcopy.cpp
/// Project          : regcopy
/// Date of creation : 2010-07-26
/// Author(s)        : Oliver Schneider
///
///////////////////////////////////////////////////////////////////////////////

// $Id$

///////////////////////////////////////////////////////////////////////////////
// #define UNICODE
// #define _UNICODE
// These two defines are given implicitly through the settings of C_DEFINES in
// the SOURCES file of the project. Hence change them there and there only.
///////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <tchar.h>
#include "regcopy.h"

#define DBGPRINTF _tprintf

#if !defined ASSERT
#define ASSERT assert
#endif

#if !HAVE_GLOBAL_VERYSIMPLEBUF
namespace
{
    template <typename T> class CVerySimpleBuf
    {
    public:
        CVerySimpleBuf(size_t count = 0)
            : m_buf(0)
            , m_count(0)
        {
            reAlloc(count);
        }

        CVerySimpleBuf(const CVerySimpleBuf& rval)
            : m_buf(0)
            , m_count(0)
        {
            operator=(rval);
        }

        CVerySimpleBuf(const T* buf)
            : m_buf(0)
            , m_count(0)
        {
            if(buf)
            {
                operator=(buf);
            }
        }

        ~CVerySimpleBuf()
        {
            delete[] m_buf;
        }

        CVerySimpleBuf& operator=(const CVerySimpleBuf& rval)
        {
            if(&rval != this)
            {
                reAlloc(0);
                if(rval.getBuf() && reAlloc(rval.getCount()))
                {
                    memcpy(getBuf(), rval.getBuf(), getMin_(getByteCount(), rval.getByteCount()));
                }
            }
            return *this;
        }

        CVerySimpleBuf& operator=(const T* buf)
        {
            reAlloc(0);
            if(buf)
            {
                const size_t len = getBufLenZ_<const T*>(buf);
                if(!len)
                {
                    reAlloc(1);
                }
                else if(reAlloc(len))
                {
                    memcpy(getBuf(), buf, getMin_(sizeof(T) * len, getByteCount()));
                }
            }
            return *this;
        }

        CVerySimpleBuf& operator+=(const CVerySimpleBuf& rval)
        {
            if(rval.getCountZ() && reAlloc(getCountZ() + rval.getCountZ()))
            {
                memcpy(getBuf() + getCountZ(), rval.getBuf(), sizeof(T) * rval.getCountZ());
            }
            return *this;
        }

        CVerySimpleBuf& operator+=(const T* buf)
        {
            const size_t len = getBufLenZ_<const T*>(buf);
            if(len && reAlloc(getCountZ() + len))
            {
                memcpy(getBuf() + getCountZ(), buf, sizeof(T) * len);
            }
            return *this;
        }

        inline T* getBuf() const
        {
            return m_buf;
        };

        inline operator bool() const
        {
            return (0 != m_buf);
        }

        inline bool operator!() const
        {
            return (0 != m_buf);
        }

        void clear()
        {
            if(m_buf)
            {
                memset(m_buf, 0, m_count * sizeof(T));
            }
        }

        bool reAlloc(size_t count)
        {
            T* tempBuf = 0;
            size_t count_ = 0;
            if(count)
            {
                count_ = getCeil_(count+1);
                if(count_ <= m_count)
                {
                    memset(m_buf + count, 0, sizeof(T) * (m_count - count));
                    return true;
                }
                if(0 != (tempBuf = new T[count_]))
                {
                    memset(tempBuf, 0, sizeof(T) * count_);
                }
                if(tempBuf && m_buf)
                {
                    memcpy(tempBuf, m_buf, sizeof(T) * getMin_(count, m_count));
                }
            }
            delete[] m_buf;
            m_buf = tempBuf;
            m_count = count_;
            return (0 != m_buf);
        }

        inline size_t getCount() const
        {
            return m_count;
        }

        inline size_t getCountZ() const
        {
            return getBufLenZ_<const T*>(m_buf);
        }

        inline size_t getByteCount() const
        {
            return m_count * sizeof(T);
        }
    protected:
        T* m_buf;
        size_t m_count;

        inline size_t getCeil_(size_t count) const
        {
            const size_t align = sizeof(void*);
            return (((sizeof(T) * count) + (align - 1)) & (~(align - 1))) / sizeof(T);
        }

        inline size_t getMin_(size_t a, size_t b) const
        {
            return ((a < b) ? a : b);
        }

        template <typename T> static size_t getBufLenZ_(const char* val)
        {
            return (val) ? strlen(val) : 0;
        }

        template <typename T> static size_t getBufLenZ_(const wchar_t* val)
        {
            return (val) ? wcslen(val) : 0;
        }
    };
}
#endif

// Does *not* copy 'class' data of keys, nor ACLs
LONG RegCopyKey(HKEY hkeySource, HKEY hkeyTarget, LPCTSTR lpszTgtSubKey)
{
    DWORD dwDisposition;

    HKEY hkeyTgtSubKey = NULL;
    // Create the target key
    LONG lError = ::RegCreateKeyEx(hkeyTarget, lpszTgtSubKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hkeyTgtSubKey, &dwDisposition);
    if(ERROR_SUCCESS != lError)
    {
        return lError;
    }

    // Retrieve the maximum size of elements inside the source key
    DWORD dwDataSize, dwNameLength;
    lError = ::RegQueryInfoKey(hkeySource, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &dwNameLength, &dwDataSize, NULL, NULL);
    if(ERROR_SUCCESS != lError)
    {
        ::RegCloseKey(hkeyTgtSubKey);
        return lError;
    }
    // Adjust the maximum length for the name
    ++dwNameLength;
    CVerySimpleBuf<TCHAR> name(dwNameLength);
    CVerySimpleBuf<BYTE> valueData(dwDataSize);

    for(DWORD dwKeyIndex = 0; ; dwKeyIndex++)
    {
        DBGPRINTF(_T(" Outside value loop: dwNameLength == %u, dwDataSize == %u\n"), dwNameLength, dwDataSize);
        // Copy values *directly* under this key
        for(DWORD dwValueIndex = 0; ; dwValueIndex++)
        {
            DWORD dwRegType;
            DBGPRINTF(_T(" Before value-mem loop: dwNameLength == %u, dwDataSize == %u\n"), dwNameLength, dwDataSize);
            // Readjust length from actual buffer sizes
            dwNameLength = name.getCount();
            dwDataSize = valueData.getCount();
            // This loop ensures we allocate enough memory
            lError = ERROR_NOT_ENOUGH_MEMORY;
            for( ; ERROR_NOT_ENOUGH_MEMORY == lError; )
            {
                DBGPRINTF(_T("  Inside value-mem loop[%u]: dwNameLength == %u, dwDataSize == %u\n"), dwValueIndex, dwNameLength, dwDataSize);
                lError = ::RegEnumValue(hkeySource, dwValueIndex, name.getBuf(), &dwNameLength, NULL, &dwRegType, valueData.getBuf(), &dwDataSize);
                DBGPRINTF(_T("  After RegEnumValue[%u]: dwNameLength == %u, dwDataSize == %u\n"), dwValueIndex, dwNameLength, dwDataSize);
                if(ERROR_NOT_ENOUGH_MEMORY == lError)
                {
                    name.reAlloc(dwNameLength *= 2);
                    valueData.reAlloc(dwDataSize *= 2);
                }
            }
            DBGPRINTF(_T(" After value-mem loop: dwNameLength == %u, dwDataSize == %u\n"), dwNameLength, dwDataSize);
            DBGPRINTF(_T(" Error code: %ld\n"), lError);

            // No more values?
            if(ERROR_NO_MORE_ITEMS == lError)
            {
                DBGPRINTF(_T(" No more values left\n"));
                break;
            }
            if(ERROR_SUCCESS != lError)
            {
                ::RegCloseKey(hkeyTgtSubKey);
                return lError;
            }

            // Set the value on the target key
            DBGPRINTF(_T(" Setting value -> %s (type %u)\n"), name.getBuf(), dwRegType);
            lError = ::RegSetValueEx(hkeyTgtSubKey, name.getBuf(), 0, dwRegType, valueData.getBuf(), dwDataSize);
            if(ERROR_SUCCESS != lError)
            {
                ::RegCloseKey(hkeyTgtSubKey);
                return lError;
            }
        }
        if(dwNameLength < MAX_PATH + 1)
        {
            DBGPRINTF(_T(" dwNameLength was too small\n"));
            dwNameLength = MAX_PATH + 1;
            if(!name.reAlloc(dwNameLength))
            {
                ::RegCloseKey(hkeyTgtSubKey);
                return ERROR_NOT_ENOUGH_MEMORY;
            }
        }
        dwNameLength = (DWORD)name.getCount();
        // This loop ensures we allocate enough memory
        lError = ERROR_NOT_ENOUGH_MEMORY;
        for( ; ERROR_NOT_ENOUGH_MEMORY == lError; )
        {
            FILETIME ftLastWrite;
            DBGPRINTF(_T("  Inside key-mem loop[%u]: dwNameLength == %u\n"), dwKeyIndex, dwNameLength);
            lError = ::RegEnumKeyEx(hkeySource, dwKeyIndex, name.getBuf(), &dwNameLength, NULL, NULL, 0, &ftLastWrite);
            if(ERROR_NOT_ENOUGH_MEMORY == lError)
            {
                name.reAlloc(dwNameLength *= 2);
            }
        }
        DBGPRINTF(_T(" After key-mem loop: dwNameLength == %u\n"), dwNameLength);
        DBGPRINTF(_T(" Error code: %ld\n"), lError);

        // No more sub-keys?
        if(ERROR_NO_MORE_ITEMS == lError)
        {
            DBGPRINTF(_T(" No more keys left\n"));
            break;
        }

        // Avoid endless looping if the target key is beneath the source key
        if(0 == lstrcmpi(name.getBuf(), lpszTgtSubKey))
        {
            DBGPRINTF(_T(" Avoiding infinite loop for %s\n"), lpszTgtSubKey);
            continue;
        }

        HKEY hkeySrcSubKey = NULL;
        // Open the (next) source sub-key
        lError = ::RegOpenKeyEx(hkeySource, name.getBuf(), 0, KEY_ALL_ACCESS, &hkeySrcSubKey);

        // Bail out on error
        if(ERROR_SUCCESS != lError)
        {
            DBGPRINTF(_T(" Error code: %ld\n"), lError);
            ::RegCloseKey(hkeyTgtSubKey);
            return lError;
        }

        DBGPRINTF(_T(" Recursing one level down -> %s\n"), name.getBuf());
        lError = RegCopyKey(hkeySrcSubKey, hkeyTgtSubKey, name.getBuf());

        if(ERROR_SUCCESS != lError)
        {
            DBGPRINTF(_T(" Error code: %ld\n"), lError);
            ::RegCloseKey(hkeyTgtSubKey);
            ::RegCloseKey(hkeySrcSubKey);
            return lError;
        }

        lError = ::RegCloseKey(hkeySrcSubKey);

        if(ERROR_SUCCESS != lError)
        {
            DBGPRINTF(_T(" Error code: %ld\n"), lError);
            ::RegCloseKey(hkeyTgtSubKey);
            return lError;
        }
    }
    ::RegCloseKey(hkeyTgtSubKey);
    return (ERROR_NO_MORE_ITEMS == lError) ? ERROR_SUCCESS : lError;
}

int __cdecl _tmain(int argc, _TCHAR *argv[])
{
    HKEY hKey1;
    DWORD dwDisposition;
    LONG lError = ::RegCreateKeyEx(HKEY_LOCAL_MACHINE, _T("SOFTWARE\\test1"), 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hKey1, &dwDisposition);
    DBGPRINTF(_T("Error code from RegCreateKeyEx: %ld\n"), lError);

    if(ERROR_SUCCESS == lError)
    {
        HKEY hKey2;
        lError = ::RegCreateKeyEx(HKEY_LOCAL_MACHINE, _T("SOFTWARE"), 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hKey2, &dwDisposition);
        DBGPRINTF(_T("Error code from RegCreateKeyEx: %ld\n"), lError);
        if(ERROR_SUCCESS == lError)
        {
            lError = RegCopyKey(hKey1, hKey2, _T("test2"));
            DBGPRINTF(_T("Error code from RegCopyKey: %ld\n"), lError);
        }
        ::RegCloseKey(hKey2);
    }
    ::RegCloseKey(hKey1);

    return 0;
}
