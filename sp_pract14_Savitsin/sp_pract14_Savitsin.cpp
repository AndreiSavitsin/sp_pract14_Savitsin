#include <iostream>
#include <Windows.h>
#include <random>

#define MAX_LOGS 1000
#define MAX_TEXT 128



struct LogRecord
{
    DWORD threadId;
    int priority;
    DWORD tick;

    char message[MAX_TEXT];
};

struct LogBuffer
{
    LogRecord records[MAX_LOGS];
    LONG index;
};

LogBuffer g_log;
CRITICAL_SECTION g_cs;
volatile LONG g_stop = 0;

volatile LONG g_countT1 = 0;
volatile LONG g_countT2 = 0;
volatile LONG g_countT3 = 0;

bool g_useCS = false;

void Lock()
{
    if (g_useCS)
    {
        EnterCriticalSection(&g_cs);
    }
}

void Unlock()
{
    if (g_useCS)
    {
        LeaveCriticalSection(&g_cs);
    }
}

DWORD WINAPI LoggerThread(LPVOID param)
{
    const char* name = (const char*)param;
    int prior = GetThreadPriority(GetCurrentThread());

    std::mt19937 mt(GetCurrentThreadId());

    while (g_stop == 0)
    {
        Lock();
        LONG index = g_log.index;
        if (index < MAX_LOGS)
        {
            g_log.records[index].threadId = GetCurrentThreadId();
            g_log.records[index].priority = prior;
            g_log.records[index].tick = GetTickCount();
            strcpy_s(g_log.records[index].message, MAX_TEXT, name);
            g_log.index = index + 1;

            if (strcmp(name, "T1") == 0) InterlockedIncrement(&g_countT1);
            else if (strcmp(name, "T2") == 0) InterlockedIncrement(&g_countT2);
            else if (strcmp(name, "T3") == 0) InterlockedIncrement(&g_countT3);

        }

        Unlock();

        Sleep(10 + mt() % 91);
    }

    return 0;
}

DWORD WINAPI  ObserverThread(LPVOID)
{
    while (g_stop == 0)
    {
        Lock();
        LONG index = g_log.index;
        LONG start = (index > 5) ? index - 5 : 0;

        std::cout << "Последние " << (index - start) << " записей:\n";

        for (LONG i = start; i < index; i++)
        {
            std::cout << i << " | id потока: " << g_log.records[i].threadId <<
                " | приоритет: " << g_log.records[i].priority <<
                " | время: " << g_log.records[i].tick <<
                " | сообщение: " << g_log.records[i].message << std::endl;
        }

        Unlock();

        Sleep(100);
    }

    return 0;
}

int main()
{
    setlocale(LC_ALL, "Russian");

    srand(GetTickCount());

    std::cout << "Выберите режим работы:\n";
    std::cout << "1 - с критическими секциями\n";
    std::cout << "0 - без критических секций\n";
    int choice;
    std::cin >> choice;

    g_useCS = choice;

    SetProcessAffinityMask(GetCurrentProcess(), 1);

    g_log.index = 0;

    InitializeCriticalSection(&g_cs);

    HANDLE threads[4];

    threads[0] = CreateThread(NULL, 0, LoggerThread, (LPVOID)"T1", 0, NULL);
    SetThreadPriority(threads[0], THREAD_PRIORITY_HIGHEST);

    threads[1] = CreateThread(NULL, 0, LoggerThread, (LPVOID)"T2", 0, NULL);
    SetThreadPriority(threads[1], THREAD_PRIORITY_NORMAL);

    threads[2] = CreateThread(NULL, 0, LoggerThread, (LPVOID)"T3", 0, NULL);
    SetThreadPriority(threads[2], THREAD_PRIORITY_BELOW_NORMAL);

    threads[3] = CreateThread(NULL, 0, ObserverThread, NULL, 0, NULL);
    SetThreadPriority(threads[3], THREAD_PRIORITY_LOWEST);

    Sleep(5000);

    InterlockedExchange(&g_stop, 1);
    WaitForMultipleObjects(4, threads, TRUE, INFINITE);

    for (int i = 0; i < 4; i++)
    {
        CloseHandle(threads[i]);
    }

    DeleteCriticalSection(&g_cs);

    std::cout << "\nРезультаты\n";
    std::cout << "T1 (Высокий)\t" << g_countT1 << "\n";
    std::cout << "T2 (Обычный)\t" << g_countT2 << "\n";
    std::cout << "T3 (Ниже обычного)\t" << g_countT3 << "\n";
    std::cout << "Сумма по потокам\t" << (g_countT1 + g_countT2 + g_countT3) << "\n";
    std::cout << "Всего в журнале\t" << g_log.index << "\n";
    std::cout << "Потеряно\t" << ((g_countT1 + g_countT2 + g_countT3) - g_log.index) << "\n";
    
    return 0;
}