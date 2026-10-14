/*
 * Copyright 2020 Rémi Bernon for CodeWeavers
 * Copyright 2014 Aric Stewart for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#if 0
#pragma makedep unix
#endif

#include <stdarg.h>
#include <stdlib.h>

#include "windef.h"
#include "winbase.h"
#include "winnls.h"

#include "wine/debug.h"

#include "ntgdi_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(font);

#define MS_OTTO_TAG MS_MAKE_TAG('O','T','T','O')
#define MS_HEAD_TAG MS_MAKE_TAG('h','e','a','d')
#define MS_HHEA_TAG MS_MAKE_TAG('h','h','e','a')
#define MS_OS_2_TAG MS_MAKE_TAG('O','S','/','2')
#define MS_EBSC_TAG MS_MAKE_TAG('E','B','S','C')
#define MS_EBDT_TAG MS_MAKE_TAG('E','B','D','T')
#define MS_CBDT_TAG MS_MAKE_TAG('C','B','D','T')
#define MS_NAME_TAG MS_MAKE_TAG('n','a','m','e')
#define MS_CFF__TAG MS_MAKE_TAG('C','F','F',' ')
#define MS_CFF2_TAG MS_MAKE_TAG('C','F','F','2')
#define MS_FVAR_TAG MS_MAKE_TAG('f','v','a','r')
#define MS_GLYF_TAG MS_MAKE_TAG('g','l','y','f')
#define MS_GVAR_TAG MS_MAKE_TAG('g','v','a','r')
#define MS_STAT_TAG MS_MAKE_TAG('S','T','A','T')
#define MS_WGHT_TAG MS_MAKE_TAG('w','g','h','t')
#define MS_ITAL_TAG MS_MAKE_TAG('i','t','a','l')
#define MS_SLNT_TAG MS_MAKE_TAG('s','l','n','t')

/* limits on the variable fonts whose named instances are exposed, and on the STAT tables naming them */
#define MAX_VAR_AXES      64
#define MAX_VAR_INSTANCES 256
#define MAX_STAT_AXES     32
#define MAX_STAT_VALUES   1024

#ifdef WORDS_BIGENDIAN
#define GET_BE_WORD(x) (x)
#define GET_BE_DWORD(x) (x)
#else
#define GET_BE_WORD(x)  RtlUshortByteSwap(x)
#define GET_BE_DWORD(x) RtlUlongByteSwap(x)
#endif

#pragma pack(push,2)
struct ttc_header_v1
{
    CHAR TTCTag[4];
    DWORD Version;
    DWORD numFonts;
    DWORD OffsetTable[1];
};

struct ttc_sfnt_v1
{
    DWORD version;
    WORD numTables;
    WORD searchRange;
    WORD entrySelector;
    WORD rangeShift;
};

struct tt_tablerecord
{
    DWORD tag;
    DWORD checkSum;
    DWORD offset;
    DWORD length;
};

struct tt_os2_v1
{
    USHORT version;
    SHORT xAvgCharWidth;
    USHORT usWeightClass;
    USHORT usWidthClass;
    SHORT fsType;
    SHORT ySubscriptXSize;
    SHORT ySubscriptYSize;
    SHORT ySubscriptXOffset;
    SHORT ySubscriptYOffset;
    SHORT ySuperscriptXSize;
    SHORT ySuperscriptYSize;
    SHORT ySuperscriptXOffset;
    SHORT ySuperscriptYOffset;
    SHORT yStrikeoutSize;
    SHORT yStrikeoutPosition;
    SHORT sFamilyClass;
    PANOSE panose;
    ULONG ulUnicodeRange1;
    ULONG ulUnicodeRange2;
    ULONG ulUnicodeRange3;
    ULONG ulUnicodeRange4;
    CHAR achVendID[4];
    USHORT fsSelection;
    USHORT usFirstCharIndex;
    USHORT usLastCharIndex;
    /* According to the Apple spec, original version didn't have the below fields,
     * version numbers were taken from the OpenType spec.
     */
    /* version 0 (TrueType 1.5) */
    USHORT sTypoAscender;
    USHORT sTypoDescender;
    USHORT sTypoLineGap;
    USHORT usWinAscent;
    USHORT usWinDescent;
    /* version 1 (TrueType 1.66) */
    ULONG ulCodePageRange1;
    ULONG ulCodePageRange2;
};

struct tt_namerecord
{
    WORD platformID;
    WORD encodingID;
    WORD languageID;
    WORD nameID;
    WORD length;
    WORD offset;
};

struct tt_name_v0
{
    WORD format;
    WORD count;
    WORD stringOffset;
    struct tt_namerecord nameRecord[1];
};

struct tt_head
{
    USHORT majorVersion;
    USHORT minorVersion;
    ULONG revision;
    ULONG checksumadj;
    ULONG magic;
    USHORT flags;
    USHORT unitsPerEm;
    ULONGLONG created;
    ULONGLONG modified;
    SHORT xMin;
    SHORT yMin;
    SHORT xMax;
    SHORT yMax;
    USHORT macStyle;
    USHORT lowestRecPPEM;
    SHORT direction_hint;
    SHORT index_format;
    SHORT glyphdata_format;
};

struct tt_fvar_header
{
    DWORD version;
    WORD axesArrayOffset;
    WORD reserved;
    WORD axisCount;
    WORD axisSize;
    WORD instanceCount;
    WORD instanceSize;
};

struct tt_fvar_axis
{
    DWORD axisTag;
    DWORD minValue;
    DWORD defaultValue;
    DWORD maxValue;
    WORD flags;
    WORD axisNameID;
};

struct tt_fvar_instance
{
    WORD subfamilyNameID;
    WORD flags;
    DWORD coordinates[1];
    /* WORD postScriptNameID; optional */
};

struct tt_stat_header
{
    WORD majorVersion;
    WORD minorVersion;
    WORD designAxisSize;
    WORD designAxisCount;
    DWORD designAxesOffset;
    WORD axisValueCount;
    DWORD offsetToAxisValueOffsets;
};

struct tt_stat_axis
{
    DWORD axisTag;
    WORD axisNameID;
    WORD axisOrdering;
};

struct tt_stat_axis_value
{
    WORD format;
    WORD axisIndex;      /* axisCount in format 4 */
    WORD flags;
    WORD valueNameID;
    DWORD value;         /* nominalValue in format 2, the first axis value record in format 4 */
    DWORD rangeMinValue; /* linkedValue in format 3 */
    DWORD rangeMaxValue;
};

