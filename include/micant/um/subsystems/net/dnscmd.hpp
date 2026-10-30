#pragma once

#include <string>
#include <vector>
#include <string_view>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <span>
#include <cstdint>

#include "micant/um/servers/dns_server.hpp"

namespace micant::dns::dnscmd::um {

using namespace micant::dns::server::um;

class DnsCmdCommandExecutor {
public:
    static DnsCmdCommandExecutor& Instance() noexcept {
        static DnsCmdCommandExecutor s_instance;
        return s_instance;
    }

    int execute(std::span<const std::string_view> args, std::string& outResult) {
        if (args.empty() || args[0] == "/?" || args[0] == "/help" || args[0] == "help") {
            return executeHelp(outResult);
        }

        std::string cmd(args[0]);
        std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

        if (cmd == "/enumzones" || cmd == "enumzones") {
            return executeEnumZones(outResult);
        } else if (cmd == "/zoneadd" || cmd == "zoneadd") {
            return executeZoneAdd(args.subspan(1), outResult);
        } else if (cmd == "/zonedelete" || cmd == "zonedelete") {
            return executeZoneDelete(args.subspan(1), outResult);
        } else if (cmd == "/recordadd" || cmd == "recordadd") {
            return executeRecordAdd(args.subspan(1), outResult);
        } else if (cmd == "/recorddelete" || cmd == "recorddelete") {
            return executeRecordDelete(args.subspan(1), outResult);
        } else if (cmd == "/enumrecords" || cmd == "enumrecords") {
            return executeEnumRecords(args.subspan(1), outResult);
        } else if (cmd == "/clearcache" || cmd == "clearcache") {
            return executeClearCache(outResult);
        } else if (cmd == "/zonerefresh" || cmd == "zonerefresh") {
            return executeZoneRefresh(args.subspan(1), outResult);
        } else if (cmd == "/info" || cmd == "info") {
            return executeInfo(outResult);
        } else {
            outResult = "DNSCMD: Command \"" + cmd + "\" is unrecognized.\r\n"
                        "Type 'dnscmd /?' for command usage.\r\n";
            return 87; // ERROR_INVALID_PARAMETER
        }
    }

private:
    DnsCmdCommandExecutor() = default;

    int executeHelp(std::string& out) {
        out = "DNS Server Command-Line Administration Tool (DNSCMD.EXE)\r\n"
              "Version 10.0.26100.1 (MicaNT Sovereign Enterprise Edition)\r\n\r\n"
              "Usage: dnscmd [<ServerName>] <Command> [<CommandParameters>]\r\n\r\n"
              "Commands:\r\n"
              "  /enumzones                     - Enumerate all authoritative forward and reverse zones\r\n"
              "  /zoneadd <zone> /primary       - Create a new primary authoritative zone\r\n"
              "  /zonedelete <zone> [/f]        - Delete an authoritative zone\r\n"
              "  /recordadd <zone> <node> <type> <data> - Add a resource record to a zone\r\n"
              "  /recorddelete <zone> <node> <type> [<data>] [/f] - Delete a resource record\r\n"
              "  /enumrecords <zone> <node>     - Enumerate resource records at a specific node\r\n"
              "  /clearcache                    - Clear the DNS server recursive resolution cache\r\n"
              "  /zonerefresh <zone>            - Force refresh and serial synchronization for zone\r\n"
              "  /info                          - Display server status, version, and query vitals\r\n\r\n"
              "Command completed successfully.\r\n";
        return 0;
    }

    int executeEnumZones(std::string& out) {
        auto& engine = DnsServerEngine::Instance();
        auto zones = engine.getAllZones();

        std::ostringstream ss;
        ss << "Enumerated zone list:\r\n"
           << "  Zone name               Type       Storage          Status\r\n"
           << "  -------------------------------------------------------------\r\n";

        for (const auto& z : zones) {
            std::string typeStr = (z.zoneType == ZoneType::Primary) ? "Primary" : "Secondary";
            ss << "  " << std::left << std::setw(24) << z.zoneName
               << std::setw(11) << typeStr
               << std::setw(17) << "File"
               << "Running\r\n";
        }
        ss << "Command completed successfully.\r\n";
        out = ss.str();
        return 0;
    }

    int executeZoneAdd(std::span<const std::string_view> args, std::string& out) {
        if (args.empty()) {
            out = "DNSCMD: Missing zone name for /zoneadd.\r\n";
            return 87;
        }

        std::string zoneName(args[0]);
        ZoneType type = ZoneType::Primary;
        if (args.size() > 1) {
            std::string t(args[1]);
            std::transform(t.begin(), t.end(), t.begin(), ::tolower);
            if (t == "/secondary" || t == "secondary") type = ZoneType::Secondary;
        }

        auto& engine = DnsServerEngine::Instance();
        if (!engine.addZone(zoneName, type)) {
            out = "DNSCMD: Failed to create zone \"" + zoneName + "\". Zone already exists.\r\n";
            return 183;
        }

        out = "DNS Server successfully created zone " + zoneName + ".\r\n"
              "Command completed successfully.\r\n";
        return 0;
    }

    int executeZoneDelete(std::span<const std::string_view> args, std::string& out) {
        if (args.empty()) {
            out = "DNSCMD: Missing zone name for /zonedelete.\r\n";
            return 87;
        }

        std::string zoneName(args[0]);
        auto& engine = DnsServerEngine::Instance();
        if (!engine.deleteZone(zoneName)) {
            out = "DNSCMD: Failed to delete zone \"" + zoneName + "\". Zone not found.\r\n";
            return 2;
        }

        out = "DNS Server successfully deleted zone " + zoneName + ".\r\n"
              "Command completed successfully.\r\n";
        return 0;
    }

