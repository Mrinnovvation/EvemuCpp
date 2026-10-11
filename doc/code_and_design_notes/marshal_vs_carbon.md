# EVEmu's marshal vs. CCP's own

In July 2026 CCP/Fenris Creations published the Carbon engine framework. Two of
its components describe the wire format this server speaks:

| component | what it gives us | licence |
|---|---|---|
| [`carbonengine/blue`](https://github.com/carbonengine/blue) | `src/Marshal.{h,cpp}` — the real `enum PYTYPES` and reader; `src/PyRowSet.cpp` — `DBRow`, `RLEPack`/`RLEUnPack`, row layout | MIT |
| [`carbonengine/io`](https://github.com/carbonengine/io) | `src/protocol.h` — the machoNet packet header; patched CPython `socket`/`ssl` | PSF-2.0 |

This note records what that source settles. It is a reference for
`EVEMarshalOpcodes.h`, not a changelog.

## Opcodes

Every opcode this project had marked unknown or guessed is now named. The
numbering is otherwise identical, which is the headline: our marshal layer was
essentially right.

| byte | EVEmu | Carbon | note |
|---|---|---|---|
| 0x0C | `Op_PyUnused1` (commented out) | `TY_COMPLEX` | python complex; we never emit one |
| 0x0E | `Op_PyEmptyString` *"not 100% sure"* | `TY_STR_EMPTY` | confirmed correct |
| 0x17 | `Op_PyObject` | `TY_INSTANCE` | **PY27 only** — see below |
| 0x18 | `Op_Pyunk2` (commented out) | `TY_BLUEWRAPPER` | deprecated, already removed |
| 0x19 | `Op_PySubStruct` | `TY_CALLBACK` | |
| 0x1A | `Op_Pyunk3  //"load"` | `TY_PICKLE` | old-style pickle, deprecated |
| 0x1B | `Op_PySavedStreamElement` | `TY_REFERENCE` | index into the shared-object map |
| 0x1D | `Op_PyUnused2` (commented out) | `TY_COMPRESSED` | reserved, never used |
| 0x1E | `Op_PyUnused3` (commented out) | — | unassigned in Carbon too |
| 0x21 | `Op_cPicked` | `TY_PICKLER` | new-style, one pickler per stream |
| 0x22 | `Op_PyObjectEx1` | `TY_REDUCE` | `__reduce__` protocol |
| 0x23 | `Op_PyObjectEx2` *"not 100% completed"* | `TY_NEWOBJ` | `__newobj__` case of protocol 2 |
| 0x2A | `Op_PyPackedRow` *"still working on this one..."* | `TY_DBROW` | fully specified below |
| 0x2D | `Op_PackedTerminator` | `TY_MARK` | terminator for 0x22/0x23, not for packed rows |
| 0x2E | `Op_PyWStringUTF8` | `TY_UTF8_OBSOLETE` | **PY27 only** |
| 0x2F | `Op_PyVarInteger` *"variable length integer field???"* | `TY_LONG` | "a proper long" |
| 0x7D | — | `TY_SIGNATURE2` | **new**: versioned stream header |

Masks and the header byte match exactly: `PyRepSaveMask` = `TY_SHAREDFLAG` =
0x40, `PyRepOpcodeMask` = `TY_TYPEMASK` = 0x3F, `MarshalHeaderByte` =
`TY_SIGNATURE` = 0x7E. `PyRepUnknownMask` (0x80) has no counterpart, which is
consistent — `TY_SHAREDFLAG` carries the comment *"don't go above that"*, so a
set 0x80 is invalid rather than meaningful.

Two naming differences that are not disagreements: 0x0D is `TY_STR` where we say
`Op_PyBuffer`, and 0x13 is `TY_BUFFER` where we say `Op_PyLongString`. Under
Python 2 a `str` *is* a byte string, so both readings describe the same bytes.

### What a current client would not send

In Carbon, `TY_INSTANCE` (0x17) and `TY_UTF8_OBSOLETE` (0x2E) are registered only
inside `#ifdef PY27_COMPATIBILITY_MODE`, under the comment *"Handle types that
are never sent by Python 3."* Both are load-bearing here — 0x17 is our
`PyObject`, and 0x2E is how we encode every wide string. Anyone aiming at a
client newer than Crucible should expect to replace both, and to handle
`TY_SIGNATURE2` streams.

## Packed rows (0x2A / `TY_DBROW`)

From `DBRow::Read` and `DBRow::Write`:

```
0x2A
  <marshalled blue.DBRowDescriptor>
  <size> <RLE bytes>              # bare size prefix, no opcode byte
  <marshalled object> * N         # one per STR/WSTR/BYTES column, in declaration order
```

The size prefix is `WriteInteger`: one byte, or 0xFF followed by an int32 —
identical to our `PutSizeEx`/`ReadSizeEx`.

### Layout

`RowDescriptor::InitFromTypedList` assigns each column a size class via
`ColumnDescriptor::TypeSize`:

| class | width | DBTYPEs |
|---|---|---|
| 4 | 8 bytes | I8, UI8, R8, CY, FILETIME, DBTIMESTAMP |
| 3 | 4 bytes | I4, UI4, R4 |
| 2 | 2 bytes | I2, UI2 |
| 1 | 1 byte | I1, UI1 |
| 0 | 1 bit | BOOL |
| 5 | — | STR, WSTR, BYTES: marshalled separately, after the RLE blob |
| -1 | — | EMPTY: virtual, no storage |

Fields are then packed largest class first, each class in declaration order,
followed by the boolean bits, followed by one null flag per *real* column
(objects included, virtuals excluded):

```
offset = 0
for class in 4, 3, 2, 1:
    offsets[class] = offset
    offset += count[class] * width[class]
offset *= 8                       # bools and null flags are counted in bits
offsets[0] = offset ; offset += count[0]
sNull = offset      ; offset += nRealCols
dataLen = (offset + 7) / 8
```

A column's null flag is bit `sNull + columnIndex`. Our encoder and decoder both
agree with this, so null flags land in the right place.

### The one divergence found

`dataLen` is `(bits + 7) / 8`. We used
`(byteDataBits >> 3) + ((bools + nulls) >> 3) + 1`, which rounds up even when
`bools + nulls` already divides by 8 — a surplus byte in a common case. For the
`live_updates` row (five I4 columns, three WSTR, no booleans) CCP's formula gives
21 bytes and ours gave 22.

This was benign in both directions: `RLEUnPack` stops at its own `outlen` and
zero-fills the tail, so a reader ignores the extra byte and a short buffer gets
padded. It was still not the length the client computes, so
`EVEMarshal.cpp`/`EVEUnmarshal.cpp` now use the exact form.

### RLE

`RLEPack`/`RLEUnPack` encode one run per nibble, low nibble of each byte first.
With `count = nibble - 8`:

- `count >= 0` — a run of `count + 1` zero bytes
- `count < 0` — `-count` literal bytes follow

A trailing high nibble is deliberately left unread; that is how the encoder trims
trailing zeroes, and the decoder zero-fills to `dataLen` instead.

## machoNet framing

`io/src/protocol.h` — the 4-byte prefix is no longer a plain length:

```c
ceHeaderBitsMask              = 0xF0000000,
ceHeaderSizeMask              = 0x0FFFFFFF,
ceHeaderExpectPayloadOffset   = 1 << 28,  // where the OOB data ends
ceHeaderBitZlibCompressed     = 1 << 29,
ceHeaderBitSnappyCompressed   = 1 << 30,  // no longer supported
ceHeaderExtraHeaderBitsFollow = 1 << 31,  // reserved [unimplemented]
```

Crucible uses the whole 32 bits as a length. Since our packets never approach
256 MB the size field still agrees; it is the flags, the out-of-band region and
the sequence number (`recvpacketoob` returns `(packet, oob, sequence)`) that we
have no notion of.

## Verifying against a running server

`utils/dev/evemu_login.py` implements this format in Python and logs in without
the game client. It decodes the `TY_DBROW` rows in `CryptoHandshakeAck`, which
can be checked directly:

```
python3 utils/dev/evemu_login.py --user test --password test --source .
docker compose exec db mariadb -u evemu -pevemu evemu -e "select * from liveupdates"
```
