#include "mu.h"
#include "../include/crux/dns.h"

#include <arpa/inet.h>

static void
test_a(void)
{
	static const uint8_t RR[] = {
		0x48, 0xc9, 0x81, 0x80, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
		0x06, 0x67, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x03, 0x63, 0x6f, 0x6d, 0x00,
		0x00, 0x01, 0x00, 0x01, 0xc0, 0x0c, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00,
		0x00, 0xa0, 0x00, 0x04, 0xd8, 0x3a, 0xc2, 0xae
	};

	char buf[256];

	struct xdns dns;
	mu_assert_int_eq(xdns_load(&dns, RR, sizeof(RR)), 0);

	struct xdns_iter iter;
	xdns_iter_init(&iter, &dns);

	mu_assert_uint_eq(iter.hdr.id, 18633);
	mu_assert_uint_eq(iter.hdr.qr, 1);
	mu_assert_uint_eq(iter.hdr.opcode, XDNS_QUERY);
	mu_assert_uint_eq(iter.hdr.aa, false);
	mu_assert_uint_eq(iter.hdr.tc, false);
	mu_assert_uint_eq(iter.hdr.rd, true);
	mu_assert_uint_eq(iter.hdr.ra, true);
	mu_assert_uint_eq(iter.hdr.rcode, XDNS_NOERROR);
	mu_assert_uint_eq(iter.hdr.qdcount, 1);
	mu_assert_uint_eq(iter.hdr.ancount, 1);
	mu_assert_uint_eq(iter.hdr.nscount, 0);
	mu_assert_uint_eq(iter.hdr.arcount, 0);

	mu_assert_int_eq(xdns_iter_next(&iter), XDNS_MORE);
	mu_assert_int_eq(iter.section, XDNS_S_QD);
	mu_assert_int_eq(iter.query.qtype, XDNS_A);
	mu_assert_int_eq(iter.query.qclass, XDNS_IN);
	mu_assert_str_eq(iter.name, "google.com.");

	mu_assert_int_eq(xdns_iter_next(&iter), XDNS_MORE);
	mu_assert_str_eq(iter.name, "google.com.");
	mu_assert_int_eq(iter.section, XDNS_S_AN);
	mu_assert_int_eq(iter.rr.rtype, XDNS_A);
	mu_assert_int_eq(iter.rr.rclass, XDNS_IN);
	mu_assert_int_eq(iter.rr.ttl, 160);
	mu_assert_str_eq(
			inet_ntop(AF_INET, &iter.rdata.a, buf, sizeof(buf)),
			"216.58.194.174");

	mu_assert_int_eq(xdns_iter_next(&iter), XDNS_DONE);
}

