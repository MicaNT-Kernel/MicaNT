#pragma once

#include <string>
#include <vector>
#include <string_view>
#include <sstream>
#include <algorithm>
#include <span>
#include <cstdint>

#include "micant/um/servers/iis_was.hpp"

namespace micant::iis::appcmd::um {

using namespace micant::iis::um;

class AppCmdCommandExecutor {
public:
    static AppCmdCommandExecutor& Instance() noexcept {
        static AppCmdCommandExecutor s_instance;
        return s_instance;
    }

    int execute(std::span<const std::string_view> args, std::string& outResult) {
        if (args.empty() || args[0] == "help" || args[0] == "/?") {
            return executeHelp(outResult);
        }

        std::string verb(args[0]);
        std::transform(verb.begin(), verb.end(), verb.begin(), ::tolower);

        if (verb == "list") {
            return executeList(args.subspan(1), outResult);
        } else if (verb == "add") {
            return executeAdd(args.subspan(1), outResult);
        } else if (verb == "delete") {
            return executeDelete(args.subspan(1), outResult);
        } else if (verb == "start") {
            return executeStart(args.subspan(1), outResult);
        } else if (verb == "stop") {
            return executeStop(args.subspan(1), outResult);
        } else if (verb == "recycle") {
            return executeRecycle(args.subspan(1), outResult);
        } else {
            outResult = "ERROR ( message:The command \"" + verb + "\" is not supported by appcmd.exe. )\r\n";
            return 87; // ERROR_INVALID_PARAMETER
        }
    }

private:
    AppCmdCommandExecutor() = default;

    int executeHelp(std::string& out) {
        out = "Internet Information Services (IIS) Command-Line Tool (APPCMD.EXE)\r\n"
              "Version 10.0.26100.5900 (MicaNT Sovereign Enterprise Edition)\r\n\r\n"
              "Usage: appcmd <command> <object-type> <identifier> [/<switch>:<value> ...]\r\n\r\n"
              "Supported object types:\r\n"
              "  SITE        - Manage website definitions and bindings\r\n"
              "  APPPOOL     - Manage application pools, recycling, and worker process model\r\n"
              "  WP          - List active worker processes (w3wp.exe)\r\n"
              "  REQ         - List currently executing HTTP requests in flight\r\n"
              "  CONFIG      - View web server configuration sections and rules\r\n\r\n"
              "Supported commands:\r\n"
              "  list        - Display objects of specified type\r\n"
              "  add         - Create a new object (site, apppool)\r\n"
              "  delete      - Remove an existing object\r\n"
              "  start       - Start a website or application pool\r\n"
              "  stop        - Stop a website or application pool\r\n"
              "  recycle     - Recycle an application pool\r\n";
        return 0;
    }

    int executeList(std::span<const std::string_view> args, std::string& out) {
        if (args.empty()) {
            out = "ERROR ( message:Missing object type for 'list' command. Expected SITE, APPPOOL, WP, REQ, or CONFIG. )\r\n";
            return 87;
        }

        std::string objType(args[0]);
        std::transform(objType.begin(), objType.end(), objType.begin(), ::tolower);
        auto& was = WindowsProcessActivationService::Instance();

        if (objType == "site" || objType == "sites") {
            std::string filter;
            if (args.size() > 1 && !args[1].starts_with("/")) {
                filter = std::string(args[1]);
                if (filter.front() == '"' && filter.back() == '"') {
                    filter = filter.substr(1, filter.size() - 2);
                }
            }

            auto sites = was.getAllSites();
            std::ostringstream ss;
            for (const auto& site : sites) {
                if (!filter.empty() && site.name != filter && std::to_string(site.id) != filter) {
                    continue;
                }
                std::string stateStr = (site.state == SiteState::Started) ? "Started" : "Stopped";
                std::string bindingsStr;
                for (const auto& b : site.bindings) {
                    if (!bindingsStr.empty()) bindingsStr += ",";
                    bindingsStr += b.protocol + "/" + b.bindingInformation;
                }
                ss << "SITE \"" << site.name << "\" (id:" << site.id
                   << ",bindings:" << bindingsStr
                   << ",state:" << stateStr << ")\r\n";
            }
            out = ss.str();
            return 0;
        } else if (objType == "apppool" || objType == "apppools") {
            std::string filter;
            if (args.size() > 1 && !args[1].starts_with("/")) {
                filter = std::string(args[1]);
                if (filter.front() == '"' && filter.back() == '"') {
                    filter = filter.substr(1, filter.size() - 2);
                }
            }

            auto pools = was.getAllAppPools();
            std::ostringstream ss;
            for (const auto& pool : pools) {
                if (!filter.empty() && pool.name != filter) {
                    continue;
                }
                std::string stateStr = (pool.state == AppPoolState::Started) ? "Started" : "Stopped";
                std::string modeStr = (pool.pipelineMode == PipelineMode::Integrated) ? "Integrated" : "Classic";
                ss << "APPPOOL \"" << pool.name << "\" (MgdVersion:" << pool.managedRuntimeVersion
                   << ",MgdMode:" << modeStr
                   << ",state:" << stateStr << ")\r\n";
            }
            out = ss.str();
            return 0;
        } else if (objType == "wp" || objType == "wps") {
            auto workers = was.getWorkers();
            std::ostringstream ss;
            for (const auto& wp : workers) {
                if (wp->isRunning) {
                    ss << "WP \"" << wp->pid << "\" (apppool:" << wp->appPoolName << ")\r\n";
                }
            }
            out = ss.str();
            return 0;
        } else if (objType == "req" || objType == "requests") {
            auto workers = was.getWorkers();
            std::ostringstream ss;
            for (const auto& wp : workers) {
                if (wp->isRunning && wp->activeRequests > 0) {
                    ss << "REQUEST \"0x8000000100000001\" (url:GET /iisstart.htm,time:10 msec,client:127.0.0.1,stage:SendResponse,apppool:"
                       << wp->appPoolName << ")\r\n";
                }
            }
            if (ss.str().empty()) {
                ss << "REQUEST \"0x8000000100000001\" (url:GET /iisstart.htm,time:0 msec,client:127.0.0.1,stage:Ready,apppool:DefaultAppPool)\r\n";
            }
            out = ss.str();
            return 0;
        } else if (objType == "config") {
            std::string section = (args.size() > 1) ? std::string(args[1]) : "system.webServer/defaultDocument";
            std::ostringstream ss;
            ss << "CONFIG \"MACHINE/WEBROOT/APPHOST\" (section:" << section << ")\r\n"
               << "<" << section << ">\r\n"
               << "  <files>\r\n"
               << "    <add value=\"iisstart.htm\" />\r\n"
               << "    <add value=\"default.htm\" />\r\n"
               << "    <add value=\"index.html\" />\r\n"
               << "  </files>\r\n"
               << "</" << section << ">\r\n";
            out = ss.str();
            return 0;
        } else {
            out = "ERROR ( message:Unknown object type \"" + objType + "\" )\r\n";
            return 87;
        }
    }

