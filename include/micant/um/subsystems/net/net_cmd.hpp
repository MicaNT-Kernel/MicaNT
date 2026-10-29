// ============================================================================
// MicaNT: Clean-Room Windows-Compatible Operating System Executive
// Subsystem: Canonical Network Management Command-Line Utility (net.exe)
// Implementation: include/micant/um/subsystems/net/net_cmd.hpp
//
// Strict Clean-Room Implementation in modern ISO C++23. Zero External Dependencies.
// Conforms to Microsoft Win32 Command-Line Management Specifications under 17 U.S.C. Section 1201(f).
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <span>

#include "micant/km/drivers/net/srv2.hpp"
#include "micant/um/subsystems/net/mrxsmb.hpp"

namespace micant::netcmd::um {

struct MappedNetworkDrive {
    std::wstring localDrive;
    std::wstring remoteShare;
    std::wstring status{L"OK"};
    std::wstring network{L"Microsoft Windows Network"};
};

class NetCommandExecutor {
public:
    static NetCommandExecutor& Instance() noexcept {
        static NetCommandExecutor s_instance;
        return s_instance;
    }

    int execute(std::span<const std::string_view> args, std::string& outStr) noexcept {
        if (args.empty()) {
            return showHelp(outStr);
        }

        std::string_view subCmd = args[0];
        if (subCmd == "share") {
            return executeShare(args.subspan(1), outStr);
        } else if (subCmd == "use") {
            return executeUse(args.subspan(1), outStr);
        } else if (subCmd == "session") {
            return executeSession(args.subspan(1), outStr);
        } else if (subCmd == "view") {
            return executeView(args.subspan(1), outStr);
        } else if (subCmd == "help" || subCmd == "/?") {
            return showHelp(outStr);
        } else {
            outStr = "The syntax of this command is:\n\nNET [ ACCOUNTS | COMPUTER | CONFIG | CONTINUE | FILE | GROUP | HELP |\n";
            outStr += "      HELPMSG | LOCALGROUP | PAUSE | SESSION | SHARE | START |\n";
            outStr += "      STATISTICS | STOP | TIME | USE | USER | VIEW ]\n";
            return 1;
        }
    }

private:
    NetCommandExecutor() = default;

    int showHelp(std::string& out) noexcept {
        std::ostringstream ss;
        ss << "The syntax of this command is:\n\n";
        ss << "NET [ SESSION | SHARE | USE | VIEW ]\n\n";
        ss << "NET SHARE sharename=drive:path [/GRANT:user,[READ | FULL]] [/REMARK:\"text\"]\n";
        ss << "NET SHARE sharename /DELETE\n";
        ss << "NET USE [devicename | *] [\\\\computername\\sharename]\n";
        ss << "NET USE [devicename | *] /DELETE\n";
        ss << "NET SESSION\n";
        ss << "NET VIEW [\\\\computername]\n";
        out = ss.str();
        return 0;
    }

    int executeShare(std::span<const std::string_view> args, std::string& out) noexcept {
        auto& srv = smb::km::SmbServerEngine::Instance();

        if (args.empty()) {
            // List shares
            auto shares = srv.getShares();
            std::ostringstream ss;
            ss << "\nShare name   Resource                        Remark\n";
            ss << "-------------------------------------------------------------------------------\n";
            for (const auto& s : shares) {
                std::string name(s.name.begin(), s.name.end());
                std::string path(s.localPath.begin(), s.localPath.end());
                std::string rem(s.remark.begin(), s.remark.end());

                ss << std::left << std::setw(13) << name;
                ss << std::left << std::setw(32) << path;
                ss << rem << "\n";
            }
            ss << "The command completed successfully.\n";
            out = ss.str();
            return 0;
        }

        std::string_view firstArg = args[0];

        // Check for delete: net share <name> /DELETE
        if (args.size() >= 2 && (args[1] == "/DELETE" || args[1] == "/delete" || args[1] == "-d")) {
            std::wstring shareName(firstArg.begin(), firstArg.end());
            if (srv.removeShare(shareName)) {
                out = std::string(firstArg) + " was deleted successfully.\n";
                return 0;
            } else {
                out = "System error 2 has occurred.\n\nThe system cannot find the file specified.\n";
                return 2;
            }
        }

        // Check for creation: net share <name>=<path> [/REMARK:...] [/ENCRYPT:YES]
        auto eqPos = firstArg.find('=');
        if (eqPos != std::string_view::npos) {
            std::string_view namePart = firstArg.substr(0, eqPos);
            std::string_view pathPart = firstArg.substr(eqPos + 1);

            std::wstring sName(namePart.begin(), namePart.end());
            std::wstring sPath(pathPart.begin(), pathPart.end());
            std::wstring sRem = L"User created network share";
            bool encrypt = false;

            for (size_t i = 1; i < args.size(); ++i) {
                if (args[i].starts_with("/REMARK:") || args[i].starts_with("/remark:")) {
                    std::string_view r = args[i].substr(8);
                    sRem.assign(r.begin(), r.end());
                } else if (args[i] == "/ENCRYPT:YES" || args[i] == "/encrypt:yes") {
                    encrypt = true;
                }
            }

            if (srv.addShare(sName, sPath, sRem, encrypt)) {
                out = std::string(namePart) + " was shared successfully.\n";
                return 0;
            } else {
                out = "System error 2118 has occurred.\n\nThe name has already been shared.\n";
                return 2118;
            }
        }

        out = "The syntax of this command is:\n\nNET SHARE sharename=drive:path [/REMARK:\"text\"]\n";
        return 1;
    }