struct tt_stat_axis_value_record
{
    WORD axisIndex;
    DWORD value;
};
#pragma pack(pop)

#define STAT_OLDER_SIBLING_FONT_ATTRIBUTE 0x0001
#define STAT_ELIDABLE_AXIS_VALUE_NAME     0x0002

enum OPENTYPE_PLATFORM_ID
{
    OPENTYPE_PLATFORM_UNICODE = 0,
    OPENTYPE_PLATFORM_MAC,
    OPENTYPE_PLATFORM_ISO,
    OPENTYPE_PLATFORM_WIN,
    OPENTYPE_PLATFORM_CUSTOM
};

enum TT_NAME_WIN_ENCODING_ID
{
    TT_NAME_WIN_ENCODING_SYMBOL = 0,
    TT_NAME_WIN_ENCODING_UNICODE_BMP,
    TT_NAME_WIN_ENCODING_SJIS,
    TT_NAME_WIN_ENCODING_PRC,
    TT_NAME_WIN_ENCODING_BIG5,
    TT_NAME_WIN_ENCODING_WANSUNG,
    TT_NAME_WIN_ENCODING_JOHAB,
    TT_NAME_WIN_ENCODING_RESERVED1,
    TT_NAME_WIN_ENCODING_RESERVED2,
    TT_NAME_WIN_ENCODING_RESERVED3,
    TT_NAME_WIN_ENCODING_UNICODE_FULL
};

enum TT_NAME_UNICODE_ENCODING_ID
{
    TT_NAME_UNICODE_ENCODING_1_0 = 0,
    TT_NAME_UNICODE_ENCODING_1_1,
    TT_NAME_UNICODE_ENCODING_ISO_10646,
    TT_NAME_UNICODE_ENCODING_2_0_BMP,
    TT_NAME_UNICODE_ENCODING_2_0_FULL,
    TT_NAME_UNICODE_ENCODING_VAR,
    TT_NAME_UNICODE_ENCODING_FULL,
};

enum TT_NAME_MAC_ENCODING_ID
{
    TT_NAME_MAC_ENCODING_ROMAN = 0,
    TT_NAME_MAC_ENCODING_JAPANESE,
    TT_NAME_MAC_ENCODING_TRAD_CHINESE,
    TT_NAME_MAC_ENCODING_KOREAN,
    TT_NAME_MAC_ENCODING_ARABIC,
    TT_NAME_MAC_ENCODING_HEBREW,
    TT_NAME_MAC_ENCODING_GREEK,
    TT_NAME_MAC_ENCODING_RUSSIAN,
    TT_NAME_MAC_ENCODING_RSYMBOL,
    TT_NAME_MAC_ENCODING_DEVANAGARI,
    TT_NAME_MAC_ENCODING_GURMUKHI,
    TT_NAME_MAC_ENCODING_GUJARATI,
    TT_NAME_MAC_ENCODING_ORIYA,
    TT_NAME_MAC_ENCODING_BENGALI,
    TT_NAME_MAC_ENCODING_TAMIL,
    TT_NAME_MAC_ENCODING_TELUGU,
    TT_NAME_MAC_ENCODING_KANNADA,
    TT_NAME_MAC_ENCODING_MALAYALAM,
    TT_NAME_MAC_ENCODING_SINHALESE,
    TT_NAME_MAC_ENCODING_BURMESE,
    TT_NAME_MAC_ENCODING_KHMER,
    TT_NAME_MAC_ENCODING_THAI,
    TT_NAME_MAC_ENCODING_LAOTIAN,
    TT_NAME_MAC_ENCODING_GEORGIAN,
    TT_NAME_MAC_ENCODING_ARMENIAN,
    TT_NAME_MAC_ENCODING_SIMPL_CHINESE,
    TT_NAME_MAC_ENCODING_TIBETAN,
    TT_NAME_MAC_ENCODING_MONGOLIAN,
    TT_NAME_MAC_ENCODING_GEEZ,
    TT_NAME_MAC_ENCODING_SLAVIC,
    TT_NAME_MAC_ENCODING_VIETNAMESE,
    TT_NAME_MAC_ENCODING_SINDHI,
    TT_NAME_MAC_ENCODING_UNINTERPRETED
};

enum OPENTYPE_NAME_ID
{
    OPENTYPE_NAME_COPYRIGHT_NOTICE = 0,
    OPENTYPE_NAME_FAMILY,
    OPENTYPE_NAME_SUBFAMILY,
    OPENTYPE_NAME_UNIQUE_IDENTIFIER,
    OPENTYPE_NAME_FULLNAME,
    OPENTYPE_NAME_VERSION_STRING,
    OPENTYPE_NAME_POSTSCRIPT,
    OPENTYPE_NAME_TRADEMARK,
    OPENTYPE_NAME_MANUFACTURER,
    OPENTYPE_NAME_DESIGNER,
    OPENTYPE_NAME_DESCRIPTION,
    OPENTYPE_NAME_VENDOR_URL,
    OPENTYPE_NAME_DESIGNER_URL,
    OPENTYPE_NAME_LICENSE_DESCRIPTION,
    OPENTYPE_NAME_LICENSE_INFO_URL,
    OPENTYPE_NAME_RESERVED_ID15,
    OPENTYPE_NAME_TYPOGRAPHIC_FAMILY,
    OPENTYPE_NAME_TYPOGRAPHIC_SUBFAMILY,
    OPENTYPE_NAME_COMPATIBLE_FULLNAME,
    OPENTYPE_NAME_SAMPLE_TEXT,
    OPENTYPE_NAME_POSTSCRIPT_CID,
    OPENTYPE_NAME_WWS_FAMILY,
    OPENTYPE_NAME_WWS_SUBFAMILY
};

enum OS2_FSSELECTION
{
    OS2_FSSELECTION_ITALIC           = 1 << 0,
    OS2_FSSELECTION_UNDERSCORE       = 1 << 1,
    OS2_FSSELECTION_NEGATIVE         = 1 << 2,
    OS2_FSSELECTION_OUTLINED         = 1 << 3,
    OS2_FSSELECTION_STRIKEOUT        = 1 << 4,
    OS2_FSSELECTION_BOLD             = 1 << 5,
    OS2_FSSELECTION_REGULAR          = 1 << 6,
    OS2_FSSELECTION_USE_TYPO_METRICS = 1 << 7,
    OS2_FSSELECTION_WWS              = 1 << 8,
    OS2_FSSELECTION_OBLIQUE          = 1 << 9
};

