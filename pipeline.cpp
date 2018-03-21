/*
    Pheniqs : PHilology ENcoder wIth Quality Statistics
    Copyright (C) 2017  Lior Galanti
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

#include "pipeline.h"

/*  Pivot */

Pivot::Pivot(Pipeline& pipeline) :
    index(pipeline.pivots.size()),
    action(pipeline.environment.action),
    decoder(pipeline.environment.decoder),
    multiplex_barcode(pipeline.environment.total_multiplex_barcode_segments),
    molecular_barcode(pipeline.environment.total_molecular_barcode_segments),
    accumulator(*pipeline.environment.input_specification),
    pipeline(pipeline),
    disable_quality_control(pipeline.environment.disable_quality_control),
    long_read(pipeline.environment.long_read),
    leading_segment(NULL) {

    /* create input segments */
    input.reserve(pipeline.environment.total_input_segments);
    for(size_t i = 0; i < pipeline.environment.total_input_segments; i++) {
        input.emplace_back(i, i + 1, pipeline.environment.total_input_segments, pipeline.environment.platform);
    }

    /* create output segments */
    output.reserve(pipeline.environment.total_output_segments);
    for(size_t i = 0; i < pipeline.environment.total_output_segments; i++) {
        output.emplace_back(i, i + 1, pipeline.environment.total_output_segments, pipeline.environment.platform);
    }

    /* set first output segment READ1 flag ON */
    if(pipeline.environment.total_output_segments > 0) {
        output[0].flag |= uint16_t(HtsFlag::READ1);
    }

    /* set last output segment READ2 flag ON */
    if(pipeline.environment.total_output_segments > 1) {
        output[pipeline.environment.total_output_segments - 1].flag |= uint16_t(HtsFlag::READ2);
    }

    leading_segment = &(input[pipeline.environment.leading_segment_index]);

    /*
        in long read accumulation mode channel statistics
        are collected on the pivots and only at the end summed up
        on the channel. otherwise they are collected directly on the channel
    */
    if(!disable_quality_control && long_read) {
        switch (action) {
            case ProgramAction::DEMULTIPLEX: {
                channel_by_barcode.reserve(pipeline.environment.channel_specifications.size());
                for(const auto specification : pipeline.environment.channel_specifications) {
                    if(!specification->multiplex_barcode.empty()) {
                        channel_by_barcode.emplace(make_pair(specification->multiplex_barcode, *specification));
                    }
                }
                break;
            };
            case ProgramAction::QUALITY: {
                channel_by_read_group_id.reserve(pipeline.environment.channel_specifications.size());
                for(const auto specification : pipeline.environment.channel_specifications) {
                    if(specification->rg.ID.l > 0) {
                        channel_by_read_group_id.emplace(make_pair(specification->rg, *specification));
                    }
                }
                break;
            };
            default:
                break;
        }
    }
    clear();
};
void Pivot::start() {
    pivot_thread = thread(&Pivot::run, this);
};
inline void Pivot::clear() {
    /* Those get assigned either way so no need to reset them
    determined = false;
    filtered = false;
    multiplex_probability = 0;
    conditioned_multiplex_probability = 0;
    multiplex_distance = 0;
    decoded_multiplex_channel = NULL; */
    multiplex_barcode.clear();
    molecular_barcode.clear();
    for(auto& segment : input) segment.clear();
    for(auto& segment : output) segment.clear();
};
void Pivot::run() {
    switch (action) {
        case ProgramAction::DEMULTIPLEX: {
            switch(decoder) {
            case(Decoder::PAMLD): {
                while(pipeline.pull(*this)) {
                    validate();
                    transform();
                    decode_with_pamld();
                    encode_auxiliary();
                    push();
                    increment();
                    clear();
                }
                break;
            };
            case(Decoder::MDD): {
                while(pipeline.pull(*this)) {
                    validate();
                    transform();
                    decode_with_mdd();
                    encode_auxiliary();
                    push();
                    increment();
                    clear();
                }
                break;
            };
            case(Decoder::BENCHMARK): {
                while(pipeline.pull(*this)) {
                    validate();
                    transform();
                    decode_with_mdd();
                    encode_mmd_benchmark_auxiliary();
                    decode_with_pamld();
                    encode_pamld_benchmark_auxiliary();
                    encode_auxiliary();
                    push();
                    increment();
                    clear();
                }
                break;
            };
            default: break;
            }
            break;
        };
        case ProgramAction::QUALITY: {
            while(pipeline.pull(*this)) {
                validate();
                transform();
                decode_tagged_channel();
                copy_auxiliary();
                push();
                increment();
                clear();
            }
            break;
        };
        default:
            break;
    }
};
inline void Pivot::validate() {
    if(input.size() > 1) {
        /* validate that all segments in the pivot have the same identifier */
        const kstring_t& baseline = input[0].name;
        for(size_t i = 1; i < input.size(); i++) {
            Segment& segment = input[i];
            if((baseline.l != segment.name.l) || strncmp(baseline.s, segment.name.s, baseline.l)) {
                throw SequenceError("read segments out of sync " + string(segment.name.s, segment.name.l) + " and " + string(baseline.s, baseline.l));
            }
        }
    }
};
inline void Pivot::transform() {
    // populate the output
    for (auto& transform : pipeline.environment.template_transforms ) {
        const Segment& from = input[transform.token.input_segment_index];
        Segment& to = output[transform.output_segment_index];
        to.sequence.append(from.sequence, transform);
    }

    // populate the multiplex barcode
    for (auto& transform : pipeline.environment.multiplex_barcode_transforms) {
        const Segment& from = input[transform.token.input_segment_index];
        multiplex_barcode.append(transform.output_segment_index, from.sequence, transform);
    }

    // populate the molecular barcode
    for (auto& transform : pipeline.environment.molecular_barcode_transforms) {
        const Segment& from = input[transform.token.input_segment_index];
        molecular_barcode.append(transform.output_segment_index, from.sequence, transform);
    }

    // assign the pivot qc_fail flag from the leader
    filtered = leading_segment->get_qcfail();

    for (auto& segment : output) {
        kputsn(leading_segment->name.s, leading_segment->name.l, &segment.name);
        segment.set_qcfail(leading_segment->get_qcfail());
        segment.auxiliary.XI = leading_segment->auxiliary.XI;
    }
};
inline void Pivot::decode_with_pamld() {
    multiplex_distance = pipeline.environment.multiplex_barcode_width();
    decoded_multiplex_channel = pipeline.undetermined;
    conditioned_multiplex_probability = 0;
    multiplex_probability = 0;
    determined = false;

    /*  Compute P(observed|barcode) for each barcode
        Keep track of the channel that yield the maximal prior adjusted probability.
        If r is the observed sequence and b is the barcode sequence
        P(r|b) is the probability that r was observed given b was sequenced.
        Accumulate all prior adjusted probabilities P(b) * P(r|b), in sigma
        using the Kahan summation algorithm to minimize floating point drift
        see https://en.wikipedia.org/wiki/Kahan_summation_algorithm
    */
    double adjusted = 0;
    double compensation = 0;
    double sigma = 0;
    double y = 0;
    double t = 0;
    double c = 0;
    double p = 0;
    size_t d = 0;
    for (auto channel : pipeline.channels) {
        channel->multiplex_barcode.accurate_decoding_probability(multiplex_barcode, c, d);
        p = c * channel->concentration;
        y = p - compensation;
        t = sigma + y;
        compensation = (t - sigma) - y;
        sigma = t;
        if (p > adjusted) {
            decoded_multiplex_channel = channel;
            conditioned_multiplex_probability = c;
            multiplex_distance = d;
            adjusted = p;
        }
    }

    /*  Compute P(barcode|observed)
        P(b|r), the probability that b was sequenced given r was observed
        P(b|r) = P(r|b) * P(b) / ( P(noise) * P(r|noise) + sigma )
        where sigma = sum of P(r|b) * P(b) over b
    */
    multiplex_probability = adjusted / (sigma + pipeline.environment.adjusted_noise_probability);

    /* Check for decoding failure and assign to the undetermined channel if decoding failed */
    if (conditioned_multiplex_probability > pipeline.conditioned_probability_threshold &&
        multiplex_probability > pipeline.environment.confidence) {
        determined = true;

    } else {
        decoded_multiplex_channel = pipeline.undetermined;
        conditioned_multiplex_probability = 0;
        multiplex_distance = 0;
        multiplex_probability = 1;
    }
};
inline void Pivot::decode_with_mdd() {
    multiplex_distance = pipeline.environment.multiplex_barcode_width();
    decoded_multiplex_channel = pipeline.undetermined;
    determined = false;

    /* First try a perfect match to the full barcode sequence */
    auto record = pipeline.channel_by_barcode.find(multiplex_barcode);
    if (record != pipeline.channel_by_barcode.end()) {
        decoded_multiplex_channel = record->second;
        multiplex_distance = 0;
        determined = true;

    } else {
        /* If no exact match was found try error correction */
        for (auto channel : pipeline.channels) {
            if (channel->multiplex_barcode.corrected_match(multiplex_barcode, multiplex_distance)) {
                decoded_multiplex_channel = channel;
                determined = true;
                break;
            }
        }
        if(!determined) {
            multiplex_distance = 0;
        }
    }
};
inline void Pivot::decode_tagged_channel() {
    if(leading_segment->auxiliary.RG.l > 0) {
        string rg(leading_segment->auxiliary.RG.s, leading_segment->auxiliary.RG.l);
        auto record = pipeline.channel_by_read_group_id.find(rg);
        if (record != pipeline.channel_by_read_group_id.end()) {
            decoded_multiplex_channel = record->second;
        }
    }
};
inline void Pivot::encode_auxiliary () {
    if (decoded_multiplex_channel != NULL) {
        for(auto& segment: output) {

            if(decoder == Decoder::PAMLD) {
                segment.auxiliary.DQ = 1.0 - multiplex_probability;
            }

            // sequence auxiliary tags
            segment.sequence.expected_error(segment.auxiliary.EE);

            // pivot auxiliary tags
            segment.auxiliary.set_multiplex_barcode(multiplex_barcode);
            segment.auxiliary.set_molecular_barcode(molecular_barcode);

            // channel auxiliary tags
            kputsn(decoded_multiplex_channel->rg.ID.s, decoded_multiplex_channel->rg.ID.l, &segment.auxiliary.RG);

            /* those dont change between consecutive reads in the same channel
            kputsn(decoded_multiplex_channel->rg.LB.s, decoded_multiplex_channel->rg.LB.l, &segment.auxiliary.LB);
            kputsn(decoded_multiplex_channel->rg.PU.s, decoded_multiplex_channel->rg.PU.l, &segment.auxiliary.PU);
            kputsn(decoded_multiplex_channel->rg.FS.s, decoded_multiplex_channel->rg.FS.l, &segment.auxiliary.FS);
            kputsn(decoded_multiplex_channel->rg.PG.s, decoded_multiplex_channel->rg.PG.l, &segment.auxiliary.PG);
            kputsn(decoded_multiplex_channel->rg.CO.s, decoded_multiplex_channel->rg.CO.l, &segment.auxiliary.CO); */
        }
    }
};
inline void Pivot::copy_auxiliary () {
    for(auto& segment: output) {
        segment.auxiliary.imitate(leading_segment->auxiliary);
    }
};
inline void Pivot::push() {
    if (decoded_multiplex_channel != NULL) {
        decoded_multiplex_channel->push(*this);
    }
};
inline void Pivot::increment() {
    if(!disable_quality_control) {
        /* input quality tracking */
        accumulator.increment(filtered, input);

        /* long read track channel quality on the pivot */
        if(long_read) {
            switch (action) {
                case ProgramAction::DEMULTIPLEX: {
                    if(!decoded_multiplex_channel->barcode_key.empty()) {
                        const auto& record = channel_by_barcode.find(decoded_multiplex_channel->barcode_key);
                        if(record != channel_by_barcode.end()) {
                            record->second.increment(
                                filtered,
                                multiplex_probability,
                                multiplex_distance,
                                output
                            );
                        }
                    }
                    break;
                };
                case ProgramAction::QUALITY: {
                    if(!decoded_multiplex_channel->rg_key.empty()) {
                        const auto& record = channel_by_read_group_id.find(decoded_multiplex_channel->rg_key);
                        if(record != channel_by_read_group_id.end()) {
                            record->second.increment(
                                filtered,
                                multiplex_probability,
                                multiplex_distance,
                                output
                            );
                        }
                    }
                    break;
                };
                default:
                    break;
            }
        }
    }
};
inline void Pivot::encode_mmd_benchmark_auxiliary () {
    if(decoded_multiplex_channel != NULL) {
        for(auto& segment: output) {
            segment.auxiliary.set_mdd_barcode(decoded_multiplex_channel->multiplex_barcode);
            segment.auxiliary.YD = multiplex_distance;
        }
    }
};
inline void Pivot::encode_pamld_benchmark_auxiliary () {
    if(decoded_multiplex_channel != NULL) {
        for(auto& segment: output) {
            segment.auxiliary.set_pamld_barcode(decoded_multiplex_channel->multiplex_barcode);
            segment.auxiliary.XD = multiplex_distance;
            segment.auxiliary.XP = conditioned_multiplex_probability;
        }
    }
};

