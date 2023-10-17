#pragma once

#include <iostream>

typedef wchar_t WCHAR;

//////////////////////////// »ç¿ë¹ý ////////////////////////////
/// // test.config
/// anyValue = 5
/// 
/// // .cpp
/// int myVar;
/// GetInt("test.config", "anyValue", &myVar);
////////////////////////////////////////////////////////////////

class ConfigReader
{
public:
    static bool GetInt(const char* fileName, const char* valueName, int* outNumber)
    {
        FILE* fptr_configFile;

        if (fopen_s(&fptr_configFile, fileName, "r") == EINVAL || fptr_configFile == nullptr)
        {
            return false;
        }

        size_t valueNameLength = strlen(valueName);

        fseek(fptr_configFile, 0, SEEK_END);
        size_t fileSize = ftell(fptr_configFile);
        rewind(fptr_configFile);

        if (fileSize <= valueNameLength)
        {
            fclose(fptr_configFile);
            return false;
        }

        char* fileString = new char[fileSize];

        fread(fileString, fileSize, 1, fptr_configFile);

        size_t i = 0;
        while (strncmp(fileString + i, valueName, valueNameLength) != 0)
        {
            i++;

            if (fileSize - i <= valueNameLength)
            {
                fclose(fptr_configFile);
                delete[] fileString;
                return false;
            }
        }

        i += valueNameLength;

        while (fileString[i] != '=')
        {
            i++;

            if (i >= fileSize - 1)
            {
                fclose(fptr_configFile);
                delete[] fileString;
                return false;
            }
        }

        sscanf_s(fileString + i + 1, "%d", outNumber);

        fclose(fptr_configFile);
        delete[] fileString;

        return true;
    }
    static bool GetInt(const char* fileName, const char* valueName, unsigned int* outNumber)
    {
        FILE* fptr_configFile;

        if (fopen_s(&fptr_configFile, fileName, "r") == EINVAL || fptr_configFile == nullptr)
        {
            return false;
        }

        size_t valueNameLength = strlen(valueName);

        fseek(fptr_configFile, 0, SEEK_END);
        size_t fileSize = ftell(fptr_configFile);
        rewind(fptr_configFile);

        if (fileSize <= valueNameLength)
        {
            fclose(fptr_configFile);
            return false;
        }

        char* fileString = new char[fileSize];

        fread(fileString, fileSize, 1, fptr_configFile);

        size_t i = 0;
        while (strncmp(fileString + i, valueName, valueNameLength) != 0)
        {
            i++;

            if (fileSize - i <= valueNameLength)
            {
                fclose(fptr_configFile);
                delete[] fileString;
                return false;
            }
        }

        i += valueNameLength;

        while (fileString[i] != '=')
        {
            i++;

            if (i >= fileSize - 1)
            {
                fclose(fptr_configFile);
                delete[] fileString;
                return false;
            }
        }

        sscanf_s(fileString + i + 1, "%u", outNumber);

        fclose(fptr_configFile);
        delete[] fileString;

        return true;
    }
    static bool GetInt(const char* fileName, const char* valueName, long long* outNumber)
    {
        FILE* fptr_configFile;

        if (fopen_s(&fptr_configFile, fileName, "r") == EINVAL || fptr_configFile == nullptr)
        {
            return false;
        }

        size_t valueNameLength = strlen(valueName);

        fseek(fptr_configFile, 0, SEEK_END);
        size_t fileSize = ftell(fptr_configFile);
        rewind(fptr_configFile);

        if (fileSize <= valueNameLength)
        {
            fclose(fptr_configFile);
            return false;
        }

        char* fileString = new char[fileSize];

        fread(fileString, fileSize, 1, fptr_configFile);

        size_t i = 0;
        while (strncmp(fileString + i, valueName, valueNameLength) != 0)
        {
            i++;

            if (fileSize - i <= valueNameLength)
            {
                fclose(fptr_configFile);
                delete[] fileString;
                return false;
            }
        }

        i += valueNameLength;

        while (fileString[i] != '=')
        {
            i++;

            if (i >= fileSize - 1)
            {
                fclose(fptr_configFile);
                delete[] fileString;
                return false;
            }
        }

        sscanf_s(fileString + i + 1, "%lld", outNumber);

        fclose(fptr_configFile);
        delete[] fileString;

        return true;
    }
    static bool GetInt(const char* fileName, const char* valueName, unsigned long long* outNumber)
    {
        FILE* fptr_configFile;

        if (fopen_s(&fptr_configFile, fileName, "r") == EINVAL || fptr_configFile == nullptr)
        {
            return false;
        }

        size_t valueNameLength = strlen(valueName);

        fseek(fptr_configFile, 0, SEEK_END);
        size_t fileSize = ftell(fptr_configFile);
        rewind(fptr_configFile);

        if (fileSize <= valueNameLength)
        {
            fclose(fptr_configFile);
            return false;
        }

        char* fileString = new char[fileSize];

        fread(fileString, fileSize, 1, fptr_configFile);

        size_t i = 0;
        while (strncmp(fileString + i, valueName, valueNameLength) != 0)
        {
            i++;

            if (fileSize - i <= valueNameLength)
            {
                fclose(fptr_configFile);
                delete[] fileString;
                return false;
            }
        }

        i += valueNameLength;

        while (fileString[i] != '=')
        {
            i++;

            if (i >= fileSize - 1)
            {
                fclose(fptr_configFile);
                delete[] fileString;
                return false;
            }
        }

        sscanf_s(fileString + i + 1, "%llu", outNumber);

        fclose(fptr_configFile);
        delete[] fileString;

        return true;
    }

