#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <deque>
#include <mutex>
#include <chrono>
#include <functional>
#include <sstream>
#include <atomic>
#include <algorithm>
#include <span>
#include <optional>
#include <cstdint>
#include <cstring>
#include <regex>

#include "micant/km/drivers/net/http.hpp"
#include "micant/um/subsystems/net/httpapi.hpp"
#include "micant/um/servers/scm.hpp"
#include "micant/um/subsystems/winbase/version.hpp"

namespace micant::iis::um {

// ============================================================================
// 1. Data Structures: AppPool, Sites, Pipeline Stages, Context
// ============================================================================

enum class AppPoolState {
    Starting,
    Started,
    Stopping,
    Stopped
};

enum class PipelineMode {
    Integrated,
    Classic
};

struct AppPoolRecycleConfig {
    uint32_t periodicRestartTimeMinutes{1740}; // Canonical default: 29 hours (1740 min)
    uint32_t periodicRestartRequests{0};       // 0 = disabled
    uint64_t periodicRestartPrivateMemoryBytes{0}; // 0 = disabled
    std::vector<std::string> scheduleTimes;
};

struct AppPoolRapidFailConfig {
    bool enabled{true};
    uint32_t maxCrashes{5};
    uint32_t crashIntervalMinutes{5};
};

struct AppPoolProcessModel {
    uint32_t maxWorkerProcesses{1};            // 1 = standard, >1 = Web Garden
    uint32_t idleTimeoutMinutes{20};
    std::string identityType{"ApplicationPoolIdentity"};
    bool pingingEnabled{true};
    uint32_t pingIntervalSeconds{30};
    uint32_t pingResponseTimeSeconds{90};
    uint32_t shutdownTimeLimitSeconds{90};
};

struct AppPoolDefinition {
    std::string name;
    AppPoolState state{AppPoolState::Started};
    PipelineMode pipelineMode{PipelineMode::Integrated};
    std::string managedRuntimeVersion{"v4.0"}; // "v4.0", "v2.0", or "" (No Managed Code)
    bool autoStart{true};
    AppPoolRecycleConfig recycleConfig;
    AppPoolRapidFailConfig rapidFailConfig;
    AppPoolProcessModel processModel;
    uint64_t kernelQueueId{0};
    httpapi::um::HANDLE kernelQueueHandle{nullptr};
    std::vector<uint32_t> workerPids;
    std::vector<uint64_t> crashTimestamps; // ms epoch
    uint64_t totalRequestsServed{0};
    uint64_t totalRecycles{0};
    bool disabledByRapidFail{false};
};

enum class SiteState {
    Starting,
    Started,
    Stopping,
    Stopped
};

struct SiteBinding {
    std::string protocol{"http"}; // "http" or "https"
    std::string bindingInformation{"*:80:"}; // format: "ip:port:host"
    uint16_t port{80};
    std::string host;
    std::string ip{"*"};
};

struct SiteDefinition {
    uint32_t id{1};
    std::string name{"Default Web Site"};
    SiteState state{SiteState::Started};
    std::string appPoolName{"DefaultAppPool"};
    std::string physicalPath{"C:\\inetpub\\wwwroot"};
    std::vector<SiteBinding> bindings;
    httpapi::um::HTTP_SERVER_SESSION_ID serverSessionId{0};
    httpapi::um::HTTP_URL_GROUP_ID urlGroupId{0};
    uint64_t totalRequests{0};
    uint64_t totalBytesSent{0};
};

enum class PipelineStage {
    BeginRequest = 0,
    AuthenticateRequest = 1,
    AuthorizeRequest = 2,
    ResolveRequestCache = 3,
    MapRequestHandler = 4,
    AcquireRequestState = 5,
    PreExecuteRequestHandler = 6,
    ExecuteRequestHandler = 7,
    PostExecuteRequestHandler = 8,
    UpdateRequestCache = 9,
    SendResponse = 10,
    EndRequest = 11
};

inline const char* PipelineStageToString(PipelineStage stage) noexcept {
    switch (stage) {
        case PipelineStage::BeginRequest: return "BeginRequest";
        case PipelineStage::AuthenticateRequest: return "AuthenticateRequest";
        case PipelineStage::AuthorizeRequest: return "AuthorizeRequest";
        case PipelineStage::ResolveRequestCache: return "ResolveRequestCache";
        case PipelineStage::MapRequestHandler: return "MapRequestHandler";
        case PipelineStage::AcquireRequestState: return "AcquireRequestState";
        case PipelineStage::PreExecuteRequestHandler: return "PreExecuteRequestHandler";
        case PipelineStage::ExecuteRequestHandler: return "ExecuteRequestHandler";
        case PipelineStage::PostExecuteRequestHandler: return "PostExecuteRequestHandler";
        case PipelineStage::UpdateRequestCache: return "UpdateRequestCache";
        case PipelineStage::SendResponse: return "SendResponse";
        case PipelineStage::EndRequest: return "EndRequest";
        default: return "UnknownStage";
    }
}

struct HttpContext {
    km::http::HTTP_REQUEST_V2 request;
    km::http::HTTP_RESPONSE_V2 response;
    std::string siteName;
    std::string appPoolName;
    std::string physicalPath;
    std::map<std::string, std::string> serverVariables;
    std::map<std::string, std::string> items;
    bool responseCompleted{false};
    bool skipRemainingStages{false};
    km::http::HTTP_CACHE_POLICY cachePolicy;
    std::string authenticatedUser{"IUSR"};
    bool isAuthenticated{false};
    PipelineStage currentStage{PipelineStage::BeginRequest};
    uint64_t requestStartTimeMs{0};
};

// ============================================================================
// 2. HTTP Modules: StaticFile, UrlRewrite 2.1, Compression, AspNetCore Reverse Proxy
// ============================================================================

class IHttpModule {
public:
    virtual ~IHttpModule() = default;
    [[nodiscard]] virtual std::string getName() const = 0;
    virtual bool processStage(PipelineStage stage, HttpContext& context) = 0;
};

// Freestanding CRC32 helper for GZIP RFC 1952 compliance
inline uint32_t CalculateCrc32(const uint8_t* data, size_t length) noexcept {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int k = 0; k < 8; ++k) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
        }
    }
    return ~crc;
}

