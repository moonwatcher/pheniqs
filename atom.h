/*
    Pheniqs : PHilology ENcoder wIth Quality Statistics
    Copyright (C) 2018  Lior Galanti
    NYU Center for Genetics and System Biology

    Author: Lior Galanti <lior.galanti@nyu.edu>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef PHENIQS_ATOM_H
#define PHENIQS_ATOM_H

#include <set>
#include <unordered_map>

#include <htslib/hts.h>

#include "error.h"
#include "json.h"
#include "kstring.h"

using std::set;
using std::copy;
using std::hash;
using std::setw;
using std::endl;
using std::cerr;
using std::cout;
using std::fixed;
using std::string;
using std::vector;
using std::ostream;
using std::ifstream;
using std::ios_base;
using std::exception;
using std::to_string;
using std::make_pair;
using std::setprecision;
using std::unordered_map;

/*  defined in htslib/hts.h */
void to_string(const htsFormatCategory& value, string& result);
bool from_string(const char* value, htsFormatCategory& result);
ostream& operator<<(ostream& o, const htsFormatCategory& hts_format_category);

void to_string(const htsExactFormat& value, string& result);
bool from_string(const char* value, htsExactFormat& result);
ostream& operator<<(ostream& o, const htsExactFormat& hts_exact_format);

void to_string(const htsCompression& value, string& result);
bool from_string(const char* value, htsCompression& result);
ostream& operator<<(ostream& o, const htsCompression& hts_compression);

/*  SAM format flags

    PAIRED          read is paired in sequencing, no matter whether it is mapped in a pair
    PROPER_PAIR     read is mapped in a proper pair
    UNMAP           read itself is unmapped; conflictive with PROPER_PAIR
    MUNMAP          mate is unmapped
    REVERSE         read is mapped to the reverse strand
    MREVERSE        mate is mapped to the reverse strand
    READ1           the first segment in the template
    READ2           the last segment in the template
    SECONDARY       not primary alignment
    QCFAIL          QC failure
    DUP             optical or PCR duplicate
    SUPPLEMENTARY   supplementary alignment
*/
enum class HtsFlag : uint16_t {
    PAIRED         = 0x1,
    PROPER_PAIR    = 0x2,
    UNMAP          = 0x4,
    MUNMAP         = 0x8,
    REVERSE        = 0x10,
    MREVERSE       = 0x20,
    READ1          = 0x40,
    READ2          = 0x80,
    SECONDARY      = 0x100,
    QCFAIL         = 0x200,
    DUP            = 0x400,
    SUPPLEMENTARY  = 0x800,
};

enum class HtsSortOrder : int8_t {
    UNKNOWN    = -1,
    UNSORTED   =  0,
    QUERYNAME  =  1,
    COORDINATE =  2,
};
void to_string(const HtsSortOrder& value, string& result);
bool from_string(const char* value, HtsSortOrder& result);
void to_kstring(const HtsSortOrder& value, kstring_t& result);
bool from_string(const string& value, HtsSortOrder& result);
ostream& operator<<(ostream& o, const HtsSortOrder& value);
void encode_key_value(const string& key, const HtsSortOrder& value, Value& container, Document& document);

enum class HtsGrouping : int8_t {
    UNKNOWN   = -1,
    NONE      =  0,
    QUERY     =  1,
    REFERENCE =  2,
};
void to_string(const HtsGrouping& value, string& result);
bool from_string(const char* value, HtsGrouping& result);
bool from_string(const string& value, HtsGrouping& result);
void to_kstring(const HtsGrouping& value, kstring_t& result);
ostream& operator<<(ostream& o, const HtsGrouping& value);
void encode_key_value(const string& key, const HtsGrouping& value, Value& container, Document& document);

enum class Platform : uint8_t {
    UNKNOWN,
    CAPILLARY,
    LS454,
    ILLUMINA,
    SOLID,
    HELICOS,
    IONTORRENT,
    ONT,
    PACBIO,
};
void to_string(const Platform& value, string& result);
bool from_string(const char* value, Platform& result);
void to_kstring(const Platform& value, kstring_t& result);
bool from_string(const string& value, Platform& result);
ostream& operator<<(ostream& o, const Platform& value);
void encode_key_value(const string& key, const Platform& value, Value& container, Document& document);