static BOOL opentype_get_table_ptr( const void *data, size_t size, const struct ttc_sfnt_v1 *ttc_sfnt_v1,
                                    UINT32 table_tag, const void **table_ptr, UINT32 *table_size )
{
    const struct tt_tablerecord *table_record;
    UINT16 i, table_count;
    UINT32 offset, length;
    size_t records_offset;

    if (!ttc_sfnt_v1) return FALSE;

    table_record = (const struct tt_tablerecord *)(ttc_sfnt_v1 + 1);
    table_count = GET_BE_WORD( ttc_sfnt_v1->numTables );
    records_offset = (const char *)table_record - (const char *)data;
    if (table_count > (size - records_offset) / sizeof(*table_record)) return FALSE;
    for (i = 0; i < table_count; i++, table_record++)
    {
        if (table_record->tag != table_tag) continue;
        offset = GET_BE_DWORD( table_record->offset );
        length = GET_BE_DWORD( table_record->length );
        if (offset > size || length > size - offset) return FALSE;
        if (table_size && length < *table_size) return FALSE;

        if (table_ptr) *table_ptr = (const char *)data + offset;
        if (table_size) *table_size = length;
        return TRUE;
    }

    return FALSE;
}

static BOOL opentype_get_tt_os2_v1( const void *data, size_t size, const struct ttc_sfnt_v1 *ttc_sfnt_v1,
                                    const struct tt_os2_v1 **tt_os2_v1 )
{
    UINT32 table_size = sizeof(**tt_os2_v1);
    return opentype_get_table_ptr( data, size, ttc_sfnt_v1, MS_OS_2_TAG, (const void **)tt_os2_v1, &table_size );
}

static BOOL opentype_get_tt_head( const void *data, size_t size, const struct ttc_sfnt_v1 *ttc_sfnt_v1,
                                  const struct tt_head **tt_head )
{
    UINT32 table_size = sizeof(**tt_head);
    return opentype_get_table_ptr( data, size, ttc_sfnt_v1, MS_HEAD_TAG, (const void **)tt_head, &table_size );
}

static UINT get_name_record_codepage( enum OPENTYPE_PLATFORM_ID platform, USHORT encoding )
{
    switch (platform)
    {
    case OPENTYPE_PLATFORM_UNICODE:
        return 0;
    case OPENTYPE_PLATFORM_MAC:
        switch (encoding)
        {
        case TT_NAME_MAC_ENCODING_ROMAN:
            return 10000;
        case TT_NAME_MAC_ENCODING_JAPANESE:
            return 10001;
        case TT_NAME_MAC_ENCODING_TRAD_CHINESE:
            return 10002;
        case TT_NAME_MAC_ENCODING_KOREAN:
            return 10003;
        case TT_NAME_MAC_ENCODING_ARABIC:
            return 10004;
        case TT_NAME_MAC_ENCODING_HEBREW:
            return 10005;
        case TT_NAME_MAC_ENCODING_GREEK:
            return 10006;
        case TT_NAME_MAC_ENCODING_RUSSIAN:
            return 10007;
        case TT_NAME_MAC_ENCODING_SIMPL_CHINESE:
            return 10008;
        case TT_NAME_MAC_ENCODING_THAI:
            return 10021;
        default:
            WARN( "default ascii encoding used for encoding %d, platform %d\n", encoding, platform );
            return 20127;
        }
        break;
    case OPENTYPE_PLATFORM_WIN:
        switch (encoding)
        {
        case TT_NAME_WIN_ENCODING_SYMBOL:
        case TT_NAME_WIN_ENCODING_UNICODE_BMP:
        case TT_NAME_WIN_ENCODING_UNICODE_FULL:
            return 0;
        case TT_NAME_WIN_ENCODING_SJIS:
            return 932;
        case TT_NAME_WIN_ENCODING_PRC:
            return 936;
        case TT_NAME_WIN_ENCODING_BIG5:
            return 950;
        case TT_NAME_WIN_ENCODING_WANSUNG:
            return 20949;
        case TT_NAME_WIN_ENCODING_JOHAB:
            return 1361;
        default:
            WARN( "default ascii encoding used for encoding %d, platform %d\n", encoding, platform );
            return 20127;
        }
        break;
    default:
        FIXME( "unknown platform %d\n", platform );
        break;
    }

    return 0;
}

