#include "Collection.h"

#include <iostream>

namespace Spino {

    Collection::Collection(DocType& doc, std::string name) : name(name), doc(doc)  {
        auto& arr = doc[name.c_str()];
        if(arr.IsArray() == false) {
            std::cout << "WARNING: collection " 
                << name 
                << " is not an array. The database is corrupt." 
                << std::endl;
        }
        id_counter = 0;	
        last_append_timestamp = std::time(0); 
    }

    Collection::~Collection() {
        for(auto i : indices) {
            delete i;
        }
    }

    std::string Collection::getName() const {
        return name;
    }

    void Collection::indexNewDoc() {
        auto& arr = doc[name.c_str()];
        ValueType& newdoc = arr[arr.Size()-1];

        for(auto& idx : indices) {
            auto v = idx->field.Get(newdoc);
            if(v) {
                if(v->IsString()) {
                    Value val;
                    val.type = TYPE_STRING;
                    val.str = v->GetString();
                    idx->index.insert({val, arr.Size()-1});
                } else if(v->IsNumber()) {
                    Value val;
                    val.type = TYPE_NUMERIC;
                    val.numeric = v->GetDouble();
                    idx->index.insert({val, arr.Size()-1});
                }
            }
        }
    }


    void Collection::append(ValueType& d) {
        auto& arr = doc[name.c_str()];
        uint32_t timestamp = std::time(0);
        uint32_t tmp_timestamp = timestamp;

        char idstr[16];
        idstr[15] = 0;
		int p = 9;
        while(p >= 0) {
            idstr[p--] = char(tmp_timestamp%10 + '0');
            tmp_timestamp /= 10;	
        }	

        uint32_t tmp_idcounter = ++id_counter;
		p = 15;
        while(p >= 10) {
            idstr[p--] = (tmp_idcounter%10) + '0';
            tmp_idcounter /= 10;	
        }

        if(last_append_timestamp - timestamp != 0) {
            id_counter = 0;
        }
        last_append_timestamp = timestamp;

        ValueType _id;
        _id.SetString(idstr, 16, doc.GetAllocator());

        d.AddMember("_id", _id, doc.GetAllocator());

        arr.PushBack(d.GetObject(), doc.GetAllocator());

        indexNewDoc();
    }

    void Collection::append(const char* s) {
        DocType d;
        d.Parse(s);
        append(d.GetObject());
    }

    void Collection::updateById(const char* id_cstr, const char* update) {
        auto& arr = doc[name.c_str()];
        uint32_t domIdx;
        if(domIndexFromId(id_cstr, domIdx)) {
            DocType j;
            j.Parse(update);

            mergeObjects(arr[domIdx], j.GetObject());
            hashmap.clear();
        } else {
        }
    }

    void Collection::update(const char* search, const char* update) {
        auto& arr = doc[name.c_str()];
        DocType j;
        j.Parse(update);

        Spino::QueryParser parser(search);
        auto block = parser.parse_expression();


        if(arr.IsArray()) {
            for (ValueType::ValueIterator itr = arr.Begin();
                    itr != arr.End(); ++itr) {
                Spino::QueryExecutor exec(&(*itr));
                if(exec.resolve(block)) {
                    mergeObjects(*itr, j.GetObject());
                } 
            }
        }
        else {
            cout << "ERROR: collection "
                << name << " is not an array. DOM corrupted." << endl;
        }

        hashmap.clear();
    }

    void Collection::createIndex(const char* s) {
        auto& arr = doc[name.c_str()];
        auto idx = new Collection::Index();
        idx->field_name = s;
        stringstream ss(s);
        string intermediate;
        string ptr;
        while(getline(ss, intermediate, '.')) {
            ptr += "/" + intermediate;
        }
        idx->field = PointerType(ptr.c_str());

        auto n = arr.Size();

        for(uint32_t i = 0; i < n; i++) {
            ValueType& doc = arr[i].GetObject();
            auto v = idx->field.Get(doc);

            if(v != nullptr) {
                // only add string and number values to the index
                if(v->IsString()) {
                    Value val;
                    val.type = TYPE_STRING;
                    val.str = v->GetString();
                    idx->index.insert({val, i});
                }
                else if(v->IsNumber()) {
                    Value val;
                    val.type = TYPE_NUMERIC;
                    val.numeric = v->GetDouble();
                    idx->index.insert({val, i});
                } else {
                    // can't index objects or arrays, etc
                }
            }
        }

        indices.push_back(idx);
    }

    void Collection::dropIndex(const char* s) {
        for(auto itr = indices.begin(); itr != indices.end(); ) {
            auto index = *itr;
            if(index->field_name == s) {
                delete index;	
                indices.erase(itr);
                return;
            }
            itr++;
        }
    }

