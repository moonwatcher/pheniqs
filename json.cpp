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

#include "json.h"

void print_json(const Value& node, ostream& o) {
    StringBuffer buffer;
    PrettyWriter< StringBuffer > writer(buffer);
    node.Accept(writer);
    o << buffer.GetString() << endl;
};
Document* load_json(const string& path) {
    Document* document(NULL);
    if(access(path.c_str(), R_OK) != -1) {
        document = new Document();
        ifstream file(path);
        const string content((istreambuf_iterator< char >(file)), istreambuf_iterator< char >());
        file.close();
        if(document->Parse(content.c_str()).HasParseError()) {
            string message(GetParseError_En(document->GetParseError()));
            message += " at position ";
            message += to_string(document->GetErrorOffset());
            throw ConfigurationError(message);
        }
    }
    return document;
};

void merge_json_value(const Value& base, Value& ontology, Document& document) {
    if(!base.IsNull()) {
        if(!ontology.IsNull()) {
            if(base.IsObject()) {
                if(ontology.IsObject()) {
                    for(auto& element : base.GetObject()) {
                        Value::MemberIterator reference = ontology.FindMember(element.name);
                        if(reference != ontology.MemberEnd()) {
                            try {
                                merge_json_value(element.value, reference->value, document);
                            } catch(ConfigurationError& error) {
                                throw ConfigurationError(string(element.name.GetString(), element.name.GetStringLength()) + " " + error.message);
                            }
                        } else {
                            ontology.AddMember (
                                Value(element.name, document.GetAllocator()).Move(),
                                Value(element.value, document.GetAllocator()).Move(),
                                document.GetAllocator()
                            );
                        }
                    }
                } else { throw ConfigurationError("element is not a dictionary"); }
            }
        } else {
            ontology.CopyFrom(base, document.GetAllocator());
        }
    }
};
void project_json_value(const Value& base, const Value& ontology, Value& container, Document& document) {
    container.SetNull();
    if(!base.IsNull() && !ontology.IsNull()) {
        if(base.IsObject()) {
            if(ontology.IsObject()) {
                container.SetObject();
                for(auto& record : base.GetObject()) {
                    Value child;
                    Value::ConstMemberIterator element = ontology.FindMember(record.name);
                    if(element != ontology.MemberEnd()) {
                        project_json_value(record.value, element->value, child, document);
                    } else {
                        child.CopyFrom(record.value, document.GetAllocator());
                    }
                    container.AddMember(Value(record.name, document.GetAllocator()).Move(), child.Move(), document.GetAllocator());
                }
            } else if(ontology.IsArray()) {
                container.SetArray();
                for(const auto& element : ontology.GetArray()) {
                    Value child;
                    project_json_value(base, element, child, document);
                    container.PushBack(child.Move(), document.GetAllocator());
                }
            }
        }
    }
    if(!ontology.IsNull() && container.IsNull()) {
        container.CopyFrom(ontology, document.GetAllocator());
    }
};
void clean_json_value(Value& ontology, Document& document) {
    if(!ontology.IsNull()) {
        if(ontology.IsObject()) {
            Value clean(kObjectType);
            for(auto& element : ontology.GetObject()) {
                clean_json_value(element.value, document);
                if(!element.value.IsNull()) {
                    clean.AddMember(element.name.Move(), element.value.Move(), document.GetAllocator());
                }
            }
            if(clean.ObjectEmpty()) {
                clean.SetNull();
            }
            ontology.Swap(clean);

        } else if(ontology.IsArray()) {
            Value clean(kArrayType);
            for(auto& element : ontology.GetArray()) {
                clean_json_value(element, document);
                if(!element.IsNull()) {
                    clean.PushBack(element.Move(), document.GetAllocator());
                }
                if(clean.Empty()) {
                    clean.SetNull();
                }
            }
            ontology.Swap(clean);

        } else if(ontology.IsBool() && !ontology.GetBool()) {
            ontology.SetNull();

        }
    }
};

