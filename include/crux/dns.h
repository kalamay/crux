#ifndef CRUX_DNS_H
#define CRUX_DNS_H

#include <stdint.h>
#include <stdbool.h>
#include <netinet/in.h>

#define XDNS_MAX_LABEL 63
#define XDNS_MAX_NAME 255
#define XDNS_MAX_UDP 512
#define XDNS_MAX_OPT_UDP 4096


#define XX(name, code, desc) XDNS_##name = code,

#define XDNS_SECTION_MAP(XX) \
	XX(S_QD,    0, "question") \
	XX(S_AN,    1, "answer") \
	XX(S_NS,    2, "authority") \
	XX(S_AR,    3, "additional") \

enum xdns_section {
	XDNS_SECTION_MAP(XX)
};

#define XDNS_TYPE_MAP(XX) \
	XX(A,              1, "IPv4 host address") \
	XX(NS,             2, "authoritative server") \
	XX(CNAME,          5, "canonical name") \
	XX(SOA,            6, "start of authority zone") \
	XX(PTR,           12, "domain name pointer") \
	XX(MX,            15, "mail routing information") \
	XX(TXT,           16, "one or more text strings") \
	XX(AAAA,          28, "IPv6 Address") \
	XX(SRV,           33, "server selection") \
	XX(OPT,           41, "EDNS0 option") \
	XX(ANY,          255, "wildcard match") \

enum xdns_type {
	XDNS_TYPE_MAP(XX)
};

#define XDNS_TYPE_ALT_MAP(XX) \
	XX(MD,             3, "mail destination") \
	XX(MF,             4, "mail forwarder") \
	XX(MB,             7, "mailbox domain name") \
	XX(MG,             8, "mail group member") \
	XX(MR,             9, "mail rename name") \
	XX(NULL,          10, "null resource record") \
	XX(WKS,           11, "well known service") \
	XX(HINFO,         13, "host information") \
	XX(MINFO,         14, "mailbox information") \
	XX(RP,            17, "responsible person") \
	XX(AFSDB,         18, "AFS cell database") \
	XX(X25,           19, "X_25 calling address") \
	XX(ISDN,          20, "ISDN calling address") \
	XX(RT,            21, "router") \
	XX(NSAP,          22, "NSAP address") \
	XX(NSAP_PTR,      23, "reverse NSAP lookup") \
	XX(SIG,           24, "security signature") \
	XX(KEY,           25, "security key") \
	XX(PX,            26, "X.400 mail mapping") \
	XX(GPOS,          27, "geographical position") \
	XX(LOC,           29, "location information") \
	XX(NXT,           30, "next domain") \
	XX(EID,           31, "endpoint identifier") \
	XX(NIMLOC,        32, "nimrod locator") \
	XX(ATMA,          34, "ATM address") \
	XX(NAPTR,         35, "naming authority pointer") \
	XX(KX,            36, "key exchange") \
	XX(CERT,          37, "certification record") \
	XX(A6,            38, "IPv6 address") \
	XX(DNAME,         39, "IPv6 non-terminal DNAME") \
	XX(SINK,          40, "kitchen sink") \
	XX(APL,           42, "address prefix list") \
	XX(DS,            43, "delegation signer") \
	XX(SSHFP,         44, "SSH key fingerprint") \
	XX(IPSECKEY,      45, "IPSECKEY") \
	XX(RRSIG,         46, "RRSIG") \
	XX(NSEC,          47, "denial of existence") \
	XX(DNSKEY,        48, "DNSKEY") \
	XX(DHCID,         49, "DHCP client identifier") \
	XX(NSEC3,         50, "hashed authenticated denial of existence") \
	XX(NSEC3PARAM,    51, "hashed authenticated denial of existence") \
	XX(TLSA,          52, "TLSA certificate association") \
	XX(HIP,           55, "host identity protocol") \
	XX(CDNSKEY,       60, "child DNSKEY") \
	XX(SPF,           99, "sender policy framework for email") \
	XX(UINFO,        100, "IANA reserved") \
	XX(UID,          101, "IANA reserved") \
	XX(GID,          102, "IANA reserved") \
	XX(UNSPEC,       103, "IANA reserved") \
	XX(TKEY,         249, "transaction key") \
	XX(TSIG,         250, "transaction signature") \
	XX(IXFR,         251, "incremental zone transfer") \
	XX(AXFR,         252, "transfer zone of authority") \
	XX(MAILB,        253, "transfer mailbox records") \
	XX(MAILA,        254, "transfer mail agent records") \
	XX(URI,          256, "uniform resource identifier") \
	XX(CAA,          257, "certification authority authorization") \
	XX(TA,         32768, "DNSSEC trust authorities") \
	XX(DLV,        32769, "DNSSEC lookaside validation") \