    /**
     * The _id field is gauranteed to be ordered so we can do a binary search
     */
    std::string Collection::findOneById(const char* id_cstr) const {
        uint32_t m;
        if(domIndexFromId(id_cstr, m)) {
            auto& list = doc[name.c_str()];
            rapidjson::StringBuffer sb;
            rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
            list[m].Accept(writer);
            return sb.GetString();
        }
        return "";
    }

    bool Collection::domIndexFromId(const char* id_cstr, uint32_t& domIdx) const {
        auto& arr = doc[name.c_str()];
        uint64_t tsc = fast_atoi_len(id_cstr, 10);
        uint64_t countc = fast_atoi_len(&id_cstr[10], 6);

        auto n = arr.Size();
        auto R = n-1;
        uint32_t L = 0;
        while(L <= R) {
            auto m = (L+R)/2;

            if(m > arr.Size()-1) {
                return false;
            }

            const char* id_to_test = arr[m].GetObject()["_id"].GetString();
            uint64_t timestamp = fast_atoi_len(id_to_test, 10);

            if(timestamp < tsc) {
                L = m+1;
            }
            else if(timestamp > tsc) {
                R = m-1;
            }
            else {
                uint64_t count = fast_atoi_len(&id_to_test[10], 6);
                if(count < countc) {
                    L = m+1;
                } 
                else if(count > countc) {
                    R = m-1;
                }
                else {
                    domIdx = m;
                    return true;
                }
            }
        }
        return false;
    }

    std::string Collection::findOne(const char* s) {
        std::string v; //result

        //check hashmap for existing query results
        std::string str = s;
        auto hash = fnv1a_hash(str);
        auto it = hashmap.find(hash);
        if (it != hashmap.end()) {
            return it->second;
        }

        //check if it's an index search
        QueryParser parser(s);
        std::shared_ptr<BasicFieldComparison> bfc = nullptr;
        bfc = parser.parse_basic_comparison();

        if(bfc != nullptr) {
            for(auto idx : indices) {
                if(idx->field_name == bfc->field_name) {
                    auto iter = idx->index.find(bfc->v);
                    if(iter != idx->index.end()) {
                        auto n = iter->second;

                        rapidjson::StringBuffer sb;
                        rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
                        doc[name.c_str()][n].Accept(writer);
                        v = sb.GetString();
                    }
                    break;
                }
            }
        }

        //if it's not an index search, do a linear search using a cursor
        if(v == "") {
            LinearCursor cursor(doc[name.c_str()], s);
            v = cursor.next();
        }

        //if there is a result, add it to the hashmap for future reference
        if(v != "") {
            hashmap[hash] = v;
        }

        return v;
    }

    /**
     * fnv-1a is a very fast hashing algorithm with low collision rates
     * at least, that's what the internet told me
     */
    uint32_t Collection::fnv1a_hash(std::string& s) {
        auto length = s.length()+1;
        unsigned int hash = OFFSET_BASIS;
        for (size_t i = 0; i < length; ++i)
        {
            hash ^= s[i]++;
            hash *= FNV_PRIME;
        }
        return hash;
    }

    BaseCursor* Collection::find(const char* s) const {
        //check if it's an index search
        QueryParser parser(s);
        std::shared_ptr<BasicFieldComparison> bfc = nullptr;
        bfc = parser.parse_basic_comparison();

        if(bfc != nullptr) {
            for(auto& idx : indices) {
                if(idx->field_name == bfc->field_name) {
                    auto range = idx->index.equal_range(bfc->v);
                    return new IndexCursor(range, doc[name.c_str()]);
                }
            }
        }

        return new LinearCursor(doc[name.c_str()], s);
    }

    void Collection::removeDomIdxFromIndex(uint32_t domIdx) {
        for(auto idx : indices) {
            std::vector<std::pair<Spino::Value, int>> idx_copies;
            auto iitr = idx->index.begin();
            while(iitr != idx->index.end()) {
                auto entry = *iitr;
                if(entry.second == domIdx) {
                    iitr = idx->index.erase(iitr);
                } 
                else if(entry.second > domIdx) {
                    idx_copies.push_back({iitr->first, iitr->second-1});
                    iitr = idx->index.erase(iitr);
                }
                else {	
                    iitr++;
                }
            }

            for(auto i : idx_copies) {
                idx->index.insert(i);
            }
        }
    }