template<> bool decode_value_by_key< bool >(const Value::Ch* key, bool& value, const Value& container) {
    if(container.IsObject()) {
        Value::ConstMemberIterator reference = container.FindMember(key);
        if(reference != container.MemberEnd()) {
            if(!reference->value.IsNull()) {
                if(reference->value.IsBool()) {
                    value = reference->value.GetBool();
                    return true;
                } else { throw ConfigurationError(string(key) + " element is not a boolean"); }
            } else {
                value = false;
                return true;
            }
        } else {
            value = false;
            return true;
        }
    } else { throw ConfigurationError(string(key) + " container is not a dictionary"); }
    return false;
};
template<> bool decode_value_by_key< uint8_t >(const Value::Ch* key, uint8_t& value, const Value& container) {
    if(container.IsObject()) {
        Value::ConstMemberIterator reference = container.FindMember(key);
        if(reference != container.MemberEnd() && !reference->value.IsNull()) {
            unsigned v;
            if(reference->value.IsUint() && (v = reference->value.GetUint()) <= numeric_limits< uint8_t >::max()) {
                value = static_cast< uint8_t >(v);
                return true;
            } else { throw ConfigurationError(string(key) + " element is not an 8 bit unsigned integer"); }
        }
    } else { throw ConfigurationError(string(key) + " container is not a dictionary"); }
    return false;
};
template<> bool decode_value_by_key< vector< uint8_t > >(const Value::Ch* key, vector< uint8_t >& value, const Value& container) {
    if(container.IsObject()) {
        Value::ConstMemberIterator reference = container.FindMember(key);
        if(reference != container.MemberEnd() && !reference->value.IsNull()) {
            if(reference->value.IsArray()) {
                size_t index(0);
                unsigned v;
                value.resize(reference->value.Size());
                for(const auto& element : reference->value.GetArray()) {
                    if(element.IsUint() && (v = element.GetUint()) <= numeric_limits< uint8_t >::max()) {
                        value[index] = static_cast < uint8_t >(v);
                        ++index;
                    } else { throw ConfigurationError(string(key) + " element at position " + to_string(index) + " is not an 8 bit unsigned integer"); }
                }
                return true;
            } else { throw ConfigurationError(string(key) + " element is not an array"); }
        }
    } else { throw ConfigurationError(string(key) + " container is not a dictionary"); }
    return false;
};
template<> bool decode_value_by_key< int32_t >(const Value::Ch* key, int32_t& value, const Value& container) {
    if(container.IsObject()) {
        Value::ConstMemberIterator reference = container.FindMember(key);
        if(reference != container.MemberEnd() && !reference->value.IsNull()) {
            if(reference->value.IsInt()) {
                value = reference->value.GetInt();
                return true;
            } else { throw ConfigurationError(string(key) + " element is not a 32 bit integer"); }
        }
    } else { throw ConfigurationError(string(key) + " container is not a dictionary"); }
    return false;
};
template<> bool decode_value_by_key< vector< int32_t > >(const Value::Ch* key, vector< int32_t >& value, const Value& container) {
    if(container.IsObject()) {
        Value::ConstMemberIterator reference = container.FindMember(key);
        if(reference != container.MemberEnd() && !reference->value.IsNull()) {
            if(reference->value.IsArray()){
                size_t index(0);
                value.resize(reference->value.Size());
                for(const auto& element : reference->value.GetArray()) {
                    if(element.IsInt()) {
                        value[index] = static_cast < uint8_t >(element.GetInt());
                        ++index;
                    } else { throw ConfigurationError(string(key) + " element at position " + to_string(index) + " is not a 32 bit integer"); }
                }
                return true;
            } else { throw ConfigurationError(string(key) + " element is not an array"); }
        }
    } else { throw ConfigurationError(string(key) + " container is not a dictionary"); }
    return false;
};
template<> bool decode_value_by_key< uint32_t >(const Value::Ch* key, uint32_t& value, const Value& container) {
    if(container.IsObject()) {
        Value::ConstMemberIterator reference = container.FindMember(key);
        if(reference != container.MemberEnd() && !reference->value.IsNull()) {
            if(reference->value.IsUint()) {
                value = reference->value.GetUint();
                return true;
            } else { throw ConfigurationError(string(key) + " element is not a 32 bit unsigned integer"); }
        }
    } else { throw ConfigurationError(string(key) + " container is not a dictionary"); }
    return false;
};
template<> bool decode_value_by_key< int64_t >(const Value::Ch* key, int64_t& value, const Value& container) {
    if(container.IsObject()) {
        Value::ConstMemberIterator reference = container.FindMember(key);
        if(reference != container.MemberEnd() && !reference->value.IsNull()) {
            if(reference->value.IsInt64()) {
                value = reference->value.GetInt64();
                return true;
            } else { throw ConfigurationError(string(key) + " element is not a 64 bit integer"); }
        }
    } else { throw ConfigurationError(string(key) + " container is not a dictionary"); }
    return false;
};
template<> bool decode_value_by_key< uint64_t >(const Value::Ch* key, uint64_t& value, const Value& container) {
    if(container.IsObject()) {
        Value::ConstMemberIterator reference = container.FindMember(key);
        if(reference != container.MemberEnd() && !reference->value.IsNull()) {
            if(reference->value.IsUint64()) {
                value = reference->value.GetUint64();
                return true;
            } else { throw ConfigurationError(string(key) + " element is not a 64 bit unsigned integer"); }
        }
    } else { throw ConfigurationError(string(key) + " container is not a dictionary"); }
    return false;
};
template<> bool decode_value_by_key< vector< uint64_t > >(const Value::Ch* key, vector< uint64_t >& value, const Value& container) {
    if(container.IsObject()) {
        Value::ConstMemberIterator reference = container.FindMember(key);
        if(reference != container.MemberEnd() && !reference->value.IsNull()) {
            if(reference->value.IsArray()){
                size_t index(0);
                value.resize(reference->value.Size());
                for(const auto& element : reference->value.GetArray()) {
                    if(element.IsUint64()) {
                        value[index] = element.GetUint64();
                        ++index;
                    } else { throw ConfigurationError(string(key) + " element at position " + to_string(index) + " is not an unsigned 64 bit integer"); }
                }
                return true;
            } else { throw ConfigurationError(string(key) + " element is not an array"); }
        }
    } else { throw ConfigurationError(string(key) + " container is not a dictionary"); }
    return false;
};
template<> bool decode_value_by_key< double >(const Value::Ch* key, double& value, const Value& container) {
    if(container.IsObject()) {
        Value::ConstMemberIterator reference = container.FindMember(key);
        if(reference != container.MemberEnd() && !reference->value.IsNull()) {
            if(reference->value.IsNumber()) {
                value = reference->value.GetDouble();
                return true;
            } else { throw ConfigurationError(string(key) + " element is not numeric"); }
        }
    } else { throw ConfigurationError(string(key) + " container is not a dictionary"); }
    return false;
};
template<> bool decode_value_by_key< vector< double > >(const Value::Ch* key, vector< double >& value, const Value& container) {
    if(container.IsObject()) {
        Value::ConstMemberIterator reference = container.FindMember(key);
        if(reference != container.MemberEnd() && !reference->value.IsNull()) {
            if(reference->value.IsArray()) {
                size_t index(0);
                value.resize(reference->value.Size());
                for(const auto& element : reference->value.GetArray()) {
                    if(element.IsNumber()) {
                        value[index] = element.GetDouble();
                        ++index;
                    } else { throw ConfigurationError(string(key) + " element at position " + to_string(index) + " is not numeric"); }
                }
                return true;
            } else { throw ConfigurationError(string(key) + " element is not an array"); }
        }
    } else { throw ConfigurationError(string(key) + " container is not a dictionary"); }
    return false;
};
template<> bool decode_value_by_key< string >(const Value::Ch* key, string& value, const Value& container) {
    if(container.IsObject()) {
        Value::ConstMemberIterator reference = container.FindMember(key);
        if(reference != container.MemberEnd() && !reference->value.IsNull()) {
            if(reference->value.IsString()) {
                value.assign(reference->value.GetString(), reference->value.GetStringLength());
                return true;
            } else { throw ConfigurationError(string(key) + " element is not a string"); }
        }
    } else { throw ConfigurationError(string(key) + " container is not a dictionary"); }
    return false;
};
template<> bool decode_value_by_key< list< string > >(const Value::Ch* key, list< string >& value, const Value& container) {
    if(container.IsObject()) {
        Value::ConstMemberIterator reference = container.FindMember(key);
        if(reference != container.MemberEnd() && !reference->value.IsNull()) {
            if(reference->value.IsArray()) {
                size_t index(0);
                value.clear();
                for(const auto& element : reference->value.GetArray()) {
                    if(element.IsString()) {
                        value.emplace_back(element.GetString(), element.GetStringLength());
                        ++index;
                    } else { throw ConfigurationError(string(key) + " element at position " + to_string(index) + " is not a string"); }
                }
                return true;
            } else { throw ConfigurationError(string(key) + " element is not an array"); }
        }
    } else { throw ConfigurationError(string(key) + " container is not a dictionary"); }
    return false;
};
template<> bool decode_value_by_key< kstring_t >(const Value::Ch* key, kstring_t& value, const Value& container) {
    if(container.IsObject()) {
        Value::ConstMemberIterator reference = container.FindMember(key);
        if(reference != container.MemberEnd() && !reference->value.IsNull()) {
            if(reference->value.IsString()) {
                if(reference->value.GetStringLength() < numeric_limits< size_t >::max()) {
                    ks_put_string(reference->value.GetString(), static_cast< size_t >(reference->value.GetStringLength()), value);
                    return true;
                } else { throw ConfigurationError(string(key) + " element is too long"); }
            } else { throw ConfigurationError(string(key) + " element is not a string"); }
        }
    } else { throw ConfigurationError(string(key) + " container is not a dictionary"); }
    return false;
};