// Minimal pure freestanding RFC 1952 Gzip wrapper with Deflate stored blocks
inline std::vector<uint8_t> CreateGzipStream(const std::vector<uint8_t>& input) {
    std::vector<uint8_t> gz;
    gz.reserve(input.size() + 32);

    // Gzip Header (10 bytes)
    gz.push_back(0x1F); // ID1
    gz.push_back(0x8B); // ID2
    gz.push_back(0x08); // CM = Deflate
    gz.push_back(0x00); // FLG = None
    gz.push_back(0x00); gz.push_back(0x00); gz.push_back(0x00); gz.push_back(0x00); // MTIME
    gz.push_back(0x00); // XFL
    gz.push_back(0x0B); // OS = NTFS / Windows

    // Deflate Stored Block(s)
    size_t offset = 0;
    while (offset < input.size() || input.empty()) {
        size_t chunk = std::min<size_t>(input.size() - offset, 65535);
        bool isFinal = (offset + chunk >= input.size());
        uint8_t header = isFinal ? 0x01 : 0x00; // BFINAL, BTYPE=00 (stored)
        gz.push_back(header);

        uint16_t len = static_cast<uint16_t>(chunk);
        uint16_t nlen = ~len;
        gz.push_back(static_cast<uint8_t>(len & 0xFF));
        gz.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
        gz.push_back(static_cast<uint8_t>(nlen & 0xFF));
        gz.push_back(static_cast<uint8_t>((nlen >> 8) & 0xFF));

        if (chunk > 0) {
            gz.insert(gz.end(), input.begin() + offset, input.begin() + offset + chunk);
            offset += chunk;
        } else {
            break;
        }
    }

    // CRC32 (4 bytes little-endian)
    uint32_t crc = CalculateCrc32(input.data(), input.size());
    gz.push_back(static_cast<uint8_t>(crc & 0xFF));
    gz.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));
    gz.push_back(static_cast<uint8_t>((crc >> 16) & 0xFF));
    gz.push_back(static_cast<uint8_t>((crc >> 24) & 0xFF));

    // ISIZE (4 bytes little-endian)
    uint32_t isize = static_cast<uint32_t>(input.size() & 0xFFFFFFFF);
    gz.push_back(static_cast<uint8_t>(isize & 0xFF));
    gz.push_back(static_cast<uint8_t>((isize >> 8) & 0xFF));
    gz.push_back(static_cast<uint8_t>((isize >> 16) & 0xFF));
    gz.push_back(static_cast<uint8_t>((isize >> 24) & 0xFF));

    return gz;
}

// Forward declare WAS for Virtual File System access
class WindowsProcessActivationService;

// 1. Static File Module (ETag, 304 Not Modified, Byte-Range RFC 7233)
class StaticFileModule : public IHttpModule {
public:
    [[nodiscard]] std::string getName() const override { return "StaticFileModule"; }

    bool processStage(PipelineStage stage, HttpContext& context) override;
};

// 2. URL Rewrite 2.1 Engine (Rules, Conditions, Rewrites & Redirects)
struct RewriteCondition {
    std::string inputVariable; // e.g. "{HTTPS}", "{HTTP_HOST}"
    std::string pattern;
    bool negate{false};
};

struct RewriteRule {
    std::string name;
    std::string matchUrlPattern; // regex pattern
    std::vector<RewriteCondition> conditions;
    enum ActionType { Rewrite, Redirect, CustomResponse } actionType{Rewrite};
    std::string targetUrl;
    uint16_t redirectStatusCode{301};
    bool stopProcessing{true};
};

class UrlRewriteModule : public IHttpModule {
public:
    UrlRewriteModule() {
        // Pre-seed canonical canonical rules
        RewriteRule httpsRedirect;
        httpsRedirect.name = "CanonicalHttpsRedirect";
        httpsRedirect.matchUrlPattern = "(.*)";
        httpsRedirect.conditions.push_back({"{HTTPS}", "OFF", false});
        httpsRedirect.actionType = RewriteRule::Redirect;
        httpsRedirect.redirectStatusCode = 301;
        httpsRedirect.targetUrl = "https://{HTTP_HOST}/{R:1}";
        httpsRedirect.stopProcessing = true;
        // Not enabled by default unless configured
    }

    [[nodiscard]] std::string getName() const override { return "UrlRewriteModule"; }

    void addRule(const RewriteRule& rule) {
        rules_.push_back(rule);
    }

    const std::vector<RewriteRule>& getRules() const noexcept {
        return rules_;
    }