    int executeRecordAdd(std::span<const std::string_view> args, std::string& out) {
        if (args.size() < 4) {
            out = "DNSCMD: Usage: dnscmd /recordadd <zone> <node> <type> <data...>\r\n";
            return 87;
        }

        std::string zoneName(args[0]);
        std::string nodeName(args[1]);
        std::string typeStr(args[2]);
        uint16_t type = StringToRecordType(typeStr);

        std::string data;
        for (size_t i = 3; i < args.size(); ++i) {
            if (!data.empty()) data += " ";
            data += args[i];
        }

        std::string fqdn;
        if (nodeName == "@") {
            fqdn = zoneName;
        } else if (nodeName.find('.') != std::string::npos) {
            fqdn = nodeName;
        } else {
            fqdn = nodeName + "." + zoneName;
        }

        auto& engine = DnsServerEngine::Instance();
        auto zone = engine.getZone(zoneName);
        if (!zone) {
            out = "DNSCMD: Zone \"" + zoneName + "\" not found.\r\n";
            return 2;
        }

        DnsResourceRecord rec;
        rec.name = fqdn;
        rec.type = type;
        rec.rclass = CLASS_IN;
        rec.ttl = 3600;
        rec.rdata = data;
        rec.dataText = data;
        rec.syncRdataBytes();

        zone->addRecord(rec);

        out = "DNS Server successfully added " + RecordTypeToString(type) + " record " + fqdn + ".\r\n"
              "Command completed successfully.\r\n";
        return 0;
    }

    int executeRecordDelete(std::span<const std::string_view> args, std::string& out) {
        if (args.size() < 3) {
            out = "DNSCMD: Usage: dnscmd /recorddelete <zone> <node> <type> [<data>] [/f]\r\n";
            return 87;
        }

        std::string zoneName(args[0]);
        std::string nodeName(args[1]);
        std::string typeStr(args[2]);
        uint16_t type = StringToRecordType(typeStr);

        std::string data;
        if (args.size() > 3 && !args[3].starts_with("/")) {
            data = args[3];
        }

        std::string fqdn = (nodeName == "@") ? zoneName : (nodeName + "." + zoneName);
        auto& engine = DnsServerEngine::Instance();
        auto zone = engine.getZone(zoneName);
        if (!zone) {
            out = "DNSCMD: Zone \"" + zoneName + "\" not found.\r\n";
            return 2;
        }

        if (!zone->deleteRecord(fqdn, type, data)) {
            out = "DNSCMD: Record " + fqdn + " (" + RecordTypeToString(type) + ") not found in zone.\r\n";
            return 2;
        }

        out = "DNS Server successfully deleted record " + fqdn + ".\r\n"
              "Command completed successfully.\r\n";
        return 0;
    }

    int executeEnumRecords(std::span<const std::string_view> args, std::string& out) {
        if (args.size() < 2) {
            out = "DNSCMD: Usage: dnscmd /enumrecords <zone> <node>\r\n";
            return 87;
        }

        std::string zoneName(args[0]);
        std::string nodeName(args[1]);
        std::string fqdn = (nodeName == "@") ? zoneName : (nodeName + "." + zoneName);

        auto& engine = DnsServerEngine::Instance();
        auto zone = engine.getZone(zoneName);
        if (!zone) {
            out = "DNSCMD: Zone \"" + zoneName + "\" not found.\r\n";
            return 2;
        }

        auto recs = zone->findRecords(fqdn, TYPE_ANY);
        std::ostringstream ss;
        ss << "Returned records for node " << nodeName << " (" << fqdn << "):\r\n";
        for (const auto& r : recs) {
            ss << std::left << std::setw(24) << nodeName
               << std::setw(8) << r.ttl
               << std::setw(8) << RecordTypeToString(r.type)
               << r.dataText << "\r\n";
        }
        ss << "Command completed successfully.\r\n";
        out = ss.str();
        return 0;
    }

    int executeClearCache(std::string& out) {
        auto& engine = DnsServerEngine::Instance();
        engine.getCache().clear();
        out = "DNS Server cache successfully cleared.\r\n"
              "Command completed successfully.\r\n";
        return 0;
    }

    int executeZoneRefresh(std::span<const std::string_view> args, std::string& out) {
        if (args.empty()) {
            out = "DNSCMD: Missing zone name for /zonerefresh.\r\n";
            return 87;
        }

        std::string zoneName(args[0]);
        auto& engine = DnsServerEngine::Instance();
        auto zone = engine.getZone(zoneName);
        if (!zone) {
            out = "DNSCMD: Zone \"" + zoneName + "\" not found.\r\n";
            return 2;
        }

        zone->soa.serial++;
        out = "DNS Server successfully refreshed zone " + zoneName + " (New Serial: " + std::to_string(zone->soa.serial) + ").\r\n"
              "Command completed successfully.\r\n";
        return 0;
    }

    int executeInfo(std::string& out) {
        auto& engine = DnsServerEngine::Instance();
        std::ostringstream ss;
        ss << "DNS Server Information:\r\n"
           << "  Version:        10.0.26100.1 (MicaNT Sovereign Enterprise Edition)\r\n"
           << "  Server Status:  Running\r\n"
           << "  Zones Hosted:   " << engine.getAllZones().size() << "\r\n"
           << "  Cache Entries:  " << engine.getCache().size() << "\r\n"
           << "  Total Queries:  " << engine.getTotalQueries() << "\r\n"
           << "  Cache Hits:     " << engine.getCacheHits() << "\r\n"
           << "  Cache Misses:   " << engine.getCacheMisses() << "\r\n"
           << "  Dynamic Updates:" << engine.getDynamicUpdates() << "\r\n"
           << "Command completed successfully.\r\n";
        out = ss.str();
        return 0;
    }
};

} // namespace micant::dns::dnscmd::um
