#define BOOST_CHRONO_DONT_PROVIDES_DEPRECATED_IO_SINCE_V2_0_0
#include "dsb/util.hpp"

#ifdef _WIN32
#   include <Windows.h>
#else
#   include <unistd.h>
#endif

#include <cstdio>

#include "boost/chrono.hpp"
#include "boost/filesystem.hpp"
#include "boost/foreach.hpp"
#include "boost/uuid/random_generator.hpp"
#include "boost/uuid/uuid_io.hpp"


void dsb::util::EncodeUint16(uint16_t source, char target[2])
{
    target[0] = source & 0xFF;
    target[1] = (source >> 8) & 0xFF;
}


uint16_t dsb::util::DecodeUint16(const char source[2])
{
    return static_cast<unsigned char>(source[0])
        | (static_cast<unsigned char>(source[1]) << 8);
}


std::string dsb::util::RandomUUID()
{
    boost::uuids::random_generator gen;
    return boost::uuids::to_string(gen());
}


std::string dsb::util::Timestamp()
{
    const auto t = boost::chrono::system_clock::now();
    std::ostringstream ss;
    ss << boost::chrono::time_fmt(boost::chrono::timezone::utc, "%Y%m%dT%H%M%SZ") << t;
    return ss.str();
}


dsb::util::TempDir::TempDir()
    : m_path(boost::filesystem::temp_directory_path()
             / boost::filesystem::unique_path())
{
    boost::filesystem::create_directory(m_path);
}

dsb::util::TempDir::~TempDir()
{
    boost::system::error_code ec;
    boost::filesystem::remove_all(m_path, ec);
}

const boost::filesystem::path& dsb::util::TempDir::Path() const
{
    return m_path;
}


#ifdef _WIN32
namespace
{
    std::vector<wchar_t> Utf8ToUtf16(const std::string& utf8)
    {
        auto buf = std::vector<wchar_t>();
        if (utf8.empty()) return buf;

        const auto len = MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            utf8.data(),
            utf8.size(),
            nullptr,
            0);
        if (len == 0) {
            if (GetLastError() == ERROR_NO_UNICODE_TRANSLATION) {
                throw std::runtime_error("Invalid UTF-8 characters in string");
            } else {
                throw std::logic_error("Internal error in " __FUNCTION__);
            }
        }
        buf.resize(len);
        MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            utf8.data(),
            utf8.size(),
            buf.data(),
            buf.size());
        return buf;
    }
}
#endif

#ifndef _WIN32
namespace
{
    bool IsExecutable(const std::string& path)
    {
        return access(path.c_str(), X_OK) == 0;
    }
}
#endif


void dsb::util::SpawnProcess(
    const std::string& program,
    const std::vector<std::string>& args)
{
#ifdef _WIN32
    auto cmdLine = Utf8ToUtf16(program);
    BOOST_FOREACH (const auto& arg, args) {
        cmdLine.push_back(' ');
        cmdLine.push_back('"');
        const auto argW = Utf8ToUtf16(arg);
        cmdLine.insert(cmdLine.end(), argW.begin(), argW.end());
        cmdLine.push_back('"');
    }
    cmdLine.push_back(0);

    STARTUPINFOW startupInfo;
    std::memset(&startupInfo, 0, sizeof(startupInfo));
    startupInfo.cb = sizeof(startupInfo);

    PROCESS_INFORMATION processInfo;
    if (CreateProcessW(
        nullptr,
        cmdLine.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NEW_CONSOLE,
        nullptr,
        nullptr,
        &startupInfo,
        &processInfo)) {
        return;
    }

#else // not Win32
    if (!IsExecutable(program)) {
        throw std::runtime_error("Not an executable file: "+program);
    }

    std::vector<const char*> argz;
    std::cout << program;
    argz.push_back(program.c_str());
    for (auto it = begin(args); it != end(args); ++it) {
        std::cout << ' ' << *it;
        argz.push_back(it->c_str());
    }
    std::cout << std::endl;
    argz.push_back(nullptr);

    const auto pid = fork();
    if (pid == 0) {
        // We are in child process; execute program immediately.
        // NOTE: execv() doesn't actually modify the argument vector, so the
        // const_cast is OK. For the gritty details, see:
        // http://pubs.opengroup.org/onlinepubs/9699919799/functions/exec.html
        execv(program.c_str(), const_cast<char* const*>(argz.data()));
        std::perror("dsb::util::SpawnProcess(): failed to execute program");
        _exit(1);
    } else if (pid > 0) {
        // We are in parent process; return immediately.
        return;
    }
#endif
    throw std::runtime_error("Failed to start process: " + program);
}


boost::filesystem::path dsb::util::ThisExePath()
{
#if defined(_WIN32)
    std::vector<wchar_t> buf(MAX_PATH);
    for (;;) {
        const auto len = GetModuleFileNameW(nullptr, buf.data(), buf.size());
        if (len == 0) {
            throw std::runtime_error("Failed to determine executable path");
        } else if (len >= buf.size()) {
            buf.resize(len * 2);
        } else {
            return boost::filesystem::path(buf.begin(), buf.begin()+len);
        }
    }
#elif defined(__linux__)
    return boost::filesystem::read_symlink("/proc/self/exe");
#else
    assert (!"ThisExePath() not implemented for POSIX platforms yet");
#endif
}
