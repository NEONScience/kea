// Copyright (C) 2012-2019 Internet Systems Consortium, Inc. ("ISC")
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <config.h>
#include <dhcp/option4_client_fqdn.h>
#include <dhcp/option6_client_fqdn.h>
#include <dhcpsrv/d2_client_mgr.h>
#include <testutils/test_to_element.h>
#include <exceptions/exceptions.h>
#include <util/strutil.h>

#include <boost/algorithm/string.hpp>
#include <gtest/gtest.h>

using namespace std;
using namespace isc::asiolink;
using namespace isc::dhcp;
using namespace isc::util;
using namespace isc::test;
using namespace isc::data;
using namespace isc;

namespace {

/// @brief Tests conversion of NameChangeFormat between enum and strings.
TEST(ReplaceClientNameModeTest, formatEnumConversion){
    ASSERT_EQ(D2ClientConfig::stringToReplaceClientNameMode("never"),
              D2ClientConfig::RCM_NEVER);
    ASSERT_EQ(D2ClientConfig::stringToReplaceClientNameMode("always"),
              D2ClientConfig::RCM_ALWAYS);
    ASSERT_EQ(D2ClientConfig::stringToReplaceClientNameMode("when-present"),
              D2ClientConfig::RCM_WHEN_PRESENT);
    ASSERT_EQ(D2ClientConfig::stringToReplaceClientNameMode("when-not-present"),
              D2ClientConfig::RCM_WHEN_NOT_PRESENT);
    ASSERT_THROW(D2ClientConfig::stringToReplaceClientNameMode("BOGUS"),
                 isc::BadValue);

    ASSERT_EQ(D2ClientConfig::
              replaceClientNameModeToString(D2ClientConfig::RCM_NEVER),
              "never");
    ASSERT_EQ(D2ClientConfig::
              replaceClientNameModeToString(D2ClientConfig::RCM_ALWAYS),
              "always");
    ASSERT_EQ(D2ClientConfig::
              replaceClientNameModeToString(D2ClientConfig::RCM_WHEN_PRESENT),
              "when-present");
    ASSERT_EQ(D2ClientConfig::
              replaceClientNameModeToString(D2ClientConfig::
                                            RCM_WHEN_NOT_PRESENT),
              "when-not-present");
}

/// @brief Checks constructors and accessors of D2ClientConfig.
TEST(D2ClientConfigTest, constructorsAndAccessors) {
    D2ClientConfigPtr d2_client_config;

    // Verify default constructor creates a disabled instance.
    ASSERT_NO_THROW(d2_client_config.reset(new D2ClientConfig()));
    EXPECT_FALSE(d2_client_config->getEnableUpdates());

    // Verify the enable-updates can be toggled.
    d2_client_config->enableUpdates(true);
    EXPECT_TRUE(d2_client_config->getEnableUpdates());
    d2_client_config->enableUpdates(false);
    EXPECT_FALSE(d2_client_config->getEnableUpdates());

    d2_client_config.reset();

    bool enable_updates = true;
    isc::asiolink::IOAddress server_ip("127.0.0.1");
    size_t server_port = 477;
    isc::asiolink::IOAddress sender_ip("127.0.0.1");
    size_t sender_port = 478;
    size_t max_queue_size = 2048;
    dhcp_ddns::NameChangeProtocol ncr_protocol = dhcp_ddns::NCR_UDP;
    dhcp_ddns::NameChangeFormat ncr_format = dhcp_ddns::FMT_JSON;
    std::string generated_prefix = "the_prefix";
    std::string qualifying_suffix = "the.suffix.";
    std::string hostname_char_set = "[^A-Z]";
    std::string hostname_char_replacement = "*";

    // Verify that we can construct a valid, enabled instance.
    ASSERT_NO_THROW(d2_client_config.reset(new
                                           D2ClientConfig(enable_updates,
                                                          server_ip,
                                                          server_port,
                                                          sender_ip,
                                                          sender_port,
                                                          max_queue_size,
                                                          ncr_protocol,
                                                          ncr_format)));
    ASSERT_TRUE(d2_client_config);

    // Add user context
    std::string user_context = "{ \"comment\": \"bar\", \"foo\": 1 }";
    EXPECT_FALSE(d2_client_config->getContext());
    d2_client_config->setContext(Element::fromJSON(user_context));

    // Verify that the accessors return the expected values.
    EXPECT_EQ(d2_client_config->getEnableUpdates(), enable_updates);

    EXPECT_EQ(d2_client_config->getServerIp(), server_ip);
    EXPECT_EQ(d2_client_config->getServerPort(), server_port);
    EXPECT_EQ(d2_client_config->getSenderIp(), sender_ip);
    EXPECT_EQ(d2_client_config->getSenderPort(), sender_port);
    EXPECT_EQ(d2_client_config->getMaxQueueSize(), max_queue_size);
    EXPECT_EQ(d2_client_config->getNcrProtocol(), ncr_protocol);
    EXPECT_EQ(d2_client_config->getNcrFormat(), ncr_format);
    ASSERT_TRUE(d2_client_config->getContext());
    EXPECT_EQ(d2_client_config->getContext()->str(), user_context);

    // Verify that toText called by << operator doesn't bomb.
    ASSERT_NO_THROW(std::cout << "toText test:" << std::endl <<
                    *d2_client_config << std::endl);

    // Verify what toElement returns.
    std::string expected = "{\n"
        "\"comment\": \"bar\",\n"
        "\"enable-updates\": true,\n"
        "\"server-ip\": \"127.0.0.1\",\n"
        "\"server-port\": 477,\n"
        "\"sender-ip\": \"127.0.0.1\",\n"
        "\"sender-port\": 478,\n"
        "\"max-queue-size\": 2048,\n"
        "\"ncr-protocol\": \"UDP\",\n"
        "\"ncr-format\": \"JSON\",\n"
        "\"user-context\": { \"foo\": 1 }\n"
        "}\n";
    runToElementTest<D2ClientConfig>(expected, *d2_client_config);

    // Verify that constructor does not allow use of NCR_TCP.
    /// @todo obviously this becomes invalid once TCP is supported.
    ASSERT_THROW(d2_client_config.reset(new
                                        D2ClientConfig(enable_updates,
                                                       server_ip,
                                                       server_port,
                                                       sender_ip,
                                                       sender_port,
                                                       max_queue_size,
                                                       dhcp_ddns::NCR_TCP,
                                                       ncr_format)),
                 D2ClientError);

    Optional<std::string> opt_hostname_char_set("", true);
    Optional<std::string> opt_hostname_char_replacement("", true);

    // Veeify that constructor handles optional hostname char stuff.
    ASSERT_NO_THROW(d2_client_config.reset(new
                                           D2ClientConfig(enable_updates,
                                                          server_ip,
                                                          server_port,
                                                          sender_ip,
                                                          sender_port,
                                                          max_queue_size,
                                                          ncr_protocol,
                                                          ncr_format)));
    ASSERT_TRUE(d2_client_config);

    // Verify what toElement returns.
    expected = "{\n"
        "\"enable-updates\": true,\n"
        "\"server-ip\": \"127.0.0.1\",\n"
        "\"server-port\": 477,\n"
        "\"sender-ip\": \"127.0.0.1\",\n"
        "\"sender-port\": 478,\n"
        "\"max-queue-size\": 2048,\n"
        "\"ncr-protocol\": \"UDP\",\n"
        "\"ncr-format\": \"JSON\"\n"
        "}\n";
    runToElementTest<D2ClientConfig>(expected, *d2_client_config);

    /// @todo if additional validation is added to ctor, this test needs to
    /// expand accordingly.
}

/// @brief Tests the equality and inequality operators of D2ClientConfig.
TEST(D2ClientConfigTest, equalityOperator) {
    D2ClientConfigPtr ref_config;
    D2ClientConfigPtr test_config;

    isc::asiolink::IOAddress ref_address("127.0.0.1");
    isc::asiolink::IOAddress test_address("127.0.0.2");

    // Create an instance to use as a reference.
    ASSERT_NO_THROW(ref_config.reset(new D2ClientConfig(true,
                    ref_address, 477, ref_address, 478, 1024,
                    dhcp_ddns::NCR_UDP, dhcp_ddns::FMT_JSON)));
    ASSERT_TRUE(ref_config);

    // Check a configuration that is identical to reference configuration.
    ASSERT_NO_THROW(test_config.reset(new D2ClientConfig(true,
                    ref_address, 477, ref_address, 478, 1024,
                    dhcp_ddns::NCR_UDP, dhcp_ddns::FMT_JSON)));
    ASSERT_TRUE(test_config);
    EXPECT_TRUE(*ref_config == *test_config);
    EXPECT_FALSE(*ref_config != *test_config);

    // Check a configuration that differs only by enable flag.
    ASSERT_NO_THROW(test_config.reset(new D2ClientConfig(false,
                    ref_address, 477, ref_address, 478, 1024,
                    dhcp_ddns::NCR_UDP, dhcp_ddns::FMT_JSON)));
    ASSERT_TRUE(test_config);
    EXPECT_FALSE(*ref_config == *test_config);
    EXPECT_TRUE(*ref_config != *test_config);

    // Check a configuration that differs only by server ip.
    ASSERT_NO_THROW(test_config.reset(new D2ClientConfig(true,
                    test_address, 477, ref_address, 478, 1024,
                    dhcp_ddns::NCR_UDP, dhcp_ddns::FMT_JSON)));
    ASSERT_TRUE(test_config);
    EXPECT_FALSE(*ref_config == *test_config);
    EXPECT_TRUE(*ref_config != *test_config);

    // Check a configuration that differs only by server port.
    ASSERT_NO_THROW(test_config.reset(new D2ClientConfig(true,
                    ref_address, 333, ref_address, 478, 1024,
                    dhcp_ddns::NCR_UDP, dhcp_ddns::FMT_JSON)));
    ASSERT_TRUE(test_config);
    EXPECT_FALSE(*ref_config == *test_config);
    EXPECT_TRUE(*ref_config != *test_config);

    // Check a configuration that differs only by sender ip.
    ASSERT_NO_THROW(test_config.reset(new D2ClientConfig(true,
                    ref_address, 477, test_address, 478, 1024,
                    dhcp_ddns::NCR_UDP, dhcp_ddns::FMT_JSON)));
    ASSERT_TRUE(test_config);
    EXPECT_FALSE(*ref_config == *test_config);
    EXPECT_TRUE(*ref_config != *test_config);

    // Check a configuration that differs only by sender port.
    ASSERT_NO_THROW(test_config.reset(new D2ClientConfig(true,
                    ref_address, 477, ref_address, 333, 1024,
                    dhcp_ddns::NCR_UDP, dhcp_ddns::FMT_JSON)));
    ASSERT_TRUE(test_config);
    EXPECT_FALSE(*ref_config == *test_config);
    EXPECT_TRUE(*ref_config != *test_config);

    // Check a configuration that differs only by max queue size.
    ASSERT_NO_THROW(test_config.reset(new D2ClientConfig(true,
                    ref_address, 477, ref_address, 478, 2048,
                    dhcp_ddns::NCR_UDP, dhcp_ddns::FMT_JSON)));
    ASSERT_TRUE(test_config);
    EXPECT_FALSE(*ref_config == *test_config);
    EXPECT_TRUE(*ref_config != *test_config);
}

/// @brief Checks the D2ClientMgr constructor.
TEST(D2ClientMgr, constructor) {
    D2ClientMgrPtr d2_client_mgr;

    // Verify we can construct with the default constructor.
    ASSERT_NO_THROW(d2_client_mgr.reset(new D2ClientMgr()));

    // After construction, D2 configuration should be disabled.
    // Fetch it and verify this is the case.
    D2ClientConfigPtr original_config = d2_client_mgr->getD2ClientConfig();
    ASSERT_TRUE(original_config);
    EXPECT_FALSE(original_config->getEnableUpdates());

    // Make sure convenience method agrees.
    EXPECT_FALSE(d2_client_mgr->ddnsEnabled());
}

/// @brief Checks passing the D2ClientMgr a valid D2 client configuration.
/// @todo Once NameChangeSender is integrated, this test needs to expand, and
/// additional scenario tests will need to be written.
TEST(D2ClientMgr, validConfig) {
    D2ClientMgrPtr d2_client_mgr;

    // Construct the manager and fetch its initial configuration.
    ASSERT_NO_THROW(d2_client_mgr.reset(new D2ClientMgr()));
    D2ClientConfigPtr original_config = d2_client_mgr->getD2ClientConfig();
    ASSERT_TRUE(original_config);

    // Verify that we cannot set the config to an empty pointer.
    D2ClientConfigPtr new_cfg;
    ASSERT_THROW(d2_client_mgr->setD2ClientConfig(new_cfg), D2ClientError);

    // Create a new, enabled config.
    ASSERT_NO_THROW(new_cfg.reset(new D2ClientConfig(true,
                                  isc::asiolink::IOAddress("127.0.0.1"), 477,
                                  isc::asiolink::IOAddress("127.0.0.1"), 478,
                                  1024,
                                  dhcp_ddns::NCR_UDP, dhcp_ddns::FMT_JSON)));

    // Verify that we can assign a new, non-empty configuration.
    ASSERT_NO_THROW(d2_client_mgr->setD2ClientConfig(new_cfg));

    // Verify that we can fetch the newly assigned configuration.
    D2ClientConfigPtr updated_config = d2_client_mgr->getD2ClientConfig();
    ASSERT_TRUE(updated_config);
    EXPECT_TRUE(updated_config->getEnableUpdates());

    // Make sure convenience method agrees with the updated configuration.
    EXPECT_TRUE(d2_client_mgr->ddnsEnabled());

    // Make sure the configuration we fetched is the one  we assigned,
    // and not the original configuration.
    EXPECT_EQ(*new_cfg, *updated_config);
    EXPECT_NE(*original_config, *updated_config);
}

/// @brief Checks passing the D2ClientMgr a valid D2 client configuration
/// using IPv6 service.
TEST(D2ClientMgr, ipv6Config) {
    D2ClientMgrPtr d2_client_mgr;

    // Construct the manager and fetch its initial configuration.
    ASSERT_NO_THROW(d2_client_mgr.reset(new D2ClientMgr()));
    D2ClientConfigPtr original_config = d2_client_mgr->getD2ClientConfig();
    ASSERT_TRUE(original_config);

    // Create a new, enabled config.
    D2ClientConfigPtr new_cfg;
    ASSERT_NO_THROW(new_cfg.reset(new D2ClientConfig(true,
                                  isc::asiolink::IOAddress("::1"), 477,
                                  isc::asiolink::IOAddress("::1"), 478,
                                  1024,
                                  dhcp_ddns::NCR_UDP, dhcp_ddns::FMT_JSON)));

    // Verify that we can assign a new, non-empty configuration.
    ASSERT_NO_THROW(d2_client_mgr->setD2ClientConfig(new_cfg));

    // Verify that we can fetch the newly assigned configuration.
    D2ClientConfigPtr updated_config = d2_client_mgr->getD2ClientConfig();
    ASSERT_TRUE(updated_config);
    EXPECT_TRUE(updated_config->getEnableUpdates());

    // Make sure convenience method agrees with the updated configuration.
    EXPECT_TRUE(d2_client_mgr->ddnsEnabled());

    // Make sure the configuration we fetched is the one we assigned,
    // and not the original configuration.
    EXPECT_EQ(*new_cfg, *updated_config);
    EXPECT_NE(*original_config, *updated_config);
}

/// @brief Tests that analyzeFqdn detects invalid combination of both the
/// client S and N flags set to true.
TEST(D2ClientMgr, analyzeFqdnInvalidCombination) {
    D2ClientMgr mgr;
    bool server_s = false;
    bool server_n = false;

    DdnsParams ddns_params;

    // client S=1 N=1 is invalid.  analyzeFqdn should throw.
    ASSERT_THROW(mgr.analyzeFqdn(true, true, server_s, server_n, ddns_params),
                 isc::BadValue);
}

/// @brief Tests that analyzeFqdn generates correct server S and N flags when
/// updates are enabled and all overrides are off.
TEST(D2ClientMgr, analyzeFqdnEnabledNoOverrides) {
    D2ClientMgr mgr;
    bool server_s = false;
    bool server_n = false;

    // Create enabled configuration with all controls off (no overrides).
    DdnsParams ddns_params;
    ddns_params.enable_updates_ = true;
    ddns_params.override_no_update_ = false;
    ddns_params.override_client_update_ = false;
    ddns_params.replace_client_name_mode_ = D2ClientConfig::RCM_NEVER;
    ddns_params.generated_prefix_ = "";
    ddns_params.qualifying_suffix_ = "";

    // client S=0 N=0 means client wants to do forward update.
    // server S should be 0 (server is not doing forward updates)
    // and server N should be 0 (server doing reverse updates)
    mgr.analyzeFqdn(false, false, server_s, server_n, ddns_params);
    EXPECT_FALSE(server_s);
    EXPECT_FALSE(server_n);

    // client S=1 N=0 means client wants server to do forward update.
    // server S should be 1 (server is doing forward updates)
    // and server N should be 0 (server doing updates)
    mgr.analyzeFqdn(true, false, server_s, server_n, ddns_params);
    EXPECT_TRUE(server_s);
    EXPECT_FALSE(server_n);


    // client S=0 N=1 means client wants no one to do forward updates.
    // server S should be 0 (server is  not forward updates)
    // and server N should be 1 (server is not doing any updates)
    mgr.analyzeFqdn(false, true, server_s, server_n, ddns_params);
    EXPECT_FALSE(server_s);
    EXPECT_TRUE(server_n);
}

/// @brief Tests that analyzeFqdn generates correct server S and N flags when
/// updates are enabled and override-no-update is on.
TEST(D2ClientMgr, analyzeFqdnEnabledOverrideNoUpdate) {
    D2ClientMgr mgr;
    bool server_s = false;
    bool server_n = false;

    // Create enabled configuration with override-no-update true.
    DdnsParams ddns_params;
    ddns_params.enable_updates_ = true;
    ddns_params.override_no_update_ = true;
    ddns_params.override_client_update_ = false;
    ddns_params.replace_client_name_mode_ = D2ClientConfig::RCM_NEVER;
    ddns_params.generated_prefix_ = "";
    ddns_params.qualifying_suffix_ = "";

    // client S=0 N=0 means client wants to do forward update.
    // server S should be 0 (server is not doing forward updates)
    // and server N should be 0 (server is doing reverse updates)
    mgr.analyzeFqdn(false, false, server_s, server_n, ddns_params);
    EXPECT_FALSE(server_s);
    EXPECT_FALSE(server_n);

    // client S=1 N=0 means client wants server to do forward update.
    // server S should be 1 (server is doing forward updates)
    // and server N should be 0 (server doing updates)
    mgr.analyzeFqdn(true, false, server_s, server_n, ddns_params);
    EXPECT_TRUE(server_s);
    EXPECT_FALSE(server_n);

    // client S=0 N=1 means client wants no one to do forward updates.
    // server S should be 1 (server is doing forward updates)
    // and server N should be 0 (server is doing updates)
    mgr.analyzeFqdn(false, true, server_s, server_n, ddns_params);
    EXPECT_TRUE(server_s);
    EXPECT_FALSE(server_n);
}

/// @brief Tests that analyzeFqdn generates correct server S and N flags when
/// updates are enabled and override-client-update is on.
TEST(D2ClientMgr, analyzeFqdnEnabledOverrideClientUpdate) {
    D2ClientMgr mgr;
    bool server_s = false;
    bool server_n = false;

    // Create enabled configuration with override-client-update true.
    DdnsParams ddns_params;
    ddns_params.enable_updates_ = true;
    ddns_params.override_no_update_ = false;
    ddns_params.override_client_update_ = true;
    ddns_params.replace_client_name_mode_ = D2ClientConfig::RCM_NEVER;
    ddns_params.generated_prefix_ = "";
    ddns_params.qualifying_suffix_ = "";


    // client S=0 N=0 means client wants to do forward update.
    // server S should be 1 (server is doing forward updates)
    // and server N should be 0 (server doing updates)
    mgr.analyzeFqdn(false, false, server_s, server_n, ddns_params);
    EXPECT_TRUE(server_s);
    EXPECT_FALSE(server_n);

    // client S=1 N=0 means client wants server to do forward update.
    // server S should be 1 (server is doing forward updates)
    // and server N should be 0 (server doing updates)
    mgr.analyzeFqdn(true, false, server_s, server_n, ddns_params);
    EXPECT_TRUE(server_s);
    EXPECT_FALSE(server_n);

    // client S=0 N=1 means client wants no one to do forward updates.
    // server S should be 0 (server is  not forward updates)
    // and server N should be 1 (server is not doing any updates)
    mgr.analyzeFqdn(false, true, server_s, server_n, ddns_params);
    EXPECT_FALSE(server_s);
    EXPECT_TRUE(server_n);
}

/// @brief Verifies the adustFqdnFlags template with Option4ClientFqdn objects.
/// Ensures that the method can set the N, S, and O flags properly.
/// Other permutations are covered by analyzeFqdnFlag tests.
TEST(D2ClientMgr, adjustFqdnFlagsV4) {
    D2ClientMgr mgr;
    Option4ClientFqdnPtr request;
    Option4ClientFqdnPtr response;

    // Create enabled configuration with override-no-update true.
    DdnsParams ddns_params;
    ddns_params.enable_updates_ = true;
    ddns_params.override_no_update_ = true;
    ddns_params.override_client_update_ = false;
    ddns_params.replace_client_name_mode_ = D2ClientConfig::RCM_NEVER;
    ddns_params.generated_prefix_ = "";
    ddns_params.qualifying_suffix_ = "";

    // client S=0 N=0 means client wants to do forward update.
    // server S should be 0 (server is not doing forward updates)
    // and server N should be 0 (server is doing reverse updates)
    // and server O should be 0
    request.reset(new Option4ClientFqdn(0, Option4ClientFqdn::RCODE_CLIENT(),
                                        "", Option4ClientFqdn::PARTIAL));
    response.reset(new Option4ClientFqdn(*request));
    response->resetFlags();

    mgr.adjustFqdnFlags<Option4ClientFqdn>(*request, *response, ddns_params);
    EXPECT_FALSE(response->getFlag(Option4ClientFqdn::FLAG_S));
    EXPECT_FALSE(response->getFlag(Option4ClientFqdn::FLAG_N));
    EXPECT_FALSE(response->getFlag(Option4ClientFqdn::FLAG_O));

    // client S=1 N=0 means client wants server to do forward update.
    // server S should be 1 (server is doing forward updates)
    // and server N should be 0 (server doing updates)
    // and server O should be 0
    request.reset(new Option4ClientFqdn(Option4ClientFqdn::FLAG_S,
                                        Option4ClientFqdn::RCODE_CLIENT(),
                                        "", Option4ClientFqdn::PARTIAL));
    response.reset(new Option4ClientFqdn(*request));
    response->resetFlags();

    mgr.adjustFqdnFlags<Option4ClientFqdn>(*request, *response, ddns_params);
    EXPECT_TRUE(response->getFlag(Option4ClientFqdn::FLAG_S));
    EXPECT_FALSE(response->getFlag(Option4ClientFqdn::FLAG_N));
    EXPECT_FALSE(response->getFlag(Option4ClientFqdn::FLAG_O));

    // client S=0 N=1 means client wants no one to do updates
    // server S should be 1 (server is doing forward updates)
    // and server N should be 0 (server doing updates)
    // and O should be 1 (overriding client S)
    request.reset(new Option4ClientFqdn(Option4ClientFqdn::FLAG_N,
                                        Option4ClientFqdn::RCODE_CLIENT(),
                                        "", Option4ClientFqdn::PARTIAL));
    response.reset(new Option4ClientFqdn(*request));
    response->resetFlags();

    mgr.adjustFqdnFlags<Option4ClientFqdn>(*request, *response, ddns_params);
    EXPECT_TRUE(response->getFlag(Option4ClientFqdn::FLAG_S));
    EXPECT_FALSE(response->getFlag(Option4ClientFqdn::FLAG_N));
    EXPECT_TRUE(response->getFlag(Option4ClientFqdn::FLAG_O));
}

/// @brief Verified the getUpdateDirections template method with
/// Option4ClientFqdn objects.
TEST(D2ClientMgr, updateDirectionsV4) {
    D2ClientMgr mgr;
    Option4ClientFqdnPtr response;

    bool do_forward = false;
    bool do_reverse = false;

    // Response S=0, N=0 should mean do reverse only.
    response.reset(new Option4ClientFqdn(0,
                                         Option4ClientFqdn::RCODE_CLIENT(),
                                         "", Option4ClientFqdn::PARTIAL));
    mgr.getUpdateDirections(*response, do_forward, do_reverse);
    EXPECT_FALSE(do_forward);
    EXPECT_TRUE(do_reverse);

    // Response S=0, N=1 should mean don't do either.
    response.reset(new Option4ClientFqdn(Option4ClientFqdn::FLAG_N,
                                         Option4ClientFqdn::RCODE_CLIENT(),
                                         "", Option4ClientFqdn::PARTIAL));
    mgr.getUpdateDirections(*response, do_forward, do_reverse);
    EXPECT_FALSE(do_forward);
    EXPECT_FALSE(do_reverse);

    // Response S=1, N=0 should mean do both.
    response.reset(new Option4ClientFqdn(Option4ClientFqdn::FLAG_S,
                                         Option4ClientFqdn::RCODE_CLIENT(),
                                         "", Option4ClientFqdn::PARTIAL));
    mgr.getUpdateDirections(*response, do_forward, do_reverse);
    EXPECT_TRUE(do_forward);
    EXPECT_TRUE(do_reverse);

    // Response S=1, N=1 isn't possible.
}

/// @brief Tests the qualifyName method's ability to construct FQDNs
TEST(D2ClientMgr, qualifyName) {
    D2ClientMgr mgr;
    bool do_not_dot = false;
    bool do_dot = true;

    // Create enabled configuration
    DdnsParams ddns_params;
    ddns_params.enable_updates_ = true;
    ddns_params.override_no_update_ = false;
    ddns_params.override_client_update_ = false;
    ddns_params.replace_client_name_mode_ = D2ClientConfig::RCM_NEVER;
    ddns_params.generated_prefix_ = "prefix";
    ddns_params.qualifying_suffix_ = "suffix.com";

    // Verify that the qualifying suffix gets appended with a trailing dot added.
    std::string partial_name = "somehost";
    std::string qualified_name = mgr.qualifyName(partial_name, ddns_params, do_dot);
    EXPECT_EQ("somehost.suffix.com.", qualified_name);

    // Verify that the qualifying suffix gets appended without a trailing dot.
    partial_name = "somehost";
    qualified_name = mgr.qualifyName(partial_name, ddns_params, do_not_dot);
    EXPECT_EQ("somehost.suffix.com", qualified_name);

    // Verify that an empty suffix and false flag, does not change the name
    ddns_params.qualifying_suffix_ = "";
    partial_name = "somehost";
    qualified_name = mgr.qualifyName(partial_name, ddns_params, do_not_dot);
    EXPECT_EQ("somehost", qualified_name);

    // Verify that a qualifying suffix that already has a trailing
    // dot gets appended without doubling the dot.
    ddns_params.qualifying_suffix_ = "hasdot.com.";
    qualified_name = mgr.qualifyName(partial_name, ddns_params, do_dot);
    EXPECT_EQ("somehost.hasdot.com.", qualified_name);

    // Verify that the qualifying suffix gets appended without an
    // extraneous dot when partial_name ends with a "."
    qualified_name = mgr.qualifyName("somehost.", ddns_params, do_dot);
    EXPECT_EQ("somehost.hasdot.com.", qualified_name);

    // Verify that a name with a trailing dot does not get an extraneous
    // dot when the suffix is blank
    ddns_params.qualifying_suffix_ = "";
    qualified_name = mgr.qualifyName("somehost.", ddns_params, do_dot);
    EXPECT_EQ("somehost.", qualified_name);

    // Verify that a name with no trailing dot gets just a dot when the
    // suffix is blank
    qualified_name = mgr.qualifyName("somehost", ddns_params, do_dot);
    EXPECT_EQ("somehost.", qualified_name);

    // Verify that a name with no trailing dot does not get dotted when the
    // suffix is blank and trailing dot is false
    qualified_name = mgr.qualifyName("somehost", ddns_params, do_not_dot);
    EXPECT_EQ("somehost", qualified_name);

    // Verify that a name with trailing dot gets "undotted" when the
    // suffix is blank and trailing dot is false
    qualified_name = mgr.qualifyName("somehost.", ddns_params, do_not_dot);
    EXPECT_EQ("somehost", qualified_name);

}


/// @brief Tests the generateFdqn method's ability to construct FQDNs
TEST(D2ClientMgr, generateFqdn) {
    D2ClientMgr mgr;
    bool do_dot = true;

    // Create enabled configuration
    DdnsParams ddns_params;
    ddns_params.enable_updates_ = true;
    ddns_params.override_no_update_ = false;
    ddns_params.override_client_update_ = false;
    ddns_params.replace_client_name_mode_ = D2ClientConfig::RCM_NEVER;
    ddns_params.generated_prefix_ = "prefix";
    ddns_params.qualifying_suffix_ = "suffix.com";

    // Verify that it works with an IPv4 address.
    asiolink::IOAddress v4address("192.0.2.75");
    EXPECT_EQ("prefix-192-0-2-75.suffix.com.",
              mgr.generateFqdn(v4address, ddns_params, do_dot));

    // Verify that it works with an IPv6 address.
    asiolink::IOAddress v6address("2001:db8::2");
    EXPECT_EQ("prefix-2001-db8--2.suffix.com.",
              mgr.generateFqdn(v6address, ddns_params, do_dot));

    // Create a disabled config.
    ddns_params.enable_updates_ = false;

    // Verify names generate properly with a disabled configuration.
    EXPECT_EQ("prefix-192-0-2-75.suffix.com.",
               mgr.generateFqdn(v4address, ddns_params, do_dot));
    EXPECT_EQ("prefix-2001-db8--2.suffix.com.",
               mgr.generateFqdn(v6address, ddns_params, do_dot));
}

/// @brief Tests adjustDomainName template method with Option4ClientFqdn
TEST(D2ClientMgr, adjustDomainNameV4) {
    D2ClientMgr mgr;

    // Create enabled configuration
    DdnsParams ddns_params;
    ddns_params.enable_updates_ = true;
    ddns_params.override_no_update_ = false;
    ddns_params.override_client_update_ = false;
    ddns_params.replace_client_name_mode_ = D2ClientConfig::RCM_NEVER;
    ddns_params.generated_prefix_ = "prefix";
    ddns_params.qualifying_suffix_ = "suffix.com";

    struct Scenario {
        std::string description_;
        D2ClientConfig::ReplaceClientNameMode mode_;
        std::string client_name_;
        Option4ClientFqdn::DomainNameType client_name_type_;
        std::string expected_name_;
        Option4ClientFqdn::DomainNameType expected_name_type_;
    };

    std::vector<Scenario> scenarios = {
        {
            "RCM_NEVER #1, empty client name",
            D2ClientConfig::RCM_NEVER,
            "", Option4ClientFqdn::PARTIAL,
            "", Option4ClientFqdn::PARTIAL
        },
        {
            "RCM_NEVER #2, partial client name",
            D2ClientConfig::RCM_NEVER,
            "myhost", Option4ClientFqdn::PARTIAL,
            "myhost.suffix.com.", Option4ClientFqdn::FULL
        },
        {
            "RCM_NEVER #3, full client name",
            D2ClientConfig::RCM_NEVER,
            "myhost.example.com.", Option4ClientFqdn::FULL,
            "myhost.example.com.", Option4ClientFqdn::FULL
        },
        {
            "RCM_ALWAYS #1, empty client name",
            D2ClientConfig::RCM_ALWAYS,
            "", Option4ClientFqdn::PARTIAL,
            "", Option4ClientFqdn::PARTIAL
        },
        {
            "RCM_ALWAYS #2, partial client name",
            D2ClientConfig::RCM_ALWAYS,
            "myhost", Option4ClientFqdn::PARTIAL,
            "", Option4ClientFqdn::PARTIAL
        },
        {
            "RCM_ALWAYS #3, full client name",
            D2ClientConfig::RCM_ALWAYS,
            "myhost.example.com.", Option4ClientFqdn::FULL,
            "", Option4ClientFqdn::PARTIAL
        },
        {
            "RCM_WHEN_PRESENT #1, empty client name",
            D2ClientConfig::RCM_WHEN_PRESENT,
            "", Option4ClientFqdn::PARTIAL,
            "", Option4ClientFqdn::PARTIAL
        },
        {
            "RCM_WHEN_PRESENT #2, partial client name",
            D2ClientConfig::RCM_WHEN_PRESENT,
            "myhost", Option4ClientFqdn::PARTIAL,
            "", Option4ClientFqdn::PARTIAL
        },
        {
            "RCM_WHEN_PRESENT #3, full client name",
            D2ClientConfig::RCM_WHEN_PRESENT,
            "myhost.example.com.", Option4ClientFqdn::FULL,
            "", Option4ClientFqdn::PARTIAL
        },
        {
            "RCM_WHEN_NOT_PRESENT #1, empty client name",
            D2ClientConfig::RCM_WHEN_NOT_PRESENT,
            "", Option4ClientFqdn::PARTIAL,
            "", Option4ClientFqdn::PARTIAL
        },
        {
            "RCM_WHEN_NOT_PRESENT #2, partial client name",
            D2ClientConfig::RCM_WHEN_NOT_PRESENT,
            "myhost", Option4ClientFqdn::PARTIAL,
            "myhost.suffix.com.", Option4ClientFqdn::FULL
        },
        {
            "RCM_WHEN_NOT_PRESENT #3, full client name",
            D2ClientConfig::RCM_WHEN_NOT_PRESENT,
            "myhost.example.com.", Option4ClientFqdn::FULL,
            "myhost.example.com.", Option4ClientFqdn::FULL,
        }
    };

    for (auto scenario : scenarios) {
        SCOPED_TRACE(scenario.description_);
        {
            ddns_params.replace_client_name_mode_ = scenario.mode_;
            Option4ClientFqdn request (0, Option4ClientFqdn::RCODE_CLIENT(),
                                       scenario.client_name_,
                                       scenario.client_name_type_);

            Option4ClientFqdn response(request);
            mgr.adjustDomainName<Option4ClientFqdn>(request, response, ddns_params);
            EXPECT_EQ(scenario.expected_name_, response.getDomainName());
            EXPECT_EQ(scenario.expected_name_type_, response.getDomainNameType());
        }
    }
}

/// @brief Tests adjustDomainName template method with Option6ClientFqdn
TEST(D2ClientMgr, adjustDomainNameV6) {
    D2ClientMgr mgr;

    // Create enabled configuration
    DdnsParams ddns_params;
    ddns_params.enable_updates_ = true;
    ddns_params.override_no_update_ = false;
    ddns_params.override_client_update_ = false;
    ddns_params.replace_client_name_mode_ = D2ClientConfig::RCM_NEVER;
    ddns_params.generated_prefix_ = "prefix";
    ddns_params.qualifying_suffix_ = "suffix.com";

    struct Scenario {
        std::string description_;
        D2ClientConfig::ReplaceClientNameMode mode_;
        std::string client_name_;
        Option6ClientFqdn::DomainNameType client_name_type_;
        std::string expected_name_;
        Option6ClientFqdn::DomainNameType expected_name_type_;
    };

    std::vector<Scenario> scenarios = {
        {
            "RCM_NEVER #1, empty client name",
            D2ClientConfig::RCM_NEVER,
            "", Option6ClientFqdn::PARTIAL,
            "", Option6ClientFqdn::PARTIAL
        },
        {
            "RCM_NEVER #2, partial client name",
            D2ClientConfig::RCM_NEVER,
            "myhost", Option6ClientFqdn::PARTIAL,
            "myhost.suffix.com.", Option6ClientFqdn::FULL
        },
        {
            "RCM_NEVER #3, full client name",
            D2ClientConfig::RCM_NEVER,
            "myhost.example.com.", Option6ClientFqdn::FULL,
            "myhost.example.com.", Option6ClientFqdn::FULL
        },
        {
            "RCM_ALWAYS #1, empty client name",
            D2ClientConfig::RCM_ALWAYS,
            "", Option6ClientFqdn::PARTIAL,
            "", Option6ClientFqdn::PARTIAL
        },
        {
            "RCM_ALWAYS #2, partial client name",
            D2ClientConfig::RCM_ALWAYS,
            "myhost", Option6ClientFqdn::PARTIAL,
            "", Option6ClientFqdn::PARTIAL
        },
        {
            "RCM_ALWAYS #3, full client name",
            D2ClientConfig::RCM_ALWAYS,
            "myhost.example.com.", Option6ClientFqdn::FULL,
            "", Option6ClientFqdn::PARTIAL
        },
        {
            "RCM_WHEN_PRESENT #1, empty client name",
            D2ClientConfig::RCM_WHEN_PRESENT,
            "", Option6ClientFqdn::PARTIAL,
            "", Option6ClientFqdn::PARTIAL
        },
        {
            "RCM_WHEN_PRESENT #2, partial client name",
            D2ClientConfig::RCM_WHEN_PRESENT,
            "myhost", Option6ClientFqdn::PARTIAL,
            "", Option6ClientFqdn::PARTIAL
        },
        {
            "RCM_WHEN_PRESENT #3, full client name",
            D2ClientConfig::RCM_WHEN_PRESENT,
            "myhost.example.com.", Option6ClientFqdn::FULL,
            "", Option6ClientFqdn::PARTIAL
        },
        {
            "RCM_WHEN_NOT_PRESENT #1, empty client name",
            D2ClientConfig::RCM_WHEN_NOT_PRESENT,
            "", Option6ClientFqdn::PARTIAL,
            "", Option6ClientFqdn::PARTIAL
        },
        {
            "RCM_WHEN_NOT_PRESENT #2, partial client name",
            D2ClientConfig::RCM_WHEN_NOT_PRESENT,
            "myhost", Option6ClientFqdn::PARTIAL,
            "myhost.suffix.com.", Option6ClientFqdn::FULL
        },
        {
            "RCM_WHEN_NOT_PRESENT #3, full client name",
            D2ClientConfig::RCM_WHEN_NOT_PRESENT,
            "myhost.example.com.", Option6ClientFqdn::FULL,
            "myhost.example.com.", Option6ClientFqdn::FULL,
        }
    };

    for (auto scenario : scenarios) {
        SCOPED_TRACE(scenario.description_);
        {
            ddns_params.replace_client_name_mode_ = scenario.mode_;
            Option6ClientFqdn request(0, scenario.client_name_,
                                      scenario.client_name_type_);

            Option6ClientFqdn response(request);
            mgr.adjustDomainName<Option6ClientFqdn>(request, response, ddns_params);
            EXPECT_EQ(scenario.expected_name_, response.getDomainName());
            EXPECT_EQ(scenario.expected_name_type_, response.getDomainNameType());
        }
    }
}

/// @brief Verifies the adustFqdnFlags template with Option6ClientFqdn objects.
/// Ensures that the method can set the N, S, and O flags properly.
/// Other permutations are covered by analyzeFqdnFlags tests.
TEST(D2ClientMgr, adjustFqdnFlagsV6) {
    D2ClientMgr mgr;
    Option6ClientFqdnPtr request;
    Option6ClientFqdnPtr response;

    // Create enabled configuration with override-no-update true.
    DdnsParams ddns_params;
    ddns_params.enable_updates_ = true;
    ddns_params.override_no_update_ = true;
    ddns_params.override_client_update_ = false;
    ddns_params.replace_client_name_mode_ = D2ClientConfig::RCM_NEVER;
    ddns_params.generated_prefix_ = "";
    ddns_params.qualifying_suffix_ = "";

    // client S=0 N=0 means client wants to do forward update.
    // server S should be 0 (server is not doing forward updates)
    // and server N should be 0 (server doing reverse updates)
    // and server O should be 0
    request.reset(new Option6ClientFqdn(0, "", Option6ClientFqdn::PARTIAL));
    response.reset(new Option6ClientFqdn(*request));
    response->resetFlags();

    mgr.adjustFqdnFlags<Option6ClientFqdn>(*request, *response, ddns_params);
    EXPECT_FALSE(response->getFlag(Option6ClientFqdn::FLAG_S));
    EXPECT_FALSE(response->getFlag(Option6ClientFqdn::FLAG_N));
    EXPECT_FALSE(response->getFlag(Option6ClientFqdn::FLAG_O));

    // client S=1 N=0 means client wants server to do forward update.
    // server S should be 1 (server is doing forward updates)
    // and server N should be 0 (server doing updates)
    // and server O should be 0
    request.reset(new Option6ClientFqdn(Option6ClientFqdn::FLAG_S,
                                        "", Option6ClientFqdn::PARTIAL));
    response.reset(new Option6ClientFqdn(*request));
    response->resetFlags();

    mgr.adjustFqdnFlags<Option6ClientFqdn>(*request, *response, ddns_params);
    EXPECT_TRUE(response->getFlag(Option6ClientFqdn::FLAG_S));
    EXPECT_FALSE(response->getFlag(Option6ClientFqdn::FLAG_N));
    EXPECT_FALSE(response->getFlag(Option6ClientFqdn::FLAG_O));

    // client S=0 N=1 means client wants no one to do updates
    // server S should be 1 (server is doing forward updates)
    // and server N should be 0 (server doing updates)
    // and O should be 1 (overriding client S)
    request.reset(new Option6ClientFqdn(Option6ClientFqdn::FLAG_N,
                                        "", Option6ClientFqdn::PARTIAL));
    response.reset(new Option6ClientFqdn(*request));
    response->resetFlags();

    mgr.adjustFqdnFlags<Option6ClientFqdn>(*request, *response, ddns_params);
    EXPECT_TRUE(response->getFlag(Option6ClientFqdn::FLAG_S));
    EXPECT_FALSE(response->getFlag(Option6ClientFqdn::FLAG_N));
    EXPECT_TRUE(response->getFlag(Option6ClientFqdn::FLAG_O));
}


/// @brief Verified the getUpdateDirections template method with
/// Option6ClientFqdn objects.
TEST(D2ClientMgr, updateDirectionsV6) {
    D2ClientMgr mgr;
    Option6ClientFqdnPtr response;

    bool do_forward = false;
    bool do_reverse = false;

    // Response S=0, N=0 should mean do reverse only.
    response.reset(new Option6ClientFqdn(0,
                                         "", Option6ClientFqdn::PARTIAL));
    mgr.getUpdateDirections(*response, do_forward, do_reverse);
    EXPECT_FALSE(do_forward);
    EXPECT_TRUE(do_reverse);

    // Response S=0, N=1 should mean don't do either.
    response.reset(new Option6ClientFqdn(Option6ClientFqdn::FLAG_N,
                                         "", Option6ClientFqdn::PARTIAL));
    mgr.getUpdateDirections(*response, do_forward, do_reverse);
    EXPECT_FALSE(do_forward);
    EXPECT_FALSE(do_reverse);

    // Response S=1, N=0 should mean do both.
    response.reset(new Option6ClientFqdn(Option6ClientFqdn::FLAG_S,
                                         "", Option6ClientFqdn::PARTIAL));
    mgr.getUpdateDirections(*response, do_forward, do_reverse);
    EXPECT_TRUE(do_forward);
    EXPECT_TRUE(do_reverse);

    // Response S=1, N=1 isn't possible.
}

/// @brief Tests v4 FQDN name sanitizing
TEST(D2ClientMgr, sanitizeFqdnV4) {
    D2ClientMgr mgr;

    // Create enabled configuration with override-no-update true.
    DdnsParams ddns_params;
    ddns_params.enable_updates_ = true;
    ddns_params.override_no_update_ = false;
    ddns_params.override_client_update_ = false;
    ddns_params.replace_client_name_mode_ = D2ClientConfig::RCM_NEVER;
    ddns_params.generated_prefix_ = "prefix";
    ddns_params.qualifying_suffix_ = "suffix.com";
    ddns_params.hostname_char_set_ = "[^A-Za-z0-9-]";
    ddns_params.hostname_char_replacement_ = "x";

    // Get the sanitizer.
    str::StringSanitizerPtr hostname_sanitizer;
    ASSERT_NO_THROW(hostname_sanitizer = ddns_params.getHostnameSanitizer());
    ASSERT_TRUE(hostname_sanitizer);

    struct Scenario {
        std::string description_;
        std::string client_name_;
        Option4ClientFqdn::DomainNameType name_type_;
        std::string expected_name_;
    };

    std::vector<Scenario> scenarios = {
        {
        "full FQDN, name unchanged",
        "One.123.example.com.",
        Option4ClientFqdn::FULL,
        "one.123.example.com."
        },
        {
        "partial FQDN, name unchanged, but qualified",
        "One.123",
        Option4ClientFqdn::PARTIAL,
        "one.123.suffix.com."
        },
        {
        "full FQDN, scrubbed",
        "O#n^e.123.ex&a*mple.com.",
        Option4ClientFqdn::FULL,
        "oxnxe.123.exxaxmple.com."
        },
        {
        "partial FQDN, scrubbed and qualified",
        "One.1+2|3",
        Option4ClientFqdn::PARTIAL,
        "one.1x2x3.suffix.com."
        },
        {
        "full FQDN with characters that get escaped",
        "O n e.123.exa(m)ple.com.",
        Option4ClientFqdn::FULL,
        "oxnxe.123.exaxmxple.com."
        },
        {
        "full FQDN with escape sequences",
        "O\032n\032e.123.example.com.",
        Option4ClientFqdn::FULL,
        "oxnxe.123.example.com."
        }
    };

    for (auto scenario : scenarios) {
        SCOPED_TRACE(scenario.description_);
        {
            Option4ClientFqdn request(0, Option4ClientFqdn::RCODE_CLIENT(),
                                      scenario.client_name_, scenario.name_type_);
            Option4ClientFqdn response(request);

            mgr.adjustDomainName<Option4ClientFqdn>(request, response, ddns_params);
            EXPECT_EQ(scenario.expected_name_, response.getDomainName());
            EXPECT_EQ(Option4ClientFqdn::FULL, response.getDomainNameType());
        }
    }
}

/// @brief Tests v6 FQDN name sanitizing
/// @todo This test currently verifies that Option6ClientFqdn::DomainName
/// downcases strings used to construct it.  For some reason, currently
/// uknown, Option4ClientFqdn preserves the case, while Option6ClientFqdn
/// downcases it (see setDomainName() in both classes.  See Trac #5700.
TEST(D2ClientMgr, sanitizeFqdnV6) {
    D2ClientMgr mgr;

    // Create enabled configuration with override-no-update true.
    DdnsParams ddns_params;
    ddns_params.enable_updates_ = true;
    ddns_params.override_no_update_ = false;
    ddns_params.override_client_update_ = false;
    ddns_params.replace_client_name_mode_ = D2ClientConfig::RCM_NEVER;
    ddns_params.generated_prefix_ = "prefix";
    ddns_params.qualifying_suffix_ = "suffix.com";
    ddns_params.hostname_char_set_ = "[^A-Za-z0-9-]";
    ddns_params.hostname_char_replacement_ = "x";

    // Get the sanitizer.
    str::StringSanitizerPtr hostname_sanitizer;
    ASSERT_NO_THROW(hostname_sanitizer = ddns_params.getHostnameSanitizer());
    ASSERT_TRUE(hostname_sanitizer);

    struct Scenario {
        std::string description_;
        std::string client_name_;
        Option6ClientFqdn::DomainNameType name_type_;
        std::string expected_name_;
    };

    std::vector<Scenario> scenarios = {
        {
            "full FQDN, name unchanged",
            "One.123.example.com.",
            Option6ClientFqdn::FULL,
            "one.123.example.com."
        },
        {
            "partial FQDN, name unchanged, but qualified",
            "One.123",
            Option6ClientFqdn::PARTIAL,
            "one.123.suffix.com."
        },
        {
            "full FQDN, scrubbed",
            "O#n^e.123.ex&a*mple.com.",
            Option6ClientFqdn::FULL,
            "oxnxe.123.exxaxmple.com."
        },
        {
            "partial FQDN, scrubbed and qualified",
            "One.1+2|3",
            Option6ClientFqdn::PARTIAL,
            "one.1x2x3.suffix.com."
        },
        {
            "full FQDN with characters that get escaped",
            "O n e.123.exa(m)ple.com.",
            Option6ClientFqdn::FULL,
            "oxnxe.123.exaxmxple.com."
        },
        {
            "full FQDN with escape sequences",
            "O\032n\032e.123.example.com.",
            Option6ClientFqdn::FULL,
            "oxnxe.123.example.com."
        }
    };

    Option6ClientFqdnPtr response;
    for (auto scenario : scenarios) {
        SCOPED_TRACE(scenario.description_);
        {
            Option6ClientFqdn request(0, scenario.client_name_, scenario.name_type_);
            Option6ClientFqdn response(request);

            mgr.adjustDomainName<Option6ClientFqdn>(request, response, ddns_params);
            EXPECT_EQ(scenario.expected_name_, response.getDomainName());
            EXPECT_EQ(Option6ClientFqdn::FULL, response.getDomainNameType());
        }
    }
}

} // end of anonymous namespace