template <> bool decode_value_by_key(const Value::Ch* key, const Value& container) {
    if(container.IsObject()) {
        Value::ConstMemberIterator reference = container.FindMember(key);
        if(reference != container.MemberEnd()) {
            if(!reference->value.IsNull()) {
                if(reference->value.IsBool()) {
                    return reference->value.GetBool();
                } else { throw ConfigurationError(string(key) + " element is not a boolean"); }
            } else { return false; }
        } else { return false; }
    } else { throw ConfigurationError(string(key) + " container is not a dictionary"); }
};
template <> uint8_t decode_value_by_key(const Value::Ch* key, const Value& container) {
    if(container.IsObject()) {
        Value::ConstMemberIterator reference = container.FindMember(key);
        if(reference != container.MemberEnd()) {
            if(!reference->value.IsNull()) {
                unsigned value;
                if(reference->value.IsUint() && (value = reference->value.GetUint()) <= numeric_limits< uint8_t >::max()) {
                    return static_cast< uint8_t >(value);
                } else { throw ConfigurationError(string(key) + " element is not an 8 bit unsigned integer"); }
            } else { throw ConfigurationError(string(key) + " is null"); }
        } else { throw ConfigurationError(string(key) + " not found"); }
    } else { throw ConfigurationError(string(key) + " container is not a dictionary"); }
};
template <> vector< uint8_t > decode_value_by_key(const Value::Ch* key, const Value& container) {
    if(container.IsObject()) {
        Value::ConstMemberIterator reference = container.FindMember(key);
        if(reference != container.MemberEnd()) {
            if(!reference->value.IsNull()) {
                if(reference->value.IsArray()) {
                    size_t index(0);
                    unsigned value;
                    vector< uint8_t > array(reference->value.Size());
                    for(const auto& element : reference->value.GetArray()) {
                        if(element.IsUint() && (value = element.GetUint()) <= numeric_limits< uint8_t >::max()) {
                            array[index] = static_cast < uint8_t >(value);
                            ++index;
                        } else { throw ConfigurationError(string(key) + " element at position " + to_string(index) + " is not an 8 bit unsigned integer"); }
                    }
                    return array;
                } else { throw ConfigurationError(string(key) + " element is not an array"); }
            } else { throw ConfigurationError(string(key) + " is null"); }
        } else { throw ConfigurationError(string(key) + " not found"); }
    } else { throw ConfigurationError(string(key) + " container is not a dictionary"); }
};
template <> int32_t decode_value_by_key(const Value::Ch* key, const Value& container) {
    if(container.IsObject()) {
        Value::ConstMemberIterator reference = container.FindMember(key);
        if(reference != container.MemberEnd()) {
            if(!reference->value.IsNull()) {
                if(reference->value.IsInt()) {
                    return reference->value.GetInt();
                } else { throw ConfigurationError(string(key) + " element is not a 32 bit integer"); }
            } else { throw ConfigurationError(string(key) + " is null"); }
        } else { throw ConfigurationError(string(key) + " not found"); }
    } else { throw ConfigurationError(string(key) + " container is not a dictionary"); }
};
template <> vector< int32_t > decode_value_by_key(const Value::Ch* key, const Value& container) {
    if(container.IsObject()) {
        Value::ConstMemberIterator reference = container.FindMember(key);
        if(reference != container.MemberEnd()) {
            if(!reference->value.IsNull()) {
                if(reference->value.IsArray()) {
                    size_t index(0);
                    vector< int32_t > array(reference->value.Size());
                    for(const auto& element : reference->value.GetArray()) {
                        if(element.IsInt()) {
                            array[index] = element.GetInt();
                            ++index;
                        } else { throw ConfigurationError(string(key) + " element at position " + to_string(index) + " is not a 32 bit integer"); }
                    }
                    return array;
                } else { throw ConfigurationError(string(key) + " element is not an array"); }
            } else { throw ConfigurationError(string(key) + " is null"); }
        } else { throw ConfigurationError(string(key) + " not found"); }
    } else { throw ConfigurationError(string(key) + " container is not a dictionary"); }
};
template <> uint32_t decode_value_by_key(const Value::Ch* key, const Value& container) {
    if(container.IsObject()) {
        Value::ConstMemberIterator reference = container.FindMember(key);
        if(reference != container.MemberEnd()) {
            if(!reference->value.IsNull()) {
                if(reference->value.IsUint()) {
                    return reference->value.GetUint();
                } else { throw ConfigurationError(string(key) + " element is not a 32 bit unsigned integer"); }
            } else { throw ConfigurationError(string(key) + " is null"); }
        } else { throw ConfigurationError(string(key) + " not found"); }
    } else { throw ConfigurationError(string(key) + " container is not a dictionary"); }
};
template <> int64_t decode_value_by_key(const Value::Ch* key, const Value& container) {
    if(container.IsObject()) {
        Value::ConstMemberIterator reference = container.FindMember(key);
        if(reference != container.MemberEnd()) {
            if(!reference->value.IsNull()) {
                if(reference->value.IsInt64()) {
                    return reference->value.GetInt64();
                } else { throw ConfigurationError(string(key) + " element is not a 64 bit integer"); }
            } else { throw ConfigurationError(string(key) + " is null"); }
        } else { throw ConfigurationError(string(key) + " not found"); }
    } else { throw ConfigurationError(string(key) + " container is not a dictionary"); }
};
template <> uint64_t decode_value_by_key(const Value::Ch* key, const Value& container) {
    if(container.IsObject()) {
        Value::ConstMemberIterator reference = container.FindMember(key);
        if(reference != container.MemberEnd()) {
            if(!reference->value.IsNull()) {
                if(reference->value.IsUint64()) {
                    return reference->value.GetUint64();
                } else { throw ConfigurationError(string(key) + " element is not a 64 bit unsigned integer"); }
            } else { throw ConfigurationError(string(key) + " is null"); }
        } else { throw ConfigurationError(string(key) + " not found"); }
    } else { throw ConfigurationError(string(key) + " container is not a dictionary"); }
};
template <> vector< uint64_t > decode_value_by_key(const Value::Ch* key, const Value& container) {
    if(container.IsObject()) {
        Value::ConstMemberIterator reference = container.FindMember(key);
        if(reference != container.MemberEnd()) {
            if(!reference->value.IsNull()) {
                if(reference->value.IsArray()) {
                    size_t index(0);
                    vector< uint64_t > array(reference->value.Size());
                    for(const auto& element : reference->value.GetArray()) {
                        if(element.IsUint64()) {
                            array[index] = element.GetUint64();
                            ++index;
                        } else { throw ConfigurationError(string(key) + " element at position " + to_string(index) + " is not an unsigned 64 bit integer"); }
                    }
                    return array;
                } else { throw ConfigurationError(string(key) + " element is not an array"); }
            } else { throw ConfigurationError(string(key) + " is null"); }
        } else { throw ConfigurationError(string(key) + " not found"); }
    } else { throw ConfigurationError(string(key) + " container is not a dictionary"); }
};
template <> double decode_value_by_key(const Value::Ch* key, const Value& container) {
    if(container.IsObject()) {
        Value::ConstMemberIterator reference = container.FindMember(key);
        if(reference != container.MemberEnd()) {
            if(!reference->value.IsNull()) {
                if(reference->value.IsNumber()) {
                    return reference->value.GetDouble();
                } else { throw ConfigurationError(string(key) + " element is not numeric"); }
            } else { throw ConfigurationError(string(key) + " is null"); }
        } else { throw ConfigurationError(string(key) + " not found"); }
    } else { throw ConfigurationError(string(key) + " container is not a dictionary"); }
};
template <> vector< double > decode_value_by_key(const Value::Ch* key, const Value& container) {
    if(container.IsObject()) {
        Value::ConstMemberIterator reference = container.FindMember(key);
        if(reference != container.MemberEnd()) {
            if(!reference->value.IsNull()) {
                if(reference->value.IsArray()) {
                    size_t index(0);
                    vector< double > array(reference->value.Size());
                    for(const auto& element : reference->value.GetArray()) {
                        if(element.IsNumber()) {
                            array[index] = element.GetDouble();
                            ++index;
                        } else { throw ConfigurationError(string(key) + " element at position " + to_string(index) + " is not numeric"); }
                    }
                    return array;
                } else { throw ConfigurationError(string(key) + " element is not an array"); }
            } else { throw ConfigurationError(string(key) + " is null"); }
        } else { throw ConfigurationError(string(key) + " not found"); }
    } else { throw ConfigurationError(string(key) + " container is not a dictionary"); }
};
template <> string decode_value_by_key(const Value::Ch* key, const Value& container) {
    if(container.IsObject()) {
        Value::ConstMemberIterator reference = container.FindMember(key);
        if(reference != container.MemberEnd()) {
            if(!reference->value.IsNull()) {
                if(reference->value.IsString()) {
                    return string(reference->value.GetString(), reference->value.GetStringLength());
                } else { throw ConfigurationError(string(key) + " element is not a string"); }
            } else { throw ConfigurationError(string(key) + " is null"); }
        } else { throw ConfigurationError(string(key) + " not found"); }
    } else { throw ConfigurationError(string(key) + " container is not a dictionary"); }
};
template <> list< string > decode_value_by_key(const Value::Ch* key, const Value& container) {
    if(container.IsObject()) {
        Value::ConstMemberIterator reference = container.FindMember(key);
        if(reference != container.MemberEnd()) {
            if(!reference->value.IsNull()) {
                if(reference->value.IsArray()) {
                    size_t index(0);
                    list< string > array;
                    for(const auto& element : reference->value.GetArray()) {
                        if(element.IsString()) {
                            array.emplace_back(reference->value.GetString(), reference->value.GetStringLength());
                            ++index;
                        } else { throw ConfigurationError(string(key) + " element at position " + to_string(index) + " is not a string"); }
                    }
                    return array;
                } else { throw ConfigurationError(string(key) + " element is not an array"); }
            } else { throw ConfigurationError(string(key) + " is null"); }
        } else { throw ConfigurationError(string(key) + " not found"); }
    } else { throw ConfigurationError(string(key) + " container is not a dictionary"); }
};
template <> kstring_t decode_value_by_key(const Value::Ch* key, const Value& container) {
    if(container.IsObject()) {
        Value::ConstMemberIterator reference = container.FindMember(key);
        if(reference != container.MemberEnd()) {
            if(!reference->value.IsNull()) {
                if(reference->value.IsString()) {
                    if(reference->value.GetStringLength() < numeric_limits< size_t >::max()) {
                        kstring_t value({ 0, 0, NULL });
                        ks_put_string(reference->value.GetString(), static_cast< size_t >(reference->value.GetStringLength()), value);
                        return value;
                    } else { throw ConfigurationError(string(key) + " element must be a string shorter than " + to_string(numeric_limits< int >::max())); }
                } else { throw ConfigurationError(string(key) + " element is not a string"); }
            } else { throw ConfigurationError(string(key) + " is null"); }
        } else { throw ConfigurationError(string(key) + " not found"); }
    } else { throw ConfigurationError(string(key) + " container is not a dictionary"); }
};