    bool processStage(PipelineStage stage, HttpContext& context) override {
        if (stage != PipelineStage::BeginRequest) return true;
        if (rules_.empty() || context.responseCompleted) return true;

        std::string rawPath = context.request.rawUrl;
        auto qpos = rawPath.find('?');
        std::string pathOnly = (qpos != std::string::npos) ? rawPath.substr(0, qpos) : rawPath;

        for (const auto& rule : rules_) {
            try {
                std::regex re(rule.matchUrlPattern, std::regex::ECMAScript | std::regex::icase);
                std::smatch match;
                if (std::regex_search(pathOnly, match, re)) {
                    // Check conditions
                    bool allConditionsPassed = true;
                    for (const auto& cond : rule.conditions) {
                        std::string condVal;
                        if (cond.inputVariable == "{HTTPS}") {
                            condVal = context.request.cookedFullUrl.starts_with(L"https://") ? "ON" : "OFF";
                        } else if (cond.inputVariable == "{HTTP_HOST}") {
                            if (context.request.headers.contains("Host")) {
                                condVal = context.request.headers.at("Host");
                            }
                        } else {
                            condVal = "";
                        }

                        bool condMatch = (condVal == cond.pattern);
                        if (cond.negate) condMatch = !condMatch;
                        if (!condMatch) {
                            allConditionsPassed = false;
                            break;
                        }
                    }

                    if (allConditionsPassed) {
                        if (rule.actionType == RewriteRule::Redirect) {
                            context.response.StatusCode = rule.redirectStatusCode;
                            context.response.reason = (rule.redirectStatusCode == 301) ? "Moved Permanently" : "Found";
                            std::string loc = rule.targetUrl;
                            // Replace placeholders {R:1}
                            if (match.size() > 1) {
                                std::string r1 = match[1].str();
                                size_t r1Pos = loc.find("{R:1}");
                                if (r1Pos != std::string::npos) {
                                    loc.replace(r1Pos, 5, r1);
                                }
                            }
                            if (loc.find("{HTTP_HOST}") != std::string::npos && context.request.headers.contains("Host")) {
                                size_t hPos = loc.find("{HTTP_HOST}");
                                loc.replace(hPos, 11, context.request.headers.at("Host"));
                            }
                            context.response.headers["Location"] = loc;
                            context.response.headers["Server"] = "Microsoft-IIS/10.0";
                            context.response.entityBody.clear();
                            context.responseCompleted = true;
                            context.skipRemainingStages = true;
                            return true;
                        } else if (rule.actionType == RewriteRule::Rewrite) {
                            std::string newTarget = rule.targetUrl;
                            if (match.size() > 1) {
                                std::string r1 = match[1].str();
                                size_t r1Pos = newTarget.find("{R:1}");
                                if (r1Pos != std::string::npos) {
                                    newTarget.replace(r1Pos, 5, r1);
                                }
                            }
                            context.request.rawUrl = newTarget;
                            std::wstring wtarget(newTarget.begin(), newTarget.end());
                            context.request.cookedPath = wtarget;
                        }
                        if (rule.stopProcessing) break;
                    }
                }
            } catch (...) {
                // Regex syntax failure fallback
            }
        }
        return true;
    }

private:
    std::vector<RewriteRule> rules_;
};

// 3. HTTP Compression Module (Gzip / Deflate RFC 1952)
class HttpCompressionModule : public IHttpModule {
public:
    [[nodiscard]] std::string getName() const override { return "HttpCompressionModule"; }

    bool processStage(PipelineStage stage, HttpContext& context) override {
        if (stage != PipelineStage::SendResponse) return true;
        if (context.response.entityBody.size() < 64) return true; // Do not compress tiny payloads

        // Check client Accept-Encoding
        if (!context.request.headers.contains("Accept-Encoding")) return true;
        const std::string& acceptEnc = context.request.headers.at("Accept-Encoding");
        if (acceptEnc.find("gzip") == std::string::npos) return true;

        // Verify content type is compressible
        std::string contentType = context.response.headers.contains("Content-Type") 
                                ? context.response.headers.at("Content-Type") : "";
        if (contentType.find("text/") == std::string::npos &&
            contentType.find("json") == std::string::npos &&
            contentType.find("javascript") == std::string::npos &&
            contentType.find("xml") == std::string::npos) {
            return true;
        }

        // Compress via Gzip RFC 1952
        std::vector<uint8_t> compressed = CreateGzipStream(context.response.entityBody);
        context.response.entityBody = std::move(compressed);
        context.response.headers["Content-Encoding"] = "gzip";
        context.response.headers["Vary"] = "Accept-Encoding";
        context.response.headers["Content-Length"] = std::to_string(context.response.entityBody.size());
        return true;
    }
};

// 4. ASP.NET Core Reverse Proxy Gateway Module (AspNetCoreModuleV2)
class AspNetCoreReverseProxyModule : public IHttpModule {
public:
    [[nodiscard]] std::string getName() const override { return "AspNetCoreModuleV2"; }

    void setBackendTarget(const std::string& pathPrefix, const std::string& upstreamHostPort) {
        backendPrefix_ = pathPrefix;
        upstream_ = upstreamHostPort;
    }

    bool processStage(PipelineStage stage, HttpContext& context) override {
        if (stage != PipelineStage::ExecuteRequestHandler) return true;
        if (backendPrefix_.empty() || !context.request.rawUrl.starts_with(backendPrefix_)) {
            return true;
        }

        // Forwarding to ASP.NET Core Kestrel backend
        context.serverVariables["HTTP_X_FORWARDED_FOR"] = "127.0.0.1";
        context.serverVariables["HTTP_X_FORWARDED_PROTO"] = context.request.cookedFullUrl.starts_with(L"https") ? "https" : "http";
        if (context.request.headers.contains("Host")) {
            context.serverVariables["HTTP_X_FORWARDED_HOST"] = context.request.headers.at("Host");
        }

        // Simulate high-speed reverse proxy pass to Kestrel upstream
        context.response.StatusCode = 200;
        context.response.reason = "OK";
        context.response.headers["Server"] = "Kestrel";
        context.response.headers["X-Powered-By"] = "ASP.NET Core 8.0 (ANCM-InProcess)";
        context.response.headers["Content-Type"] = "application/json; charset=utf-8";

        std::string jsonBody = "{\"status\":\"ok\",\"upstream\":\"" + upstream_ + "\","
                               "\"service\":\"AspNetCoreBackendService\",\"path\":\"" + context.request.rawUrl + "\"}";
        context.response.entityBody.assign(jsonBody.begin(), jsonBody.end());
        context.response.headers["Content-Length"] = std::to_string(context.response.entityBody.size());
        context.responseCompleted = true;
        return true;
    }

private:
    std::string backendPrefix_{"/api/"};
    std::string upstream_{"127.0.0.1:5000"};
};

// 5. Custom Error Module (Canonical IIS 10.x HTML Pages)
class CustomErrorModule : public IHttpModule {
public:
    [[nodiscard]] std::string getName() const override { return "CustomErrorModule"; }