static const LANGID mac_langid_table[] =
{
    MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),
    MAKELANGID(LANG_FRENCH, SUBLANG_DEFAULT),
    MAKELANGID(LANG_GERMAN, SUBLANG_DEFAULT),
    MAKELANGID(LANG_ITALIAN, SUBLANG_DEFAULT),
    MAKELANGID(LANG_DUTCH, SUBLANG_DEFAULT),
    MAKELANGID(LANG_SWEDISH, SUBLANG_DEFAULT),
    MAKELANGID(LANG_SPANISH, SUBLANG_DEFAULT),
    MAKELANGID(LANG_DANISH, SUBLANG_DEFAULT),
    MAKELANGID(LANG_PORTUGUESE, SUBLANG_DEFAULT),
    MAKELANGID(LANG_NORWEGIAN, SUBLANG_DEFAULT),
    MAKELANGID(LANG_HEBREW, SUBLANG_DEFAULT),
    MAKELANGID(LANG_JAPANESE, SUBLANG_DEFAULT),
    MAKELANGID(LANG_ARABIC, SUBLANG_DEFAULT),
    MAKELANGID(LANG_FINNISH, SUBLANG_DEFAULT),
    MAKELANGID(LANG_GREEK, SUBLANG_DEFAULT),
    MAKELANGID(LANG_ICELANDIC, SUBLANG_DEFAULT),
    MAKELANGID(LANG_MALTESE, SUBLANG_DEFAULT),
    MAKELANGID(LANG_TURKISH, SUBLANG_DEFAULT),
    MAKELANGID(LANG_CROATIAN, SUBLANG_DEFAULT),
    MAKELANGID(LANG_CHINESE_TRADITIONAL, SUBLANG_DEFAULT),
    MAKELANGID(LANG_URDU, SUBLANG_DEFAULT),
    MAKELANGID(LANG_HINDI, SUBLANG_DEFAULT),
    MAKELANGID(LANG_THAI, SUBLANG_DEFAULT),
    MAKELANGID(LANG_KOREAN, SUBLANG_DEFAULT),
    MAKELANGID(LANG_LITHUANIAN, SUBLANG_DEFAULT),
    MAKELANGID(LANG_POLISH, SUBLANG_DEFAULT),
    MAKELANGID(LANG_HUNGARIAN, SUBLANG_DEFAULT),
    MAKELANGID(LANG_ESTONIAN, SUBLANG_DEFAULT),
    MAKELANGID(LANG_LATVIAN, SUBLANG_DEFAULT),
    MAKELANGID(LANG_SAMI, SUBLANG_DEFAULT),
    MAKELANGID(LANG_FAEROESE, SUBLANG_DEFAULT),
    MAKELANGID(LANG_FARSI, SUBLANG_DEFAULT),
    MAKELANGID(LANG_RUSSIAN, SUBLANG_DEFAULT),
    MAKELANGID(LANG_CHINESE_SIMPLIFIED, SUBLANG_DEFAULT),
    MAKELANGID(LANG_DUTCH, SUBLANG_DUTCH_BELGIAN),
    MAKELANGID(LANG_IRISH, SUBLANG_DEFAULT),
    MAKELANGID(LANG_ALBANIAN, SUBLANG_DEFAULT),
    MAKELANGID(LANG_ROMANIAN, SUBLANG_DEFAULT),
    MAKELANGID(LANG_CZECH, SUBLANG_DEFAULT),
    MAKELANGID(LANG_SLOVAK, SUBLANG_DEFAULT),
    MAKELANGID(LANG_SLOVENIAN, SUBLANG_DEFAULT),
    0,
    MAKELANGID(LANG_SERBIAN, SUBLANG_DEFAULT),
    MAKELANGID(LANG_MACEDONIAN, SUBLANG_DEFAULT),
    MAKELANGID(LANG_BULGARIAN, SUBLANG_DEFAULT),
    MAKELANGID(LANG_UKRAINIAN, SUBLANG_DEFAULT),
    MAKELANGID(LANG_BELARUSIAN, SUBLANG_DEFAULT),
    MAKELANGID(LANG_UZBEK, SUBLANG_DEFAULT),
    MAKELANGID(LANG_KAZAK, SUBLANG_DEFAULT),
    MAKELANGID(LANG_AZERI, SUBLANG_AZERI_CYRILLIC),
    0,
    MAKELANGID(LANG_ARMENIAN, SUBLANG_DEFAULT),
    MAKELANGID(LANG_GEORGIAN, SUBLANG_DEFAULT),
    0,
    MAKELANGID(LANG_KYRGYZ, SUBLANG_DEFAULT),
    MAKELANGID(LANG_TAJIK, SUBLANG_DEFAULT),
    MAKELANGID(LANG_TURKMEN, SUBLANG_DEFAULT),
    MAKELANGID(LANG_MONGOLIAN, SUBLANG_DEFAULT),
    MAKELANGID(LANG_MONGOLIAN, SUBLANG_MONGOLIAN_CYRILLIC_MONGOLIA),
    MAKELANGID(LANG_PASHTO, SUBLANG_DEFAULT),
    0,
    MAKELANGID(LANG_KASHMIRI, SUBLANG_DEFAULT),
    MAKELANGID(LANG_SINDHI, SUBLANG_DEFAULT),
    MAKELANGID(LANG_TIBETAN, SUBLANG_DEFAULT),
    MAKELANGID(LANG_NEPALI, SUBLANG_DEFAULT),
    MAKELANGID(LANG_SANSKRIT, SUBLANG_DEFAULT),
    MAKELANGID(LANG_MARATHI, SUBLANG_DEFAULT),
    MAKELANGID(LANG_BENGALI, SUBLANG_DEFAULT),
    MAKELANGID(LANG_ASSAMESE, SUBLANG_DEFAULT),
    MAKELANGID(LANG_GUJARATI, SUBLANG_DEFAULT),
    MAKELANGID(LANG_PUNJABI, SUBLANG_DEFAULT),
    MAKELANGID(LANG_ORIYA, SUBLANG_DEFAULT),
    MAKELANGID(LANG_MALAYALAM, SUBLANG_DEFAULT),
    MAKELANGID(LANG_KANNADA, SUBLANG_DEFAULT),
    MAKELANGID(LANG_TAMIL, SUBLANG_DEFAULT),
    MAKELANGID(LANG_TELUGU, SUBLANG_DEFAULT),
    MAKELANGID(LANG_SINHALESE, SUBLANG_DEFAULT),
    0,
    MAKELANGID(LANG_KHMER, SUBLANG_DEFAULT),
    MAKELANGID(LANG_LAO, SUBLANG_DEFAULT),
    MAKELANGID(LANG_VIETNAMESE, SUBLANG_DEFAULT),
    MAKELANGID(LANG_INDONESIAN, SUBLANG_DEFAULT),
    0,
    MAKELANGID(LANG_MALAY, SUBLANG_DEFAULT),
    0,
    MAKELANGID(LANG_AMHARIC, SUBLANG_DEFAULT),
    MAKELANGID(LANG_TIGRIGNA, SUBLANG_DEFAULT),
    0,
    0,
    MAKELANGID(LANG_SWAHILI, SUBLANG_DEFAULT),
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    MAKELANGID(LANG_WELSH, SUBLANG_DEFAULT),
    MAKELANGID(LANG_BASQUE, SUBLANG_DEFAULT),
    MAKELANGID(LANG_CATALAN, SUBLANG_DEFAULT),
    0,
    MAKELANGID(LANG_QUECHUA, SUBLANG_DEFAULT),
    0,
    0,
    MAKELANGID(LANG_TATAR, SUBLANG_DEFAULT),
    MAKELANGID(LANG_UIGHUR, SUBLANG_DEFAULT),
    0,
    0,
    0,
    MAKELANGID(LANG_GALICIAN, SUBLANG_DEFAULT),
    MAKELANGID(LANG_AFRIKAANS, SUBLANG_DEFAULT),
    MAKELANGID(LANG_BRETON, SUBLANG_DEFAULT),
    MAKELANGID(LANG_INUKTITUT, SUBLANG_DEFAULT),
    MAKELANGID(LANG_SCOTTISH_GAELIC, SUBLANG_DEFAULT),
    0,
    MAKELANGID(LANG_IRISH, SUBLANG_IRISH_IRELAND),
    0,
    0,
    MAKELANGID(LANG_GREENLANDIC, SUBLANG_DEFAULT),
    MAKELANGID(LANG_AZERI, SUBLANG_AZERI_LATIN),
};