enum xdns_type_alt {
	XDNS_TYPE_ALT_MAP(XX)
};

#define XDNS_CLASS_MAP(XX) \
	XX(IN,      1, "Internet") \
	XX(CH,      3, "CHAOS") \
	XX(HS,      4, "Hesiod") \

enum xdns_class {
	XDNS_CLASS_MAP(XX)
};

#define XDNS_OPCODE_MAP(XX) \
	XX(QUERY,   0, "standard query") \
	XX(IQUERY,  1, "inverse query") \
	XX(STATUS,  2, "server status request") \

enum xdns_opcode {
	XDNS_OPCODE_MAP(XX)
};

#define XDNS_RCODE_MAP(XX) \
	XX(NOERROR,  0, "no error condition") \
	XX(FORMERR,  1, "server was unable to interpret the query") \
	XX(SERVFAIL, 2, "server was unable to process the query") \
	XX(NXDOMAIN, 3, "domain referenced in query does not exist") \
	XX(NOTIMP,   4, "server does not support the request type") \
	XX(REFUSED,  5, "server is refusing to perform the operation") \

enum xdns_rcode {
	XDNS_RCODE_MAP(XX)
};

#undef XX

#pragma pack(push, 1)

/**
 *    0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *  |                      ID                       |
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *  |QR|   OPCODE  |AA|TC|RD|RA|   Z    |   RCODE   |
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *  |                    QDCOUNT                    |
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *  |                    ANCOUNT                    |
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *  |                    NSCOUNT                    |
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *  |                    ARCOUNT                    |
 *  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 */
struct xdns_header {
	uint16_t id;                 /** message identifier */
#if defined(BYTE_ORDER) && BYTE_ORDER == BIG_ENDIAN
	unsigned qr:1;               /** query (0) or response (1) */
	enum xdns_opcode opcode:4; /** kind of query */
	bool aa:1;                   /** is response an authoritative answer */
	bool tc:1;                   /** is response message truncated */
	bool rd:1;                   /** is recursion desired for query */
	bool ra:1;                   /** is recursion available indicated in response */
	unsigned z:3;                /** (unused) */
	enum xdns_rcode rcode:4;   /** response code */
#else
	bool rd:1;                   /** is recursion desired for query */
	bool tc:1;                   /** is response message truncated */
	bool aa:1;                   /** is response an authoritative answer */
	enum xdns_opcode opcode:4; /** kind of query */
	unsigned qr:1;               /** query (0) or response (1) */
	enum xdns_rcode rcode:4;   /** response code */
	unsigned z:3;                /** (unused) */
	bool ra:1;                   /** is recursion available indicated in response */
#endif
	union {
		struct {
			uint16_t qdcount;    /** entries in the question section */
			uint16_t ancount;    /** resource records in the answer section */
			uint16_t nscount;    /** name server resource records in the authority records section */
			uint16_t arcount;    /** resource records in the additional records section */
		};
		uint16_t counts[4];
	};
};

/**
 *   0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * |                                               |
 * /                    QNAME                      /
 * /                                               /
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * |                    QTYPE                      |
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * |                    QCLASS                     |
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 */
struct xdns_query {
	// char qname[...]
	enum xdns_type qtype:16;   /** type of the query */
	enum xdns_class qclass:16; /** class of the query */
};

/**
 *   0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * |                                               |
 * /                                               /
 * /                      NAME                     /
 * |                                               |
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * |                      TYPE                     |
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * |                     CLASS                     |
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * |                      TTL                      |
 * |                                               |
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * |                   RDLENGTH                    |
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--|
 * /                     RDATA                     /
 * /                                               /
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 */
struct xdns_rr {
	// char name[...]
	enum xdns_type rtype:16;   /** type of the resource */
	enum xdns_class rclass:16; /** class of the resource */
	int32_t ttl;                 /** time interval that the resource record may be cached */
	uint16_t rdlength;           /** length in octets of the RDATA field */
};