    bool processStage(PipelineStage stage, HttpContext& context) override {
        if (stage != PipelineStage::EndRequest && stage != PipelineStage::SendResponse) return true;
        if (context.response.StatusCode < 400 || !context.response.entityBody.empty()) return true;

        std::ostringstream ss;
        ss << "<!DOCTYPE html>\r\n"
           << "<html><head><title>IIS 10.0 Detailed Error - " << context.response.StatusCode << ".0</title>\r\n"
           << "<style>body{font-family:'Segoe UI',sans-serif;background:#FAFAFA;color:#333;margin:40px;}\r\n"
           << "h1{color:#B71C1C;font-size:24px;}h2{font-size:16px;color:#555;}hr{border:0;border-top:1px solid #CCC;}\r\n"
           << ".box{background:#FFF;border:1px solid #E0E0E0;padding:20px;border-radius:4px;}</style></head>\r\n"
           << "<body><div class=\"box\"><h1>HTTP Error " << context.response.StatusCode << ".0 - " << context.response.reason << "</h1>\r\n"
           << "<h2>The requested resource cannot be found or handled.</h2><hr>\r\n"
           << "<p><strong>Module:</strong> IIS Web Core (" << PipelineStageToString(context.currentStage) << ")<br>\r\n"
           << "<strong>Notification:</strong> " << PipelineStageToString(context.currentStage) << "<br>\r\n"
           << "<strong>Site:</strong> " << context.siteName << "<br>\r\n"
           << "<strong>AppPool:</strong> " << context.appPoolName << "<br>\r\n"
           << "<strong>Server:</strong> Microsoft-IIS/10.0</p></div></body></html>\r\n";

        std::string errHtml = ss.str();
        context.response.entityBody.assign(errHtml.begin(), errHtml.end());
        context.response.headers["Content-Type"] = "text/html; charset=utf-8";
        context.response.headers["Content-Length"] = std::to_string(context.response.entityBody.size());
        return true;
    }
};

// ============================================================================
// 3. 11-Stage Native Module Pipeline Engine
// ============================================================================

class NativeModulePipeline {
public:
    NativeModulePipeline() {
        // Register default IIS module chain
        urlRewriteModule_ = std::make_shared<UrlRewriteModule>();
        staticFileModule_ = std::make_shared<StaticFileModule>();
        compressionModule_ = std::make_shared<HttpCompressionModule>();
        aspNetCoreModule_ = std::make_shared<AspNetCoreReverseProxyModule>();
        customErrorModule_ = std::make_shared<CustomErrorModule>();

        modules_.push_back(urlRewriteModule_);
        modules_.push_back(staticFileModule_);
        modules_.push_back(aspNetCoreModule_);
        modules_.push_back(compressionModule_);
        modules_.push_back(customErrorModule_);
    }

    void addModule(std::shared_ptr<IHttpModule> module) {
        modules_.push_back(module);
    }

    std::shared_ptr<UrlRewriteModule> getUrlRewriteModule() const { return urlRewriteModule_; }
    std::shared_ptr<AspNetCoreReverseProxyModule> getAspNetCoreModule() const { return aspNetCoreModule_; }

    void execute(HttpContext& context) {
        static constexpr PipelineStage stages[] = {
            PipelineStage::BeginRequest,
            PipelineStage::AuthenticateRequest,
            PipelineStage::AuthorizeRequest,
            PipelineStage::ResolveRequestCache,
            PipelineStage::MapRequestHandler,
            PipelineStage::AcquireRequestState,
            PipelineStage::PreExecuteRequestHandler,
            PipelineStage::ExecuteRequestHandler,
            PipelineStage::PostExecuteRequestHandler,
            PipelineStage::UpdateRequestCache,
            PipelineStage::SendResponse,
            PipelineStage::EndRequest
        };

        for (PipelineStage stage : stages) {
            context.currentStage = stage;
            for (auto& module : modules_) {
                module->processStage(stage, context);
                if (context.skipRemainingStages) break;
            }
            if (context.skipRemainingStages) break;
        }

        // Ensure Server header
        if (!context.response.headers.contains("Server")) {
            context.response.headers["Server"] = "Microsoft-IIS/10.0";
        }
    }

private:
    std::vector<std::shared_ptr<IHttpModule>> modules_;
    std::shared_ptr<UrlRewriteModule> urlRewriteModule_;
    std::shared_ptr<StaticFileModule> staticFileModule_;
    std::shared_ptr<HttpCompressionModule> compressionModule_;
    std::shared_ptr<AspNetCoreReverseProxyModule> aspNetCoreModule_;
    std::shared_ptr<CustomErrorModule> customErrorModule_;
};

// ============================================================================
// 4. Worker Process Model (w3wp.exe)
// ============================================================================

class WorkerProcess {
public:
    uint32_t pid{0};
    std::string appPoolName;
    httpapi::um::HANDLE queueHandle{nullptr};
    std::atomic<bool> isRunning{false};
    std::atomic<bool> isDraining{false};
    std::atomic<uint64_t> requestsProcessed{0};
    std::atomic<uint64_t> activeRequests{0};
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point lastActivityTime;
    std::shared_ptr<NativeModulePipeline> pipeline;

    WorkerProcess(uint32_t p, std::string pool, httpapi::um::HANDLE qh)
        : pid(p), appPoolName(std::move(pool)), queueHandle(qh),
          startTime(std::chrono::system_clock::now()),
          lastActivityTime(std::chrono::system_clock::now()),
          pipeline(std::make_shared<NativeModulePipeline>()) {}

    bool start() {
        isRunning = true;
        isDraining = false;
        return true;
    }

    void stop() {
        isRunning = false;
    }

    void initiateDrain() {
        isDraining = true;
    }

    // Process a single request pulled from the bound HTTP.sys request queue
    bool processOneRequest(const std::string& siteName, const std::string& physPath) {
        if (!isRunning || !queueHandle) return false;

        km::http::HTTP_REQUEST_V2 req;
        httpapi::um::ULONG bytesReceived = 0;
        httpapi::um::ULONG status = httpapi::um::HttpReceiveHttpRequest(
            queueHandle, 0, 0, &req, sizeof(req), &bytesReceived, nullptr);

        if (status != httpapi::um::NO_ERROR) {
            return false;
        }

        activeRequests++;
        lastActivityTime = std::chrono::system_clock::now();

        HttpContext ctx;
        ctx.request = std::move(req);
        ctx.siteName = siteName;
        ctx.appPoolName = appPoolName;
        ctx.physicalPath = physPath;
        ctx.requestStartTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(lastActivityTime.time_since_epoch()).count();

        // Run 11-stage pipeline
        pipeline->execute(ctx);

        // Send response back via HttpSendHttpResponse
        httpapi::um::ULONG bytesSent = 0;
        httpapi::um::HttpSendHttpResponse(
            queueHandle, ctx.request.RequestId, 0, &ctx.response,
            (ctx.cachePolicy.Policy != km::http::HttpCachePolicyNocache) ? &ctx.cachePolicy : nullptr,
            &bytesSent, nullptr, 0, nullptr, nullptr);

        requestsProcessed++;
        activeRequests--;
        return true;
    }
};