enum class Decoder : uint8_t {
    UNKNOWN,
    MDD,
    PAMLD,
    BENCHMARK,
};
void to_string(const Decoder& value, string& result);
bool from_string(const char* value, Decoder& result);
void to_kstring(const Decoder& value, kstring_t& result);
bool from_string(const string& value, Decoder& result);
ostream& operator<<(ostream& o, const Decoder& value);
bool encode_key_value(const string& key, const Decoder& value, Value& container, Document& document);

/*  @HD The header line

    VN  Format version
    SO  Sorting order of alignments
        unknown
        unsorted
        queryname
        coordinate
    GO  Grouping of alignments
        Indicating that similar alignment records are grouped together but the file is not necessarily sorted overall.
            none
            query       grouped by QNAME
            reference   grouped by RNAME/POS
*/
class HeadHDAtom {
friend class HtsHeader;
friend ostream& operator<<(ostream& o, const HeadHDAtom& hd);

public:
    kstring_t VN;
    kstring_t SO;
    kstring_t GO;

    HeadHDAtom();
    ~HeadHDAtom();
    HeadHDAtom(const HeadHDAtom& other);
    HeadHDAtom& operator=(const HeadHDAtom& other);
    void set_alignment_sort_order(const HtsSortOrder& order);
    void set_alignment_grouping(const HtsGrouping& grouping);
    void set_version(const htsFormat* format);

private:
    void encode(kstring_t& buffer) const;
    char* decode(char* position, const char* end);
};
ostream& operator<<(ostream& o, const HeadHDAtom& hd);

/*  @SQ Sequence

    SN  Reference sequence identifier.
        The value of this field is used in the alignment records in RNAME and RNEXT fields.
    LN  Reference sequence length. 32 bit signed.
    AH  Indicates that this sequence is an alternate locus.
        The value is the locus in the primary assembly for which this sequence is an alternative,
        in the format ‘chr:start-end’, ‘chr’ (if known), or ‘*’ (if unknown), where ‘chr’ is a sequence in the primary assembly.
        Must not be present on sequences in the primary assembly.
    AS  Genome assembly identifier.
    M5  MD5 checksum of the sequence in the uppercase, excluding spaces but including pads (as ‘*’s).
    SP  Species.
    UR  URI of the sequence. This value may start with one of the standard protocols, e.g http: or ftp:.
        If it does not start with one of these protocols, it is assumed to be a file-system path.
*/
class HeadSQAtom {
friend class HtsHeader;
friend ostream& operator<<(ostream& o, const HeadSQAtom& program);

public:
    kstring_t SN;
    int32_t LN;
    kstring_t AH;
    kstring_t AS;
    kstring_t M5;
    kstring_t SP;
    kstring_t UR;

    HeadSQAtom();
    ~HeadSQAtom();
    HeadSQAtom(const HeadSQAtom& other);
    HeadSQAtom& operator=(const HeadSQAtom& other);
    operator string() const;

private:
    void encode(kstring_t& buffer) const;
    char* decode(char* position, const char* end);
};
ostream& operator<<(ostream& o, const HeadSQAtom& sq);

/*  @PG Program

    ID  Program record identifier.
        Each @PG line must have a unique ID.
        The value of ID is used in the alignment PG tag and PP tags of other @PG lines.
        PG IDs may be modified when merging SAM files in order to handle collisions.
    PN  Program name
    CL  Command line
    PP  Previous @PG:ID. Must be valid reference to another @PG:ID
    DS  Description.
    VN  Program version
*/
class HeadPGAtom {
friend class HtsHeader;
friend ostream& operator<<(ostream& o, const HeadPGAtom& program);

public:
    kstring_t ID;
    kstring_t PN;
    kstring_t CL;
    kstring_t PP;
    kstring_t DS;
    kstring_t VN;

    HeadPGAtom();
    ~HeadPGAtom();
    HeadPGAtom(const HeadPGAtom& other);
    HeadPGAtom& operator=(const HeadPGAtom& other);
    operator string() const;

private:
    void encode(kstring_t& buffer) const;
    char* decode(char* position, const char* end);
};
ostream& operator<<(ostream& o, const HeadPGAtom& pg);

