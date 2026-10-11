#!/usr/bin/env python3
"""
Minimal EVEmu protocol client: performs the Crucible login handshake without
the EVE Online game client.

Implements just enough of EVE's python-marshal wire format to drive the
sequence in src/eve-common/network/EVESession.cpp:

  server -> VersionExchangeServer   (7-tuple)
  client -> VersionExchangeClient   (6-tuple)          _HandleCommand
  client -> NetCommand_VK           (None, "VK", key)  _HandleCrypto
  client -> CryptoRequestPacket     ("placebo", {})    -> "OK CC"
  client -> CryptoChallengePacket   (chal, {creds})    -> PyInt(2) + CryptoServerHandshake
  client -> CryptoHandshakeResult   (3-tuple)          -> CryptoHandshakeAck

Usage: evemu_login.py [--host H] [--port P] --user U --password P [--source DIR]
"""

import argparse
import os
import re
import socket
import struct
import sys

# --- marshal opcodes (src/eve-common/marshal/EVEMarshalOpcodes.h) -------------
NONE, TOKEN, LONGLONG, LONG, SSHORT, BYTE = 0x01, 0x02, 0x03, 0x04, 0x05, 0x06
MINUSONE, ZEROINT, ONEINT, REAL, ZEROREAL = 0x07, 0x08, 0x09, 0x0A, 0x0B
BUFFER, EMPTYSTR, CHARSTR, SHORTSTR, STRTABLE = 0x0D, 0x0E, 0x0F, 0x10, 0x11
WSTR_UCS2, LONGSTR, TUPLE, LIST, DICT, OBJECT = 0x12, 0x13, 0x14, 0x15, 0x16, 0x17
SUBSTRUCT, SAVEDELEM, CHECKSUM = 0x19, 0x1B, 0x1C
TRUE, FALSE, PICKED, OBJEX1, OBJEX2 = 0x1F, 0x20, 0x21, 0x22, 0x23
EMPTYTUPLE, ONETUPLE, EMPTYLIST, ONELIST = 0x24, 0x25, 0x26, 0x27
EMPTYWSTR, WSTR_CHAR, PACKEDROW, SUBSTREAM = 0x28, 0x29, 0x2A, 0x2B
TWOTUPLE, PACKEDTERM, WSTR_UTF8, VARINT = 0x2C, 0x2D, 0x2E, 0x2F
OPMASK, SAVEMASK = 0x3F, 0x40
HEADER = 0x7E

# Version constants (src/eve-common/EVEVersion.h) — the server only logs
# mismatches in _VerifyVersion, but send the real values anyway.
BIRTHDAY, MACHO_VERSION, BUILD = 170472, 320, 360229
VERSION_NUMBER = 7.31
PROJECT_VERSION = "EVE-EVE-TRANQUILITY@ccp"
PROJECT_CODENAME = "EVE-EVE-TRANQUILITY"
PROJECT_REGION = "ccp"


class WStr(str):
    """Marks a value to be marshalled as PyWString rather than PyString."""


class Mark:
    """TY_MARK (0x2D): marker for dynamic lists. EVEmu calls it PackedTerminator."""

    def __repr__(self):
        return "<MARK>"


MARK_VALUE = Mark()

# DBTYPE -> (size class, struct format). Size classes and the layout algorithm
# below follow RowDescriptor::InitFromTypedList and ColumnDescriptor::TypeSize
# in carbonengine/blue src/PyRowSet.cpp. Class 0 is a bit, 1/2/3/4 are
# 1/2/4/8-byte fields, 5 means the value is marshalled separately as an object.
DBTYPE = {
    11:  (0, None, "BOOL"),
    16:  (1, "<b", "I1"),
    17:  (1, "<B", "UI1"),
    2:   (2, "<h", "I2"),
    18:  (2, "<H", "UI2"),
    3:   (3, "<i", "I4"),
    19:  (3, "<I", "UI4"),
    4:   (3, "<f", "R4"),
    20:  (4, "<q", "I8"),
    21:  (4, "<Q", "UI8"),
    5:   (4, "<d", "R8"),
    6:   (4, "<q", "CY"),
    64:  (4, "<q", "FILETIME"),
    129: (5, None, "STR"),
    130: (5, None, "WSTR"),
    128: (5, None, "BYTES"),
}