static void
test_ns(void)
{
	static const uint8_t RR[] = {
		0xa0, 0x8a, 0x81, 0x80, 0x00, 0x01, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00,
		0x06, 0x67, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x03, 0x63, 0x6f, 0x6d, 0x00,
		0x00, 0x02, 0x00, 0x01, 0xc0, 0x0c, 0x00, 0x02, 0x00, 0x01, 0x00, 0x00,
		0x54, 0x5f, 0x00, 0x06, 0x03, 0x6e, 0x73, 0x34, 0xc0, 0x0c, 0xc0, 0x0c,
		0x00, 0x02, 0x00, 0x01, 0x00, 0x00, 0x54, 0x5f, 0x00, 0x06, 0x03, 0x6e,
		0x73, 0x31, 0xc0, 0x0c, 0xc0, 0x0c, 0x00, 0x02, 0x00, 0x01, 0x00, 0x00,
		0x54, 0x5f, 0x00, 0x06, 0x03, 0x6e, 0x73, 0x33, 0xc0, 0x0c, 0xc0, 0x0c,
		0x00, 0x02, 0x00, 0x01, 0x00, 0x00, 0x54, 0x5f, 0x00, 0x06, 0x03, 0x6e,
		0x73, 0x32, 0xc0, 0x0c
	};

	struct xdns dns;
	mu_assert_int_eq(xdns_load(&dns, RR, sizeof(RR)), 0);

	struct xdns_iter iter;
	xdns_iter_init(&iter, &dns);

	mu_assert_uint_eq(iter.hdr.id, 41098);
	mu_assert_uint_eq(iter.hdr.qr, 1);
	mu_assert_uint_eq(iter.hdr.opcode, XDNS_QUERY);
	mu_assert_uint_eq(iter.hdr.aa, false);
	mu_assert_uint_eq(iter.hdr.tc, false);
	mu_assert_uint_eq(iter.hdr.rd, true);
	mu_assert_uint_eq(iter.hdr.ra, true);
	mu_assert_uint_eq(iter.hdr.rcode, XDNS_NOERROR);
	mu_assert_uint_eq(iter.hdr.qdcount, 1);
	mu_assert_uint_eq(iter.hdr.ancount, 4);
	mu_assert_uint_eq(iter.hdr.nscount, 0);
	mu_assert_uint_eq(iter.hdr.arcount, 0);

	mu_assert_int_eq(xdns_iter_next(&iter), XDNS_MORE);
	mu_assert_int_eq(iter.section, XDNS_S_QD);
	mu_assert_int_eq(iter.query.qtype, XDNS_NS);
	mu_assert_int_eq(iter.query.qclass, XDNS_IN);
	mu_assert_str_eq(iter.name, "google.com.");

	mu_assert_int_eq(xdns_iter_next(&iter), XDNS_MORE);
	mu_assert_str_eq(iter.name, "google.com.");
	mu_assert_int_eq(iter.section, XDNS_S_AN);
	mu_assert_int_eq(iter.rr.rtype, XDNS_NS);
	mu_assert_int_eq(iter.rr.rclass, XDNS_IN);
	mu_assert_int_eq(iter.rr.ttl, 21599);
	mu_assert_str_eq(iter.rdata.ns, "ns4.google.com.");

	mu_assert_int_eq(xdns_iter_next(&iter), XDNS_MORE);
	mu_assert_str_eq(iter.name, "google.com.");
	mu_assert_int_eq(iter.section, XDNS_S_AN);
	mu_assert_int_eq(iter.rr.rtype, XDNS_NS);
	mu_assert_int_eq(iter.rr.rclass, XDNS_IN);
	mu_assert_int_eq(iter.rr.ttl, 21599);
	mu_assert_str_eq(iter.rdata.ns, "ns1.google.com.");

	mu_assert_int_eq(xdns_iter_next(&iter), XDNS_MORE);
	mu_assert_str_eq(iter.name, "google.com.");
	mu_assert_int_eq(iter.section, XDNS_S_AN);
	mu_assert_int_eq(iter.rr.rtype, XDNS_NS);
	mu_assert_int_eq(iter.rr.rclass, XDNS_IN);
	mu_assert_int_eq(iter.rr.ttl, 21599);
	mu_assert_str_eq(iter.rdata.ns, "ns3.google.com.");

	mu_assert_int_eq(xdns_iter_next(&iter), XDNS_MORE);
	mu_assert_str_eq(iter.name, "google.com.");
	mu_assert_int_eq(iter.section, XDNS_S_AN);
	mu_assert_int_eq(iter.rr.rtype, XDNS_NS);
	mu_assert_int_eq(iter.rr.rclass, XDNS_IN);
	mu_assert_int_eq(iter.rr.ttl, 21599);
	mu_assert_str_eq(iter.rdata.ns, "ns2.google.com.");

	mu_assert_int_eq(xdns_iter_next(&iter), XDNS_DONE);
}

