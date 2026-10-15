#include "app/diagnostics.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QStandardPaths>
#include <csignal>
#include <exception>
#include <cstdio>
#include <cstring>
#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#endif
namespace lordly {
namespace {
QString directory,crashPath;
QFile logFile;
QMutex logMutex;
QtMessageHandler previous=nullptr;
#ifdef Q_OS_WIN
HANDLE crashFile=INVALID_HANDLE_VALUE;
volatile LONG crashBytes=0;
LPTOP_LEVEL_EXCEPTION_FILTER priorFilter=nullptr;
std::terminate_handler priorTerminate=nullptr;
using SignalHandler=void(*)(int);
SignalHandler priorAbort=nullptr;
void raw(const char *text) {
    if(crashFile==INVALID_HANDLE_VALUE)return;
    LONG length=0;while(length<65536 && text[length])++length;
    const LONG start=InterlockedExchangeAdd(&crashBytes,length);
    if(start>=65536)return;
    DWORD written=0;WriteFile(crashFile,text,DWORD(qMin(length,65536-start)),&written,nullptr);FlushFileBuffers(crashFile);
}
LONG WINAPI unhandled(EXCEPTION_POINTERS *info) {
    char text[512];
    snprintf(text,sizeof(text),"CRASH: unhandled Windows exception\r\nCode: 0x%08lX\r\nAddress: %p\r\n",info->ExceptionRecord->ExceptionCode,info->ExceptionRecord->ExceptionAddress);raw(text);
    HMODULE module=nullptr;
    if(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(info->ExceptionRecord->ExceptionAddress),&module)) {
        char path[MAX_PATH]={};GetModuleFileNameA(module,path,MAX_PATH);
        snprintf(text,sizeof(text),"Module: %s\r\nModule offset: 0x%llX\r\n",path,
            static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(info->ExceptionRecord->ExceptionAddress)-reinterpret_cast<uintptr_t>(module)));raw(text);
    }
#ifdef _M_X64
    snprintf(text,sizeof(text),"RIP: %llX  RSP: %llX  RBP: %llX\r\n",info->ContextRecord->Rip,info->ContextRecord->Rsp,info->ContextRecord->Rbp);raw(text);
#endif
    return EXCEPTION_EXECUTE_HANDLER;
}
void terminated() {
    raw("CRASH: unhandled C++ exception / terminate\r\n");
    try {if(auto error=std::current_exception())std::rethrow_exception(error);}
    catch(const std::exception &e){raw(e.what());raw("\r\n");}catch(...){}
    std::_Exit(1);
}
void aborted(int) {raw("CRASH: abort / fatal application error\r\n");std::_Exit(1);}
#endif
void message(QtMsgType type,const QMessageLogContext &context,const QString &text) {
    QMutexLocker lock(&logMutex);
    if(logFile.isOpen() && logFile.size()<1024*1024) {
        const auto line=(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)+" ["+QString::number(int(type))+"] "+text.left(16000)+'\n').toUtf8();
        const auto remaining=1024*1024-logFile.size();logFile.write(line.left(remaining));logFile.flush();
    }
    lock.unlock();
    if(previous)previous(type,context,text);
}
}
QString Diagnostics::folder() {
    return directory.isEmpty()?QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/Crash logs":directory;
}
QStringList Diagnostics::crashReports() {
    QStringList result;
    for(const auto &entry:QDir(folder()).entryInfoList({"*.crash.txt"},QDir::Files,QDir::Name|QDir::Reversed)) {
        QFile file(entry.absoluteFilePath());if(file.open(QIODevice::ReadOnly)&&file.read(65536).contains("CRASH:"))result<<entry.absoluteFilePath();
    }
    return result;
}
Diagnostics::Diagnostics(QString folder) {
    directory=folder.isEmpty()?Diagnostics::folder():folder;
    if(!QDir().mkpath(directory))return;
    // Fourteen previous files plus this session's log and crash report. Open files from another instance
    // cannot be removed on Windows and are left alone.
    const auto files=QDir(directory).entryInfoList({"*.session.log","*.crash.txt"},QDir::Files,QDir::Name|QDir::Reversed);
    for(int i=14;i<files.size();++i)QFile::remove(files[i].absoluteFilePath());
    const auto stem=QDir(directory).filePath(QDateTime::currentDateTimeUtc().toString("yyyyMMdd-hhmmss-zzz")+"-"+QString::number(QCoreApplication::applicationPid()));
    logFile.setFileName(stem+".session.log");
    if(!logFile.open(QIODevice::WriteOnly))qWarning()<<"Cannot open diagnostic log:"<<logFile.errorString();
    crashPath=stem+".crash.txt";
#ifdef Q_OS_WIN
    crashFile=CreateFileW(reinterpret_cast<LPCWSTR>(crashPath.utf16()),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    crashBytes=0;
    QFile executable(QCoreApplication::applicationFilePath());QByteArray identity;
    if(executable.open(QIODevice::ReadOnly)){QCryptographicHash hash(QCryptographicHash::Sha256);if(hash.addData(&executable))identity=hash.result().toHex();}
    const auto intro=("Lordly Caliber "+QCoreApplication::applicationVersion()+"\r\nStarted: "+QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)+"\r\nExecutable: "+QCoreApplication::applicationFilePath()+"\r\nExecutable SHA256: "+QString::fromLatin1(identity)+"\r\n").toUtf8();raw(intro.constData());
    priorFilter=SetUnhandledExceptionFilter(unhandled);
    priorTerminate=std::set_terminate(terminated);priorAbort=std::signal(SIGABRT,aborted);
#endif
    previous=qInstallMessageHandler(message);
    installed_=true;
    qInfo().noquote()<<"Lordly Caliber"<<QCoreApplication::applicationVersion()<<"started; recovery snapshots and crash logs are local.";
}
Diagnostics::~Diagnostics() {
    if(!installed_)return;
    qInstallMessageHandler(previous);logFile.close();
#ifdef Q_OS_WIN
    const bool unwinding=std::uncaught_exceptions()>0;
    if(unwinding)raw("CRASH: unhandled C++ exception while closing the application\r\n");
    SetUnhandledExceptionFilter(priorFilter);
    std::set_terminate(priorTerminate);std::signal(SIGABRT,priorAbort);
    if(crashFile!=INVALID_HANDLE_VALUE)CloseHandle(crashFile);
    crashFile=INVALID_HANDLE_VALUE;
    if(!unwinding)QFile::remove(crashPath);
#endif
}
}