    int executeAdd(std::span<const std::string_view> args, std::string& out) {
        if (args.empty()) {
            out = "ERROR ( message:Missing object type for 'add' command. Expected SITE or APPPOOL. )\r\n";
            return 87;
        }

        std::string objType(args[0]);
        std::transform(objType.begin(), objType.end(), objType.begin(), ::tolower);
        auto& was = WindowsProcessActivationService::Instance();

        if (objType == "site") {
            std::string name;
            uint32_t id = 2;
            std::string bindings = "*:8080:";
            std::string physicalPath = "C:\\inetpub\\newsite";
            std::string appPool = "DefaultAppPool";

            for (size_t i = 1; i < args.size(); ++i) {
                std::string_view arg = args[i];
                if (arg.starts_with("/name:")) {
                    name = std::string(arg.substr(6));
                    stripQuotes(name);
                } else if (arg.starts_with("/id:")) {
                    id = static_cast<uint32_t>(std::stoul(std::string(arg.substr(4))));
                } else if (arg.starts_with("/bindings:")) {
                    bindings = std::string(arg.substr(10));
                    stripQuotes(bindings);
                } else if (arg.starts_with("/physicalPath:")) {
                    physicalPath = std::string(arg.substr(14));
                    stripQuotes(physicalPath);
                } else if (arg.starts_with("/appPool:")) {
                    appPool = std::string(arg.substr(9));
                    stripQuotes(appPool);
                }
            }

            if (name.empty()) {
                out = "ERROR ( message:Site name cannot be empty. Use /name:<string>. )\r\n";
                return 87;
            }

            bool created = was.createSite(name, id, bindings, physicalPath, appPool);
            if (!created) {
                out = "ERROR ( message:Cannot create site \"" + name + "\". Site name or ID already exists. )\r\n";
                return 183; // ERROR_ALREADY_EXISTS
            }

            out = "SITE object \"" + name + "\" added\r\n";
            return 0;
        } else if (objType == "apppool") {
            std::string name;
            PipelineMode mode = PipelineMode::Integrated;
            std::string clr = "v4.0";

            for (size_t i = 1; i < args.size(); ++i) {
                std::string_view arg = args[i];
                if (arg.starts_with("/name:")) {
                    name = std::string(arg.substr(6));
                    stripQuotes(name);
                } else if (arg.starts_with("/managedPipelineMode:")) {
                    std::string m = std::string(arg.substr(21));
                    stripQuotes(m);
                    if (m == "Classic") mode = PipelineMode::Classic;
                } else if (arg.starts_with("/managedRuntimeVersion:")) {
                    clr = std::string(arg.substr(23));
                    stripQuotes(clr);
                }
            }

            if (name.empty()) {
                out = "ERROR ( message:AppPool name cannot be empty. Use /name:<string>. )\r\n";
                return 87;
            }

            bool created = was.createAppPool(name, mode, clr);
            if (!created) {
                out = "ERROR ( message:Cannot create AppPool \"" + name + "\". Pool already exists. )\r\n";
                return 183;
            }

            out = "APPPOOL object \"" + name + "\" added\r\n";
            return 0;
        } else {
            out = "ERROR ( message:Unsupported object type \"" + objType + "\" for 'add' command. )\r\n";
            return 87;
        }
    }

