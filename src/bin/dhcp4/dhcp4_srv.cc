// Copyright (C) 2011  Internet Systems Consortium, Inc. ("ISC")
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
// LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
// OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
// PERFORMANCE OF THIS SOFTWARE.

#include <dhcp/dhcp4.h>
#include <dhcp/pkt4.h>
#include <dhcp/iface_mgr.h>
#include <dhcp4/dhcp4_srv.h>
#include <asiolink/io_address.h>
#include <dhcp/option4_addrlst.h>

using namespace std;
using namespace isc;
using namespace isc::dhcp;
using namespace isc::asiolink;

// These are hardcoded parameters. Currently this is a skeleton server that only
// grants those options and a single, fixed, hardcoded lease.
const std::string HARDCODED_LEASE = "192.0.2.222"; // assigned lease
const std::string HARDCODED_NETMASK = "255.255.255.0";
const uint32_t    HARDCODED_LEASE_TIME = 60; // in seconds
const std::string HARDCODED_GATEWAY = "192.0.2.1";
const std::string HARDCODED_DNS_SERVER = "192.0.2.2";
const std::string HARDCODED_DOMAIN_NAME = "isc.example.com";
const std::string HARDCODED_SERVER_ID = "192.0.2.1";

Dhcpv4Srv::Dhcpv4Srv(uint16_t port) {
    cout << "Initialization: opening sockets on port " << port << endl;

    // first call to instance() will create IfaceMgr (it's a singleton)
    // it may throw something if things go wrong
    IfaceMgr::instance();

    /// @todo: instantiate LeaseMgr here once it is imlpemented.
    IfaceMgr::instance().printIfaces();

    IfaceMgr::instance().openSockets4(port);

    setServerID();

    shutdown_ = false;
}

Dhcpv4Srv::~Dhcpv4Srv() {
    cout << "DHCPv4 server shutdown." << endl;
    IfaceMgr::instance().closeSockets();
}

bool
Dhcpv4Srv::run() {
    while (!shutdown_) {
        boost::shared_ptr<Pkt4> query; // client's message
        boost::shared_ptr<Pkt4> rsp;   // server's response

        query = IfaceMgr::instance().receive4();

        if (query) {
            try {
                query->unpack();
            } catch (const std::exception& e) {
                /// TODO: Printout reasons of failed parsing
                cout << "Failed to parse incoming packet " << endl;
                continue;
            }

            switch (query->getType()) {
            case DHCPDISCOVER:
                rsp = processDiscover(query);
                break;
            case DHCPREQUEST:
                rsp = processRequest(query);
                break;
            case DHCPRELEASE:
                processRelease(query);
                break;
            case DHCPDECLINE:
                processDecline(query);
                break;
            case DHCPINFORM:
                processInform(query);
                break;
            default:
                cout << "Unknown pkt type received:"
                     << query->getType() << endl;
            }

            cout << "Received message type " << int(query->getType()) << endl;

            // TODO: print out received packets only if verbose (or debug)
            // mode is enabled
            cout << query->toText();

            if (rsp) {
                if (rsp->getRemoteAddr().toText() == "0.0.0.0") {
                    rsp->setRemoteAddr(query->getRemoteAddr());
                }
                if (!rsp->getHops()) {
                    rsp->setRemotePort(DHCP4_CLIENT_PORT);
                } else {
                    rsp->setRemotePort(DHCP4_SERVER_PORT);
                }

                rsp->setLocalAddr(query->getLocalAddr());
                rsp->setLocalPort(DHCP4_SERVER_PORT);
                rsp->setIface(query->getIface());
                rsp->setIndex(query->getIndex());

                cout << "Replying with message type "
                     << static_cast<int>(rsp->getType()) << ":" << endl;
                cout << rsp->toText();
                cout << "----" << endl;
                if (rsp->pack()) {
                    cout << "Packet assembled correctly." << endl;
                }
                IfaceMgr::instance().send(rsp);
            }
        }

        // TODO add support for config session (see src/bin/auth/main.cc)
        //      so this daemon can be controlled from bob
    }

    return (true);
}

void
Dhcpv4Srv::setServerID() {
    /// TODO implement this for real once interface detection (ticket 1237)
    /// is done. Use hardcoded server-id for now.

#if 0
    // uncomment this once ticket 1350 is merged.
    IOAddress srvId("127.0.0.1");
    serverid_ = boost::shared_ptr<Option>(
      new Option4AddrLst(Option::V4, DHO_DHCP_SERVER_IDENTIFIER, srvId));
#endif
}