def rle_unpack(data, outlen):
    """Port of RLEUnPack (carbonengine/blue src/PyRowSet.cpp).

    Each nibble encodes one run, low nibble first. count = nibble - 8;
    count >= 0 emits count+1 zero bytes, count < 0 copies -count literals.
    A trailing high nibble is deliberately left unread, which is how the
    encoder trims trailing zeroes; the tail is zero-filled instead.
    """
    out = bytearray()
    pos, nibble, run = 0, False, 0
    while pos < len(data) and len(out) < outlen:
        if not nibble:
            run = data[pos]
            pos += 1
            count = run & 0x0F
        else:
            count = (run & 0xF0) >> 4
        nibble = not nibble
        count -= 8
        if count >= 0:
            out += b"\x00" * (count + 1)
        else:
            chunk = data[pos:pos - count]
            out += chunk
            pos += len(chunk)
    out += b"\x00" * max(0, outlen - len(out))
    return bytes(out[:outlen])


def row_layout(columns):
    """Compute the packed-row layout for [(name, dbtype), ...].

    Mirrors RowDescriptor::InitFromTypedList: fields are grouped by size class
    largest-first, then bool bits, then one null bit per real column.
    """
    sizes = [0] * 5
    n_objects = n_real = 0
    for _, dbtype in columns:
        info = DBTYPE.get(dbtype)
        if info is None:
            continue  # DBTYPE_EMPTY / unsupported: virtual, no storage
        cls = info[0]
        n_real += 1
        if cls == 5:
            n_objects += 1
        else:
            sizes[cls] += 1

    offsets = [0] * 5
    offset = 0
    for cls in (4, 3, 2, 1):
        offsets[cls] = offset
        offset += sizes[cls] * (1 << (cls - 1))
    offset *= 8                 # bools and null flags are counted in bits
    offsets[0] = offset
    offset += sizes[0]
    s_null = offset
    offset += n_real
    data_len = (offset + 7) // 8

    # Second pass: hand each column its own offset, in declaration order.
    plan, obj_index = [], 0
    for index, (name, dbtype) in enumerate(columns):
        info = DBTYPE.get(dbtype)
        if info is None:
            plan.append((name, dbtype, "virtual", None, None))
            continue
        cls, fmt, _ = info
        null_bit = s_null + index
        if cls == 5:
            plan.append((name, dbtype, "object", obj_index, null_bit))
            obj_index += 1
        elif cls == 0:
            plan.append((name, dbtype, "bool", offsets[0], null_bit))
            offsets[0] += 1
        else:
            plan.append((name, dbtype, fmt, offsets[cls], null_bit))
            offsets[cls] += 1 << (cls - 1)
    return plan, data_len, n_objects


def get_bit(data, bit):
    byte = bit >> 3
    return byte < len(data) and bool(data[byte] & (1 << (bit & 7)))


def load_string_table(source_dir):
    """Parse s_mStringTable[] out of the C++ source. Index is 1-based."""
    path = os.path.join(
        source_dir, "src/eve-common/marshal/EVEMarshalStringTable.cpp")
    try:
        with open(path, encoding="utf-8", errors="replace") as fh:
            text = fh.read()
    except OSError:
        return []
    body = text.split("s_mStringTable[]", 1)
    if len(body) < 2:
        return []
    body = body[1].split("{", 1)[1].split("};", 1)[0]
    return re.findall(r'"((?:[^"\\]|\\.)*)"', body)


# --- encoding -----------------------------------------------------------------
def put_size(n):
    return bytes([n]) if n < 0xFF else b"\xff" + struct.pack("<I", n)