/**
 *   0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
 *                                              +--+
 *                                              | 0|
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * |                   TYPE (41)                   |
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * |                    UDP MAX                    |
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * |     EXTENDED-RCODE    |        VERSION        |
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * |DO|                                            |
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * |                   RDLENGTH                    |
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--|
 * /                     RDATA                     /
 * /                                               /
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 */
struct xdns_opt {
	enum xdns_type rtype:16;   /** type of the resource */
	uint16_t udpmax;             /** maximum udp size */
	uint8_t ext_rcode;           /** upper 8 bits of extended 12-bit RCODE */
	uint8_t version;             /** version of the opt record */
	bool dof:1;                  /** DNSSEC OK flag */
	unsigned z:15;               /** (unused) */
	uint16_t rdlength;           /** length in octets of the RDATA field */
};

#define XDNS_OPT_MAKE(udp, rdlen) ((struct xdns_opt){ \
	.rtype = XDNS_OPT,                                  \
	.udpmax = udp,                                        \
	.ext_rcode = 0,                                       \
	.version = 0,                                         \
	.dof = 0,                                             \
	.z = 0,                                               \
	.rdlength = rdlen                                     \
})

extern const struct xdns_opt XDNS_OPT_DEFAULT;

#pragma pack(pop)

/**
 *   0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * /                     MNAME                     /
 * /                                               /
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * /                     RNAME                     /
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * |                    SERIAL                     |
 * |                                               |
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * |                    REFRESH                    |
 * |                                               |
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * |                     RETRY                     |
 * |                                               |
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * |                    EXPIRE                     |
 * |                                               |
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * |                    MINIMUM                    |
 * |                                               |
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 */
struct xdns_soa {
	char mname[XDNS_MAX_NAME+1];
	char rname[XDNS_MAX_NAME+1];
	uint32_t serial;
	uint32_t refresh;
	uint32_t retry;
	uint32_t expire;
	uint32_t minimum;
};

/**
 *   0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * |                  PREFERENCE                   |
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * |                                               |
 * /                     HOST                      /
 * /                                               /
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 */
struct xdns_mx {
	uint16_t preference;
	char host[XDNS_MAX_NAME+1];
};

struct xdns_txt {
	const void *data;
	size_t length;
};

/**
 *   0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * |                   PRIORITY                    |
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * |                    WEIGHT                     |
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * |                     PORT                      |
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--|
 * |                                               |
 * /                    TARGET                     /
 * /                                               /
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 */
struct xdns_srv {
	uint16_t priority;
	uint16_t weight;
	uint16_t port;
	char target[XDNS_MAX_NAME+1];
};

struct xdns {
	union {
		uint8_t buf[XDNS_MAX_OPT_UDP];
		struct xdns_header hdr;
	};
	size_t len;
};

union xdns_rdata {
	struct in_addr a;
	char ns[XDNS_MAX_NAME+1];
	char cname[XDNS_MAX_NAME+1];
	struct xdns_soa soa;
	char ptr[XDNS_MAX_NAME+1];
	struct xdns_mx mx;
	struct xdns_txt txt;
	struct in6_addr aaaa;
	struct xdns_srv srv;
};

struct xdns_iter {
	const struct xdns *p;
	struct xdns_header hdr;
	size_t pos;
	enum xdns_section section;
	uint16_t at;
	union {
		struct xdns_query query;
		struct {
			union {
				struct xdns_rr rr;
				struct xdns_opt opt;
			};
			union xdns_rdata rdata;
		};
	};
	char name[XDNS_MAX_NAME+1];
};

extern void
xdns_init(struct xdns *p, uint16_t id);

extern int
xdns_load(struct xdns *p, const void *packet, size_t len);

extern void
xdns_final(struct xdns *p);

extern ssize_t
xdns_add_query(struct xdns *p, const char *host, enum xdns_type type);

extern ssize_t
xdns_add_rr(struct xdns *p, const char *name,
		struct xdns_rr rr, const void *rdata);

extern ssize_t 
xdns_add_opt(struct xdns *p,
		struct xdns_opt opt, const void *rdata);

extern ssize_t
xdns_send(const struct xdns *p, int s, struct sockaddr *addr, socklen_t len);

extern ssize_t
xdns_recv(struct xdns *p, int s, struct sockaddr *addr, socklen_t *len);

extern void
xdns_iter_init(struct xdns_iter *i, const struct xdns *p);

#define XDNS_MORE 0
#define XDNS_DONE 1

extern int
xdns_iter_next(struct xdns_iter *i);

#endif

