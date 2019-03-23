// Copyright (C) 2018 Internet Systems Consortium, Inc. ("ISC")
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <config.h>
#include <asiolink/asio_wrapper.h>
#include <agent/ca_process.h>
#include <agent/ca_controller.h>
#include <agent/ca_response_creator_factory.h>
#include <agent/ca_log.h>
#include <asiolink/io_address.h>
#include <asiolink/io_error.h>
#include <boost/pointer_cast.hpp>

using namespace isc::asiolink;
using namespace isc::process;


namespace isc {
namespace agent {

SlaacProcess::SlaacProcess(const char* name,
                                   const asiolink::IOServicePtr& io_service)
    : DProcessBase(name, io_service, DCfgMgrBasePtr(new SlaacCfgMgr())),
      http_listeners_() {
}

SlaacProcess::~SlaacProcess() {
}

void
SlaacProcess::init() {
}

void
SlaacProcess::run() {
    LOG_INFO(agent_logger, CTRL_AGENT_STARTED).arg(VERSION);

    try {
        // Register commands.
        SlaacControllerPtr controller =
            boost::dynamic_pointer_cast<SlaacController>(
                SlaacController::instance());
        controller->registerCommands();

        // Let's process incoming data or expiring timers in a loop until
        // shutdown condition is detected.
        while (!shouldShutdown()) {
            // Remove unused listeners within the main loop because new listeners
            // are created in within a callback method. This avoids removal the
            // listeners within a callback.
            garbageCollectListeners();
            runIO();
        }
        stopIOService();
    } catch (const std::exception& ex) {
        LOG_FATAL(agent_logger, CTRL_AGENT_FAILED).arg(ex.what());
        try {
            stopIOService();
        } catch (...) {
            // Ignore double errors
        }
        isc_throw(DProcessBaseError,
                  "Process run method failed: " << ex.what());
    }

    try {
        // Deregister commands.
        SlaacControllerPtr controller =
            boost::dynamic_pointer_cast<SlaacController>(
                SlaacController::instance());
        controller->deregisterCommands();
    } catch (const std::exception&) {
        // What to do? Simply ignore...
    }

    LOG_DEBUG(agent_logger, isc::log::DBGLVL_START_SHUT, SLAAC_RUN_EXIT);
}

size_t
SlaacProcess::runIO() {
    size_t cnt = getIoService()->get_io_service().poll();
    if (!cnt) {
        cnt = getIoService()->get_io_service().run_one();
    }
    return (cnt);
}

isc::data::ConstElementPtr
SlaacProcess::shutdown(isc::data::ConstElementPtr /*args*/) {
    setShutdownFlag(true);
    return (isc::config::createAnswer(0, "Slaac is shutting down"));
}

isc::data::ConstElementPtr
SlaacProcess::configure(isc::data::ConstElementPtr config_set,
                            bool check_only) {
    // System reconfiguration often poses an interesting issue whereby the
    // configuration parsing is successful, but an attempt to use a new
    // configuration is not. This will leave us in the inconsistent state
    // when the configuration is in fact only partially applied and the
    // system's ability to operate is impaired. The use of C++ lambda is
    // a way to resolve this problem by injecting the code to the
    // simpleParseConfig which performs an attempt to open new instance
    // of the listener (if required). The lambda code will throw an
    // exception if it fails and cause the simpleParseConfig to rollback
    // configuration changes and report an error.
    ConstElementPtr answer = getCfgMgr()->simpleParseConfig(config_set,
                                                            check_only,
                                                            [this]() {
        DCfgContextBasePtr base_ctx = getCfgMgr()->getContext();
        SlaacCfgContextPtr
            ctx = boost::dynamic_pointer_cast<SlaacCfgContext>(base_ctx);

        if (!ctx) {
            isc_throw(Unexpected, "Internal logic error: bad context type");
        }

        /// @todo: Start the actual thing here.


        // Ok, seems we're good to go.
        LOG_INFO(slaac_logger, SLAAC_SERVICE_STARTED).arg(interfaces);

    });

    int rcode = 0;
    config::parseAnswer(rcode, answer);
    return (answer);
}

SlaacCfgMgrPtr
SlaacProcess::getSlaacCfgMgr() {
    return (boost::dynamic_pointer_cast<SlaacCfgMgr>(getCfgMgr()));
}

} // namespace isc::agent
} // namespace isc