/*  @RG Read Group

    ID  Read group identifier
        Each @RG line must have a unique ID.
        The value of ID is used in the RG tags of alignment records.
        Must be unique among all read groups in header section.
        Read group IDs may be modified when merging SAM files in order to handle collisions.
    LB  Library
    SM  Sample. Use pool name where a pool is being sequenced.
    PU  Platform unit Unique identifier. e.g. flowcell-barcode.lane for Illumina.
    CN  Name of sequencing center producing the read
    DS  Description
    DT  Date the run was produced ISO8601 date or datetime.
    PI  Predicted median insert size.
    PL  Platform or technology used to produce the reads.
            CAPILLARY, LS454, ILLUMINA, SOLID, HELICOS, IONTORRENT, ONT, PACBIO
    PM  Platform model
    PG  Programs used for processing the read group
    FO  Flow order.
        The array of nucleotide bases that correspond to the nucleotides used for each flow of each read.
        Multi-base flows are encoded in IUPAC format, and non-nucleotide flows by various other characters.
        Format: /\*|[ACMGRSVTWYHKDBN]+/
    KS  The array of nucleotide bases that correspond to the key sequence of each read.

    From GATK: https://software.broadinstitute.org/gatk/guide/article?id=6472

    ID  Read group identifier, RG-ID, identifies which read group each read belongs to using a reference in the RG auxiliary tag.
        RG-ID must be unique in a SAM file.
        The default convention for illumina is the same as PU.
        When used in BQSR, RG-ID is the lowest denominator that differentiates factors contributing to technical batch effects:
        therefore, a read group is effectively treated as a separate run of the instrument in data processing steps
        such as base quality score recalibration, since they are assumed to share the same error model.

    PL  Platform used to produce the read. identifies the sequencing technology used to generate the sequencing data.

    PU  Platform Unit, RG-PU, is unique read group identifier in the platform vendor dialect.
        The default convention for illumina seuqnce reads is <flowcell id>:<flowcell lane number>:<multiplex barcode>
        making it a globally unique identifier across all sequencing data in the world.
        PU is not required by GATK but takes precedence over ID for base recalibration if present.

    SM  The name of the sample sequenced in this read group.
        GATK tools treat all read groups with the same SM value as containing sequencing data for the same sample,
        and this is also the name that will be used for the sample column in the VCF file.
        When sequencing pools of samples, use a pool name instead of an individual sample name.

    LB  DNA preparation library identifier.
        MarkDuplicates uses the LB field to determine which read groups might contain molecular duplicates,
        in case the same DNA library was sequenced on multiple lanes.

    also informative: http://gatkforums.broadinstitute.org/gatk/discussion/6472/read-groups
*/
class HeadRGAtom {
friend class HtsHeader;
friend ostream& operator<<(ostream& o, const HeadRGAtom& read_group);

public:
    kstring_t ID;
    kstring_t PI;
    kstring_t LB;
    kstring_t SM;
    kstring_t PU;
    kstring_t CN;
    kstring_t DS;
    kstring_t DT;
    kstring_t PL;
    kstring_t PM;
    kstring_t PG;
    kstring_t FO;
    kstring_t KS;

    HeadRGAtom();
    ~HeadRGAtom();
    HeadRGAtom(const HeadRGAtom& other);
    HeadRGAtom& operator=(const HeadRGAtom& other);
    operator string() const;
    void set_platform(const Platform& value);
    void expand(const HeadRGAtom& other);

private:
    void encode(kstring_t& buffer) const;
    char* decode(char* position, const char* end);
};
ostream& operator<<(ostream& o, const HeadRGAtom& rg);
template<> bool decode_value< HeadRGAtom >(HeadRGAtom& value, const Value& container);
template<> bool decode_value_by_key< list< HeadRGAtom > >(const Value::Ch* key, list< HeadRGAtom >& value, const Value& container);
bool encode_value(const HeadRGAtom& value, Value& container, Document& document);
bool encode_key_value(const string& key, const HeadRGAtom& value, Value& container, Document& document);
bool encode_key_value(const string& key, const list< HeadRGAtom >& value, Value& container, Document& document);
template <> bool transcode_value< HeadRGAtom >(const Value& from, Value& to, Document& document);

/*  @CO free text comment
*/
class HeadCOAtom {
friend class HtsHeader;
friend ostream& operator<<(ostream& o, const HeadCOAtom& co);

public:
    kstring_t CO;

    HeadCOAtom();
    ~HeadCOAtom();
    HeadCOAtom(const HeadCOAtom& other);
    HeadCOAtom& operator=(const HeadCOAtom& other);

private:
    void encode(kstring_t& buffer) const;
    char* decode(char* position, const char* end);
};
ostream& operator<<(ostream& o, const HeadCOAtom& co);

#endif /* PHENIQS_ATOM_H */