// ============================================================================
// 5. Windows Process Activation Service (WAS / was.dll / w3svc.dll)
// ============================================================================

class WindowsProcessActivationService {
public:
    static WindowsProcessActivationService& Instance() noexcept {
        static WindowsProcessActivationService s_instance;
        return s_instance;
    }

    bool initialize() noexcept {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        if (initialized_) return true;

        // 1. Initialize HTTP Server API v2
        httpapi::um::HTTPAPI_VERSION ver = httpapi::um::HTTPAPI_VERSION_2;
        httpapi::um::HttpInitialize(ver, httpapi::um::HTTP_INITIALIZE_SERVER, nullptr);

        // 2. Pre-seed Default AppPool
        AppPoolDefinition defPool;
        defPool.name = "DefaultAppPool";
        defPool.state = AppPoolState::Started;
        defPool.pipelineMode = PipelineMode::Integrated;
        defPool.managedRuntimeVersion = "v4.0";
        defPool.autoStart = true;

        // Create dedicated kernel request queue for DefaultAppPool
        httpapi::um::HANDLE hDefQueue = nullptr;
        httpapi::um::HttpCreateRequestQueue(ver, L"DefaultAppPoolQueue", nullptr, 0, &hDefQueue);
        defPool.kernelQueueHandle = hDefQueue;
        defPool.kernelQueueId = httpapi::um::HttpHandleRegistry::Instance().lookupQueue(hDefQueue);

        appPools_[defPool.name] = defPool;

        // 3. Pre-seed Default Web Site (id 1, *:80:, C:\inetpub\wwwroot)
        SiteDefinition defSite;
        defSite.id = 1;
        defSite.name = "Default Web Site";
        defSite.state = SiteState::Started;
        defSite.appPoolName = "DefaultAppPool";
        defSite.physicalPath = "C:\\inetpub\\wwwroot";
        defSite.bindings.push_back({"http", "*:80:", 80, "", "*"});

        // Create Server Session & URL Group for Default Web Site
        httpapi::um::HTTP_SERVER_SESSION_ID sessId = 0;
        httpapi::um::HttpCreateServerSession(ver, &sessId, 0);
        defSite.serverSessionId = sessId;

        httpapi::um::HTTP_URL_GROUP_ID urlGroupId = 0;
        httpapi::um::HttpCreateUrlGroup(sessId, &urlGroupId, 0);
        defSite.urlGroupId = urlGroupId;

        // Bind URL Group to DefaultAppPool request queue
        httpapi::um::HTTP_BINDING_INFO bindInfo{};
        bindInfo.Flags = 0x1;
        bindInfo.RequestQueueHandle = hDefQueue;
        httpapi::um::HttpSetUrlGroupProperty(urlGroupId, httpapi::um::HttpServerBindingProperty, &bindInfo, sizeof(bindInfo));

        // Add default URL prefix
        httpapi::um::HttpAddUrlToUrlGroup(urlGroupId, L"http://+:80/", 0, 0);

        sites_[defSite.name] = defSite;

        // 4. Seed Virtual File System with canonical IIS welcome assets
        siteFiles_["Default Web Site"]["/iisstart.htm"] = 
            "<!DOCTYPE html>\n"
            "<html>\n"
            "<head><title>IIS Windows Server</title></head>\n"
            "<body>\n"
            "  <div style=\"text-align:center;margin-top:100px;font-family:'Segoe UI',sans-serif;\">\n"
            "    <h1 style=\"color:#0078D7;font-size:48px;\">Internet Information Services</h1>\n"
            "    <p style=\"font-size:20px;color:#555;\">Version 10.0 on MicaNT Sovereign Enterprise Kernel</p>\n"
            "  </div>\n"
            "</body>\n"
            "</html>\n";

        siteFiles_["Default Web Site"]["/web.config"] = 
            "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
            "<configuration>\n"
            "  <system.webServer>\n"
            "    <defaultDocument>\n"
            "      <files>\n"
            "        <add value=\"iisstart.htm\" />\n"
            "      </files>\n"
            "    </defaultDocument>\n"
            "  </system.webServer>\n"
            "</configuration>\n";

        siteFiles_["Default Web Site"]["/robots.txt"] = "User-agent: *\nDisallow: /admin\n";

        // 5. Spawn initial worker process for DefaultAppPool
        spawnWorkerProcessLocked("DefaultAppPool");

        initialized_ = true;
        return true;
    }

    // --- AppPool Operations ---

    bool createAppPool(const std::string& name, PipelineMode mode = PipelineMode::Integrated, const std::string& clr = "v4.0") noexcept {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        if (appPools_.find(name) != appPools_.end()) return false;

        AppPoolDefinition pool;
        pool.name = name;
        pool.state = AppPoolState::Started;
        pool.pipelineMode = mode;
        pool.managedRuntimeVersion = clr;

        std::wstring wname(name.begin(), name.end());
        wname += L"Queue";

        httpapi::um::HANDLE hQueue = nullptr;
        httpapi::um::HttpCreateRequestQueue(httpapi::um::HTTPAPI_VERSION_2, wname.c_str(), nullptr, 0, &hQueue);
        pool.kernelQueueHandle = hQueue;
        pool.kernelQueueId = httpapi::um::HttpHandleRegistry::Instance().lookupQueue(hQueue);

        appPools_[name] = pool;
        spawnWorkerProcessLocked(name);
        return true;
    }

