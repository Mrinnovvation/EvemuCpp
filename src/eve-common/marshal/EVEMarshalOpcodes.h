/*
    ------------------------------------------------------------------------------------
    LICENSE:
    ------------------------------------------------------------------------------------
    This file is part of EVEmu: EVE Online Server Emulator
    Copyright 2006 - 2021 The EVEmu Team
    For the latest information visit https://evemu.dev
    ------------------------------------------------------------------------------------
    This program is free software; you can redistribute it and/or modify it under
    the terms of the GNU Lesser General Public License as published by the Free Software
    Foundation; either version 2 of the License, or (at your option) any later
    version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License along with
    this program; if not, write to the Free Software Foundation, Inc., 59 Temple
    Place - Suite 330, Boston, MA 02111-1307, USA, or go to
    http://www.gnu.org/copyleft/lesser.txt.
    ------------------------------------------------------------------------------------
    Author:        Zhur
*/

// no clue where these come from, how they were found, or how i can implement the unfinished ones....


#ifndef EVE_MARSHAL_OPCODES_H
#define EVE_MARSHAL_OPCODES_H

/*  The 'TY_*' names in the comments below are CCP's own, from enum PYTYPES in
 *  src/Marshal.h of the Carbon engine's 'blue' component, which CCP/Fenris
 *  Creations published under the MIT license in 2026:
 *      https://github.com/carbonengine/blue
 *  They are recorded here because they settle every opcode this table used to
 *  mark as unknown or guessed.  The Op_* names are left untouched so that
 *  nothing else in the tree has to change.
 *
 *  Three notes that matter for any attempt at a newer client than Crucible:
 *    - Op_PyObject (0x17/TY_INSTANCE) and Op_PyWStringUTF8 (0x2E) are compiled
 *      into Carbon only under PY27_COMPATIBILITY_MODE, over the comment
 *      "Handle types that are never sent by Python 3."  A current client will
 *      not send either one.
 *    - TY_SIGNATURE2 (0x7D) is an alternative stream header, followed by a
 *      version byte.  We only know MarshalHeaderByte (0x7E), which is the
 *      version-0 form, and several readers branch on that version.
 *    - 0x0D/0x13 are str/bytes in CCP's table and buffer/long-string in ours;
 *      the two coincide under Python 2, where str is a byte string.
 */
enum PyRepOpcode
{
    Op_PyNone                = 0x01,    // TY_NONE
    Op_PyToken                = 0x02,    // TY_GLOBAL      a global object, by name
    Op_PyLongLong            = 0x03,    // TY_INT64
    Op_PyLong                = 0x04,    // TY_INT32
    Op_PySignedShort        = 0x05,    // TY_INT16
    Op_PyByte                = 0x06,    // TY_INT8
    Op_PyMinusOne            = 0x07,    // TY_INT_N1
    Op_PyZeroInteger        = 0x08,    // TY_INT_0
    Op_PyOneInteger            = 0x09,    // TY_INT_1
    Op_PyReal                = 0x0A,    // TY_FLOAT
    Op_PyZeroReal            = 0x0B,    // TY_FLOAT_0
    //                        = 0x0C,    // TY_COMPLEX     a python complex number; we never emit one
    Op_PyBuffer                = 0x0D,    // TY_STR
    Op_PyEmptyString        = 0x0E,    // TY_STR_EMPTY
    Op_PyCharString            = 0x0F,    // TY_STR_CHAR    a single character string
    Op_PyShortString        = 0x10,    // TY_STR_SHORT   string limited to 255 chars
    Op_PyStringTableItem    = 0x11,    // TY_STR_TABLE
    Op_PyWStringUCS2        = 0x12,    // TY_UNICODE
    Op_PyLongString            = 0x13,    // TY_BUFFER      string with length extension support
    Op_PyTuple                = 0x14,    // TY_TUPLE
    Op_PyList                = 0x15,    // TY_LIST
    Op_PyDict                = 0x16,    // TY_DICT        written value-before-key
    Op_PyObject                = 0x17,    // TY_INSTANCE    PY27 only; not sent by a Python 3 client
    //                        = 0x18,    // TY_BLUEWRAPPER deprecated, and already gone in Carbon
    Op_PySubStruct            = 0x19,    // TY_CALLBACK    another stream nested in the current stream, length implied by marshaling a single element.
    //                        = 0x1A,    // TY_PICKLE      old-style pickle fallback; deprecated
    Op_PySavedStreamElement = 0x1B,    // TY_REFERENCE   index into the stream's shared-object map
    Op_PyChecksumedStream    = 0x1C,    // TY_CRC_CHECK
    //                        = 0x1D,    // TY_COMPRESSED  reserved but unused
    //                        = 0x1E,    //                unassigned in Carbon too
    Op_PyTrue                = 0x1F,    // TY_TRUE
    Op_PyFalse                = 0x20,    // TY_FALSE
    Op_cPicked                = 0x21,    // TY_PICKLER     new-style pickle, one pickler per stream
    Op_PyObjectEx1            = 0x22,    // TY_REDUCE      __reduce__ protocol
    Op_PyObjectEx2            = 0x23,    // TY_NEWOBJ      the __newobj__ case of __reduce__ protocol 2
    Op_PyEmptyTuple            = 0x24,    // TY_TUPLE0
    Op_PyOneTuple            = 0x25,    // TY_TUPLE1
    Op_PyEmptyList            = 0x26,    // TY_LIST0
    Op_PyOneList            = 0x27,    // TY_LIST1
    Op_PyEmptyWString       = 0x28,    // TY_UNICODE_0
    Op_PyWStringUCS2Char    = 0x29,    // TY_UNICODE_1   a single character string
    Op_PyPackedRow            = 0x2A,    // TY_DBROW       descriptor, RLE'd fixed fields, then one object per str column
    Op_PySubStream            = 0x2B,    // TY_WSTREAM
    Op_PyTwoTuple            = 0x2C,    // TY_TUPLE2
    Op_PackedTerminator        = 0x2D,    // TY_MARK        terminates the append/setitem sections of 0x22 and 0x23
    Op_PyWStringUTF8        = 0x2E,    // TY_UTF8_OBSOLETE  PY27 only; retired by the Python 3 upgrade
    Op_PyVarInteger            = 0x2F,    // TY_LONG        an arbitrary-precision long, little-endian bytes
    PyRepOpcodeMask            = 0x3F    // TY_TYPEMASK

    //  seen these on main server.
    // E Unmarshal: Invalid stream received (header byte ...)
    //                          = 0x5
    //                          = 0xE
    //                          = 0xCC
    //  None of the three is a stream header in CCP's table either -- the only
    //  two are 0x7E and 0x7D -- so these were almost certainly a desynchronised
    //  reader landing mid-stream rather than an opcode we have yet to identify.
};    //6 bits

static const uint8 PyRepSaveMask = 0x40;    // TY_SHAREDFLAG
static const uint8 PyRepUnknownMask = 0x80;
static const uint8 MarshalHeaderByte = 0x7E;    //'~'  TY_SIGNATURE, the version-0 stream
/*  TY_SIGNATURE2: the versioned stream header, followed by a single version
 *  byte.  Crucible never sends it and we do not read it, but a stream that
 *  starts with this byte is marshal data, not a malformed 0x7E stream.  */
static const uint8 MarshalHeaderByteVersioned = 0x7D;

#endif//EVE_MARSHAL_OPCODES_H





