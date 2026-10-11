#include "engine/project64_session.h"
#include "engine/rom.h"
#include <QDeadlineTimer>
#include <QDir>
#include <QFileInfo>
#ifdef Q_OS_WIN
#include <windows.h>
#include <tlhelp32.h>
#include <memory>
#include <vector>
#endif
namespace lordly {
void closeProject64(const QString &executable) {
#ifdef Q_OS_WIN
    struct CloseHandleDeleter {
        void operator()(void *handle) const { CloseHandle(handle); }
    };
    using Handle = std::unique_ptr<void, CloseHandleDeleter>;
    const auto target = QFileInfo(executable).canonicalFilePath();
    if (target.isEmpty())
        throw Error("Choose an existing Project64 executable");
    HANDLE raw = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (raw == INVALID_HANDLE_VALUE)
        throw Error("Cannot check running Project64 sessions");
    Handle snapshot(raw);
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (!Process32FirstW(snapshot.get(), &entry))
        throw Error("Cannot inspect running applications");
    struct Session { Handle process; DWORD id; };
    std::vector<Session> sessions;
    do {
        if (QString::fromWCharArray(entry.szExeFile).compare("Project64.exe", Qt::CaseInsensitive))
            continue;
        Handle process(OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                                   entry.th32ProcessID));
        if (!process) {
            if (GetLastError() == ERROR_INVALID_PARAMETER) continue; // Already exited.
            throw Error("Cannot inspect Project64. Close it before trying Cold boot again.");
        }
        wchar_t path[32768];
        DWORD size = DWORD(std::size(path));
        if (!QueryFullProcessImageNameW(process.get(), 0, path, &size))
            throw Error("Cannot identify Project64. Close it before trying Cold boot again.");
        const auto running = QFileInfo(QString::fromWCharArray(path, int(size))).canonicalFilePath();
        if (running.compare(target, Qt::CaseInsensitive))
            throw Error("Another Project64 installation is open. Close it before using Cold boot.");
        sessions.push_back({std::move(process), entry.th32ProcessID});
    } while (Process32NextW(snapshot.get(), &entry));
    for (const auto &session : sessions) {
        if (WaitForSingleObject(session.process.get(), 0) == WAIT_OBJECT_0) continue;
        struct Windows { DWORD pid; bool posted = false; } windows{session.id};
        EnumWindows([](HWND window, LPARAM param) -> BOOL {
            auto &state = *reinterpret_cast<Windows *>(param);
            DWORD pid = 0;
            GetWindowThreadProcessId(window, &pid);
            if (pid == state.pid && !GetWindow(window, GW_OWNER))
                state.posted |= bool(PostMessageW(window, WM_CLOSE, 0, 0));
            return TRUE;
        }, reinterpret_cast<LPARAM>(&windows));
        if (!windows.posted && WaitForSingleObject(session.process.get(), 0) != WAIT_OBJECT_0)
            throw Error("Project64 is not ready to close. Close it and try Cold boot again.");
    }
    QDeadlineTimer deadline(10000);
    for (const auto &session : sessions)
        if (WaitForSingleObject(session.process.get(), DWORD(qMax<qint64>(0, deadline.remainingTime()))) != WAIT_OBJECT_0)
            throw Error("Project64 has not closed. Finish closing it, then try Cold boot again. The new save has not been installed.");
#else
    Q_UNUSED(executable);
    throw Error("Project64 cold boot is currently supported on Windows");
#endif
}
}