    bool deleteAppPool(const std::string& name) noexcept {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        auto it = appPools_.find(name);
        if (it == appPools_.end()) return false;

        // Check if any sites are using this app pool
        for (const auto& [siteName, site] : sites_) {
            if (site.appPoolName == name) return false;
        }

        // Retire all workers
        for (auto& wp : workerProcesses_) {
            if (wp->appPoolName == name) {
                wp->stop();
            }
        }

        if (it->second.kernelQueueHandle) {
            httpapi::um::HttpCloseRequestQueue(it->second.kernelQueueHandle);
        }

        appPools_.erase(it);
        return true;
    }

    bool startAppPool(const std::string& name) noexcept {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        auto it = appPools_.find(name);
        if (it == appPools_.end()) return false;
        it->second.state = AppPoolState::Started;
        it->second.disabledByRapidFail = false;
        spawnWorkerProcessLocked(name);
        return true;
    }

    bool stopAppPool(const std::string& name) noexcept {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        auto it = appPools_.find(name);
        if (it == appPools_.end()) return false;
        it->second.state = AppPoolState::Stopped;

        for (auto& wp : workerProcesses_) {
            if (wp->appPoolName == name) {
                wp->stop();
            }
        }
        return true;
    }

    bool recycleAppPool(const std::string& name) noexcept {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        auto it = appPools_.find(name);
        if (it == appPools_.end()) return false;

        it->second.totalRecycles++;

        // Drain existing workers and spawn fresh replacement
        for (auto& wp : workerProcesses_) {
            if (wp->appPoolName == name && wp->isRunning) {
                wp->initiateDrain();
                wp->stop();
            }
        }

        spawnWorkerProcessLocked(name);
        return true;
    }

    void reportWorkerCrash(const std::string& appPoolName) noexcept {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        auto it = appPools_.find(appPoolName);
        if (it == appPools_.end()) return;

        uint64_t now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
        it->second.crashTimestamps.push_back(now);

        // Rapid-Fail Protection verification
        if (it->second.rapidFailConfig.enabled) {
            uint64_t windowMs = static_cast<uint64_t>(it->second.rapidFailConfig.crashIntervalMinutes) * 60 * 1000;
            size_t crashesInWindow = 0;
            for (auto ts : it->second.crashTimestamps) {
                if (now >= ts && (now - ts) <= windowMs) {
                    crashesInWindow++;
                }
            }
            if (crashesInWindow >= it->second.rapidFailConfig.maxCrashes) {
                it->second.state = AppPoolState::Stopped;
                it->second.disabledByRapidFail = true;
                for (auto& wp : workerProcesses_) {
                    if (wp->appPoolName == appPoolName) {
                        wp->stop();
                    }
                }
            }
        }
    }

    std::optional<AppPoolDefinition> getAppPool(const std::string& name) const noexcept {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        auto it = appPools_.find(name);
        if (it == appPools_.end()) return std::nullopt;
        return it->second;
    }

    std::vector<AppPoolDefinition> getAllAppPools() const noexcept {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        std::vector<AppPoolDefinition> res;
        res.reserve(appPools_.size());
        for (const auto& [k, v] : appPools_) res.push_back(v);
        return res;
    }

    // --- Site Operations ---

    bool createSite(const std::string& name, uint32_t id, const std::string& bindings,
                    const std::string& physicalPath, const std::string& appPool = "DefaultAppPool") noexcept {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        if (sites_.find(name) != sites_.end()) return false;
        for (const auto& [k, s] : sites_) {
            if (s.id == id) return false;
        }

        SiteDefinition site;
        site.id = id;
        site.name = name;
        site.state = SiteState::Started;
        site.appPoolName = appPool;
        site.physicalPath = physicalPath;

        // Parse bindings: e.g. "http/*:8080:" or "*:8080:"
        SiteBinding b;
        b.bindingInformation = bindings;
        if (bindings.find("https") != std::string::npos) {
            b.protocol = "https";
            b.port = 443;
        } else {
            b.protocol = "http";
            b.port = 80;
        }
        site.bindings.push_back(b);

        // Win32 HTTP Server API Group
        httpapi::um::HTTP_SERVER_SESSION_ID sessId = 0;
        httpapi::um::HttpCreateServerSession(httpapi::um::HTTPAPI_VERSION_2, &sessId, 0);
        site.serverSessionId = sessId;

        httpapi::um::HTTP_URL_GROUP_ID urlGroupId = 0;
        httpapi::um::HttpCreateUrlGroup(sessId, &urlGroupId, 0);
        site.urlGroupId = urlGroupId;

        // Bind URL Group to target AppPool's Request Queue
        auto apIt = appPools_.find(appPool);
        if (apIt != appPools_.end() && apIt->second.kernelQueueHandle) {
            httpapi::um::HTTP_BINDING_INFO bindInfo{};
            bindInfo.Flags = 0x1;
            bindInfo.RequestQueueHandle = apIt->second.kernelQueueHandle;
            httpapi::um::HttpSetUrlGroupProperty(urlGroupId, httpapi::um::HttpServerBindingProperty, &bindInfo, sizeof(bindInfo));
        }

        // Add URL Prefix to kernel group
        std::wstring wprefix = (b.protocol == "https") ? L"https://+:" : L"http://+:";
        wprefix += std::to_wstring(b.port) + L"/";
        httpapi::um::HttpAddUrlToUrlGroup(urlGroupId, wprefix.c_str(), 0, 0);

        sites_[name] = site;
        return true;
    }

    bool deleteSite(const std::string& name) noexcept {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        auto it = sites_.find(name);
        if (it == sites_.end()) return false;

        if (it->second.urlGroupId) {
            httpapi::um::HttpCloseUrlGroup(it->second.urlGroupId);
        }
        if (it->second.serverSessionId) {
            httpapi::um::HttpCloseServerSession(it->second.serverSessionId);
        }

        siteFiles_.erase(name);
        sites_.erase(it);
        return true;
    }

    bool startSite(const std::string& name) noexcept {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        auto it = sites_.find(name);
        if (it == sites_.end()) return false;
        it->second.state = SiteState::Started;
        return true;
    }

    bool stopSite(const std::string& name) noexcept {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        auto it = sites_.find(name);
        if (it == sites_.end()) return false;
        it->second.state = SiteState::Stopped;
        return true;
    }