static LANGID get_name_record_langid( enum OPENTYPE_PLATFORM_ID platform, USHORT encoding, USHORT language )
{
    switch (platform)
    {
    case OPENTYPE_PLATFORM_WIN:
        return language;
    case OPENTYPE_PLATFORM_MAC:
        if (language < ARRAY_SIZE(mac_langid_table)) return mac_langid_table[language];
        WARN( "invalid mac lang id %d\n", language );
        break;
    case OPENTYPE_PLATFORM_UNICODE:
        switch (encoding)
        {
        case TT_NAME_UNICODE_ENCODING_1_0:
        case TT_NAME_UNICODE_ENCODING_ISO_10646:
        case TT_NAME_UNICODE_ENCODING_2_0_BMP:
            if (language < ARRAY_SIZE(mac_langid_table)) return mac_langid_table[language];
            WARN( "invalid unicode lang id %d\n", language );
            break;
        default:
            break;
        }
        break;
    default:
        FIXME( "unknown platform %d\n", platform );
        break;
    }

    return MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL);
}

static BOOL opentype_enum_font_names( const struct tt_name_v0 *header, enum OPENTYPE_PLATFORM_ID platform,
                                      USHORT name, opentype_enum_names_cb callback, void *user )
{
    const char *name_data;
    USHORT i, name_count, encoding, language, length, offset;
    USHORT platform_id = GET_BE_WORD( platform ), name_id = GET_BE_WORD( name );
    LANGID langid;
    BOOL ret = FALSE;

    switch (GET_BE_WORD( header->format ))
    {
    case 0:
    case 1:
        break;
    default:
        FIXME( "unsupported name format %d\n", GET_BE_WORD( header->format ) );
        return FALSE;
    }

    name_data = (const char *)header + GET_BE_WORD( header->stringOffset );
    name_count = GET_BE_WORD( header->count );
    for (i = 0; i < name_count; i++)
    {
        const struct tt_namerecord *record = &header->nameRecord[i];
        struct opentype_name opentype_name;

        if (record->nameID != name_id) continue;
        if (record->platformID != platform_id) continue;

        language = GET_BE_WORD( record->languageID );
        if (language >= 0x8000)
        {
            FIXME( "handle name format 1\n" );
            continue;
        }

        encoding = GET_BE_WORD( record->encodingID );
        offset = GET_BE_WORD( record->offset );
        length = GET_BE_WORD( record->length );
        langid = get_name_record_langid( platform, encoding, language );

        opentype_name.codepage = get_name_record_codepage( platform, encoding );
        opentype_name.length = length;
        opentype_name.bytes = name_data + offset;

        if ((ret = callback( langid, &opentype_name, user ))) break;
    }

    return ret;
}

BOOL opentype_get_ttc_sfnt_v1( const void *data, size_t size, DWORD index, DWORD *count, const struct ttc_sfnt_v1 **ttc_sfnt_v1 )
{
    const struct ttc_header_v1 *ttc_header_v1 = data;
    const struct tt_os2_v1 *tt_os2_v1;
    UINT32 offset, fourcc;

    *ttc_sfnt_v1 = NULL;
    *count = 1;

    if (size < sizeof(fourcc)) return FALSE;
    memcpy( &fourcc, data, sizeof(fourcc) );

    switch (fourcc)
    {
    default:
        WARN( "unsupported font format %x\n", fourcc );
        return FALSE;
    case MS_TTCF_TAG:
        if (size < offsetof( struct ttc_header_v1, OffsetTable )) return FALSE;
        if (index >= (*count = GET_BE_DWORD( ttc_header_v1->numFonts ))) return FALSE;
        if (index >= (size - offsetof( struct ttc_header_v1, OffsetTable )) / sizeof(DWORD)) return FALSE;
        offset = GET_BE_DWORD( ttc_header_v1->OffsetTable[index] );
        break;
    case 0x00000100:
    case MS_OTTO_TAG:
        offset = 0;
        break;
    }

    if (offset > size || size - offset < sizeof(**ttc_sfnt_v1)) return FALSE;
    *ttc_sfnt_v1 = (const struct ttc_sfnt_v1 *)((const char *)data + offset);

    if (!opentype_get_table_ptr( data, size, *ttc_sfnt_v1, MS_HEAD_TAG, NULL, NULL ))
    {
        WARN( "unsupported sfnt font: missing head table.\n" );
        return FALSE;
    }

    if (!opentype_get_table_ptr( data, size, *ttc_sfnt_v1, MS_HHEA_TAG, NULL, NULL ))
    {
        WARN( "unsupported sfnt font: missing hhea table.\n" );
        return FALSE;
    }

    if (!opentype_get_tt_os2_v1( data, size, *ttc_sfnt_v1, &tt_os2_v1 ))
    {
        WARN( "unsupported sfnt font: missing OS/2 table.\n" );
        return FALSE;
    }

    /* Wine uses ttfs as an intermediate step in building its bitmap fonts;
       we don't want to load these. */
    if (!memcmp( tt_os2_v1->achVendID, "Wine", sizeof(tt_os2_v1->achVendID) ) &&
        opentype_get_table_ptr( data, size, *ttc_sfnt_v1, MS_EBSC_TAG, NULL, NULL ))
    {
        TRACE( "ignoring wine bitmap-only sfnt font.\n" );
        return FALSE;
    }

    if (opentype_get_table_ptr( data, size, *ttc_sfnt_v1, MS_EBDT_TAG, NULL, NULL ) ||
        opentype_get_table_ptr( data, size, *ttc_sfnt_v1, MS_CBDT_TAG, NULL, NULL ))
    {
        WARN( "unsupported sfnt font: embedded bitmap data.\n" );
        return FALSE;
    }

    return TRUE;
}

BOOL opentype_get_tt_name_v0( const void *data, size_t size, const struct ttc_sfnt_v1 *ttc_sfnt_v1,
                              const struct tt_name_v0 **tt_name_v0 )
{
    UINT32 table_size = sizeof(**tt_name_v0);
    return opentype_get_table_ptr( data, size, ttc_sfnt_v1, MS_NAME_TAG, (const void **)tt_name_v0, &table_size );
}