    void Collection::reconstructIndices() {
        auto& arr = doc[name.c_str()];
        auto n = arr.Size();

        for(auto& idx : indices) {
            idx->index.clear();

            for(uint32_t i = 0; i < n; i++) {
                ValueType& doc = arr[i].GetObject();
                auto v = idx->field.Get(doc);

                if(v != nullptr) {
                    // only add string and number values to the index
                    if(v->IsString()) {
                        Value val;
                        val.type = TYPE_STRING;
                        val.str = v->GetString();
                        idx->index.insert({val, i});
                    }
                    else if(v->IsNumber()) {
                        Value val;
                        val.type = TYPE_NUMERIC;
                        val.numeric = v->GetDouble();
                        idx->index.insert({val, i});
                    } 
                }
            }
        }
    }

    void Collection::dropById(const char* s) {
        auto& arr = doc[name.c_str()];
        uint32_t domIdx;
        if(domIndexFromId(s, domIdx)) {
            removeDomIdxFromIndex(domIdx);

            auto iter = arr.Begin();
            iter += domIdx;

            arr.Erase(iter);
            hashmap.clear();
        }
    }


    void Collection::dropOne(const char* j) {
        drop(j, 1);
    }

    uint32_t Collection::drop(const char* j, uint32_t limit) {
        // TODO
        // do an index search first
        //
        auto& arr = doc[name.c_str()];
        Spino::QueryParser parser(j);
        auto block = parser.parse_expression();

        uint32_t count = 0;
        for (ValueType::ConstValueIterator itr = arr.Begin();
                itr != arr.End(); ) {
            Spino::QueryExecutor exec(&(*itr));
            if(exec.resolve(block)) {
                count++;
                itr = arr.Erase(itr);
                if(count >= limit) {
                    break;
                } 
            } else {
                itr++;
            }
        }

        if(count > 0) {
            hashmap.clear();
            reconstructIndices();
        }

        return count;
    }

    uint32_t Collection::dropOlderThan(uint64_t timestamp) {
        auto& arr = doc[name.c_str()];
        timestamp /= 1000; // convert to seconds since epoch

        uint32_t n = arr.Size();
        if(n == 0) {
            return 0;
        }

        uint32_t R = n-1;
        uint32_t L = 0;
        uint32_t m;
        // this search fill find the nearest document to the specified timestamp
        // and anything up to that can be deleted
        while(L <= R) {
            m = (L+R)/2;

            if(m > arr.Size()-1) {
                return 0;
            }

            const char* id_to_test = arr[m].GetObject()["_id"].GetString();
            uint64_t id_timestamp = fast_atoi_len(id_to_test, 10);

            if(id_timestamp < timestamp) {
                L = m+1;
            }
            else if(id_timestamp > timestamp) {
                R = m-1;
            }
            else {
                break;
            }
        }

        if(L > 0) {
            ValueType::ConstValueIterator itr = arr.Begin();
            for(uint32_t i = 0; i < L; i++) {
                itr++;
            }

            arr.Erase(arr.Begin(), itr);

            hashmap.clear();
            reconstructIndices();
        }
        return m;

    }

    bool Collection::mergeObjects(ValueType& dstObject, ValueType& srcObject)
    {
        for (auto srcIt = srcObject.MemberBegin(); srcIt != srcObject.MemberEnd(); ++srcIt)
        {
            auto dstIt = dstObject.FindMember(srcIt->name);
            if (dstIt == dstObject.MemberEnd())
            {
                ValueType dstName ;
                dstName.CopyFrom(srcIt->name, doc.GetAllocator());
                ValueType dstVal ;
                dstVal.CopyFrom(srcIt->value, doc.GetAllocator());

                dstObject.AddMember(dstName, dstVal, doc.GetAllocator());

                dstName.CopyFrom(srcIt->name, doc.GetAllocator());
                dstIt = dstObject.FindMember(dstName);

                if (dstIt == dstObject.MemberEnd()) {
                    return false ;
                }
            }
            else
            {
                auto srcT = srcIt->value.GetType() ;
                auto dstT = dstIt->value.GetType() ;
                if(srcT != dstT)
                    return false ;

                if (srcIt->value.IsArray())
                {
                    for (auto arrayIt = srcIt->value.Begin(); arrayIt != srcIt->value.End(); ++arrayIt)
                    {
                        ValueType dstVal ;
                        dstVal.CopyFrom(*arrayIt, doc.GetAllocator()) ;
                        dstIt->value.PushBack(dstVal, doc.GetAllocator());
                    }
                }
                else if (srcIt->value.IsObject())
                {
                    if(!mergeObjects(dstIt->value, srcIt->value))
                        return false ;
                }
                else
                {
                    dstIt->value.CopyFrom(srcIt->value, doc.GetAllocator()) ;
                }
            }
        }

        return true ;
    }



}
