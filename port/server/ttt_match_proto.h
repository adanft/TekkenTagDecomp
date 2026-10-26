/*
 * The matchmaking protocol (T46, docs/port/matchmaking.md): what the game (port/src/plat_net.c) and the rendezvous
 * server (port/server/ttt-server.c) send each other over UDP. Shared by both so the two cannot drift apart.
 *
 * Every message starts with a 12-byte header, little-endian:
 *   0  u32  magic MS_MAGIC ("TTMS")
 *   4  u8   type (MS_REQ_* from a game, MS_S_* from the server)
 *   5  u8   MS_VERSION
 *   6  u16  0
 *   8  u32  cookie: a random number the game picks once per run; the server keys a game by (address, cookie)
 *
 * A game's requests (MS_REQ_CREATE, MS_REQ_JOIN, MS_REQ_QUICK, MS_REQ_LEAVE) are MS_REQ_SIZE bytes, zero-padded:
 *   12 u32  the netplay protocol version (plat_net.c's PROTO)
 *   16 u64  the build id (Port_StateBuildId): only equal builds are paired
 *   24 ep   the game's own (local) address, for two players behind the same NAT
 *   44 u8[8] the room code (MS_REQ_JOIN), upper case, NUL-padded
 *   52 u8[32] the player's name, UTF-8, NUL-padded
 * MS_REQ_MANIFEST is MS_MANIFEST_REQ_SIZE bytes (all zero after the header): no answer is ever larger than the
 * request it answers, so the server cannot be used to amplify a forged sender's traffic.
 *
 * The server's answers:
 *   MS_S_WAIT      12 u8[8] room code ("" in the quick-match queue), 20 u32 place in the queue, 24 ep the game's
 *                  address as the server sees it (its public endpoint)
 *   MS_S_MATCH     12 u8 role (0: host, pad 1; 1: joining, pad 2), 13 u8 relay available, 16 u32 the session id,
 *                  20 ep the peer's public endpoint, 40 ep the peer's local one, 60 u8[32] the peer's name,
 *                  92 u8[8] the room code
 *   MS_S_ERROR     12 u32 MS_ERR_*, 16 u8[64] a reason in English, NUL-padded
 *   MS_S_MANIFEST  12 u16 length, 14 the signed update manifest (docs/port/matchmaking.md)
 *
 * An endpoint (ep, 20 bytes): u8 family (0 none, 4 IPv4, 6 IPv6), u8 0, u16 port, u8[16] address (IPv4 in the first
 * four bytes, network order).
 *
 * Relay: once two games are paired, any datagram from one of them that does not start with MS_MAGIC is sent on to
 * the other one unchanged (the game's own packets, plat_net.c's "TTN1"), within a rate limit.
 */
#ifndef TTT_MATCH_PROTO_H
#define TTT_MATCH_PROTO_H

#define MS_MAGIC 0x534D5454u /* "TTMS" */
#define MS_VERSION 1
#define MS_HDR 12

enum {
    MS_REQ_CREATE = 1, /* a new room: the answer is MS_S_WAIT with its code, then MS_S_MATCH once someone joins */
    MS_REQ_JOIN = 2,   /* the room of that code */
    MS_REQ_QUICK = 3,  /* the quick-match queue: paired with the next game of the same build */
    MS_REQ_LEAVE = 4,  /* forget this game (its room, its place in the queue, its pair) */
    MS_REQ_MANIFEST = 5,
    MS_S_WAIT = 0x10,
    MS_S_MATCH = 0x11,
    MS_S_ERROR = 0x12,
    MS_S_MANIFEST = 0x13,
};

enum {
    MS_ERR_NO_ROOM = 1,   /* no room has that code (or it expired) */
    MS_ERR_ROOM_FULL = 2, /* (not sent: a room is paired at once) */
    MS_ERR_VERSION = 3,   /* the room's game runs another build or protocol */
    MS_ERR_FULL = 4,      /* the server has no room for another game */
    MS_ERR_BAD = 5,       /* a malformed request or another MS_VERSION */
    MS_ERR_NO_MANIFEST = 6,
};

#define MS_REQ_SIZE 160
#define MS_MANIFEST_REQ_SIZE 1232
#define MS_MANIFEST_MAX (MS_MANIFEST_REQ_SIZE - MS_HDR - 2)
#define MS_EP 20
#define MS_NAME 32
#define MS_CODE 8
#define MS_CODE_LEN 6 /* the codes the server makes: 6 of MS_CODE_CHARS */
#define MS_CODE_CHARS "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"

/* request fields */
#define MS_RQ_PROTO 12
#define MS_RQ_BUILD 16
#define MS_RQ_LOCAL 24
#define MS_RQ_CODE 44
#define MS_RQ_NAME 52
/* MS_S_WAIT */
#define MS_W_CODE 12
#define MS_W_PLACE 20
#define MS_W_PUBLIC 24
#define MS_W_SIZE 44
/* MS_S_MATCH */
#define MS_M_ROLE 12
#define MS_M_RELAY 13
#define MS_M_SESSION 16
#define MS_M_PUBLIC 20
#define MS_M_LOCAL 40
#define MS_M_NAME 60
#define MS_M_CODE 92
#define MS_M_SIZE 100
/* MS_S_ERROR */
#define MS_E_CODE 12
#define MS_E_TEXT 16
#define MS_E_SIZE 80

#endif