BOOL opentype_enum_names( const struct tt_name_v0 *header, USHORT name_id, opentype_enum_names_cb callback, void *user )
{
    if (opentype_enum_font_names( header, OPENTYPE_PLATFORM_WIN, name_id, callback, user ))
        return TRUE;
    if (opentype_enum_font_names( header, OPENTYPE_PLATFORM_MAC, name_id, callback, user ))
        return TRUE;
    if (opentype_enum_font_names( header, OPENTYPE_PLATFORM_UNICODE, name_id, callback, user ))
        return TRUE;
    return FALSE;
}

BOOL opentype_enum_family_names( const struct tt_name_v0 *header, opentype_enum_names_cb callback, void *user )
{
    return opentype_enum_names( header, OPENTYPE_NAME_FAMILY, callback, user );
}

BOOL opentype_enum_style_names( const struct tt_name_v0 *header, opentype_enum_names_cb callback, void *user )
{
    return opentype_enum_names( header, OPENTYPE_NAME_SUBFAMILY, callback, user );
}

BOOL opentype_enum_full_names( const struct tt_name_v0 *header, opentype_enum_names_cb callback, void *user )
{
    return opentype_enum_names( header, OPENTYPE_NAME_FULLNAME, callback, user );
}

BOOL opentype_get_properties( const void *data, size_t size, const struct ttc_sfnt_v1 *ttc_sfnt_v1,
                              DWORD *version, FONTSIGNATURE *fs, DWORD *ntm_flags, UINT *weight )
{
    const struct tt_os2_v1 *tt_os2_v1;
    const struct tt_head *tt_head;
    const void *cff_header;
    UINT32 table_size = 0;
    USHORT idx, selection;
    DWORD flags = 0;

    if (!opentype_get_tt_head( data, size, ttc_sfnt_v1, &tt_head )) return FALSE;
    if (!opentype_get_tt_os2_v1( data, size, ttc_sfnt_v1, &tt_os2_v1 )) return FALSE;

    *version = GET_BE_DWORD( tt_head->revision );

    fs->fsUsb[0] = GET_BE_DWORD( tt_os2_v1->ulUnicodeRange1 );
    fs->fsUsb[1] = GET_BE_DWORD( tt_os2_v1->ulUnicodeRange2 );
    fs->fsUsb[2] = GET_BE_DWORD( tt_os2_v1->ulUnicodeRange3 );
    fs->fsUsb[3] = GET_BE_DWORD( tt_os2_v1->ulUnicodeRange4 );

    if (tt_os2_v1->version == 0)
    {
        idx = GET_BE_WORD( tt_os2_v1->usFirstCharIndex );
        if (idx >= 0xf000 && idx < 0xf100) fs->fsCsb[0] = FS_SYMBOL;
        else fs->fsCsb[0] = FS_LATIN1;
        fs->fsCsb[1] = 0;
    }
    else
    {
        fs->fsCsb[0] = GET_BE_DWORD( tt_os2_v1->ulCodePageRange1 );
        fs->fsCsb[1] = GET_BE_DWORD( tt_os2_v1->ulCodePageRange2 );
    }

    selection = GET_BE_WORD( tt_os2_v1->fsSelection );

    if (selection & OS2_FSSELECTION_ITALIC) flags |= NTM_ITALIC;
    if (selection & OS2_FSSELECTION_BOLD) flags |= NTM_BOLD;
    if (selection & OS2_FSSELECTION_REGULAR) flags |= NTM_REGULAR;
    if (flags == 0) flags = NTM_REGULAR;
    *weight = GET_BE_WORD( tt_os2_v1->usWeightClass );

    if (opentype_get_table_ptr( data, size, ttc_sfnt_v1, MS_CFF__TAG, &cff_header, &table_size ))
        flags |= NTM_PS_OPENTYPE;

    *ntm_flags = flags;
    return TRUE;
}

/* like FreeType, treat zero-length tables as missing */
static BOOL opentype_has_table( const void *data, size_t size, const struct ttc_sfnt_v1 *ttc_sfnt_v1, UINT32 tag )
{
    UINT32 table_size = 1;
    return opentype_get_table_ptr( data, size, ttc_sfnt_v1, tag, NULL, &table_size );
}

struct opentype_var_font
{
    const struct tt_fvar_axis *axes;
    const BYTE *instances;
    UINT axis_count;
    UINT instance_size;
    UINT record_count;    /* number of named instance records in fvar */
    UINT instance_count;  /* number of exposed named instances, the first records */
    const struct tt_stat_header *stat;
    UINT32 stat_size;
};

static BOOL opentype_get_var_font( const void *data, size_t size, const struct ttc_sfnt_v1 *ttc_sfnt_v1,
                                   struct opentype_var_font *var )
{
    UINT32 fvar_size = sizeof(struct tt_fvar_header), stat_size = sizeof(struct tt_stat_header);
    const struct tt_fvar_header *fvar;
    const struct tt_stat_header *stat;
    UINT axes_offset, design_axis_count, axis_value_count;

    if (!opentype_get_table_ptr( data, size, ttc_sfnt_v1, MS_FVAR_TAG, (const void **)&fvar, &fvar_size ))
        return FALSE;

    /* use the same checks as FreeType, so that it accepts every named instance we expose */
    axes_offset = GET_BE_WORD( fvar->axesArrayOffset );
    var->axis_count = GET_BE_WORD( fvar->axisCount );
    var->instance_size = GET_BE_WORD( fvar->instanceSize );
    var->record_count = GET_BE_WORD( fvar->instanceCount );
    if (GET_BE_DWORD( fvar->version ) != 0x00010000 ||
        GET_BE_WORD( fvar->axisSize ) != sizeof(struct tt_fvar_axis) ||
        !var->axis_count || var->axis_count > 0x3ffe ||
        (var->instance_size != 4 + 4 * var->axis_count && var->instance_size != 6 + 4 * var->axis_count) ||
        var->record_count > 0x7eff ||
        axes_offset + (UINT64)sizeof(struct tt_fvar_axis) * var->axis_count +
        (UINT64)var->instance_size * var->record_count > fvar_size)
    {
        WARN( "invalid fvar table\n" );
        return FALSE;
    }
    if (!var->record_count)
    {
        TRACE( "no named instances\n" );
        return FALSE;
    }

    /* FreeType has no named instances for Multiple Master CFF fonts, and before 2.13
     * it ignores the fvar table of TrueType fonts that have no gvar table */
    if (!(opentype_has_table( data, size, ttc_sfnt_v1, MS_GLYF_TAG ) &&
          opentype_has_table( data, size, ttc_sfnt_v1, MS_GVAR_TAG )) &&
        !opentype_has_table( data, size, ttc_sfnt_v1, MS_CFF2_TAG ))
    {
        TRACE( "no glyf and gvar tables, or CFF2 table\n" );
        return FALSE;
    }