static void
test_cname(void)
{
	static const uint8_t RR[] = {
		0x7a, 0xd9, 0x81, 0x80, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
		0x03, 0x67, 0x68, 0x73, 0x06, 0x67, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x03,
		0x63, 0x6f, 0x6d, 0x00, 0x00, 0x05, 0x00, 0x01, 0xc0, 0x0c, 0x00, 0x05,
		0x00, 0x01, 0x00, 0x00, 0x54, 0x45, 0x00, 0x08, 0x03, 0x67, 0x68, 0x73,
		0x01, 0x6c, 0xc0, 0x10
	};

	struct xdns dns;
	mu_assert_int_eq(xdns_load(&dns, RR, sizeof(RR)), 0);

	struct xdns_iter iter;
	xdns_iter_init(&iter, &dns);

	mu_assert_uint_eq(iter.hdr.id, 31449);
	mu_assert_uint_eq(iter.hdr.qr, 1);
	mu_assert_uint_eq(iter.hdr.opcode, XDNS_QUERY);
	mu_assert_uint_eq(iter.hdr.aa, false);
	mu_assert_uint_eq(iter.hdr.tc, false);
	mu_assert_uint_eq(iter.hdr.rd, true);
	mu_assert_uint_eq(iter.hdr.ra, true);
	mu_assert_uint_eq(iter.hdr.rcode, XDNS_NOERROR);
	mu_assert_uint_eq(iter.hdr.qdcount, 1);
	mu_assert_uint_eq(iter.hdr.ancount, 1);
	mu_assert_uint_eq(iter.hdr.nscount, 0);
	mu_assert_uint_eq(iter.hdr.arcount, 0);

	mu_assert_int_eq(xdns_iter_next(&iter), XDNS_MORE);
	mu_assert_int_eq(iter.section, XDNS_S_QD);
	mu_assert_int_eq(iter.query.qtype, XDNS_CNAME);
	mu_assert_int_eq(iter.query.qclass, XDNS_IN);
	mu_assert_str_eq(iter.name, "ghs.google.com.");

	mu_assert_int_eq(xdns_iter_next(&iter), XDNS_MORE);
	mu_assert_str_eq(iter.name, "ghs.google.com.");
	mu_assert_int_eq(iter.section, XDNS_S_AN);
	mu_assert_int_eq(iter.rr.rtype, XDNS_CNAME);
	mu_assert_int_eq(iter.rr.rclass, XDNS_IN);
	mu_assert_int_eq(iter.rr.ttl, 21573);
	mu_assert_str_eq(iter.rdata.cname, "ghs.l.google.com.");

	mu_assert_int_eq(xdns_iter_next(&iter), XDNS_DONE);
}

