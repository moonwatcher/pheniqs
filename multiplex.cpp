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

#include "multiplex.h"

/*  AveragePhreadAccumulator */

AveragePhreadAccumulator::AveragePhreadAccumulator() :
    count(0),
    min_value(0),
    max_value(0),
    sum_value(0),
    mean_value(0),
    distribution(EFFECTIVE_PHRED_RANGE, 0) {
};
void AveragePhreadAccumulator::finalize() {
    if(count > 0) {
        mean_value = sum_value / double(count);
    }
};
AveragePhreadAccumulator& AveragePhreadAccumulator::operator=(const AveragePhreadAccumulator& rhs) {
    if(this != &rhs) {
        count = rhs.count;
        min_value = rhs.min_value;
        max_value = rhs.max_value;
        sum_value = rhs.sum_value;
        mean_value = rhs.mean_value;
        distribution = rhs.distribution;
    }
    return *this;
};
AveragePhreadAccumulator& AveragePhreadAccumulator::operator+=(const AveragePhreadAccumulator& rhs) {
    count += rhs.count;
    sum_value += rhs.sum_value;
    min_value = min(min_value, rhs.min_value);
    max_value = max(max_value, rhs.max_value);
    for(size_t i(0); i < distribution.size(); ++i) {
        distribution[i] += rhs.distribution[i];
    }
    return *this;
};

/*  SegmentAccumulator */

SegmentAccumulator::SegmentAccumulator() try :
    capacity(0),
    shortest(numeric_limits< int32_t >::max()),
    nucleic_acid_count_by_code(IUPAC_CODE_SIZE, 0) {

    } catch(Error& error) {
        error.push("SegmentAccumulator");
        throw;
};
void SegmentAccumulator::finalize() {
    if(shortest == numeric_limits< int32_t >::max()) {
        shortest = 0;
    }
    for(auto& c : cycle_by_index) {
        c.finalize();
    }
    average_phred.finalize();
};
SegmentAccumulator& SegmentAccumulator::operator+=(const SegmentAccumulator& rhs) {
    if(rhs.capacity > capacity) {
        cycle_by_index.resize(rhs.capacity);
        capacity = rhs.capacity;
    }
    shortest = min(shortest, rhs.shortest);
    for(uint8_t c(0); c < nucleic_acid_count_by_code.size(); ++c) {
        nucleic_acid_count_by_code[c] += rhs.nucleic_acid_count_by_code[c];
    }

    for(int32_t i(0); i < rhs.capacity; ++i) {
        cycle_by_index[i] += rhs.cycle_by_index[i];
    }
    average_phred += rhs.average_phred;
    return *this;
};
bool encode_value(const SegmentAccumulator& value, Value& container, Document& document) {
    if(container.IsObject()) {
        Document::AllocatorType& allocator = document.GetAllocator();

        encode_key_value("min sequence length", value.shortest, container, document);
        encode_key_value("max sequence length", value.capacity, container, document);
        Value quality_control_by_cycle(kObjectType);
        Value quality_control_by_nucleotide(kArrayType);
        for(uint8_t n(0); n < value.nucleic_acid_count_by_code.size(); ++n) {
            if(value.nucleic_acid_count_by_code[n] > 0) {
                Value cycle_quality_distribution(kObjectType);
                Value cycle_count(kArrayType);
                Value cycle_quality_first_quartile(kArrayType);
                Value cycle_quality_third_quartile(kArrayType);
                Value cycle_quality_interquartile_range(kArrayType);
                Value cycle_quality_left_whisker(kArrayType);
                Value cycle_quality_right_whisker(kArrayType);
                Value cycle_quality_min(kArrayType);
                Value cycle_quality_max(kArrayType);
                Value cycle_quality_mean(kArrayType);
                Value cycle_quality_median(kArrayType);
                for(size_t c(0); c < value.cycle_by_index.size(); ++c) {
                    cycle_count.PushBack(Value(value.cycle_by_index[c].nucleotide_by_code[n].count).Move(), allocator);
                    cycle_quality_first_quartile.PushBack(Value(value.cycle_by_index[c].nucleotide_by_code[n].Q1).Move(), allocator);
                    cycle_quality_third_quartile.PushBack(Value(value.cycle_by_index[c].nucleotide_by_code[n].Q3).Move(), allocator);
                    cycle_quality_interquartile_range.PushBack(Value(value.cycle_by_index[c].nucleotide_by_code[n].IQR).Move(), allocator);
                    cycle_quality_left_whisker.PushBack(Value(value.cycle_by_index[c].nucleotide_by_code[n].LW).Move(), allocator);
                    cycle_quality_right_whisker.PushBack(Value(value.cycle_by_index[c].nucleotide_by_code[n].RW).Move(), allocator);
                    cycle_quality_min.PushBack(Value(value.cycle_by_index[c].nucleotide_by_code[n].min_quality).Move(), allocator);
                    cycle_quality_max.PushBack(Value(value.cycle_by_index[c].nucleotide_by_code[n].max_quality).Move(), allocator);
                    cycle_quality_mean.PushBack(Value(value.cycle_by_index[c].nucleotide_by_code[n].mean_quality).Move(), allocator);
                    cycle_quality_median.PushBack(Value(value.cycle_by_index[c].nucleotide_by_code[n].median_quality).Move(), allocator);
                }
                cycle_quality_distribution.AddMember("cycle count", cycle_count, allocator);
                cycle_quality_distribution.AddMember("cycle quality first quartile", cycle_quality_first_quartile, allocator);
                cycle_quality_distribution.AddMember("cycle quality third quartile", cycle_quality_third_quartile, allocator);
                cycle_quality_distribution.AddMember("cycle quality interquartile range", cycle_quality_interquartile_range, allocator);
                cycle_quality_distribution.AddMember("cycle quality left whisker", cycle_quality_left_whisker, allocator);
                cycle_quality_distribution.AddMember("cycle quality right whisker", cycle_quality_right_whisker, allocator);
                cycle_quality_distribution.AddMember("cycle quality min", cycle_quality_min, allocator);
                cycle_quality_distribution.AddMember("cycle quality max", cycle_quality_max, allocator);
                cycle_quality_distribution.AddMember("cycle quality mean", cycle_quality_mean, allocator);
                cycle_quality_distribution.AddMember("cycle quality median", cycle_quality_median, allocator);
                if(n > 0) {
                    Value cycle_nucleotide_quality_report(kObjectType);
                    encode_key_value("nucleotide count", value.nucleic_acid_count_by_code[n], cycle_nucleotide_quality_report, document);
                    encode_key_value("nucleotide", string(1, BamToAmbiguousAscii[n]), cycle_nucleotide_quality_report, document);
                    cycle_nucleotide_quality_report.AddMember("cycle quality distribution", cycle_quality_distribution, allocator);
                    quality_control_by_nucleotide.PushBack(cycle_nucleotide_quality_report, allocator);
                } else {
                    quality_control_by_cycle.AddMember("cycle quality distribution", cycle_quality_distribution, allocator);
                }
            }
        }
        container.AddMember("quality control by nucleotide", quality_control_by_nucleotide, allocator);
        container.AddMember("quality control by cycle", quality_control_by_cycle, allocator);

        Value average_phred_report(kObjectType);
        encode_key_value("average phred score min", value.average_phred.min_value, average_phred_report, document);
        encode_key_value("average phred score max", value.average_phred.max_value, average_phred_report, document);
        encode_key_value("average phred score mean", value.average_phred.mean_value, average_phred_report, document);

        Value segment_accumulator(kArrayType);
        for(size_t i(0); i < value.average_phred.distribution.size(); ++i) {
            segment_accumulator.PushBack(Value(value.average_phred.distribution[i]).Move(), allocator);
        }
        average_phred_report.AddMember("average phred score distribution", segment_accumulator, allocator);
        container.AddMember("average phred score report", average_phred_report, allocator);
        return true;
    } else { throw ConfigurationError("feed accumulator element must be a dictionary"); }
};