    static bool GetInt(const WCHAR* fileName, const WCHAR* valueName, int* outNumber)
    {
        FILE* fptr_configFile;

        if (_wfopen_s(&fptr_configFile, fileName, L"r") == EINVAL || fptr_configFile == nullptr)
        {
            return false;
        }

        size_t valueNameLength = wcslen(valueName);

        fseek(fptr_configFile, 0, SEEK_END);
        size_t fileSize = ftell(fptr_configFile);
        rewind(fptr_configFile);

        if (fileSize <= valueNameLength)
        {
            fclose(fptr_configFile);
            return false;
        }

        WCHAR* fileString = new WCHAR[fileSize];

        fread(fileString, fileSize, 1, fptr_configFile);

        size_t i = 0;
        while (wcsncmp(fileString + i, valueName, valueNameLength) != 0)
        {
            i++;

            if (fileSize - i <= valueNameLength)
            {
                fclose(fptr_configFile);
                delete[] fileString;
                return false;
            }
        }

        i += valueNameLength;

        while (fileString[i] != L'=')
        {
            i++;

            if (i >= fileSize - 1)
            {
                fclose(fptr_configFile);
                delete[] fileString;
                return false;
            }
        }

        swscanf_s(fileString + i + 1, L"%d", outNumber);

        fclose(fptr_configFile);
        delete[] fileString;

        return true;
    }
    static bool GetInt(const WCHAR* fileName, const WCHAR* valueName, unsigned int* outNumber)
    {
        FILE* fptr_configFile;

        if (_wfopen_s(&fptr_configFile, fileName, L"r") == EINVAL || fptr_configFile == nullptr)
        {
            return false;
        }

        size_t valueNameLength = wcslen(valueName);

        fseek(fptr_configFile, 0, SEEK_END);
        size_t fileSize = ftell(fptr_configFile);
        rewind(fptr_configFile);

        if (fileSize <= valueNameLength)
        {
            fclose(fptr_configFile);
            return false;
        }

        WCHAR* fileString = new WCHAR[fileSize];

        fread(fileString, fileSize, 1, fptr_configFile);

        size_t i = 0;
        while (wcsncmp(fileString + i, valueName, valueNameLength) != 0)
        {
            i++;

            if (fileSize - i <= valueNameLength)
            {
                fclose(fptr_configFile);
                delete[] fileString;
                return false;
            }
        }

        i += valueNameLength;

        while (fileString[i] != L'=')
        {
            i++;

            if (i >= fileSize - 1)
            {
                fclose(fptr_configFile);
                delete[] fileString;
                return false;
            }
        }

        swscanf_s(fileString + i + 1, L"%u", outNumber);

        fclose(fptr_configFile);
        delete[] fileString;

        return true;
    }
    static bool GetInt(const WCHAR* fileName, const WCHAR* valueName, long long* outNumber)
    {
        FILE* fptr_configFile;

        if (_wfopen_s(&fptr_configFile, fileName, L"r") == EINVAL || fptr_configFile == nullptr)
        {
            return false;
        }

        size_t valueNameLength = wcslen(valueName);

        fseek(fptr_configFile, 0, SEEK_END);
        size_t fileSize = ftell(fptr_configFile);
        rewind(fptr_configFile);

        if (fileSize <= valueNameLength)
        {
            fclose(fptr_configFile);
            return false;
        }

        WCHAR* fileString = new WCHAR[fileSize];

        fread(fileString, fileSize, 1, fptr_configFile);

        size_t i = 0;
        while (wcsncmp(fileString + i, valueName, valueNameLength) != 0)
        {
            i++;

            if (fileSize - i <= valueNameLength)
            {
                fclose(fptr_configFile);
                delete[] fileString;
                return false;
            }
        }

        i += valueNameLength;

        while (fileString[i] != L'=')
        {
            i++;

            if (i >= fileSize - 1)
            {
                fclose(fptr_configFile);
                delete[] fileString;
                return false;
            }
        }

        swscanf_s(fileString + i + 1, L"%lld", outNumber);

        fclose(fptr_configFile);
        delete[] fileString;

        return true;
    }
    static bool GetInt(const WCHAR* fileName, const WCHAR* valueName, unsigned long long* outNumber)
    {
        FILE* fptr_configFile;

        if (_wfopen_s(&fptr_configFile, fileName, L"r") == EINVAL || fptr_configFile == nullptr)
        {
            return false;
        }

        size_t valueNameLength = wcslen(valueName);

        fseek(fptr_configFile, 0, SEEK_END);
        size_t fileSize = ftell(fptr_configFile);
        rewind(fptr_configFile);

        if (fileSize <= valueNameLength)
        {
            fclose(fptr_configFile);
            return false;
        }

        WCHAR* fileString = new WCHAR[fileSize];

        fread(fileString, fileSize, 1, fptr_configFile);

        size_t i = 0;
        while (wcsncmp(fileString + i, valueName, valueNameLength) != 0)
        {
            i++;

            if (fileSize - i <= valueNameLength)
            {
                fclose(fptr_configFile);
                delete[] fileString;
                return false;
            }
        }

        i += valueNameLength;

        while (fileString[i] != L'=')
        {
            i++;

            if (i >= fileSize - 1)
            {
                fclose(fptr_configFile);
                delete[] fileString;
                return false;
            }
        }

        swscanf_s(fileString + i + 1, L"%llu", outNumber);

        fclose(fptr_configFile);
        delete[] fileString;

        return true;
    }

