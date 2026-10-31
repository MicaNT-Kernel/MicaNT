#ifndef MICANT_WDSUTIL_HPP
#define MICANT_WDSUTIL_HPP

#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include "micant/um/servers/wds.hpp"

namespace micant::wdsutil::um {

class WdsutilCommandExecutor {
public:
    static WdsutilCommandExecutor& instance() noexcept {
        static WdsutilCommandExecutor s_inst;
        return s_inst;
    }

    int execute(const std::vector<std::string>& args, std::string& outStr) {
        std::ostringstream ss;
        if (args.empty() || args[0] == "/?" || args[0] == "-?" || args[0] == "help") {
            ss << "Windows Deployment Services Management Utility [Version 10.0.26100.1]\r\n"
               << "Copyright (c) 2026 MicaNT Sovereign Contributors.\r\n\r\n"
               << "Usage: wdsutil [command] [options]\r\n\r\n"
               << "Commands:\r\n"
               << "  /get-server         Displays WDS server configuration and state\r\n"
               << "  /get-allimages      Enumerates boot or install images\r\n"
               << "  /add-image          Adds a new boot or install image\r\n"
               << "  /remove-image       Removes an image from the WDS catalog\r\n"
               << "  /set-server         Configures WDS server and PXE parameters\r\n"
               << "  /dump               Exports configuration script\r\n";
            outStr = ss.str();
            return 0;
        }

        std::string verb = args[0];
        for (char& c : verb) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        auto& srv = micant::wds::WindowsDeploymentServer::instance();

        // --------------------------------------------------------------------
        // 1. /get-server
        // --------------------------------------------------------------------
        if (verb == "/get-server" || verb == "-get-server") {
            auto bootImgs = srv.getAllBootImages();
            auto instImgs = srv.getAllInstallImages();
            auto groups = srv.getImageGroupNames();
            auto deps = srv.getActiveDeployments();

            ss << "Windows Deployment Services Server Configuration\r\n"
               << "================================================\r\n"
               << "Server Name:              " << srv.getServerName() << "\r\n"
               << "Server FQDN:              " << srv.getServerFqdn() << "\r\n"
               << "TFTP Root Directory:      " << srv.getTftpRoot() << "\r\n"
               << "PXE Service State:        Listening (UDP 4011 / TFTP UDP 69)\r\n"
               << "PXE Client Answer Policy: " << (srv.getPxeAnswerAll() ? "Answer all clients (Known and Unknown)" : "Answer only known pre-staged clients") << "\r\n"
               << "Registered Boot Images:   " << bootImgs.size() << "\r\n"
               << "Install Image Groups:     " << groups.size() << "\r\n"
               << "Total Install Images:     " << instImgs.size() << "\r\n"
               << "Active Deployments:       " << deps.size() << "\r\n"
               << "TFTP Blocks Transferred:  " << srv.getTotalTftpBlocksSent() << "\r\n"
               << "TFTP Bytes Transferred:   " << srv.getTotalTftpBytesSent() << "\r\n"
               << "Total PXE Boot Requests:  " << srv.getTotalPxeRequests() << "\r\n"
               << "Deployments Completed:    " << srv.getTotalDeploymentsCompleted() << "\r\n"
               << "\r\nThe command completed successfully.\r\n";
            outStr = ss.str();
            return 0;
        }

        // --------------------------------------------------------------------
        // 2. /get-allimages /imagetype:<boot|install>
        // --------------------------------------------------------------------
        if (verb == "/get-allimages" || verb == "-get-allimages") {
            std::string imgType = "boot";
            for (size_t i = 1; i < args.size(); ++i) {
                if (args[i].starts_with("/imagetype:") || args[i].starts_with("-imagetype:")) {
                    imgType = args[i].substr(args[i].find(':') + 1);
                    for (char& c : imgType) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                }
            }

            if (imgType == "boot") {
                auto bootImgs = srv.getAllBootImages();
                ss << "Boot Images on " << srv.getServerFqdn() << ":\r\n"
                   << "--------------------------------------------------------------------------------\r\n"
                   << std::left << std::setw(36) << "Image Name"
                   << std::setw(8)  << "Arch"
                   << std::setw(12) << "Size (MB)"
                   << std::setw(16) << "Build"
                   << "Status\r\n"
                   << "--------------------------------------------------------------------------------\r\n";
                for (const auto& bi : bootImgs) {
                    uint32_t mb = static_cast<uint32_t>(bi.sizeBytes / (1024 * 1024));
                    ss << std::left << std::setw(36) << bi.imageName
                       << std::setw(8)  << bi.architecture
                       << std::setw(12) << mb
                       << std::setw(16) << bi.buildNumber
                       << (bi.enabled ? "Online" : "Disabled") << "\r\n";
                }
                ss << "\r\nThe command completed successfully.\r\n";
            } else {
                auto instImgs = srv.getAllInstallImages();
                ss << "Install Images on " << srv.getServerFqdn() << ":\r\n"
                   << "--------------------------------------------------------------------------------\r\n"
                   << std::left << std::setw(24) << "Group"
                   << std::setw(32) << "Image Name"
                   << std::setw(14) << "Edition"
                   << std::setw(12) << "Size (GB)"
                   << "Status\r\n"
                   << "--------------------------------------------------------------------------------\r\n";
                for (const auto& ii : instImgs) {
                    double gb = static_cast<double>(ii.sizeBytes) / (1024.0 * 1024.0 * 1024.0);
                    std::ostringstream gbSs;
                    gbSs << std::fixed << std::setprecision(2) << gb;
                    ss << std::left << std::setw(24) << ii.imageGroup
                       << std::setw(32) << ii.imageName
                       << std::setw(14) << ii.editionId
                       << std::setw(12) << gbSs.str()
                       << (ii.enabled ? "Online" : "Disabled") << "\r\n";
                }
                ss << "\r\nThe command completed successfully.\r\n";
            }
            outStr = ss.str();
            return 0;
        }

        // --------------------------------------------------------------------
        // 3. /add-image /imagefile:<path> /imagetype:<boot|install>
        // --------------------------------------------------------------------
        if (verb == "/add-image" || verb == "-add-image") {
            std::string imgFile, imgType = "boot", imgGroup = "MicaNT-Enterprise", imgName;
            for (size_t i = 1; i < args.size(); ++i) {
                if (args[i].starts_with("/imagefile:") || args[i].starts_with("-imagefile:")) {
                    imgFile = args[i].substr(args[i].find(':') + 1);
                } else if (args[i].starts_with("/imagetype:") || args[i].starts_with("-imagetype:")) {
                    imgType = args[i].substr(args[i].find(':') + 1);
                    for (char& c : imgType) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                } else if (args[i].starts_with("/imagegroup:") || args[i].starts_with("-imagegroup:")) {
                    imgGroup = args[i].substr(args[i].find(':') + 1);
                } else if (args[i].starts_with("/name:") || args[i].starts_with("-name:") ||
                           args[i].starts_with("/filename:") || args[i].starts_with("-filename:")) {
                    imgName = args[i].substr(args[i].find(':') + 1);
                }
            }

            if (imgName.empty()) {
                size_t slash = imgFile.find_last_of("/\\");
                imgName = (slash != std::string::npos) ? imgFile.substr(slash + 1) : imgFile;
            }

            if (imgType == "boot") {
                micant::wds::WdsBootImage bi{};
                bi.imageName = imgName;
                bi.description = "Imported Boot Image: " + imgName;
                bi.wimPath = imgFile;
                bi.architecture = "x64";
                bi.sizeBytes = 400 * 1024 * 1024;
                bi.enabled = true;
                srv.addBootImage(bi);
                ss << "Boot image '" << imgName << "' was successfully added to the server.\r\n"
                   << "The command completed successfully.\r\n";
            } else {
                micant::wds::WdsInstallImage ii{};
                ii.imageName = imgName;
                ii.description = "Imported Install Image: " + imgName;
                ii.imageGroup = imgGroup;
                ii.wimPath = imgFile;
                ii.editionId = "Enterprise";
                ii.architecture = "x64";
                ii.sizeBytes = 4ULL * 1024 * 1024 * 1024;
                ii.enabled = true;
                srv.addInstallImage(ii);
                ss << "Install image '" << imgName << "' was successfully added to image group '" << imgGroup << "'.\r\n"
                   << "The command completed successfully.\r\n";
            }
            outStr = ss.str();
            return 0;
        }

        // --------------------------------------------------------------------
        // 4. /remove-image /image:<name> /imagetype:<boot|install>
        // --------------------------------------------------------------------
        if (verb == "/remove-image" || verb == "-remove-image") {
            std::string imgName, imgType = "boot", imgGroup = "MicaNT-Enterprise";
            for (size_t i = 1; i < args.size(); ++i) {
                if (args[i].starts_with("/image:") || args[i].starts_with("-image:")) {
                    imgName = args[i].substr(args[i].find(':') + 1);
                } else if (args[i].starts_with("/imagetype:") || args[i].starts_with("-imagetype:")) {
                    imgType = args[i].substr(args[i].find(':') + 1);
                    for (char& c : imgType) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                } else if (args[i].starts_with("/imagegroup:") || args[i].starts_with("-imagegroup:")) {
                    imgGroup = args[i].substr(args[i].find(':') + 1);
                }
            }

            bool removed = false;
            if (imgType == "boot") {
                removed = srv.removeBootImage(imgName);
            } else {
                removed = srv.removeInstallImage(imgGroup, imgName);
            }

            if (removed) {
                ss << "Image '" << imgName << "' was successfully removed from the WDS catalog.\r\n"
                   << "The command completed successfully.\r\n";
            } else {
                ss << "Error: The specified image '" << imgName << "' was not found in the catalog.\r\n";
            }
            outStr = ss.str();
            return removed ? 0 : 1;
        }

        // --------------------------------------------------------------------
        // 5. /set-server /answerclients:<all|known|none>
        // --------------------------------------------------------------------
        if (verb == "/set-server" || verb == "-set-server") {
            for (size_t i = 1; i < args.size(); ++i) {
                if (args[i].starts_with("/answerclients:") || args[i].starts_with("-answerclients:")) {
                    std::string mode = args[i].substr(args[i].find(':') + 1);
                    for (char& c : mode) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    if (mode == "all") {
                        srv.setPxeAnswerMode(true);
                        ss << "Server PXE Answer Policy set to answer all clients.\r\n";
                    } else if (mode == "known") {
                        srv.setPxeAnswerMode(false);
                        ss << "Server PXE Answer Policy set to answer known pre-staged clients only.\r\n";
                    }
                }
            }
            ss << "The command completed successfully.\r\n";
            outStr = ss.str();
            return 0;
        }

        // --------------------------------------------------------------------
        // 6. /dump
        // --------------------------------------------------------------------
        if (verb == "/dump" || verb == "-dump") {
            ss << "# =====================================================================\r\n"
               << "# Windows Deployment Services Configuration Dump (wdsutil)\r\n"
               << "# Server: " << srv.getServerFqdn() << "\r\n"
               << "# =====================================================================\r\n"
               << "wdsutil /set-server /answerclients:" << (srv.getPxeAnswerAll() ? "all" : "known") << "\r\n";
            for (const auto& bi : srv.getAllBootImages()) {
                ss << "wdsutil /add-image /imagefile:\"" << bi.wimPath << "\" /imagetype:boot /name:\"" << bi.imageName << "\"\r\n";
            }
            for (const auto& ii : srv.getAllInstallImages()) {
                ss << "wdsutil /add-image /imagefile:\"" << ii.wimPath << "\" /imagetype:install /imagegroup:\"" << ii.imageGroup << "\" /name:\"" << ii.imageName << "\"\r\n";
            }
            ss << "# End of WDS Configuration Script\r\n";
            outStr = ss.str();
            return 0;
        }

        ss << "Unrecognized wdsutil command '" << verb << "'. Type 'wdsutil /?' for help.\r\n";
        outStr = ss.str();
        return 1;
    }
};

inline int ExecuteWdsutil(const std::vector<std::string>& args, std::string& outStr) {
    return WdsutilCommandExecutor::instance().execute(args, outStr);
}

inline int ExecuteWdsutilString(std::string_view cmdLine, std::string& outStr) {
    // Parse command line into arguments, respecting quotes
    std::vector<std::string> args;
    std::string cur;
    bool inQuote = false;

    for (size_t i = 0; i < cmdLine.size(); ++i) {
        char c = cmdLine[i];
        if (c == '"') {
            inQuote = !inQuote;
        } else if (c == ' ' && !inQuote) {
            if (!cur.empty()) {
                args.push_back(cur);
                cur.clear();
            }
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) args.push_back(cur);

    if (!args.empty() && (args[0] == "wdsutil" || args[0] == "wdsutil.exe")) {
        args.erase(args.begin());
    }

    return ExecuteWdsutil(args, outStr);
}

} // namespace micant::wdsutil::um

#endif // MICANT_WDSUTIL_HPP