    int executeDelete(std::span<const std::string_view> args, std::string& out) {
        if (args.empty()) {
            out = "ERROR ( message:Missing object type for 'delete' command. )\r\n";
            return 87;
        }

        std::string objType(args[0]);
        std::transform(objType.begin(), objType.end(), objType.begin(), ::tolower);
        auto& was = WindowsProcessActivationService::Instance();

        if (args.size() < 2) {
            out = "ERROR ( message:Missing object identifier for delete. )\r\n";
            return 87;
        }

        std::string name(args[1]);
        stripQuotes(name);

        if (objType == "site") {
            bool ok = was.deleteSite(name);
            if (!ok) {
                out = "ERROR ( message:Failed to delete site \"" + name + "\". Site not found. )\r\n";
                return 2;
            }
            out = "SITE object \"" + name + "\" deleted\r\n";
            return 0;
        } else if (objType == "apppool") {
            bool ok = was.deleteAppPool(name);
            if (!ok) {
                out = "ERROR ( message:Failed to delete AppPool \"" + name + "\". Pool not found or still assigned to a site. )\r\n";
                return 2;
            }
            out = "APPPOOL object \"" + name + "\" deleted\r\n";
            return 0;
        } else {
            out = "ERROR ( message:Unsupported object type \"" + objType + "\" for 'delete' command. )\r\n";
            return 87;
        }
    }

    int executeStart(std::span<const std::string_view> args, std::string& out) {
        if (args.size() < 2) {
            out = "ERROR ( message:Usage: appcmd start <site|apppool> <name> )\r\n";
            return 87;
        }

        std::string objType(args[0]);
        std::transform(objType.begin(), objType.end(), objType.begin(), ::tolower);
        std::string name(args[1]);
        stripQuotes(name);
        auto& was = WindowsProcessActivationService::Instance();

        if (objType == "site") {
            if (!was.startSite(name)) {
                out = "ERROR ( message:Failed to start site \"" + name + "\" )\r\n";
                return 2;
            }
            out = "\"" + name + "\" successfully started\r\n";
            return 0;
        } else if (objType == "apppool") {
            if (!was.startAppPool(name)) {
                out = "ERROR ( message:Failed to start AppPool \"" + name + "\" )\r\n";
                return 2;
            }
            out = "\"" + name + "\" successfully started\r\n";
            return 0;
        } else {
            out = "ERROR ( message:Unsupported object type \"" + objType + "\" for 'start' command. )\r\n";
            return 87;
        }
    }

    int executeStop(std::span<const std::string_view> args, std::string& out) {
        if (args.size() < 2) {
            out = "ERROR ( message:Usage: appcmd stop <site|apppool> <name> )\r\n";
            return 87;
        }

        std::string objType(args[0]);
        std::transform(objType.begin(), objType.end(), objType.begin(), ::tolower);
        std::string name(args[1]);
        stripQuotes(name);
        auto& was = WindowsProcessActivationService::Instance();

        if (objType == "site") {
            if (!was.stopSite(name)) {
                out = "ERROR ( message:Failed to stop site \"" + name + "\" )\r\n";
                return 2;
            }
            out = "\"" + name + "\" successfully stopped\r\n";
            return 0;
        } else if (objType == "apppool") {
            if (!was.stopAppPool(name)) {
                out = "ERROR ( message:Failed to stop AppPool \"" + name + "\" )\r\n";
                return 2;
            }
            out = "\"" + name + "\" successfully stopped\r\n";
            return 0;
        } else {
            out = "ERROR ( message:Unsupported object type \"" + objType + "\" for 'stop' command. )\r\n";
            return 87;
        }
    }

    int executeRecycle(std::span<const std::string_view> args, std::string& out) {
        if (args.size() < 2) {
            out = "ERROR ( message:Usage: appcmd recycle apppool <name> )\r\n";
            return 87;
        }

        std::string objType(args[0]);
        std::transform(objType.begin(), objType.end(), objType.begin(), ::tolower);
        std::string name(args[1]);
        stripQuotes(name);
        auto& was = WindowsProcessActivationService::Instance();

        if (objType == "apppool") {
            if (!was.recycleAppPool(name)) {
                out = "ERROR ( message:Failed to recycle AppPool \"" + name + "\" )\r\n";
                return 2;
            }
            out = "\"" + name + "\" successfully recycled\r\n";
            return 0;
        } else {
            out = "ERROR ( message:Recycle is only applicable to 'apppool'. )\r\n";
            return 87;
        }
    }

    static void stripQuotes(std::string& str) {
        if (str.size() >= 2 && str.front() == '"' && str.back() == '"') {
            str = str.substr(1, str.size() - 2);
        }
    }
};

} // namespace micant::iis::appcmd::um
