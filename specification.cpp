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

#include "specification.h"

#include <zlib.h>

/*  Feed specification
*/
FeedSpecification::FeedSpecification () :
    direction(IoDirection::UNKNOWN),
    index(numeric_limits< int32_t >::min()),
    platform(Platform::UNKNOWN),
    capacity(DEFAULT_FEED_CAPACITY),
    resolution(DEFAULT_FEED_RESOLUTION),
    phred_offset(0),
    hfile(NULL) {
};
FeedSpecification::FeedSpecification (
    const IoDirection& direction,
    const int32_t& index,
    const URL& url,
    const Platform& platform,
    const uint8_t& phred_offset) :

    direction(direction),
    index(index),
    url(url),
    platform(platform),
    capacity(DEFAULT_FEED_CAPACITY),
    resolution(DEFAULT_FEED_RESOLUTION),
    phred_offset(phred_offset),
    hfile(NULL) {
};
void FeedSpecification::set_capacity(const int& capacity) {
    if(capacity != this->capacity) {
        int aligned(static_cast< int >(capacity / resolution) * resolution);
        if(aligned < capacity) {
            aligned += resolution;
        }
        this->capacity = aligned;
    }
};
void FeedSpecification::set_resolution(const int& resolution) {
    if(resolution != this->resolution) {
        int aligned(static_cast< int >(capacity / resolution) * resolution);
        if(aligned < capacity) {
            aligned += resolution;
        }
        this->resolution = resolution;
        this->capacity = aligned;
    }
};
void FeedSpecification::register_rg(const HeadRGAtom& rg) {
    string key(rg);
    auto record = read_group_by_id.find(key);
    if(record == read_group_by_id.end()) {
        read_group_by_id.emplace(make_pair(key, HeadRGAtom(rg)));
    }
};
void FeedSpecification::register_pg(const HeadPGAtom& pg) {
    string key(pg);
    auto record = program_by_id.find(key);
    if(record == program_by_id.end()) {
        program_by_id.emplace(make_pair(key, HeadPGAtom(pg)));
    }
};
void FeedSpecification::describe(ostream& o) const {
    o << "    ";
    switch (direction) {
        case IoDirection::IN:
            o << "Input";
            break;
        case IoDirection::OUT:
            o << "Output";
            break;
        default:
            break;
    }
    o << " feed No." << index << endl;
    o << "        Type : " << url.type() << endl;
    // if(strlen(url.compression()) > 0) o << "        Compression : " << url.compression() << endl;
    o << "        Resolution : " << resolution << endl;
    o << "        Phred offset : " << to_string(phred_offset) << endl;
    o << "        Platform : " << platform << endl;
    o << "        Buffer capacity : " << capacity << endl;
    o << "        URL : " << url << endl;

    // if(!program_by_id.empty()) {
    //     o << "\tProgram : " << endl;
    //     for(auto& record : program_by_id) {
    //         o << "\t\t" << record.first << endl;
    //     }
    // }
    // if(!read_group_by_id.empty()) {
    //     o << "\tRead Groups : " << endl;
    //     for(auto& record : read_group_by_id) {
    //         o << "\t\t" << record.first << endl;
    //     }
    // }

    o << endl;
};
void FeedSpecification::probe() {
    /*  Probe input file

        Here you can potentially use hfile to probe the file
        and verify file format and potentially examine the first read
    */
    switch(direction) {
        case IoDirection::IN: {
            hfile = hopen(url.c_str(), "r");
            if(url.type() == FormatType::UNKNOWN) {
                ssize_t peeked(0);
                unsigned char* buffer(NULL);
                const ssize_t buffer_capacity(PEEK_BUFFER_CAPACITY);
                if((buffer = static_cast< unsigned char* >(malloc(buffer_capacity))) == NULL) {
                    throw OutOfMemoryError();
                }
                htsFormat format;
                if(!hts_detect_format(hfile, &format)) {
                    switch (format.format) {
                        case htsExactFormat::sam:
                            url.set_type(FormatType::SAM);
                            break;
                        case htsExactFormat::bam:
                            url.set_type(FormatType::BAM);
                            break;
                        case htsExactFormat::bai:
                            url.set_type(FormatType::BAI);
                            break;
                        case htsExactFormat::cram:
                            url.set_type(FormatType::CRAM);
                            break;
                        case htsExactFormat::crai:
                            url.set_type(FormatType::CRAI);
                            break;
                        case htsExactFormat::vcf:
                            url.set_type(FormatType::VCF);
                            break;
                        case htsExactFormat::bcf:
                            url.set_type(FormatType::BCF);
                            break;
                        case htsExactFormat::csi:
                            url.set_type(FormatType::CSI);
                            break;
                        case htsExactFormat::gzi:
                            url.set_type(FormatType::GZI);
                            break;
                        case htsExactFormat::tbi:
                            url.set_type(FormatType::TBI);
                            break;
                        case htsExactFormat::bed:
                            url.set_type(FormatType::BED);
                            break;
                        default:
                            url.set_type(FormatType::UNKNOWN);
                            break;
                    }
                }

                if(url.type() == FormatType::SAM) {
                    peeked = hpeek(hfile, buffer, buffer_capacity);
                    if(peeked > 0) {
                        switch (format.compression) {
                            case htsCompression::gzip:
                            case htsCompression::bgzf: {
                                unsigned char* decompressed_buffer(NULL);
                                if((decompressed_buffer = static_cast< unsigned char* >(malloc(buffer_capacity))) == NULL) {
                                    throw OutOfMemoryError();
                                }
                                z_stream zstream;
                                zstream.zalloc = NULL;
                                zstream.zfree = NULL;
                                zstream.next_in = buffer;
                                zstream.avail_in = static_cast< unsigned >(peeked);
                                zstream.next_out = decompressed_buffer;
                                zstream.avail_out = buffer_capacity;
                                if(inflateInit2(&zstream, 31) == Z_OK) {
                                    while(zstream.total_out < buffer_capacity) {
                                        if(inflate(&zstream, Z_SYNC_FLUSH) != Z_OK) break;
                                    }
                                    inflateEnd(&zstream);
                                    memcpy(buffer, decompressed_buffer, zstream.total_out);
                                    peeked = zstream.total_out;
                                } else {
                                    peeked = 0;
                                }
                                free(decompressed_buffer);
                                break;
                            };
                            case htsCompression::no_compression:
                                break;
                            default:
                                throw InternalError("unknown compression");
                                break;
                        }
                    }
                    if(peeked > 0) {
                        size_t state(0);
                        char* position(reinterpret_cast< char * >(buffer));
                        char* end(position + peeked);
                        while(position < end && position != NULL) {
                            if(state == 0) {
                                if(*position == '\n') {
                                    position++;
                                } else {
                                    if(*position == '@') {
                                        state = 1;
                                    } else {
                                        break;
                                    }
                                }
                            } else if(state == 1) {
                                if((*position >= 'A' && *position <= 'Z') || (*position >= 'a' && *position <= 'z')) {
                                    state = 2;
                                } else {
                                    break;
                                }
                            } else if(state == 2) {
                                if(*position == '+' && position < end && *(position + 1) == '\n') {
                                    url.set_type(FormatType::FASTQ);
                                }
                                break;
                            }
                            if((position = strchr(position, '\n')) != NULL) position++;
                        }
                    }
                }
                free(buffer);
            }
            break;
        };
        case IoDirection::OUT: {
            hfile = hopen(url.c_str(), "w");
            break;
        };
        default:
            break;
    }
};
ostream& operator<<(ostream& o, const FeedSpecification& specification) {
    o << specification.url;
    return o;
};
template<> bool decode_value< FeedSpecification >(FeedSpecification& value, const Value& container) {
    if(container.IsObject()) {
        if(container.IsObject()) {
            decode_value_by_key< IoDirection >("direction", value.direction, container);
            decode_value_by_key< int32_t >("index", value.index, container);
            decode_value_by_key< URL >("url", value.url, container);
            decode_value_by_key< Platform >("platform", value.platform, container);
            decode_value_by_key< uint8_t >("phred offset", value.phred_offset, container);
        } else { throw ConfigurationError("feed node must be a dictionary"); }
        return true;
    } else { throw ConfigurationError("channel node must be a dictionary"); }
};
template<> bool decode_value_by_key< list< FeedSpecification* > >(const Value::Ch* key, list< FeedSpecification* >& value, const Value& container) {
    Value::ConstMemberIterator collection = container.FindMember(key);
    if(collection != container.MemberEnd()) {
        if(!collection->value.IsNull()) {
            if(collection->value.IsArray() && !collection->value.Empty()) {
                for(const auto& element : collection->value.GetArray()) {
                    FeedSpecification* o = new FeedSpecification();
                    if(decode_value(*o, element)) {
                        value.push_back(o);
                    } else {
                        delete o;
                    }
                }
                return true;
            }
        }
    }
    return false;
};

