#include "json_rpc_server.h"
#include "json.h"


typedef void(*ResponseFn)(const String* response, void* ctx);

struct RpcServer {
    ResponseFn response_fn;

    String internal_error;

    void* ctx;
};


bool RcpServer_init(struct RpcServer* server, ResponseFn callback, void* ctx) {
    server->response_fn = callback;
    server->ctx = ctx;

    if (!String_create(&server->internal_error)) {
        return false;
    }
    if (!String_extend(&server->internal_error, "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32603,\"message\":\"Internal error\"},\"id\":null}")) {
        String_free(&server->internal_error);
        return false;
    }

    return true;
}


bool get_error_obj(struct RpcServer* server, int64_t code, const char* message,
                   JsonType* id, JsonObject* dest) {
    if (!JsonObject_create(dest)) {
        return false;
    }

    String s;
    if (!JsonObject_insert(dest, "id", *id) || !String_create(&s)) {
        goto fail;
    }
    if (!String_extend(&s, "2.0") || !JsonObject_insert_string(dest, "jsonrpc", s)) {
        String_free(&s);
        goto fail;
    }

    String msg;
    if (!String_create(&msg)) {
        goto fail;
    }

    JsonObject err_obj;
    if (!String_extend(&msg, message) || !JsonObject_create(&err_obj)) {
        String_free(&msg);
        goto fail;
    }

    if (!JsonObject_insert_string(&err_obj, "message", msg)) {
        JsonObject_free(&err_obj);
        String_free(&msg);
        goto fail;
    }
    if (!(JsonObject_insert_int(&err_obj, "code", code) && 
          JsonObject_insert_obj(dest, "error", err_obj))) {
        JsonObject_free(&err_obj);
        goto fail;
    }

    return true;
fail:
    JsonObject_free(dest);
    return false;
}

void send_response(struct RpcServer* server, JsonObject* response) {
    String s;
    if (String_create(&s)) {
        if (json_object_to_string(response, &s)) {
            server->response_fn(&s, server->ctx);
        } else {
            server->response_fn(&server->internal_error, server->ctx);
        }
        String_free(&s);
    } else {
        server->response_fn(&server->internal_error, server->ctx);
    }
    JsonObject_free(response);
}

void send_responses(struct RpcServer* server, JsonList* responses) {
    String s;
    if (String_create(&s)) {
        JsonType t;
        t.type = JSON_LIST;
        t.list = *responses;
        if (json_type_to_string(&t, &s)) {
            server->response_fn(&s, server->ctx);
        } else {
            server->response_fn(&server->internal_error, server->ctx);
        }
        String_free(&s);
    } else {
        server->response_fn(&server->internal_error, server->ctx);
    }
    JsonList_free(responses);

}

void send_error(struct RpcServer* server, int64_t code, const char* msg, JsonType* id) {
    JsonObject response;
    if (!get_error_obj(server, code, msg, id, &response)) {
        server->response_fn(&server->internal_error, server->ctx);
    } else {
        send_response(server, &response);
    }
}


bool RpcServer_call_method(struct RpcServer* server, String* method, JsonType* params, JsonObject* res) {

    return false;
}

bool handle_single_request(struct RpcServer* server, JsonType* req, JsonObject* res) {
    JsonType nullId;
    nullId.type = JSON_NULL;

    if (req->type != JSON_OBJECT) {
        return get_error_obj(server, -32600, "Invalid Request", &nullId, res);
    }
    JsonObject* obj = &req->object;

    JsonType* id = JsonObject_get(obj, "id");
    if (id == NULL || id->type == JSON_BOOL || id->type == JSON_OBJECT || id->type == JSON_LIST) {
        return get_error_obj(server, -32600, "Invalid Request", &nullId, res);
    }

    if (id->type == JSON_DOUBLE && ((double)(int64_t)id->dbl) != id->dbl) {
        return get_error_obj(server, -32600, "Invalid Request", &nullId, res);
    }

    String* s = JsonObject_get_string(obj, "method");
    if (s == NULL) {
        return get_error_obj(server, -32600, "Invalid Request", id, res);
    }

    JsonType* params = JsonObject_get(obj, "params");
    if (params != NULL && params->type != JSON_OBJECT && params->type != JSON_LIST) {
        return get_error_obj(server, -32600, "Invalid Request", id, res);
    }

    return RpcServer_call_method(server, s, params, res);
}

void RpcServer_handle_request(struct RpcServer* server, String* data) {
    JsonObject obj;
    JsonType nullId;
    nullId.type = JSON_NULL;


    JsonType root;
    // TODO: tell appart out of memory and parse error
    if (!json_parse_type(data->buffer, &root, NULL)) {
        send_error(server, -32700, "Parse error", &nullId);
        return;
    }

    if (root.type == JSON_LIST) {
        JsonList responses;
        if (JsonList_create(&responses)) {
            uint32_t i = 0;
            for (; i < root.list.size; ++i) {
                JsonObject response;
                if (handle_single_request(server, &root.list.data[i], &response)) {
                    if (!JsonList_append_obj(&responses, response)) {
                        JsonObject_free(&response);
                        break;
                    }
                } else {
                    break;
                }
            }
            if (i < root.list.size) {
                // This should only occur on out of memory
                JsonList_free(&responses);
                server->response_fn(&server->internal_error, server->ctx);
            } else {
                send_responses(server, &responses);
            }
        } else {
            server->response_fn(&server->internal_error, server->ctx);
        }
    } else {
        JsonObject response;
        if (!handle_single_request(server, &root, &response)) {
            server->response_fn(&server->internal_error, server->ctx);
        } else {
            send_response(server, &response);
        }
    }
    JsonType_free(&root);
}


#ifdef JSON_RPC_TESTS

#include "printf.h"

int main() {
    struct RpcServer server;
    RcpServer_init(&server, NULL, NULL);


    return 0;
}
#endif
