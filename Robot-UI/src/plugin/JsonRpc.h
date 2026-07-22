// ============================================================================
// JsonRpc.h — Thin wrapper around jsonrpcpp for Robot-UI's pipe transport
// ============================================================================
#pragma once

#include "jsonrpcpp.hpp"
#include <string>
#include <memory>

namespace JsonRpc
{

// ---- Build: construct JSON-RPC 2.0 messages for the pipe ----

/// Build a Request: {"jsonrpc":"2.0","method":"...","params":{...},"id":N}
inline std::string BuildRequest(int id, const std::string& method, const Json& params = nullptr)
{
    jsonrpcpp::Request req(jsonrpcpp::Id(id), method,
        params.is_null() ? jsonrpcpp::Parameter(std::nullptr_t{}) : jsonrpcpp::Parameter(params));
    return req.to_json().dump() + "\n";
}

/// Build a Notification (no id, no response expected)
inline std::string BuildNotification(const std::string& method, const Json& params = nullptr)
{
    jsonrpcpp::Notification n(method,
        params.is_null() ? jsonrpcpp::Parameter(std::nullptr_t{}) : jsonrpcpp::Parameter(params));
    return n.to_json().dump() + "\n";
}

/// Build a Response: {"jsonrpc":"2.0","result":{...},"id":N}
inline std::string BuildResponse(int id, const Json& result)
{
    jsonrpcpp::Response resp(jsonrpcpp::Id(id), result);
    return resp.to_json().dump() + "\n";
}

/// Build an Error response: {"jsonrpc":"2.0","error":{"code":N,"message":"..."},"id":N}
inline std::string BuildError(int id, int code, const std::string& message)
{
    jsonrpcpp::Response resp(jsonrpcpp::Id(id), jsonrpcpp::Error(message, code));
    return resp.to_json().dump() + "\n";
}

// ---- Parse: parse incoming JSON-RPC messages from the pipe ----

/// Result of parsing a single line from the pipe
struct ParsedMessage
{
    enum class Type { Unknown, Request, Response, Notification, Error };

    Type type = Type::Unknown;

    // Common: message id (request id or response id)
    int    id = 0;
    Json   result;
    // Error fields
    int    errorCode = 0;
    std::string errorMessage;

    // Request/Notification fields
    std::string method;
    Json        params;

    // Raw
    std::string raw;

    bool IsResponse()     const { return type == Type::Response || type == Type::Error; }
    bool IsNotification() const { return type == Type::Notification; }
};

/// Parse a single JSON-RPC line. Returns ParsedMessage with type info.
inline ParsedMessage Parse(const std::string& line)
{
    ParsedMessage pm;
    pm.raw = line;

    try
    {
        auto entity = jsonrpcpp::Parser::do_parse(line);
        if (!entity)
        {
            pm.type = ParsedMessage::Type::Unknown;
            return pm;
        }

        if (entity->is_response())
        {
            auto resp = std::dynamic_pointer_cast<jsonrpcpp::Response>(entity);
            // A Response may carry either "result" or "error"
            if (resp->error())
            {
                pm.type = ParsedMessage::Type::Error;
                pm.id = resp->id().int_id();
                pm.errorCode = resp->error().code();
                pm.errorMessage = resp->error().message();
            }
            else
            {
                pm.type = ParsedMessage::Type::Response;
                pm.id = resp->id().int_id();
                pm.result = resp->result();
            }
        }
        else if (entity->is_request())
        {
            auto req = std::dynamic_pointer_cast<jsonrpcpp::Request>(entity);
            pm.type = ParsedMessage::Type::Request;
            pm.id = req->id().int_id();
            pm.method = req->method();
            pm.params = req->params().to_json();
        }
        else if (entity->is_notification())
        {
            auto n = std::dynamic_pointer_cast<jsonrpcpp::Notification>(entity);
            pm.type = ParsedMessage::Type::Notification;
            pm.method = n->method();
            pm.params = n->params().to_json();
        }
        else if (entity->is_error())
        {
            auto err = std::dynamic_pointer_cast<jsonrpcpp::Error>(entity);
            pm.type = ParsedMessage::Type::Error;
            pm.errorCode = err->code();
            pm.errorMessage = err->message();
        }
    }
    catch (const std::exception&)
    {
        pm.type = ParsedMessage::Type::Unknown;
    }

    return pm;
}

} // namespace JsonRpc