def encode(val, out):
    if val is None:
        out += bytes([NONE])
    elif val is True:
        out += bytes([TRUE])
    elif val is False:
        out += bytes([FALSE])
    elif isinstance(val, WStr):
        raw = val.encode("utf-8")
        if not raw:
            out += bytes([EMPTYWSTR])
        else:
            out += bytes([WSTR_UTF8]) + put_size(len(raw)) + raw
    elif isinstance(val, int):
        if val == 0:
            out += bytes([ZEROINT])
        elif val == 1:
            out += bytes([ONEINT])
        elif val == -1:
            out += bytes([MINUSONE])
        elif 0 <= val <= 0xFF:
            out += bytes([BYTE, val])
        elif -0x8000 <= val <= 0x7FFF:
            out += bytes([SSHORT]) + struct.pack("<h", val)
        elif -0x80000000 <= val <= 0x7FFFFFFF:
            out += bytes([LONG]) + struct.pack("<i", val)
        else:
            out += bytes([LONGLONG]) + struct.pack("<q", val)
    elif isinstance(val, float):
        if val == 0.0:
            out += bytes([ZEROREAL])
        else:
            out += bytes([REAL]) + struct.pack("<d", val)
    elif isinstance(val, str):
        raw = val.encode("utf-8")
        if not raw:
            out += bytes([EMPTYSTR])
        elif len(raw) == 1:
            out += bytes([CHARSTR, raw[0]])
        else:
            # Always use LONGSTR: the server's unmarshaller accepts it for any
            # string, so we never need to emit string-table indices.
            out += bytes([LONGSTR]) + put_size(len(raw)) + raw
    elif isinstance(val, bytes):
        out += bytes([BUFFER]) + put_size(len(val)) + val
    elif isinstance(val, tuple):
        n = len(val)
        if n == 0:
            out += bytes([EMPTYTUPLE])
        elif n == 1:
            out += bytes([ONETUPLE])
        elif n == 2:
            out += bytes([TWOTUPLE])
        else:
            out += bytes([TUPLE]) + put_size(n)
        for item in val:
            encode(item, out)
    elif isinstance(val, list):
        n = len(val)
        if n == 0:
            out += bytes([EMPTYLIST])
        elif n == 1:
            out += bytes([ONELIST])
        else:
            out += bytes([LIST]) + put_size(n)
        for item in val:
            encode(item, out)
    elif isinstance(val, dict):
        out += bytes([DICT]) + put_size(len(val))
        # MarshalStream::VisitDict writes value BEFORE key.
        for key, item in val.items():
            encode(item, out)
            encode(key, out)
    else:
        raise TypeError("cannot marshal %r" % (type(val),))
    return out


def marshal(val):
    return bytes(encode(val, bytearray([HEADER]) + struct.pack("<I", 0)))