static void
test_soa(void)
{
	static const uint8_t RR[] = {
		0x07, 0xd8, 0x81, 0x80, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
		0x06, 0x67, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x03, 0x63, 0x6f, 0x6d, 0x00,
		0x00, 0x06, 0x00, 0x01, 0xc0, 0x0c, 0x00, 0x06, 0x00, 0x01, 0x00, 0x00,
		0x00, 0x3b, 0x00, 0x26, 0x03, 0x6e, 0x73, 0x34, 0xc0, 0x0c, 0x09, 0x64,
		0x6e, 0x73, 0x2d, 0x61, 0x64, 0x6d, 0x69, 0x6e, 0xc0, 0x0c, 0x08, 0x3e,
		0x00, 0xc7, 0x00, 0x00, 0x03, 0x84, 0x00, 0x00, 0x03, 0x84, 0x00, 0x00,
		0x07, 0x08, 0x00, 0x00, 0x00, 0x3c
	};

	struct xdns dns;
	memset(dns.buf, 0xFF, sizeof(dns).buf);
	mu_assert_int_eq(xdns_load(&dns, RR, sizeof(RR)), 0);

	struct xdns_iter iter;
	xdns_iter_init(&iter, &dns);

	mu_assert_uint_eq(iter.hdr.id, 2008);
	mu_assert_uint_eq(iter.hdr.qr, 1);
	mu_assert_uint_eq(iter.hdr.opcode, XDNS_QUERY);
	mu_assert_uint_eq(iter.hdr.aa, false);
	mu_assert_uint_eq(iter.hdr.tc, false);
	mu_assert_uint_eq(iter.hdr.rd, true);
	mu_assert_uint_eq(iter.hdr.ra, true);
	mu_assert_uint_eq(iter.hdr.rcode, XDNS_NOERROR);
	mu_assert_uint_eq(iter.hdr.qdcount, 1);
	mu_assert_uint_eq(iter.hdr.ancount, 1);
	mu_assert_uint_eq(iter.hdr.nscount, 0);
	mu_assert_uint_eq(iter.hdr.arcount, 0);

	mu_assert_int_eq(xdns_iter_next(&iter), XDNS_MORE);
	mu_assert_int_eq(iter.section, XDNS_S_QD);
	mu_assert_int_eq(iter.query.qtype, XDNS_SOA);
	mu_assert_int_eq(iter.query.qclass, XDNS_IN);
	mu_assert_str_eq(iter.name, "google.com.");

	mu_assert_int_eq(xdns_iter_next(&iter), XDNS_MORE);
	mu_assert_str_eq(iter.name, "google.com.");
	mu_assert_int_eq(iter.section, XDNS_S_AN);
	mu_assert_int_eq(iter.rr.rtype, XDNS_SOA);
	mu_assert_int_eq(iter.rr.rclass, XDNS_IN);
	mu_assert_int_eq(iter.rr.ttl, 59);
    mu_assert_str_eq(iter.rdata.soa.mname, "ns4.google.com.");
	mu_assert_str_eq(iter.rdata.soa.rname, "dns-admin.google.com.");
	mu_assert_int_eq(iter.rdata.soa.serial, 138281159);
	mu_assert_int_eq(iter.rdata.soa.refresh, 900);
	mu_assert_int_eq(iter.rdata.soa.retry, 900);
	mu_assert_int_eq(iter.rdata.soa.expire, 1800);
	mu_assert_int_eq(iter.rdata.soa.minimum, 60);

	mu_assert_int_eq(xdns_iter_next(&iter), XDNS_DONE);
}