    if (var->axis_count > MAX_VAR_AXES)
    {
        WARN( "ignoring the named instances of a font with %u axes\n", var->axis_count );
        return FALSE;
    }

    /* like DirectWrite, only expose the named instances of fonts with a usable STAT table */
    if (!opentype_get_table_ptr( data, size, ttc_sfnt_v1, MS_STAT_TAG, (const void **)&stat, &stat_size ))
    {
        TRACE( "no STAT table\n" );
        return FALSE;
    }
    design_axis_count = GET_BE_WORD( stat->designAxisCount );
    axis_value_count = GET_BE_WORD( stat->axisValueCount );
    if (GET_BE_WORD( stat->majorVersion ) != 1 ||
        GET_BE_WORD( stat->designAxisSize ) < sizeof(struct tt_stat_axis) ||
        !design_axis_count || !axis_value_count ||
        GET_BE_DWORD( stat->designAxesOffset ) +
        (UINT64)GET_BE_WORD( stat->designAxisSize ) * design_axis_count > stat_size ||
        GET_BE_DWORD( stat->offsetToAxisValueOffsets ) + (UINT64)sizeof(WORD) * axis_value_count > stat_size)
    {
        TRACE( "unusable STAT table\n" );
        return FALSE;
    }

    var->axes = (const struct tt_fvar_axis *)((const BYTE *)fvar + axes_offset);
    var->instances = (const BYTE *)(var->axes + var->axis_count);
    var->instance_count = min( var->record_count, MAX_VAR_INSTANCES );
    var->stat = stat;
    var->stat_size = stat_size;
    return TRUE;
}

/* index is the 1-based record index, as in bits 16-30 of FreeType face indices */
static const struct tt_fvar_instance *var_get_instance( const struct opentype_var_font *var, UINT index )
{
    return (const struct tt_fvar_instance *)(var->instances + (index - 1) * var->instance_size);
}

/* 16.16 user space coordinate of a named instance, or of the default instance for index 0 */
static INT32 var_get_coord( const struct opentype_var_font *var, UINT index, UINT axis )
{
    if (!index) return GET_BE_DWORD( var->axes[axis].defaultValue );
    return GET_BE_DWORD( var_get_instance( var, index )->coordinates[axis] );
}

static BOOL var_coords_equal( const struct opentype_var_font *var, UINT index, UINT other )
{
    UINT i;

    for (i = 0; i < var->axis_count; i++)
        if (var_get_coord( var, index, i ) != var_get_coord( var, other, i )) return FALSE;
    return TRUE;
}

static const struct tt_stat_axis *stat_get_design_axis( const struct opentype_var_font *var, UINT axis )
{
    const BYTE *axes = (const BYTE *)var->stat + GET_BE_DWORD( var->stat->designAxesOffset );
    return (const struct tt_stat_axis *)(axes + axis * GET_BE_WORD( var->stat->designAxisSize ));
}

static UINT stat_axis_ordering( const struct opentype_var_font *var, UINT axis )
{
    return GET_BE_WORD( stat_get_design_axis( var, axis )->axisOrdering );
}

/* the axis value table k of STAT, NULL if it is invalid or only meant for older fonts of the family */
static const struct tt_stat_axis_value *stat_get_axis_value( const struct opentype_var_font *var, UINT k )
{
    UINT32 offsets = GET_BE_DWORD( var->stat->offsetToAxisValueOffsets );
    const WORD *offset_array = (const WORD *)((const BYTE *)var->stat + offsets);
    UINT64 offset = (UINT64)offsets + GET_BE_WORD( offset_array[k] );
    const struct tt_stat_axis_value *value;
    UINT value_size;

    if (offset + offsetof( struct tt_stat_axis_value, value ) > var->stat_size) return NULL;
    value = (const struct tt_stat_axis_value *)((const BYTE *)var->stat + offset);
    switch (GET_BE_WORD( value->format ))
    {
    case 1: value_size = offsetof( struct tt_stat_axis_value, rangeMinValue ); break;
    case 2: value_size = sizeof(struct tt_stat_axis_value); break;
    case 3: value_size = offsetof( struct tt_stat_axis_value, rangeMaxValue ); break;
    case 4:
        value_size = offsetof( struct tt_stat_axis_value, value ) +
                     GET_BE_WORD( value->axisIndex ) * sizeof(struct tt_stat_axis_value_record);
        break;
    default: return NULL;
    }
    if (offset + value_size > var->stat_size) return NULL;
    if (GET_BE_WORD( value->flags ) & STAT_OLDER_SIBLING_FONT_ATTRIBUTE) return NULL;
    return value;
}

struct stat_label
{
    UINT ordering;
    WORD name_id;
    WORD flags;
};

/* insert the name of an axis value into the labels sorted by axis ordering */
static BOOL stat_add_label( struct stat_label *labels, UINT *count, UINT ordering,
                            const struct tt_stat_axis_value *value )
{
    UINT i;

    if (*count == MAX_STAT_AXES) return FALSE;
    for (i = *count; i > 0 && labels[i - 1].ordering > ordering; i--) labels[i] = labels[i - 1];
    labels[i].ordering = ordering;
    labels[i].name_id = GET_BE_WORD( value->valueNameID );
    labels[i].flags = GET_BE_WORD( value->flags );
    (*count)++;
    return TRUE;
}

/* Like DirectWrite, name a named instance, or the default instance for index 0, with the STAT axis
 * values of its coordinates in axis ordering, leaving out the elidable ones. Fails if a variation axis
 * has no design axis, or if its value has no name. */
static BOOL stat_get_instance_labels( const struct opentype_var_font *var, UINT index,
                                      struct opentype_var_instance *instance )
{
    UINT design_count = GET_BE_WORD( var->stat->designAxisCount );
    UINT value_count = GET_BE_WORD( var->stat->axisValueCount );
    const struct tt_stat_axis_value_record *record;
    const struct tt_stat_axis_value *value;
    struct stat_label labels[MAX_STAT_AXES];
    BOOL covered[MAX_STAT_AXES] = {0}, described[MAX_VAR_AXES] = {0};
    int fvar_axis[MAX_STAT_AXES];
    UINT i, j, k, count, label_count = 0;