/*  Channel */

Channel::Channel(const Pipeline& pipeline, const ChannelSpecification& specification) :
    index(specification.index),
    barcode_key(specification.multiplex_barcode),
    rg_key(specification.rg),
    concentration(specification.concentration),
    multiplex_barcode(specification.multiplex_barcode),
    disable_quality_control(specification.disable_quality_control),
    long_read(specification.long_read),
    include_filtered(specification.include_filtered),
    undetermined(specification.undetermined),
    writable(specification.writable()),
    rg(specification.rg),
    channel_accumulator(specification) {

    if(writable) {
        map< size_t, Feed* > feed_by_index;
        output_feeds.reserve(specification.output_urls.size());
        for(const auto& url : specification.output_urls) {
            Feed* feed = pipeline.resolve_feed(url, IoDirection::OUT);
            output_feeds.push_back(feed);
            if (feed_by_index.find(feed->index()) == feed_by_index.end()) {
                feed_by_index[feed->index()] = feed;
            }
        }
        unique_output_feeds.reserve(feed_by_index.size());
        for(const auto& record : feed_by_index) {
            unique_output_feeds.push_back(record.second);
        }
    }
};
Channel::~Channel() {
};
inline void Channel::increment(Pivot& pivot) {
    lock_guard<mutex> channel_lock(channel_mutex);
    channel_accumulator.increment (
        pivot.filtered,
        pivot.multiplex_probability,
        pivot.multiplex_distance,
        pivot.output
    );
};
void Channel::push(Pivot& pivot) {
    if(writable) {
        // acquire a push lock for all feeds in a fixed order
        vector< unique_lock< mutex > > feed_locks;
        feed_locks.reserve(unique_output_feeds.size());
        for(const auto feed : unique_output_feeds) {
            feed_locks.push_back(feed->acquire_push_lock());
        }

        // push the segments to the output feeds
        for(size_t i = 0; i < output_feeds.size(); i++) {
            output_feeds[i]->push(pivot.output[i]);
        }

        // release the locks on the feeds in reverse order
        for (auto feed_lock = feed_locks.rbegin(); feed_lock != feed_locks.rend(); ++feed_lock) {
            feed_lock->unlock();
        }
    }

    // for short reads the channels track their own quality
    if(!disable_quality_control && !long_read) {
        increment(pivot);
    }
};
void Channel::encode(Document& document, Value& value) const {
    channel_accumulator.encode(document, value);
};
void Channel::finalize(const PipelineAccumulator& pipeline_accumulator) {
    channel_accumulator.finalize(pipeline_accumulator);
};