/*  ReadAccumulator */

ReadAccumulator::ReadAccumulator(const int32_t& cardinality) try :
    segment_accumulator_by_index(cardinality) {

    } catch(Error& error) {
        error.push("Channel");
        throw;
};
void ReadAccumulator::finalize() {
    for(auto& segment_accumulator : segment_accumulator_by_index) {
        segment_accumulator.finalize();
    }
};
ReadAccumulator& ReadAccumulator::operator+=(const ReadAccumulator& rhs) {
    for(size_t index(0); index < segment_accumulator_by_index.size(); ++index) {
        segment_accumulator_by_index[index] += rhs.segment_accumulator_by_index[index];
    }
    return *this;
};
bool encode_value(const ReadAccumulator& value, Value& container, Document& document) {
    if(container.IsArray()) {
        for(auto& segment_accumulator : value.segment_accumulator_by_index) {
            Value segment_report(kObjectType);
            encode_value(segment_accumulator, segment_report, document);
            container.PushBack(segment_report.Move(), document.GetAllocator());
        }
        return true;
    } else { throw InternalError("Read accumulator container must be an array"); }
};

/*  Channel */

Channel::Channel(const Value& ontology) try :
    index(decode_value_by_key< int32_t >("index", ontology)),
    // rg(ontology),
    filter_outgoing_qc_fail(decode_value_by_key< bool >("filter outgoing qc fail", ontology)),
    enable_quality_control(decode_value_by_key< bool >("enable quality control", ontology)),
    output_feed_url_by_segment(decode_value_by_key< list< URL > >("output", ontology)),
    read_accumulator(decode_value_by_key< int32_t >("segment cardinality", ontology)) {

    } catch(Error& error) {
        error.push("Channel");
        throw;
};
Channel::Channel(const Channel& other) :
    index(other.index),
    // rg(other.rg),
    filter_outgoing_qc_fail(other.filter_outgoing_qc_fail),
    enable_quality_control(other.enable_quality_control),
    output_feed_url_by_segment(other.output_feed_url_by_segment),
    output_feed_lock_order(other.output_feed_lock_order),
    output_feed_by_segment(other.output_feed_by_segment),
    read_accumulator(other.read_accumulator) {
};
void Channel::populate(unordered_map< URL, Feed* >& feed_by_url) {
    map< int32_t, Feed* > feed_by_index;

    /* populate the output feed by segment array */
    output_feed_by_segment.reserve(output_feed_url_by_segment.size());
    for(const auto& url : output_feed_url_by_segment) {
        Feed* feed(feed_by_url[url]);
        output_feed_by_segment.emplace_back(feed);
        if(feed_by_index.count(feed->index) == 0) {
            feed_by_index.emplace(make_pair(feed->index, feed));
        }
    }
    output_feed_by_segment.shrink_to_fit();

    /* populate the output feed lock order array */
    output_feed_lock_order.reserve(feed_by_index.size());
    for(auto& record : feed_by_index) {
        /* /dev/null is not really being written to so we don't need to lock it */
        if(!record.second->is_dev_null()) {
            output_feed_lock_order.push_back(record.second);
        }
    }
    output_feed_lock_order.shrink_to_fit();
};
void Channel::finalize() {
    read_accumulator.finalize();
};
void Channel::encode(Value& container, Document& document) const {
    if(container.IsObject()) {
        // encode_value(rg, container, document);
        Value quality_control_by_segment(kArrayType);
        /*
        size_t index(0);
        for(const auto& url : output_feed_url_by_segment) {
            Value segment_report(kObjectType);
            encode_value(read_accumulator.segment_accumulator_by_index[index], segment_report, document);
            encode_key_value("url", url, segment_report, document);
            quality_control_by_segment.PushBack(segment_report.Move(), document.GetAllocator());
            ++index;
        }
        */
        encode_value(read_accumulator, quality_control_by_segment, document);
        container.AddMember("quality control by segment", quality_control_by_segment.Move(), document.GetAllocator());
    } else { throw ConfigurationError("element must be a dictionary"); }
};
Channel& Channel::operator+=(const Channel& rhs) {
    read_accumulator += rhs.read_accumulator;
    return *this;
};
template<> vector< Channel > decode_value_by_key(const Value::Ch* key, const Value& container) {
    vector< Channel > value;
    Value::ConstMemberIterator decoder_reference = container.FindMember(key);
    if(decoder_reference != container.MemberEnd()) {
        Value::ConstMemberIterator undetermined_reference = decoder_reference->value.FindMember("undetermined");
        if(undetermined_reference != decoder_reference->value.MemberEnd()) {
            Value::ConstMemberIterator codec_reference = decoder_reference->value.FindMember("codec");
            if(codec_reference != decoder_reference->value.MemberEnd()) {
                value.reserve(codec_reference->value.MemberCount() + 1);
                value.emplace_back(undetermined_reference->value);
                for(auto& record : codec_reference->value.GetObject()) {
                    value.emplace_back(record.value);
                }
            } else {
                value.reserve(1);
                value.emplace_back(undetermined_reference->value);
            }
        } else { throw ConfigurationError("decoder must declare an undetermined element"); }
    }
    return value;
};