    if (design_count > MAX_STAT_AXES || value_count > MAX_STAT_VALUES)
    {
        TRACE( "ignoring STAT table with %u design axes and %u axis values\n", design_count, value_count );
        return FALSE;
    }

    for (i = 0; i < design_count; i++)
    {
        DWORD tag = stat_get_design_axis( var, i )->axisTag;

        fvar_axis[i] = -1;
        for (j = 0; j < var->axis_count && fvar_axis[i] < 0; j++)
            if (var->axes[j].axisTag == tag) fvar_axis[i] = j;
        if (fvar_axis[i] >= 0) described[fvar_axis[i]] = TRUE;
    }

    /* instances that differ only on an axis STAT doesn't describe would get the same names */
    for (j = 0; j < var->axis_count; j++)
    {
        if (described[j]) continue;
        TRACE( "no STAT design axis for %s\n", debugstr_fourcc( var->axes[j].axisTag ) );
        return FALSE;
    }

    /* format 4 values name a combination of axis values, and take precedence */
    for (k = 0; k < value_count; k++)
    {
        UINT ordering = ~0u;
        BOOL match = TRUE;

        if (!(value = stat_get_axis_value( var, k )) || GET_BE_WORD( value->format ) != 4) continue;
        /* there is at most one record for each design axis */
        if (!(count = GET_BE_WORD( value->axisIndex )) || count > design_count) continue;
        record = (const struct tt_stat_axis_value_record *)&value->value;
        for (j = 0; j < count && match; j++)
        {
            UINT axis = GET_BE_WORD( record[j].axisIndex );

            if (axis >= design_count || fvar_axis[axis] < 0 || covered[axis] ||
                var_get_coord( var, index, fvar_axis[axis] ) != (INT32)GET_BE_DWORD( record[j].value ))
                match = FALSE;
            else ordering = min( ordering, stat_axis_ordering( var, axis ) );
        }
        if (!match) continue;
        for (j = 0; j < count; j++) covered[GET_BE_WORD( record[j].axisIndex )] = TRUE;
        if (!stat_add_label( labels, &label_count, ordering, value )) return FALSE;
    }

    for (i = 0; i < design_count; i++)
    {
        const struct tt_stat_axis_value *exact = NULL, *range = NULL, *single = NULL;
        UINT format, candidates = 0;
        INT32 coord = 0;

        if (covered[i]) continue;
        if (fvar_axis[i] >= 0) coord = var_get_coord( var, index, fvar_axis[i] );
        for (k = 0; k < value_count; k++)
        {
            if (!(value = stat_get_axis_value( var, k ))) continue;
            if ((format = GET_BE_WORD( value->format )) == 4 || GET_BE_WORD( value->axisIndex ) != i) continue;
            candidates++;
            single = value;
            if (fvar_axis[i] < 0) continue;
            /* the value of format 1 and 3, the nominal value of format 2 */
            if ((INT32)GET_BE_DWORD( value->value ) == coord)
            {
                if (!exact) exact = value;
            }
            else if (format == 2 && !range && (INT32)GET_BE_DWORD( value->rangeMinValue ) <= coord &&
                     coord <= (INT32)GET_BE_DWORD( value->rangeMaxValue ))
                range = value;
        }

        if (fvar_axis[i] >= 0)
        {
            if (!(value = exact ? exact : range))
            {
                TRACE( "no STAT axis value for %s %#x\n", debugstr_fourcc( var->axes[fvar_axis[i]].axisTag ), coord );
                return FALSE;
            }
        }
        /* an axis that does not vary describes the whole font, e.g. the ital axis of an Italic font */
        else if (candidates == 1) value = single;
        else continue;
        if (!stat_add_label( labels, &label_count, stat_axis_ordering( var, i ), value )) return FALSE;
    }

    for (i = 0; i < label_count; i++)
    {
        if (labels[i].flags & STAT_ELIDABLE_AXIS_VALUE_NAME) continue;
        if (instance->label_count == ARRAY_SIZE(instance->label_name_ids)) return FALSE;
        instance->label_name_ids[instance->label_count++] = labels[i].name_id;
    }
    return TRUE;
}

BOOL opentype_get_var_info( const void *data, size_t size, const struct ttc_sfnt_v1 *ttc_sfnt_v1,
                            struct opentype_var_info *info )
{
    struct opentype_var_font var;
    UINT i;

    if (!opentype_get_var_font( data, size, ttc_sfnt_v1, &var )) return FALSE;
    if (var.instance_count < var.record_count)
        WARN( "exposing only %u of %u named instances\n", var.instance_count, var.record_count );

    info->instance_count = var.instance_count;
    /* the first record at the default coordinates, like FreeType's default named instance */
    info->default_instance = 0;
    for (i = 1; i <= var.instance_count && !info->default_instance; i++)
        if (var_coords_equal( &var, i, 0 )) info->default_instance = i;
    return TRUE;
}

BOOL opentype_get_var_instance( const void *data, size_t size, const struct ttc_sfnt_v1 *ttc_sfnt_v1,
                                UINT index, struct opentype_var_instance *instance )
{
    struct opentype_var_font var;
    INT32 coord;
    UINT i;

    memset( instance, 0, sizeof(*instance) );
    if (!opentype_get_var_font( data, size, ttc_sfnt_v1, &var )) return FALSE;
    if (index > var.instance_count) return FALSE;

    if (index) instance->subfamily_name_id = GET_BE_WORD( var_get_instance( &var, index )->subfamilyNameID );
    instance->is_default = var_coords_equal( &var, index, 0 );
    for (i = 1; i < index && !instance->is_duplicate; i++)
        instance->is_duplicate = var_coords_equal( &var, index, i );

    for (i = 0; i < var.axis_count; i++)
    {
        coord = var_get_coord( &var, index, i );
        if (var.axes[i].axisTag == MS_WGHT_TAG)
            instance->weight = min( max( ((INT64)coord + 0x8000) >> 16, 1 ), 1000 );
        else if (var.axes[i].axisTag == MS_ITAL_TAG)
        {
            instance->has_slant_axis = TRUE;
            if (coord >= 0x8000) instance->italic = TRUE;
        }
        else if (var.axes[i].axisTag == MS_SLNT_TAG)
            instance->has_slant_axis = TRUE;
    }

    /* duplicate instances are not exposed, and need no name */
    if (instance->is_duplicate) return TRUE;
    if (!(instance->stat_valid = stat_get_instance_labels( &var, index, instance ))) instance->label_count = 0;
    return TRUE;
}