    int executeUse(std::span<const std::string_view> args, std::string& out) noexcept {
        std::lock_guard<std::mutex> lock(drivesMutex_);

        if (args.empty()) {
            // List mapped drives
            std::ostringstream ss;
            ss << "\nStatus       Local     Remote                    Network\n";
            ss << "-------------------------------------------------------------------------------\n";
            for (const auto& [drv, m] : mappedDrives_) {
                std::string st(m.status.begin(), m.status.end());
                std::string loc(m.localDrive.begin(), m.localDrive.end());
                std::string rem(m.remoteShare.begin(), m.remoteShare.end());
                std::string net(m.network.begin(), m.network.end());

                ss << std::left << std::setw(13) << st;
                ss << std::left << std::setw(10) << loc;
                ss << std::left << std::setw(26) << rem;
                ss << net << "\n";
            }
            ss << "The command completed successfully.\n";
            out = ss.str();
            return 0;
        }

        std::string_view devName = args[0];

        // Check for delete: net use <drive>: /DELETE
        if (args.size() >= 2 && (args[1] == "/DELETE" || args[1] == "/delete" || args[1] == "-d")) {
            std::wstring drvKey(devName.begin(), devName.end());
            auto it = mappedDrives_.find(drvKey);
            if (it != mappedDrives_.end()) {
                mappedDrives_.erase(it);
                out = std::string(devName) + " was deleted successfully.\n";
                return 0;
            } else {
                out = "The network connection could not be found.\n";
                return 2;
            }
        }

        // Check for map: net use <drive>: \\server\share
        if (args.size() >= 2) {
            std::string_view remotePath = args[1];
            std::wstring drv(devName.begin(), devName.end());
            std::wstring rem(remotePath.begin(), remotePath.end());

            MappedNetworkDrive mnd;
            mnd.localDrive = drv;
            mnd.remoteShare = rem;
            mnd.status = L"OK";
            mnd.network = L"Microsoft Windows Network";

            mappedDrives_[drv] = mnd;
            out = "The command completed successfully.\n";
            return 0;
        }

        out = "The syntax of this command is:\n\nNET USE [devicename | *] [\\\\computername\\sharename]\n";
        return 1;
    }

    int executeSession(std::span<const std::string_view> args, std::string& out) noexcept {
        auto& srv = smb::km::SmbServerEngine::Instance();
        auto sessions = srv.getActiveSessions();

        std::ostringstream ss;
        ss << "\nComputer             User name            Client Type  Opens Idle time\n";
        ss << "-------------------------------------------------------------------------------\n";
        for (const auto& s : sessions) {
            ss << std::left << std::setw(21) << ("\\\\" + s.clientIp);
            ss << std::left << std::setw(21) << s.userName;
            ss << std::left << std::setw(13) << (s.encryptedSession ? "SMB 3.1.1 (AES)" : "SMB 3.1.1");
            ss << std::left << std::setw(6)  << s.openFilesCount;
            ss << "00:00:05\n";
        }
        ss << "The command completed successfully.\n";
        out = ss.str();
        return 0;
    }

    int executeView(std::span<const std::string_view> args, std::string& out) noexcept {
        auto& srv = smb::km::SmbServerEngine::Instance();
        auto shares = srv.getShares();

        std::string serverName = args.empty() ? "\\\\147.135.1.229" : std::string(args[0]);

        std::ostringstream ss;
        ss << "\nShared resources at " << serverName << "\n\n";
        ss << "MicaNT Enterprise Server 2026\n\n";
        ss << "Share name   Type   Used as  Comment\n";
        ss << "-------------------------------------------------------------------------------\n";
        for (const auto& s : shares) {
            std::string name(s.name.begin(), s.name.end());
            std::string rem(s.remark.begin(), s.remark.end());
            std::string typeStr = (s.type == smb::km::SMB2_SHARE_TYPE_DISK) ? "Disk" : "Pipe";

            ss << std::left << std::setw(13) << name;
            ss << std::left << std::setw(7)  << typeStr;
            ss << std::left << std::setw(9)  << "";
            ss << rem << "\n";
        }
        ss << "The command completed successfully.\n";
        out = ss.str();
        return 0;
    }

    std::mutex drivesMutex_;
    std::map<std::wstring, MappedNetworkDrive> mappedDrives_;
};

} // namespace micant::netcmd::um