static void
test_mx(void)
{
	static const uint8_t RR[] = {
		0x38, 0x36, 0x81, 0x80, 0x00, 0x01, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00,
		0x06, 0x67, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x03, 0x63, 0x6f, 0x6d, 0x00,
		0x00, 0x0f, 0x00, 0x01, 0xc0, 0x0c, 0x00, 0x0f, 0x00, 0x01, 0x00, 0x00,
		0x02, 0x4a, 0x00, 0x11, 0x00, 0x1e, 0x04, 0x61, 0x6c, 0x74, 0x32, 0x05,
		0x61, 0x73, 0x70, 0x6d, 0x78, 0x01, 0x6c, 0xc0, 0x0c, 0xc0, 0x0c, 0x00,
		0x0f, 0x00, 0x01, 0x00, 0x00, 0x02, 0x4a, 0x00, 0x09, 0x00, 0x14, 0x04,
		0x61, 0x6c, 0x74, 0x31, 0xc0, 0x2f, 0xc0, 0x0c, 0x00, 0x0f, 0x00, 0x01,
		0x00, 0x00, 0x02, 0x4a, 0x00, 0x09, 0x00, 0x28, 0x04, 0x61, 0x6c, 0x74,
		0x33, 0xc0, 0x2f, 0xc0, 0x0c, 0x00, 0x0f, 0x00, 0x01, 0x00, 0x00, 0x02,
		0x4a, 0x00, 0x09, 0x00, 0x32, 0x04, 0x61, 0x6c, 0x74, 0x34, 0xc0, 0x2f,
		0xc0, 0x0c, 0x00, 0x0f, 0x00, 0x01, 0x00, 0x00, 0x02, 0x4a, 0x00, 0x04,
		0x00, 0x0a, 0xc0, 0x2f
	};

	struct xdns dns;
	mu_assert_int_eq(xdns_load(&dns, RR, sizeof(RR)), 0);

	struct xdns_iter iter;
	xdns_iter_init(&iter, &dns);

	mu_assert_uint_eq(iter.hdr.id, 14390);
	mu_assert_uint_eq(iter.hdr.qr, 1);
	mu_assert_uint_eq(iter.hdr.opcode, XDNS_QUERY);
	mu_assert_uint_eq(iter.hdr.aa, false);
	mu_assert_uint_eq(iter.hdr.tc, false);
	mu_assert_uint_eq(iter.hdr.rd, true);
	mu_assert_uint_eq(iter.hdr.ra, true);
	mu_assert_uint_eq(iter.hdr.rcode, XDNS_NOERROR);
	mu_assert_uint_eq(iter.hdr.qdcount, 1);
	mu_assert_uint_eq(iter.hdr.ancount, 5);
	mu_assert_uint_eq(iter.hdr.nscount, 0);
	mu_assert_uint_eq(iter.hdr.arcount, 0);

	mu_assert_int_eq(xdns_iter_next(&iter), XDNS_MORE);
	mu_assert_int_eq(iter.section, XDNS_S_QD);
	mu_assert_int_eq(iter.query.qtype, XDNS_MX);
	mu_assert_int_eq(iter.query.qclass, XDNS_IN);
	mu_assert_str_eq(iter.name, "google.com.");

	mu_assert_int_eq(xdns_iter_next(&iter), XDNS_MORE);
	mu_assert_str_eq(iter.name, "google.com.");
	mu_assert_int_eq(iter.section, XDNS_S_AN);
	mu_assert_int_eq(iter.rr.rtype, XDNS_MX);
	mu_assert_int_eq(iter.rr.rclass, XDNS_IN);
	mu_assert_int_eq(iter.rr.ttl, 586);
	mu_assert_int_eq(iter.rdata.mx.preference, 30);
	mu_assert_str_eq(iter.rdata.mx.host, "alt2.aspmx.l.google.com.");

	mu_assert_int_eq(xdns_iter_next(&iter), XDNS_MORE);
	mu_assert_str_eq(iter.name, "google.com.");
	mu_assert_int_eq(iter.section, XDNS_S_AN);
	mu_assert_int_eq(iter.rr.rtype, XDNS_MX);
	mu_assert_int_eq(iter.rr.rclass, XDNS_IN);
	mu_assert_int_eq(iter.rr.ttl, 586);
	mu_assert_int_eq(iter.rdata.mx.preference, 20);
	mu_assert_str_eq(iter.rdata.mx.host, "alt1.aspmx.l.google.com.");

	mu_assert_int_eq(xdns_iter_next(&iter), XDNS_MORE);
	mu_assert_str_eq(iter.name, "google.com.");
	mu_assert_int_eq(iter.section, XDNS_S_AN);
	mu_assert_int_eq(iter.rr.rtype, XDNS_MX);
	mu_assert_int_eq(iter.rr.rclass, XDNS_IN);
	mu_assert_int_eq(iter.rr.ttl, 586);
	mu_assert_int_eq(iter.rdata.mx.preference, 40);
	mu_assert_str_eq(iter.rdata.mx.host, "alt3.aspmx.l.google.com.");

	mu_assert_int_eq(xdns_iter_next(&iter), XDNS_MORE);
	mu_assert_str_eq(iter.name, "google.com.");
	mu_assert_int_eq(iter.section, XDNS_S_AN);
	mu_assert_int_eq(iter.rr.rtype, XDNS_MX);
	mu_assert_int_eq(iter.rr.rclass, XDNS_IN);
	mu_assert_int_eq(iter.rr.ttl, 586);
	mu_assert_int_eq(iter.rdata.mx.preference, 50);
	mu_assert_str_eq(iter.rdata.mx.host, "alt4.aspmx.l.google.com.");

	mu_assert_int_eq(xdns_iter_next(&iter), XDNS_MORE);
	mu_assert_str_eq(iter.name, "google.com.");
	mu_assert_int_eq(iter.section, XDNS_S_AN);
	mu_assert_int_eq(iter.rr.rtype, XDNS_MX);
	mu_assert_int_eq(iter.rr.rclass, XDNS_IN);
	mu_assert_int_eq(iter.rr.ttl, 586);
	mu_assert_int_eq(iter.rdata.mx.preference, 10);
	mu_assert_str_eq(iter.rdata.mx.host, "aspmx.l.google.com.");

	mu_assert_int_eq(xdns_iter_next(&iter), XDNS_DONE);
}

