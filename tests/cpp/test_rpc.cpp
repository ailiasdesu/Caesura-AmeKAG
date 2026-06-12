// test_rpc.cpp - RPC module tests (RpcServer, EditorServer)
#include "doctest.h"
#include "rpc/RpcServer.h"
#include "rpc/EditorServer.h"
#include "rpc/api/IRpcServer.h"
#include "rpc/api/IEditorServer.h"

using namespace Caesura;

TEST_CASE("RpcServer::construct and stop without run") {
    RpcServer& rpc = RpcServer::instance();
    rpc.stop();
}

TEST_CASE("RpcServer::isRunning defaults false") {
    RpcServer& rpc = RpcServer::instance();
    CHECK(rpc.isRunning() == false);
}

TEST_CASE("IRpcServer interface completeness") {
    RpcServer& rpc = RpcServer::instance();
    IRpcServer* iface = &rpc;
    CHECK(iface != nullptr);
    CHECK(iface->isRunning() == false);
}

TEST_CASE("EditorServer::construct and stop without start") {
    EditorServer& es = EditorServer::instance();
    es.stop();
}

TEST_CASE("EditorServer::isRunning defaults false") {
    EditorServer& es = EditorServer::instance();
    CHECK(es.isRunning() == false);
}

TEST_CASE("EditorServer::setWebRoot no-crash") {
    EditorServer& es = EditorServer::instance();
    es.setWebRoot("web-editor/dist");
}

TEST_CASE("EditorServer::pushLog no-crash") {
    EditorServer& es = EditorServer::instance();
    es.pushLog("info", "test log message");
}

TEST_CASE("IEditorServer interface completeness") {
    EditorServer& es = EditorServer::instance();
    IEditorServer* iface = &es;
    CHECK(iface != nullptr);
    CHECK(iface->isRunning() == false);
}