    static bool GetString(const char* fileName, const char* valueName, char* outString, const unsigned int outStringSize)
    {
        FILE* fptr_configFile;

        if (fopen_s(&fptr_configFile, fileName, "r") == EINVAL || fptr_configFile == nullptr)
        {
            return false;
        }

        size_t valueNameLength = strlen(valueName);

        fseek(fptr_configFile, 0, SEEK_END);
        size_t fileSize = ftell(fptr_configFile);
        rewind(fptr_configFile);

        if (fileSize <= valueNameLength)
        {
            fclose(fptr_configFile);
            return false;
        }

        char* fileString = new char[fileSize];

        fread(fileString, fileSize, 1, fptr_configFile);

        size_t i = 0;
        while (strncmp(fileString + i, valueName, valueNameLength) != 0)
        {
            i++;

            if (fileSize - i <= valueNameLength)
            {
                fclose(fptr_configFile);
                delete[] fileString;
                return false;
            }
        }

        i += valueNameLength;

        while (fileString[i] != '=')
        {
            i++;

            if (i >= fileSize - 1)
            {
                fclose(fptr_configFile);
                delete[] fileString;
                return false;
            }
        }

        sscanf_s(fileString + i + 1, "%s", outString, outStringSize);

        fclose(fptr_configFile);
        delete[] fileString;

        return true;
    }
    static bool GetString(const WCHAR* fileName, const WCHAR* valueName, WCHAR* outString, const unsigned int outStringSize)
    {
        FILE* fptr_configFile;

        if (_wfopen_s(&fptr_configFile, fileName, L"r") == EINVAL || fptr_configFile == nullptr)
        {
            return false;
        }

        size_t valueNameLength = wcslen(valueName);

        fseek(fptr_configFile, 0, SEEK_END);
        size_t fileSize = ftell(fptr_configFile);
        rewind(fptr_configFile);

        if (fileSize <= valueNameLength)
        {
            fclose(fptr_configFile);
            return false;
        }

        WCHAR* fileString = new WCHAR[fileSize];

        fread(fileString, fileSize, 1, fptr_configFile);

        size_t i = 0;
        while (wcsncmp(fileString + i, valueName, valueNameLength) != 0)
        {
            i++;

            if (fileSize - i <= valueNameLength)
            {
                fclose(fptr_configFile);
                delete[] fileString;
                return false;
            }
        }

        i += valueNameLength;

        while (fileString[i] != L'=')
        {
            i++;

            if (i >= fileSize - 1)
            {
                fclose(fptr_configFile);
                delete[] fileString;
                return false;
            }
        }

        swscanf_s(fileString + i + 1, L"%s", outString, outStringSize);

        fclose(fptr_configFile);
        delete[] fileString;

        return true;
    }
};