    std::optional<SiteDefinition> getSite(const std::string& name) const noexcept {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        auto it = sites_.find(name);
        if (it == sites_.end()) return std::nullopt;
        return it->second;
    }

    std::vector<SiteDefinition> getAllSites() const noexcept {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        std::vector<SiteDefinition> res;
        res.reserve(sites_.size());
        for (const auto& [k, v] : sites_) res.push_back(v);
        return res;
    }

    // --- Worker Process Accessors ---

    std::vector<std::shared_ptr<WorkerProcess>> getWorkers(const std::string& appPoolName = "") const noexcept {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        std::vector<std::shared_ptr<WorkerProcess>> res;
        for (const auto& wp : workerProcesses_) {
            if (appPoolName.empty() || wp->appPoolName == appPoolName) {
                res.push_back(wp);
            }
        }
        return res;
    }

    // --- Virtual File System for Sites ---

    void setSiteFile(const std::string& siteName, const std::string& relPath, const std::string& content) noexcept {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        siteFiles_[siteName][relPath] = content;
    }

    bool getSiteFile(const std::string& siteName, const std::string& relPath, std::string& outContent) const noexcept {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        auto sit = siteFiles_.find(siteName);
        if (sit == siteFiles_.end()) return false;
        auto fit = sit->second.find(relPath);
        if (fit == sit->second.end()) return false;
        outContent = fit->second;
        return true;
    }

    // --- Full-Journey Dispatch & Process Helper ---

    bool dispatchAndProcess(const km::http::HTTP_REQUEST_V2& req, std::string& outDirectResponse) noexcept {
        auto& drv = km::http::HttpKernelProtocolDriver::Instance();
        bool enq = drv.dispatchInboundRequest(req, outDirectResponse);
        if (!enq) {
            // Kernel cache hit or 503 error was returned directly in outDirectResponse
            return true;
        }

        // Process with active worker process for matched queue
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        for (auto& wp : workerProcesses_) {
            if (wp->isRunning) {
                // Find site matching this worker
                std::string sname = "Default Web Site";
                std::string ppath = "C:\\inetpub\\wwwroot";
                for (const auto& [sn, sd] : sites_) {
                    if (sd.appPoolName == wp->appPoolName) {
                        sname = sd.name;
                        ppath = sd.physicalPath;
                        break;
                    }
                }
                if (wp->processOneRequest(sname, ppath)) {
                    totalRequestsServed_++;
                    return true;
                }
            }
        }
        return false;
    }

    uint64_t getTotalRequestsServed() const noexcept {
        return totalRequestsServed_.load();
    }

private:
    WindowsProcessActivationService() = default;

    void spawnWorkerProcessLocked(const std::string& appPoolName) {
        static std::atomic<uint32_t> s_nextPid{1048};
        auto it = appPools_.find(appPoolName);
        if (it == appPools_.end() || !it->second.kernelQueueHandle) return;

        uint32_t pid = s_nextPid.fetch_add(4);
        auto wp = std::make_shared<WorkerProcess>(pid, appPoolName, it->second.kernelQueueHandle);
        wp->start();
        it->second.workerPids.push_back(pid);
        workerProcesses_.push_back(wp);
    }

    mutable std::recursive_mutex mutex_;
    bool initialized_{false};
    std::map<std::string, AppPoolDefinition> appPools_;
    std::map<std::string, SiteDefinition> sites_;
    std::vector<std::shared_ptr<WorkerProcess>> workerProcesses_;
    std::map<std::string, std::map<std::string, std::string>> siteFiles_;
    std::atomic<uint64_t> totalRequestsServed_{0};
};

// ============================================================================
// StaticFileModule Implementation (Needs WAS definition)
// ============================================================================

inline bool StaticFileModule::processStage(PipelineStage stage, HttpContext& context) {
    if (stage != PipelineStage::ExecuteRequestHandler) return true;
    if (context.responseCompleted) return true;

    std::string reqPath = context.request.rawUrl;
    auto qpos = reqPath.find('?');
    if (qpos != std::string::npos) reqPath = reqPath.substr(0, qpos);

    // Default document resolution
    if (reqPath == "/" || reqPath.empty()) {
        reqPath = "/iisstart.htm";
    }

    std::string content;
    bool found = WindowsProcessActivationService::Instance().getSiteFile(context.siteName, reqPath, content);
    if (!found) {
        // Return 404
        context.response.StatusCode = 404;
        context.response.reason = "Not Found";
        context.responseCompleted = false; // allow CustomErrorModule to render HTML
        return true;
    }

    // MIME type mapping
    std::string contentType = "text/plain";
    if (reqPath.ends_with(".htm") || reqPath.ends_with(".html")) contentType = "text/html; charset=utf-8";
    else if (reqPath.ends_with(".css")) contentType = "text/css";
    else if (reqPath.ends_with(".js")) contentType = "application/javascript";
    else if (reqPath.ends_with(".json")) contentType = "application/json";
    else if (reqPath.ends_with(".png")) contentType = "image/png";
    else if (reqPath.ends_with(".jpg")) contentType = "image/jpeg";
    else if (reqPath.ends_with(".svg")) contentType = "image/svg+xml";
    else if (reqPath.ends_with(".xml")) contentType = "application/xml";

    // ETag calculation
    uint32_t hash = CalculateCrc32(reinterpret_cast<const uint8_t*>(content.data()), content.size());
    std::ostringstream etagSs;
    etagSs << "W/\"" << std::hex << hash << "-" << content.size() << "\"";
    std::string etag = etagSs.str();

    // Check If-None-Match header (RFC 7232 304 Not Modified)
    if (context.request.headers.contains("If-None-Match")) {
        if (context.request.headers.at("If-None-Match") == etag) {
            context.response.StatusCode = 304;
            context.response.reason = "Not Modified";
            context.response.headers["ETag"] = etag;
            context.response.headers["Server"] = "Microsoft-IIS/10.0";
            context.response.entityBody.clear();
            context.responseCompleted = true;
            return true;
        }
    }

    // Check Byte-Range header (RFC 7233 206 Partial Content)
    if (context.request.headers.contains("Range")) {
        const std::string& rangeHeader = context.request.headers.at("Range");
        if (rangeHeader.starts_with("bytes=")) {
            std::string spec = rangeHeader.substr(6);
            auto dashPos = spec.find('-');
            if (dashPos != std::string::npos) {
                size_t startByte = std::stoull(spec.substr(0, dashPos));
                size_t endByte = content.size() - 1;
                if (dashPos + 1 < spec.size()) {
                    endByte = std::min<size_t>(std::stoull(spec.substr(dashPos + 1)), content.size() - 1);
                }

                if (startByte <= endByte && startByte < content.size()) {
                    size_t rangeLen = endByte - startByte + 1;
                    context.response.StatusCode = 206;
                    context.response.reason = "Partial Content";
                    context.response.headers["Content-Type"] = contentType;
                    context.response.headers["Content-Range"] = "bytes " + std::to_string(startByte) + "-" +
                                                               std::to_string(endByte) + "/" + std::to_string(content.size());
                    context.response.headers["ETag"] = etag;
                    context.response.headers["Accept-Ranges"] = "bytes";
                    context.response.entityBody.assign(content.begin() + startByte, content.begin() + startByte + rangeLen);
                    context.responseCompleted = true;
                    return true;
                }
            }
        }
    }

    // Standard 200 OK Response
    context.response.StatusCode = 200;
    context.response.reason = "OK";
    context.response.headers["Content-Type"] = contentType;
    context.response.headers["ETag"] = etag;
    context.response.headers["Accept-Ranges"] = "bytes";
    context.response.entityBody.assign(content.begin(), content.end());
    context.response.headers["Content-Length"] = std::to_string(content.size());
    context.responseCompleted = true;

    // Enable kernel response caching (TTL: 300s)
    context.cachePolicy.Policy = km::http::HttpCachePolicyTimeToLive;
    context.cachePolicy.SecondsToLive = 300;
    return true;
}