InputSpecification::InputSpecification() :
    decoder(Decoder::UNKNOWN),
    disable_quality_control(false),
    include_filtered(true) {
};
template<> bool decode_value< InputSpecification >(InputSpecification& value, const Value& container) {
    if(decode_value_by_key< list< URL > >("input", value.url_by_segment, container)) {
        decode_value_by_key< Decoder >("decoder", value.decoder, container);
        decode_value_by_key< bool >("disable quality control", value.disable_quality_control, container);
        decode_value_by_key< bool >("long read", value.long_read, container);
        decode_value_by_key< bool >("include filtered", value.include_filtered, container);
        return true;
    }
    return false;
};

/*  Channel specification */
ChannelSpecification::ChannelSpecification() :
    index(0),
    TC(numeric_limits< int32_t >::max()),
    FS({ 0, 0, NULL }),
    CO({ 0, 0, NULL }),
    decoder(Decoder::UNKNOWN),
    disable_quality_control(false),
    long_read(false),
    include_filtered(true),
    undetermined(false),
    concentration(0) {
};
ChannelSpecification::~ChannelSpecification() {
    ks_free(FS);
    ks_free(CO);
};
void ChannelSpecification::describe(ostream& o) const {
    o << "    " << alias() << endl;
    // o << "\tRG index : " << head_read_group->index << endl;
    o << "        ID : " << rg.ID.s << endl;
    if(!ks_empty(rg.PU)) o << "        PU : " << rg.PU.s << endl;
    if(!ks_empty(rg.LB)) o << "        LB : " << rg.LB.s << endl;
    if(!ks_empty(rg.SM)) o << "        SM : " << rg.SM.s << endl;
    if(!ks_empty(rg.CN)) o << "        CN : " << rg.CN.s << endl;
    if(!ks_empty(rg.DS)) o << "        DS : " << rg.DS.s << endl;
    if(!ks_empty(rg.DT)) o << "        DT : " << rg.DT.s << endl;
    if(!ks_empty(rg.PL)) o << "        PL : " << rg.PL.s << endl;
    if(!ks_empty(rg.PM)) o << "        PM : " << rg.PM.s << endl;
    if(!ks_empty(rg.PG)) o << "        PG : " << rg.PG.s << endl;
    if(!ks_empty(rg.FO)) o << "        FO : " << rg.FO.s << endl;
    if(!ks_empty(rg.KS)) o << "        KS : " << rg.KS.s << endl;
    if(!ks_empty(rg.PI)) o << "        PI : " << rg.PI.s << endl;
    if(TC > 0)           o << "        TC : " << TC      << endl;
    if(!ks_empty(FS))    o << "        FS : " << FS.s    << endl;
    if(!ks_empty(CO))    o << "        CO : " << CO.s    << endl;
    if(concentration > 0) {
                    o << "        PC : " << setprecision(numeric_limits<double>::digits10 + 1) << concentration << endl;
    }
    if(undetermined) {
        o << "        Undetermined : true" << endl;
    }
    if(!undetermined) {
        o << endl;
        for(size_t i = 0; i < multiplex_barcode.total_fragments(); i++) {
            o << "        Multiplex barcode No." << i << " : " << multiplex_barcode.iupac_ambiguity(i) << endl;
        }
    }
    if(!url_by_segment.empty()) {
        o << endl;
        size_t i(0);
        for(auto& url : url_by_segment) {
            o << "        Output segment No." << i << " : " << url << endl;
            i++;
        }
    }
    o << endl;
};
void ChannelSpecification::encode(Document& document, Value& node) const {
    Document::AllocatorType& allocator = document.GetAllocator();

    Value specification(kObjectType);
    encode_value(rg, specification, document);
    encode_key_value("FS", FS, specification, document);
    encode_key_value("CO", CO, specification, document);
    if(undetermined) {
        encode_key_value("undetermined", undetermined, specification, document);
    } else {
        encode_key_value("concentration", concentration, specification, document);
        multiplex_barcode.encode_configuration(document, specification, "barcode");
    }

    if(!url_by_segment.empty()) {
        Value collection(kArrayType);
        for(auto& url : url_by_segment) {
            encode_element(url, collection, document);
        }
        specification.AddMember("output", collection.Move(), allocator);
    }
    node.PushBack(specification, allocator);
};
ostream& operator<<(ostream& o, const ChannelSpecification& specification) {
    o << specification.alias();
    return o;
};
template<> bool decode_value< ChannelSpecification >(ChannelSpecification& value, const Value& container) {
    if(container.IsObject()) {
        decode_value_by_key< int32_t >("index", value.index, container);
        decode_value_by_key< uint32_t >("TC", value.TC, container);
        decode_value_by_key< kstring_t >("FS", value.FS, container);
        decode_value_by_key< kstring_t >("CO", value.CO, container);
        decode_value_by_key< Decoder >("decoder", value.decoder, container);
        decode_value_by_key< bool >("disable quality control", value.disable_quality_control, container);
        decode_value_by_key< bool >("long read", value.long_read, container);
        decode_value_by_key< bool >("include filtered", value.include_filtered, container);
        decode_value_by_key< bool >("undetermined", value.undetermined, container);
        decode_value_by_key< double >("concentration", value.concentration, container);
        decode_value_by_key< Barcode >("barcode", value.multiplex_barcode, container);
        decode_value_by_key< list< URL > >("output", value.url_by_segment, container);

        uint8_t masking_threshold;
        if(decode_value_by_key< uint8_t >("masking threshold", masking_threshold, container)) {
            value.multiplex_barcode.set_threshold(masking_threshold);
        }

        vector< uint8_t> multiplex_barcode_tolerance;
        if(decode_value_by_key< vector< uint8_t > >("multiplex barcode tolerance", multiplex_barcode_tolerance, container)) {
            value.multiplex_barcode.set_tolerance(multiplex_barcode_tolerance);
        }

        if(decode_value< HeadRGAtom >(value.rg, container)) {
            // if a read group could be decoded
        }
        return true;
    } else { throw ConfigurationError("channel node must be a dictionary"); }
};
template<> bool decode_value_by_key< list< ChannelSpecification > >(const Value::Ch* key, list< ChannelSpecification >& value, const Value& container) {
    Value::ConstMemberIterator collection = container.FindMember(key);
    if(collection != container.MemberEnd()) {
        if(!collection->value.IsNull()) {
            if(collection->value.IsArray() && !collection->value.Empty()) {
                for(const auto& element : collection->value.GetArray()) {
                    if(element.IsObject()) {
                        ChannelSpecification o;
                        if(decode_value(o, element)) {
                            value.emplace_back(o);
                        }
                    } else { throw ConfigurationError("channel node must be a dictionary"); }
                }
                return true;
            }
        }
    }
    return false;
};
template <> bool transcode_value< ChannelSpecification >(const Value& from, Value& to, Document& document) {
    if(from.IsObject()) {
        to.SetObject();
        transcode_value_by_key< string >("flowcell id", from, to, document);
        transcode_value_by_key< int32_t >("flowcell lane number", from, to, document);
        transcode_value_by_key< string >("ID", from, to, document);
        transcode_value_by_key< string >("LB", from, to, document);
        transcode_value_by_key< string >("SM", from, to, document);
        transcode_value_by_key< string >("PU", from, to, document);
        transcode_value_by_key< string >("CN", from, to, document);
        transcode_value_by_key< string >("DS", from, to, document);
        transcode_value_by_key< string >("DT", from, to, document);
        transcode_value_by_key< string >("PL", from, to, document);
        transcode_value_by_key< string >("PM", from, to, document);
        transcode_value_by_key< string >("PG", from, to, document);
        transcode_value_by_key< string >("FO", from, to, document);
        transcode_value_by_key< string >("KS", from, to, document);
        transcode_value_by_key< uint32_t >("TC", from, to, document);
        transcode_value_by_key< Decoder >("decoder", from, to, document);
        transcode_value_by_key< bool >("disable quality control", from, to, document);
        transcode_value_by_key< bool >("long read", from, to, document);
        transcode_value_by_key< bool >("include filtered", from, to, document);
        transcode_value_by_key< bool >("undetermined", from, to, document);
        transcode_value_by_key< double >("concentration", from, to, document);
        transcode_value_by_key< uint8_t >("masking threshold", from, to, document);
        transcode_value_by_key< vector< uint8_t > >("multiplex barcode tolerance", from, to, document);
        return true;
    } else { throw ConfigurationError("channel node must be a dictionary"); }
    return false;
};