# --- decoding -----------------------------------------------------------------
class Reader:
    def __init__(self, buf, table):
        self.buf, self.pos, self.table = buf, 0, table

    def u8(self):
        v = self.buf[self.pos]
        self.pos += 1
        return v

    def take(self, n):
        v = self.buf[self.pos:self.pos + n]
        self.pos += n
        return v

    def size(self):
        n = self.u8()
        return struct.unpack("<I", self.take(4))[0] if n == 0xFF else n

    def read(self):
        raw = self.u8()
        op = raw & OPMASK
        if raw & SAVEMASK:
            pass  # object is also stored in the save map; value follows inline
        if op == NONE:
            return None
        if op == TRUE:
            return True
        if op == FALSE:
            return False
        if op == ZEROINT:
            return 0
        if op == ONEINT:
            return 1
        if op == MINUSONE:
            return -1
        if op == BYTE:
            return self.u8()
        if op == SSHORT:
            return struct.unpack("<h", self.take(2))[0]
        if op == LONG:
            return struct.unpack("<i", self.take(4))[0]
        if op == LONGLONG:
            return struct.unpack("<q", self.take(8))[0]
        if op == ZEROREAL:
            return 0.0
        if op == REAL:
            return struct.unpack("<d", self.take(8))[0]
        if op == EMPTYSTR:
            return ""
        if op == CHARSTR:
            return chr(self.u8())
        if op in (LONGSTR, SHORTSTR, TOKEN):
            return self.take(self.size()).decode("utf-8", "replace")
        if op == STRTABLE:
            idx = self.u8()
            return self.table[idx - 1] if 0 < idx <= len(self.table) \
                else "<strtable:%d>" % idx
        if op == EMPTYWSTR:
            return ""
        if op == WSTR_CHAR:
            return self.take(2).decode("utf-16-le", "replace")
        if op == WSTR_UTF8:
            return self.take(self.size()).decode("utf-8", "replace")
        if op == WSTR_UCS2:
            return self.take(self.size() * 2).decode("utf-16-le", "replace")
        if op == BUFFER:
            return self.take(self.size())
        if op == EMPTYTUPLE:
            return ()
        if op == ONETUPLE:
            return (self.read(),)
        if op == TWOTUPLE:
            return (self.read(), self.read())
        if op == TUPLE:
            return tuple(self.read() for _ in range(self.size()))
        if op == EMPTYLIST:
            return []
        if op == ONELIST:
            return [self.read()]
        if op == LIST:
            return [self.read() for _ in range(self.size())]
        if op == DICT:
            out = {}
            for _ in range(self.size()):
                value = self.read()      # value first
                out[repr_key(self.read())] = value
            return out
        if op == OBJECT:
            # TY_INSTANCE: guid + state, no marker sections (ReadObjectInstance).
            return {"__type__": self.read(), "__state__": self.read()}
        if op in (OBJEX1, OBJEX2):
            # TY_REDUCE / TY_NEWOBJ: the reduce tuple, then a MARK-terminated
            # list-append section, then a MARK-terminated dict-setitem one
            # (ReadObjectReduce / ReadObjectNewobj -> ReadObjectListIter,
            #  ReadObjectDictIter in carbonengine/blue src/Marshal.cpp).
            out = {"__type__": self.read()}
            appends = []
            while True:
                item = self.read()
                if isinstance(item, Mark):
                    break
                appends.append(item)
            items = {}
            while True:
                key = self.read()
                if isinstance(key, Mark):
                    break
                items[repr_key(key)] = self.read()
            if appends:
                out["__list__"] = appends
            if items:
                out["__dict__"] = items
            return out
        if op in (SUBSTREAM, CHECKSUM):
            inner = self.take(self.size()) if op == SUBSTREAM else None
            if inner is None:
                return "<checksummed>"
            sub = Reader(inner, self.table)
            sub.pos = 5 if inner[:1] == bytes([HEADER]) else 0
            return sub.read()
        if op == SUBSTRUCT:
            return self.read()
        if op == PACKEDROW:
            return self.read_dbrow()
        if op == PACKEDTERM:
            return MARK_VALUE
        return "<op 0x%02x>" % op

    def read_dbrow(self):
        """TY_DBROW / Op_PyPackedRow: see DBRow::Read in blue src/PyRowSet.cpp.

        descriptor, then a bare-size-prefixed RLE blob (no opcode byte), then
        one marshalled object per STR/WSTR/BYTES column in declaration order.
        """
        descriptor = self.read()
        columns = extract_columns(descriptor)
        packed = self.take(self.size())

        if columns is None:
            return {"__dbrow__": "undecodable descriptor",
                    "__descriptor__": descriptor, "__packed__": packed}

        plan, data_len, n_objects = row_layout(columns)
        data = rle_unpack(packed, data_len)
        objects = [self.read() for _ in range(n_objects)]

        row = {}
        for name, dbtype, kind, where, null_bit in plan:
            if kind == "virtual":
                row[name] = None
            elif null_bit is not None and get_bit(data, null_bit):
                row[name] = None
            elif kind == "object":
                row[name] = objects[where] if where < len(objects) else None
            elif kind == "bool":
                row[name] = get_bit(data, where)
            else:
                width = struct.calcsize(kind)
                chunk = data[where:where + width]
                if len(chunk) < width:
                    row[name] = None
                else:
                    value = struct.unpack(kind, chunk)[0]
                    row[name] = value / 10000.0 if dbtype == 6 else value
        return row


def extract_columns(descriptor):
    """Pull [(name, dbtype), ...] out of a marshalled blue.DBRowDescriptor.

    The decoder represents an object as {"__type__": ..., "__args__": ...};
    for a descriptor __type__ is ('blue.DBRowDescriptor', ((name, type), ...)).
    """
    if not isinstance(descriptor, dict):
        return None
    spec = descriptor.get("__type__")
    if not (isinstance(spec, tuple) and len(spec) == 2):
        return None
    name, payload = spec
    if name != "blue.DBRowDescriptor":
        return None
    if isinstance(payload, tuple) and len(payload) == 1:
        payload = payload[0]
    if not isinstance(payload, (tuple, list)):
        return None
    columns = []
    for entry in payload:
        if isinstance(entry, (tuple, list)) and len(entry) >= 2:
            columns.append((entry[0], entry[1]))
        else:
            return None
    return columns


def repr_key(key):
    return key if isinstance(key, str) else repr(key)


def unmarshal(buf, table):
    reader = Reader(buf, table)
    if buf[:1] == bytes([HEADER]):
        reader.pos = 5  # header byte + uint32 mapcount
    return reader.read()