static void
test_txt(void)
{
	static const uint8_t RR[] = {
		0xfd, 0x1c, 0x81, 0x80, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
		0x06, 0x67, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x03, 0x63, 0x6f, 0x6d, 0x00,
		0x00, 0x10, 0x00, 0x01, 0xc0, 0x0c, 0x00, 0x10, 0x00, 0x01, 0x00, 0x00,
		0x0e, 0x0f, 0x00, 0x24, 0x23, 0x76, 0x3d, 0x73, 0x70, 0x66, 0x31, 0x20,
		0x69, 0x6e, 0x63, 0x6c, 0x75, 0x64, 0x65, 0x3a, 0x5f, 0x73, 0x70, 0x66,
		0x2e, 0x67, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x2e, 0x63, 0x6f, 0x6d, 0x20,
		0x7e, 0x61, 0x6c, 0x6c
	};

	char buf[256];

	struct xdns dns;
	mu_assert_int_eq(xdns_load(&dns, RR, sizeof(RR)), 0);

	struct xdns_iter iter;
	xdns_iter_init(&iter, &dns);

	mu_assert_uint_eq(iter.hdr.id, 64796);
	mu_assert_uint_eq(iter.hdr.qr, 1);
	mu_assert_uint_eq(iter.hdr.opcode, XDNS_QUERY);
	mu_assert_uint_eq(iter.hdr.aa, false);
	mu_assert_uint_eq(iter.hdr.tc, false);
	mu_assert_uint_eq(iter.hdr.rd, true);
	mu_assert_uint_eq(iter.hdr.ra, true);
	mu_assert_uint_eq(iter.hdr.rcode, XDNS_NOERROR);
	mu_assert_uint_eq(iter.hdr.qdcount, 1);
	mu_assert_uint_eq(iter.hdr.ancount, 1);
	mu_assert_uint_eq(iter.hdr.nscount, 0);
	mu_assert_uint_eq(iter.hdr.arcount, 0);

	mu_assert_int_eq(xdns_iter_next(&iter), XDNS_MORE);
	mu_assert_int_eq(iter.section, XDNS_S_QD);
	mu_assert_int_eq(iter.query.qtype, XDNS_TXT);
	mu_assert_int_eq(iter.query.qclass, XDNS_IN);
	mu_assert_str_eq(iter.name, "google.com.");

	mu_assert_int_eq(xdns_iter_next(&iter), XDNS_MORE);
	mu_assert_str_eq(iter.name, "google.com.");
	mu_assert_int_eq(iter.section, XDNS_S_AN);
	mu_assert_int_eq(iter.rr.rtype, XDNS_TXT);
	mu_assert_int_eq(iter.rr.rclass, XDNS_IN);
	mu_assert_int_eq(iter.rr.ttl, 3599);

	memcpy(buf, iter.rdata.txt.data, iter.rdata.txt.length);
	buf[iter.rdata.txt.length] = 0;

	mu_assert_str_eq(buf, "#v=spf1 include:_spf.google.com ~all");

	mu_assert_int_eq(xdns_iter_next(&iter), XDNS_DONE);
}