/*  Multiplexer */

Multiplexer::Multiplexer(const Value& ontology) try :
    filter_outgoing_qc_fail(decode_value_by_key< bool >("filter outgoing qc fail", ontology)),
    enable_quality_control(decode_value_by_key< bool >("enable quality control", ontology)),
    channel_by_index(decode_value_by_key< vector< Channel > >("multiplex", ontology)) {

    } catch(Error& error) {
        error.push("Multiplexer");
        throw;
};
Multiplexer::Multiplexer(const Multiplexer& other) :
    filter_outgoing_qc_fail(other.filter_outgoing_qc_fail),
    enable_quality_control(other.enable_quality_control),
    channel_by_index(other.channel_by_index) {
};
Multiplexer& Multiplexer::operator+=(const Multiplexer& rhs) {
    if(enable_quality_control) {
        for(size_t index(0); index < channel_by_index.size(); ++index) {
            channel_by_index[index] += rhs.channel_by_index[index];
        }
    }
    return *this;
};
void Multiplexer::finalize() {
    if(enable_quality_control) {
        for(auto& channel : channel_by_index) {
            channel.finalize();
        }
    }
};
void Multiplexer::encode(Value& container, Document& document) const {
    if(enable_quality_control) {
        if(container.IsObject()) {
            Value channel_array(kArrayType);
            for(auto& channel : channel_by_index) {
                Value channel_report(kObjectType);
                channel.encode(channel_report, document);
                channel_array.PushBack(channel_report.Move(), document.GetAllocator());
            }
            container.AddMember("multiplex", channel_array.Move(), document.GetAllocator());
        } else { throw ConfigurationError("element must be a dictionary"); }
    }
};