/*  Pipeline */

Pipeline::Pipeline(Environment& environment) :
    environment(environment),
    conditioned_probability_threshold(environment.random_word_probability),
    disable_quality_control(environment.disable_quality_control),
    long_read(environment.long_read),
    end_of_input(false),
    thread_pool({NULL, 0}),
    undetermined(NULL),
    input_accumulator(NULL),
    output_accumulator(NULL) {

    thread_pool.pool = hts_tpool_init(environment.threads);
    if (!thread_pool.pool) {
        throw InternalError("error creating thread pool");
    }
};
Pipeline::~Pipeline() {
    hts_tpool_destroy(thread_pool.pool);
    for(auto feed : unique_input_feeds) {
        delete feed;
    }
    for(auto feed : unique_output_feeds) {
        delete feed;
    }
    input_feeds.clear();
    unique_input_feeds.clear();
    unique_output_feeds.clear();

    for(auto pivot : pivots) {
        delete pivot;
    }
    for(auto channel : channels) {
        delete channel;
    }
    delete undetermined;
    undetermined = NULL;
    channels.clear();
    channel_by_barcode.clear();
    channel_by_read_group_id.clear();

    delete input_accumulator;
    delete output_accumulator;
};
void Pipeline::initialize_channels() {
    channels.reserve(environment.channel_specifications.size());
    for(const auto specification : environment.channel_specifications) {
        Channel* channel = new Channel(*this, *specification);
        if(!channel->undetermined) {
            channels.push_back(channel);

        } else {
            /*  the undetermined channel is not present in the channels array
                and is referenced separately by the undetermined pointer */
            undetermined = channel;
        }

        /* channel by read group id lookup table */
        if(!channel->rg_key.empty()) {
            if (channel_by_read_group_id.find(channel->rg_key) == channel_by_read_group_id.end()) {
                channel_by_read_group_id[channel->rg_key] = channel;
            }
        }

        /* channel by concatenated barcode string lookup table */
        if(!channel->barcode_key.empty()) {
            if (channel_by_barcode.find(channel->barcode_key) == channel_by_barcode.end()) {
                channel_by_barcode[channel->barcode_key] = channel;
            }
        }
    }
};
void Pipeline::initialize_pivots() {
    pivots.reserve(environment.transforms);
    for(size_t i = 0; i < environment.transforms; i++) {
        Pivot* pivot = new Pivot(*this);
        pivots.push_back(pivot);
    }
};
void Pipeline::execute() {
    switch (environment.action) {
        case ProgramAction::DEMULTIPLEX: {
            environment.validate_urls();
            load_input_feeds();
            load_output_feeds();
            break;
        };
        case ProgramAction::QUALITY: {
            probe(environment.input_url);
            environment.load_transformation();
            environment.load_channels();
            environment.load_input_specification();
            load_input_feeds();
            break;
        };
        default:
            break;
    }

    initialize_channels();
    initialize_pivots();
    start();
    for (auto pivot : pivots) {
        pivot->pivot_thread.join();
    }
    stop();
    if(!disable_quality_control) {
        finalize();
        encode_report(cout);
    }
};
bool Pipeline::pull(Pivot& pivot) {
    vector< unique_lock< mutex > > feed_locks;
    feed_locks.reserve(unique_input_feeds.size());

    // acquire a pull lock for all feeds in a fixed order
    for(const auto feed : unique_input_feeds) {
        feed_locks.push_back(feed->acquire_pull_lock());
    }

    // pull into pivot input segments from input feeds
    for(size_t i = 0; i < pivot.input.size(); i++) {
        if(!input_feeds[i]->pull(pivot.input[i])) {
            end_of_input = true;
        }
    }

    // release the locks on the feeds in reverse order
    for (auto feed_lock = feed_locks.rbegin(); feed_lock != feed_locks.rend(); ++feed_lock) {
        feed_lock->unlock();
    }
    return !end_of_input;
};
void Pipeline::encode_report(ostream& o) const {
    switch (environment.action) {
        case ProgramAction::DEMULTIPLEX: {
            encode_demultiplex_report(o);
            break;
        };
        case ProgramAction::QUALITY: {
            encode_quality_report(o);
            break;
        };
        default:
            break;
    }
    o << endl;
};
void Pipeline::encode_demultiplex_report(ostream& o) const {
    Document document;
    document.SetObject();
    Document::AllocatorType& allocator = document.GetAllocator();

    Value v;

    Value output_report;
    encode_output_report(document, output_report);
    document.AddMember("demultiplex output report", output_report, allocator);

    Value input_report;
    encode_input_report(document, input_report);
    document.AddMember("demultiplex input report", input_report, allocator);

    StringBuffer buffer;
    PrettyWriter< StringBuffer > writer(buffer);
    document.Accept(writer);
    o << buffer.GetString();
};
void Pipeline::encode_input_report(Document& document, Value& value) const {
    value.SetObject();
    input_accumulator->encode(document, value);
};
void Pipeline::encode_output_report(Document& document, Value& value) const {
    Document::AllocatorType& allocator = document.GetAllocator();
    value.SetObject();

    output_accumulator->encode(document, value);

    Value channel_reports;
    channel_reports.SetArray();

    /* encode the undetermined channel first */
    if(undetermined != NULL) {
        Value channel_report;
        channel_report.SetObject();
        undetermined->encode(document, channel_report);
        channel_reports.PushBack(channel_report, allocator);
    }

    /* encode the classified channels */
    for (auto channel : channels) {
        Value channel_report;
        channel_report.SetObject();
        channel->encode(document, channel_report);
        channel_reports.PushBack(channel_report, allocator);
    }
    value.AddMember("read group quality reports", channel_reports, allocator);
};
void Pipeline::encode_quality_report(ostream& o) const {
    Document document;
    document.SetObject();
    Document::AllocatorType& allocator = document.GetAllocator();

    Value v;

    Value output_report;
    encode_output_report(document, output_report);
    document.AddMember("demultiplex output report", output_report, allocator);

    Value input_report;
    encode_input_report(document, input_report);
    document.AddMember("demultiplex input report", input_report, allocator);

    StringBuffer buffer;
    PrettyWriter< StringBuffer > writer(buffer);
    document.Accept(writer);
    o << buffer.GetString();
};
void Pipeline::finalize() {
    /* collect statistics from all parallel pivots */
    for(const auto pivot : pivots) {
        *input_accumulator += pivot->accumulator;

        if(long_read) {
            /* collect output statistics from pivots on the channel accumulators */
            switch (environment.action) {
                case ProgramAction::DEMULTIPLEX: {
                    for(const auto& record : pivot->channel_by_barcode) {
                        const auto& channel = channel_by_barcode.find(record.first);
                        if (channel != channel_by_barcode.end()) {
                            channel->second->channel_accumulator += record.second;
                        }
                    }
                    break;
                };
                case ProgramAction::QUALITY: {
                    for(const auto& record : pivot->channel_by_read_group_id) {
                        const auto& channel = channel_by_read_group_id.find(record.first);
                        if (channel != channel_by_read_group_id.end()) {
                            channel->second->channel_accumulator += record.second;
                        }
                    }
                    break;
                };
                default:
                    break;
            }
        }
    }
    input_accumulator->finalize();

    /* collect statistics from all output channels */
    for(auto channel : channels) {
        output_accumulator->collect(channel->channel_accumulator);
    }
    if (undetermined != NULL) {
        output_accumulator->collect(undetermined->channel_accumulator);
    }

    /* finalize the pipeline accumulator */
    output_accumulator->finalize();

    /* finalize the channel accumulators, now that we know the totals */
    if (undetermined != NULL) {
        undetermined->finalize(*output_accumulator);
    }
    for(auto channel : channels) {
        channel->finalize(*output_accumulator);
    }
};
Feed* Pipeline::resolve_feed(const URL& url, const IoDirection& direction) const {
    Feed* feed = NULL;
    switch(direction) {
        case IoDirection::IN: {
            const auto& record = input_feed_by_url.find(url);
            if(record != input_feed_by_url.end()) {
                feed = record->second;
            }
            break;
        };
        case IoDirection::OUT: {
            const auto& record = output_feed_by_url.find(url);
            if(record != output_feed_by_url.end()) {
                feed = record->second;
            }
            break;
        };
    }
    return feed;
};
Feed* Pipeline::load_feed(FeedSpecification* specification) {
    Feed* feed = NULL;
    switch(specification->direction) {
        case IoDirection::IN: {
            const auto& record = input_feed_by_url.find(specification->url);
            if(record != input_feed_by_url.end()) {
                feed = record->second;
            } else {
                switch(specification->url.kind()) {
                    case FormatKind::FASTQ: {
                        feed = new FastqFeed(*specification);
                        break;
                    };
                    case FormatKind::HTS: {
                        feed = new HtsFeed(*specification);
                        break;
                    };
                    default:
                        throw InternalError("unknown input format " + string(specification->url));
                        break;
                }
                input_feed_by_url[specification->url] = feed;
                feed->set_thread_pool(&thread_pool);
            }
            break;
        };
        case IoDirection::OUT: {
            const auto& record = output_feed_by_url.find(specification->url);
            if(record != output_feed_by_url.end()) {
                feed = record->second;
            } else {
                switch(specification->url.kind()) {
                    case FormatKind::FASTQ: {
                        feed = new FastqFeed(*specification);
                        break;
                    };
                    case FormatKind::HTS: {
                        feed = new HtsFeed(*specification);
                        break;
                    };
                    default:
                        throw InternalError("unknown output format " + string(specification->url));
                        break;
                }
                output_feed_by_url[specification->url] = feed;
                feed->set_thread_pool(&thread_pool);
            }
            break;
        };
    }
    return feed;
};
void Pipeline::load_input_feeds() {
    map< size_t, Feed* > feed_by_index;

    input_feed_by_url.reserve(environment.input_feed_specification_by_url.size());
    for(const auto& record : environment.input_feed_specification_by_url) {
        FeedSpecification* specification = record.second;
        Feed* feed = load_feed(specification);
        feed_by_index[feed->index()] = feed;
    }

    input_feeds.reserve(environment.input_specification->input_urls.size());
    for(const auto& url : environment.input_specification->input_urls) {
        Feed* feed = resolve_feed(url, IoDirection::IN);
        input_feeds.push_back(feed);
    }

    unique_input_feeds.reserve(feed_by_index.size());
    for(const auto& record : feed_by_index) {
        unique_input_feeds.push_back(record.second);
    }
    if(!disable_quality_control) {
        input_accumulator = new PivotAccumulator(*environment.input_specification);
    }
};
void Pipeline::load_output_feeds() {
    map< size_t, Feed* > feed_by_index;

    output_feed_by_url.reserve(environment.output_feed_specification_by_url.size());
    for(const auto& record : environment.output_feed_specification_by_url) {
        FeedSpecification* specification = record.second;
        Feed* feed = load_feed(specification);
        feed_by_index[feed->index()] = feed;
    }

    unique_output_feeds.reserve(feed_by_index.size());
    for(const auto& record : feed_by_index) {
        unique_output_feeds.push_back(record.second);
    }

    if(!disable_quality_control) {
        output_accumulator = new PipelineAccumulator();
    }
};
void Pipeline::probe(const URL& url) {
    if(!url.empty()) {
        FeedSpecification* specification = environment.discover_feed(url, IoDirection::IN);
        specification->set_resolution(1);
        specification->set_capacity(environment.buffer_capacity);
        specification->probe();

        Feed* feed = load_feed(specification);
        feed->open();
        switch(feed->specification.url.kind()) {
            case FormatKind::FASTQ: {
                feed->replenish();
                Segment segment(specification->platform);
                size_t count = 0;
                if(feed->peek(segment, count)) {
                    count++;
                    string primary(segment.name.s, segment.name.l);
                    while(feed->peek(segment, count) && primary.size() == segment.name.l && strncmp(primary.c_str(), segment.name.s, segment.name.l)) {
                        count++;
                    }
                }
                environment.total_output_segments = count;
                environment.total_input_segments = count;
                break;
            };
            case FormatKind::HTS:{
                feed->replenish();
                Segment segment(specification->platform);
                feed->peek(segment, 0);
                environment.total_input_segments = segment.get_total_segments();
                environment.total_output_segments = segment.get_total_segments();

                const HtsHeader& header = ((HtsFeed*)feed)->get_header();
                for(const auto& record : header.read_group_by_id) {
                    ChannelSpecification* channel = environment.load_channel_from_rg(record.second);
                    channel->TC = environment.total_output_segments;
                }
                break;
            };
            default: break;
        }
        specification->set_resolution(environment.total_input_segments);
        specification->set_capacity(environment.buffer_capacity * specification->resolution);
        feed->calibrate(specification);
        environment.calibrate(url);
    }
};
void Pipeline::start() {
    for(auto feed : unique_input_feeds) {
        feed->open();
    }
    for(auto feed : unique_output_feeds) {
        feed->open();
    }
    for(auto feed : unique_input_feeds) {
        feed->start();
    }
    for(auto feed : unique_output_feeds) {
        feed->start();
    }
    for (auto pivot : pivots) {
        pivot->start();
    }
};
void Pipeline::stop() {
    /*  
        output channel buffers still have residual records
        notify all output feeds that no more input is coming 
        and they should explicitly flush
    */
    for(auto feed : unique_output_feeds) {
        feed->stop();
    }
    for(auto feed : unique_input_feeds) {
        feed->join();
    }
    for(auto feed : unique_output_feeds) {
        feed->join();
    }
};