// ============================================================================
// 6. Subsystem Registration: SCM Services & VersionDatabase
// ============================================================================

inline void RegisterIisSubsystem() {
    static bool registered = false;
    if (registered) return;
    registered = true;

    auto& was = WindowsProcessActivationService::Instance();
    was.initialize();

    auto& scm = scm::ServiceControlManager::get();

    // SCM Service 1: WAS ("Windows Process Activation Service")
    auto wasRecord = std::make_shared<scm::ServiceRecord>();
    wasRecord->serviceName = L"WAS";
    wasRecord->displayName = L"Windows Process Activation Service";
    wasRecord->serviceType = scm::SERVICE_WIN32_OWN_PROCESS;
    wasRecord->startType = scm::SERVICE_AUTO_START;
    wasRecord->errorControl = scm::SERVICE_ERROR_NORMAL;
    wasRecord->svchostGroup = "WASGroup";
    wasRecord->binaryPath = L"\\SystemRoot\\System32\\svchost.exe -k iissvcs";
    wasRecord->status.dwServiceType = wasRecord->serviceType;
    wasRecord->status.dwCurrentState = scm::SERVICE_RUNNING;
    wasRecord->status.dwControlsAccepted = scm::SERVICE_ACCEPT_STOP | scm::SERVICE_ACCEPT_PAUSE_CONTINUE;
    scm.registerServiceRecord(wasRecord);

    // SCM Service 2: W3SVC ("World Wide Web Publishing Service")
    auto w3svcRecord = std::make_shared<scm::ServiceRecord>();
    w3svcRecord->serviceName = L"W3SVC";
    w3svcRecord->displayName = L"World Wide Web Publishing Service";
    w3svcRecord->serviceType = scm::SERVICE_WIN32_SHARE_PROCESS;
    w3svcRecord->startType = scm::SERVICE_AUTO_START;
    w3svcRecord->errorControl = scm::SERVICE_ERROR_NORMAL;
    w3svcRecord->svchostGroup = "iissvcs";
    w3svcRecord->binaryPath = L"\\SystemRoot\\System32\\svchost.exe -k iissvcs";
    w3svcRecord->status.dwServiceType = w3svcRecord->serviceType;
    w3svcRecord->status.dwCurrentState = scm::SERVICE_RUNNING;
    w3svcRecord->status.dwControlsAccepted = scm::SERVICE_ACCEPT_STOP;
    scm.registerServiceRecord(w3svcRecord);

    // SCM Service 3: IISADMIN ("IIS Admin Service")
    auto iisAdminRecord = std::make_shared<scm::ServiceRecord>();
    iisAdminRecord->serviceName = L"IISADMIN";
    iisAdminRecord->displayName = L"IIS Admin Service";
    iisAdminRecord->serviceType = scm::SERVICE_WIN32_SHARE_PROCESS;
    iisAdminRecord->startType = scm::SERVICE_DEMAND_START;
    iisAdminRecord->errorControl = scm::SERVICE_ERROR_NORMAL;
    iisAdminRecord->svchostGroup = "iissvcs";
    iisAdminRecord->binaryPath = L"\\SystemRoot\\System32\\svchost.exe -k iissvcs";
    iisAdminRecord->status.dwServiceType = iisAdminRecord->serviceType;
    iisAdminRecord->status.dwCurrentState = scm::SERVICE_RUNNING;
    scm.registerServiceRecord(iisAdminRecord);

    // VersionDatabase Registration
    auto& vdb = version::VersionDatabase::Instance();
    vdb.RegisterModule("w3wp.exe", "10.0.26100.5900", "IIS Worker Process");
    vdb.RegisterModule("w3core.dll", "10.0.26100.5900", "IIS Core Web Engine");
    vdb.RegisterModule("was.dll", "10.0.26100.5900", "Windows Process Activation Service");
    vdb.RegisterModule("apphostsvc.dll", "10.0.26100.5900", "Application Host Helper Service");
    vdb.RegisterModule("appcmd.exe", "10.0.26100.5900", "IIS Command-Line Administration Tool");
    vdb.RegisterModule("rewrite.dll", "7.2.1993.0", "IIS URL Rewrite Module 2.1");
    vdb.RegisterModule("aspnetcorev2.dll", "18.0.24312.0", "ASP.NET Core Module v2 (ANCM)");
}

} // namespace micant::iis::um