# --- framed transport ---------------------------------------------------------
class Conn:
    def __init__(self, host, port, table, timeout=10):
        self.sock = socket.create_connection((host, port), timeout=timeout)
        self.sock.settimeout(timeout)
        self.table = table

    def send(self, val):
        payload = marshal(val)
        self.sock.sendall(struct.pack("<I", len(payload)) + payload)

    def _recv_exact(self, n):
        buf = b""
        while len(buf) < n:
            chunk = self.sock.recv(n - len(buf))
            if not chunk:
                raise EOFError("server closed the connection")
            buf += chunk
        return buf

    def recv(self):
        length = struct.unpack("<I", self._recv_exact(4))[0]
        return unmarshal(self._recv_exact(length), self.table)

    def close(self):
        self.sock.close()


def brief(val, limit=160):
    text = repr(val)
    return text if len(text) <= limit else text[:limit] + "..."


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=26000)
    ap.add_argument("--user", required=True)
    ap.add_argument("--password", required=True)
    ap.add_argument("--source", default=os.getcwd(),
                    help="evemu_Crucible checkout, for the marshal string table")
    args = ap.parse_args()

    table = load_string_table(args.source)
    print("string table entries: %d" % len(table))

    conn = Conn(args.host, args.port, table)
    step = 0

    def ok(msg):
        nonlocal step
        step += 1
        print("  [%d] %s" % (step, msg))

    # 1. server's version exchange
    server_version = conn.recv()
    ok("server VersionExchange: %s" % brief(server_version))
    if not (isinstance(server_version, tuple) and len(server_version) == 7):
        print("unexpected version exchange; aborting")
        return 1

    # 2. our version exchange (same tuple minus the trailing update_info None)
    conn.send((BIRTHDAY, MACHO_VERSION, 0, VERSION_NUMBER, BUILD,
               PROJECT_VERSION))
    ok("sent VersionExchangeClient")

    # 3. VK command -> moves the session to the crypto stage
    conn.send((None, "VK", ""))
    ok("sent NetCommand_VK")

    # 4. placebo crypto request -> expect "OK CC"
    conn.send(("placebo", {}))
    reply = conn.recv()
    ok("crypto reply: %s" % brief(reply))
    if reply != "OK CC":
        print("server rejected the crypto request; aborting")
        return 1

    # 5. credentials
    conn.send((
        "",
        {
            "macho_version": MACHO_VERSION,
            "boot_version": VERSION_NUMBER,
            "boot_build": BUILD,
            "boot_codename": PROJECT_CODENAME,
            "boot_region": PROJECT_REGION,
            "user_name": WStr(args.user),
            "user_password": WStr(args.password),
            "user_password_hash": None,
            "user_languageid": WStr("EN"),
            "user_affiliateid": 0,
        },
    ))
    ok("sent credentials for %r" % args.user)

    pass_version = conn.recv()
    ok("passwordVersion: %s" % brief(pass_version))

    handshake = conn.recv()
    if isinstance(handshake, tuple) and len(handshake) == 4 \
            and isinstance(handshake[3], dict):
        info = handshake[3]
        ok("LOGIN ACCEPTED - CryptoServerHandshake received")
        for key in ("macho_version", "boot_build", "boot_codename",
                    "cluster_usercount", "proxy_nodeid",
                    "user_logonqueueposition", "challenge_responsehash"):
            if key in info:
                print("        %-24s %s" % (key, info[key]))
            cfg = info.get("config_vals")
            if key == "challenge_responsehash" and isinstance(cfg, dict):
                print("        %-24s %s" % ("imageserverurl",
                                            cfg.get("imageserverurl")))
    else:
        print("  login FAILED: %s" % brief(handshake, 400))
        return 1

    # 6. complete the handshake -> expect CryptoHandshakeAck
    conn.send(("55087", "", None))
    ok("sent CryptoHandshakeResult")
    ack = conn.recv()
    ok("CryptoHandshakeAck received")
    if isinstance(ack, dict):
        for key, value in ack.items():
            print("        %-16s %s" % (key, brief(value, 300)))
        rows = [v for v in ack.values() if isinstance(v, list)]
        for row in (rows[0] if rows else []):
            if isinstance(row, dict):
                print("\n    decoded DBRow (%d columns):" % len(row))
                for col, value in row.items():
                    print("        %-18s %s" % (col, brief(value, 90)))

    conn.close()
    print("\nhandshake complete - authenticated session established")
    return 0


if __name__ == "__main__":
    sys.exit(main())