void Dhcpv4Srv::copyDefaultFields(const boost::shared_ptr<Pkt4>& question,
                                  boost::shared_ptr<Pkt4>& answer) {
    answer->setIface(question->getIface());
    answer->setIndex(question->getIndex());
    answer->setCiaddr(question->getCiaddr());

    answer->setSiaddr(IOAddress("0.0.0.0")); // explictly set this to 0
    answer->setHops(question->getHops());

    // copy MAC address
    vector<uint8_t> mac(question->getChaddr(),
                        question->getChaddr() + Pkt4::MAX_CHADDR_LEN);
    answer->setHWAddr(question->getHtype(), question->getHlen(), mac);

    // relay address
    answer->setGiaddr(question->getGiaddr());

    if (question->getGiaddr().toText() != "0.0.0.0") {
        // relayed traffic
        answer->setRemoteAddr(question->getGiaddr());
    } else {
        // direct traffic
        answer->setRemoteAddr(question->getRemoteAddr());
    }

}

void Dhcpv4Srv::appendDefaultOptions(boost::shared_ptr<Pkt4>& msg, uint8_t msg_type) {
    boost::shared_ptr<Option> opt;

    // add Message Type Option (type 53)
    std::vector<uint8_t> tmp;
    tmp.push_back(static_cast<uint8_t>(msg_type));
    opt = boost::shared_ptr<Option>(new Option(Option::V4, DHO_DHCP_MESSAGE_TYPE, tmp));
    msg->addOption(opt);

    // DHCP Server Identifier (type 54)
    opt = boost::shared_ptr<Option>
        (new Option4AddrLst(DHO_DHCP_SERVER_IDENTIFIER, IOAddress(HARDCODED_SERVER_ID)));
    msg->addOption(opt);

    // more options will be added here later
}


void Dhcpv4Srv::appendRequestedOptions(boost::shared_ptr<Pkt4>& msg) {
    boost::shared_ptr<Option> opt;

    // Domain name (type 15)
    vector<uint8_t> domain(HARDCODED_DOMAIN_NAME.begin(), HARDCODED_DOMAIN_NAME.end());
    opt = boost::shared_ptr<Option>(new Option(Option::V4, DHO_DOMAIN_NAME, domain));
    msg->addOption(opt);
    // TODO: Add Option_String class

    // DNS servers (type 6)
    opt = boost::shared_ptr<Option>
        (new Option4AddrLst(DHO_DOMAIN_NAME_SERVERS, IOAddress(HARDCODED_DNS_SERVER)));
    msg->addOption(opt);
}

void Dhcpv4Srv::tryAssignLease(boost::shared_ptr<Pkt4>& msg) {
    boost::shared_ptr<Option> opt;

    // TODO: Implement actual lease assignment here
    msg->setYiaddr(IOAddress(HARDCODED_LEASE));

    // IP Address Lease time (type 51)
    opt = boost::shared_ptr<Option>(new Option(Option::V4, DHO_DHCP_LEASE_TIME));
    opt->setUint32(HARDCODED_LEASE_TIME);
    msg->addOption(opt);
    // TODO: create Option_IntArray that holds list of integers, similar to Option4_AddrLst

    // Subnet mask (type 1)
    opt = boost::shared_ptr<Option>
        (new Option4AddrLst(DHO_SUBNET_MASK, IOAddress(HARDCODED_NETMASK)));
    msg->addOption(opt);

    // Router (type 3)
    opt = boost::shared_ptr<Option>
        (new Option4AddrLst(DHO_ROUTERS, IOAddress(HARDCODED_GATEWAY)));
    msg->addOption(opt);
}

boost::shared_ptr<Pkt4>
Dhcpv4Srv::processDiscover(boost::shared_ptr<Pkt4>& discover) {
    boost::shared_ptr<Pkt4> offer = boost::shared_ptr<Pkt4>
        (new Pkt4(DHCPOFFER, discover->getTransid()));

    copyDefaultFields(discover, offer);
    appendDefaultOptions(offer, DHCPOFFER);
    appendRequestedOptions(offer);

    tryAssignLease(offer);

    return (offer);
}

boost::shared_ptr<Pkt4>
Dhcpv4Srv::processRequest(boost::shared_ptr<Pkt4>& request) {
    boost::shared_ptr<Pkt4> ack = boost::shared_ptr<Pkt4>
        (new Pkt4(DHCPACK, request->getTransid()));

    copyDefaultFields(request, ack);
    appendDefaultOptions(ack, DHCPACK);
    appendRequestedOptions(ack);

    tryAssignLease(ack);

    return (ack);
}

void Dhcpv4Srv::processRelease(boost::shared_ptr<Pkt4>& release) {
    /// TODO: Implement this.
    cout << "Received RELEASE on " << release->getIface() << " interface." << endl;
}

void Dhcpv4Srv::processDecline(boost::shared_ptr<Pkt4>& decline) {
    /// TODO: Implement this.
    cout << "Received DECLINE on " << decline->getIface() << " interface." << endl;
}

boost::shared_ptr<Pkt4> Dhcpv4Srv::processInform(boost::shared_ptr<Pkt4>& inform) {
    /// TODO: Currently implemented echo mode. Implement this for real
    return (inform);
}