static void
test_aaaa(void)
{
	static const uint8_t RR[] = {
		0xd5, 0x7b, 0x81, 0x80, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
		0x06, 0x67, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x03, 0x63, 0x6f, 0x6d, 0x00,
		0x00, 0x1c, 0x00, 0x01, 0xc0, 0x0c, 0x00, 0x1c, 0x00, 0x01, 0x00, 0x00,
		0x01, 0x2b, 0x00, 0x10, 0x26, 0x07, 0xf8, 0xb0, 0x40, 0x05, 0x08, 0x08,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x0e
	};

	char buf[256];

	struct xdns dns;
	mu_assert_int_eq(xdns_load(&dns, RR, sizeof(RR)), 0);

	struct xdns_iter iter;
	xdns_iter_init(&iter, &dns);

	mu_assert_uint_eq(iter.hdr.id, 54651);
	mu_assert_uint_eq(iter.hdr.qr, 1);
	mu_assert_uint_eq(iter.hdr.opcode, XDNS_QUERY);
	mu_assert_uint_eq(iter.hdr.aa, false);
	mu_assert_uint_eq(iter.hdr.tc, false);
	mu_assert_uint_eq(iter.hdr.rd, true);
	mu_assert_uint_eq(iter.hdr.ra, true);
	mu_assert_uint_eq(iter.hdr.rcode, XDNS_NOERROR);
	mu_assert_uint_eq(iter.hdr.qdcount, 1);
	mu_assert_uint_eq(iter.hdr.ancount, 1);
	mu_assert_uint_eq(iter.hdr.nscount, 0);
	mu_assert_uint_eq(iter.hdr.arcount, 0);

	mu_assert_int_eq(xdns_iter_next(&iter), XDNS_MORE);
	mu_assert_int_eq(iter.section, XDNS_S_QD);
	mu_assert_int_eq(iter.query.qtype, XDNS_AAAA);
	mu_assert_int_eq(iter.query.qclass, XDNS_IN);
	mu_assert_str_eq(iter.name, "google.com.");

	mu_assert_int_eq(xdns_iter_next(&iter), XDNS_MORE);
	mu_assert_str_eq(iter.name, "google.com.");
	mu_assert_int_eq(iter.section, XDNS_S_AN);
	mu_assert_int_eq(iter.rr.rtype, XDNS_AAAA);
	mu_assert_int_eq(iter.rr.rclass, XDNS_IN);
	mu_assert_int_eq(iter.rr.ttl, 299);
	mu_assert_str_eq(
			inet_ntop(AF_INET6, &iter.rdata.aaaa, buf, sizeof(buf)),
			"2607:f8b0:4005:808::200e");

	mu_assert_int_eq(xdns_iter_next(&iter), XDNS_DONE);
}

static void
test_a_opt(void)
{
	static const uint8_t RR[] = {
		0xd9, 0xb9, 0x81, 0x80, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
		0x06, 0x67, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x03, 0x63, 0x6f, 0x6d, 0x00,
		0x00, 0x01, 0x00, 0x01, 0xc0, 0x0c, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00,
		0x00, 0xa0, 0x00, 0x04, 0xd8, 0x3a, 0xc0, 0x0e, 0x00, 0x00, 0x29, 0x02,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};

	char buf[256];

	struct xdns dns;
	mu_assert_int_eq(xdns_load(&dns, RR, sizeof(RR)), 0);

	struct xdns_iter iter;
	xdns_iter_init(&iter, &dns);

	mu_assert_uint_eq(iter.hdr.id, 55737);
	mu_assert_uint_eq(iter.hdr.qr, 1);
	mu_assert_uint_eq(iter.hdr.opcode, XDNS_QUERY);
	mu_assert_uint_eq(iter.hdr.aa, false);
	mu_assert_uint_eq(iter.hdr.tc, false);
	mu_assert_uint_eq(iter.hdr.rd, true);
	mu_assert_uint_eq(iter.hdr.ra, true);
	mu_assert_uint_eq(iter.hdr.rcode, XDNS_NOERROR);
	mu_assert_uint_eq(iter.hdr.qdcount, 1);
	mu_assert_uint_eq(iter.hdr.ancount, 1);
	mu_assert_uint_eq(iter.hdr.nscount, 0);
	mu_assert_uint_eq(iter.hdr.arcount, 1);

	mu_assert_int_eq(xdns_iter_next(&iter), XDNS_MORE);
	mu_assert_int_eq(iter.section, XDNS_S_QD);
	mu_assert_int_eq(iter.query.qtype, XDNS_A);
	mu_assert_int_eq(iter.query.qclass, XDNS_IN);
	mu_assert_str_eq(iter.name, "google.com.");

	mu_assert_int_eq(xdns_iter_next(&iter), XDNS_MORE);
	mu_assert_str_eq(iter.name, "google.com.");
	mu_assert_int_eq(iter.section, XDNS_S_AN);
	mu_assert_int_eq(iter.rr.rtype, XDNS_A);
	mu_assert_int_eq(iter.rr.rclass, XDNS_IN);
	mu_assert_int_eq(iter.rr.ttl, 160);
	mu_assert_str_eq(
			inet_ntop(AF_INET, &iter.rdata.a, buf, sizeof(buf)),
			"216.58.192.14");

	mu_assert_int_eq(xdns_iter_next(&iter), XDNS_MORE);
	mu_assert_str_eq(iter.name, ".");
	mu_assert_int_eq(iter.section, XDNS_S_AR);
	mu_assert_int_eq(iter.rr.rtype, XDNS_OPT);
	mu_assert_int_eq(iter.opt.udpmax, 512);
	mu_assert_int_eq(iter.opt.ext_rcode, 0);
	mu_assert_int_eq(iter.opt.version, 0);
	mu_assert_int_eq(iter.opt.dof, false);
	mu_assert_int_eq(iter.opt.rdlength, 0);

	mu_assert_int_eq(xdns_iter_next(&iter), XDNS_DONE);
}

static void
test_a_short_rr(void)
{
	static const uint8_t RR[] = {
		0x48, 0xc9, 0x81, 0x80, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
		0x06, 0x67, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x03, 0x63, 0x6f, 0x6d, 0x00,
		0x00, 0x01, 0x00, 0x01, 0xc0, 0x0c, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00,
		0x00, 0xa0, 0x00, 0x04, 0xd8, 0x3a, 0xc2
	};

	struct xdns dns;
	memset(dns.buf, 0xFF, sizeof(dns.buf));
	mu_assert_int_eq(xdns_load(&dns, RR, sizeof(RR)), 0);

	struct xdns_iter iter;
	xdns_iter_init(&iter, &dns);

	mu_assert_uint_eq(iter.hdr.id, 18633);
	mu_assert_uint_eq(iter.hdr.qr, 1);
	mu_assert_uint_eq(iter.hdr.opcode, XDNS_QUERY);
	mu_assert_uint_eq(iter.hdr.aa, false);
	mu_assert_uint_eq(iter.hdr.tc, false);
	mu_assert_uint_eq(iter.hdr.rd, true);
	mu_assert_uint_eq(iter.hdr.ra, true);
	mu_assert_uint_eq(iter.hdr.rcode, XDNS_NOERROR);
	mu_assert_uint_eq(iter.hdr.qdcount, 1);
	mu_assert_uint_eq(iter.hdr.ancount, 1);
	mu_assert_uint_eq(iter.hdr.nscount, 0);
	mu_assert_uint_eq(iter.hdr.arcount, 0);

	mu_assert_int_eq(xdns_iter_next(&iter), XDNS_MORE);
	mu_assert_int_eq(iter.section, XDNS_S_QD);
	mu_assert_int_eq(iter.query.qtype, XDNS_A);
	mu_assert_int_eq(iter.query.qclass, XDNS_IN);
	mu_assert_str_eq(iter.name, "google.com.");

	mu_assert_int_eq(xdns_iter_next(&iter), -EINVAL);
}

int
main(void)
{
	mu_init("dns");
	test_a();
	test_ns();
	test_cname();
	test_soa();
	test_mx();
	test_txt();
	test_aaaa();

	test_a_opt();
	test_a_short_rr();
}

