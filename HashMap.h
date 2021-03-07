#include <algorithm>
#include <exception>
#include <functional>
#include <list>
#include <memory>
#include <vector>

template<class KeyType, class ValueType>
struct HashMapNode {
    typedef HashMapNode<KeyType, ValueType>           Node;
    typedef std::shared_ptr<Node>                     pNode;
    typedef typename std::list<pNode>::iterator       PosIterator;
    typedef std::pair<const KeyType, ValueType>       MapPair;
    
    MapPair map_pair;
    PosIterator sequence_pos;
    size_t bucket_index;  // keep this value to avoid calculating hash again
    
    HashMapNode(MapPair map_pair): map_pair(map_pair) {}
    
    const KeyType& key() const {
        return map_pair.first;
    }
    
    const ValueType& value() const {
        return map_pair.second;
    }
    
    ValueType& value() {
        return map_pair.second;
    }
};

template<class KeyType, class ValueType, class Hash = std::hash<KeyType>>
class HashMap {
public:
    typedef HashMapNode<KeyType, ValueType>           Node;
    typedef std::shared_ptr<Node>                     pNode;
    typedef typename std::list<pNode>::iterator       PosIterator;
    typedef typename std::list<pNode>::const_iterator ConstPosIterator;
    typedef std::pair<const KeyType, ValueType>       MapPair;
    typedef std::list<pNode>                          BucketType;
    
    HashMap(const Hash& _hasher = Hash()): hasher(_hasher) {
        sz = 0;
        capacity = INIT_CAPACITY;
        buckets.resize(capacity, BucketType());
    }
    
    template <class Iter>  // pair{key, value}
    HashMap(Iter begin, Iter end, const Hash& _hasher = Hash()): HashMap(_hasher) {
        for (; begin != end; ++begin) {
            insert(*begin);
        }
    }
    
    HashMap(const std::initializer_list<MapPair>& list, const Hash& _hasher = Hash()): HashMap(list.begin(), list.end(), _hasher) {}
    
    class iterator {
    public:
        explicit iterator(PosIterator sequence_pos): sequence_pos(sequence_pos) {}
        
        iterator() {}
        
        iterator& operator++() {
            ++sequence_pos;
            return *this;
        }
        
        iterator operator++(int) {
            iterator prev = iterator(sequence_pos);
            ++sequence_pos;
            return prev;
        }
        
        MapPair& operator*() {
            return (*sequence_pos)->map_pair;
        }
        
        MapPair* operator->() {
            return &(*sequence_pos)->map_pair;
        }
        
        bool operator==(const iterator rhs) const {
            return sequence_pos == rhs.sequence_pos;
        }
        
        bool operator!=(const iterator rhs) const {
            return !(*this == rhs);
        }
        
    private:
        PosIterator sequence_pos;
    };
    
    iterator begin() {
        return iterator(nodes_sequence.begin());
    }
    
    iterator end() {
        return iterator(nodes_sequence.end());
    }
    
    class const_iterator {
    public:
        explicit const_iterator(ConstPosIterator sequence_pos): sequence_pos(sequence_pos) {}
        
        const_iterator() {}
        
        const_iterator& operator++() {
            ++sequence_pos;
            return *this;
        }
        
        const_iterator operator++(int) {
            const_iterator prev = const_iterator(sequence_pos);
            ++sequence_pos;
            return prev;
        }
        
        const MapPair& operator*() const {
            return (*sequence_pos)->map_pair;
        }
        
        const MapPair* operator->() const {
            return &(*sequence_pos)->map_pair;
        }
        
        bool operator==(const_iterator rhs) const {
            return sequence_pos == rhs.sequence_pos;
        }
        
        bool operator!=(const_iterator rhs) const {
            return !(*this == rhs);
        }
    private:
        ConstPosIterator sequence_pos;
    };
    
    const_iterator begin() const {
        return const_iterator(nodes_sequence.begin());
    }
    
    const_iterator end() const {
        return const_iterator(nodes_sequence.end());
    }
    
    iterator find(const KeyType& key) {
        auto& bucket = buckets[get_hash(key)];
        
        for (auto node_pnt : bucket) {
            if (node_pnt->key() == key) {
                return iterator(node_pnt->sequence_pos);
            }
        }
        return end();
    }
    
    const_iterator find(const KeyType& key) const {
        auto& bucket = buckets[get_hash(key)];
        
        for (auto node_pnt : bucket) {
            if (node_pnt->key() == key) {
                return const_iterator(node_pnt->sequence_pos);
            }
        }
        return end();
    }
    
    void insert(const MapPair& map_pair) {
        auto it = find(map_pair.first);
        if (it == end()) {
            _insert(map_pair);
        }
    }
    
    ValueType& operator[](const KeyType& key) {
        auto it = find(key);
        if (it == end()) {
            it = _insert({key, ValueType()});
        }
        return it->second;
    }
    
    void erase(const KeyType& key) {
        auto& bucket = buckets[get_hash(key)];
        for (auto it = bucket.begin(); it != bucket.end(); ++it) {
            if ((*it)->key() == key) {
                // delete
                const pNode& node_pnt = *it;
                
                nodes_sequence.erase(node_pnt->sequence_pos);
                bucket.erase(it);
                
                --sz;
                check_load_factor();
                
                return;
            }
        }
    }
    
    const ValueType& at(const KeyType& key) const {
        auto it = find(key);
        if (it == end()) {
            throw std::out_of_range("no such key");
        } else {
            return it->second;
        }
    }
    
    void clear() {
        for (const pNode& pnt : nodes_sequence) {
            buckets[pnt->bucket_index].pop_front();
        }
        nodes_sequence.clear();
        resize(INIT_CAPACITY);
        sz = 0;
    }
    
    size_t size() const {
        return sz;
    }
    
    bool empty() const {
        return (sz == 0);
    }
    
    const Hash& hash_function() const {
        return hasher;
    }
    
private:
    std::vector<BucketType> buckets;
    BucketType nodes_sequence;
    size_t capacity;
    size_t sz;
    
    Hash hasher;
    
    static constexpr double MAX_LOAD_FACTOR = 0.7;
    static constexpr size_t INIT_CAPACITY = 16;
    
    void rehash() {
        buckets.resize(capacity);
        for (pNode& node_pnt : nodes_sequence) {
            if (node_pnt->bucket_index < capacity)
                buckets[node_pnt->bucket_index].pop_front();
            node_pnt->bucket_index = get_hash(node_pnt->key());
            buckets[node_pnt->bucket_index].push_back(node_pnt);
        }
    }
    
    void check_load_factor() {  // checks a condintion for rehashing
        double load_factor = static_cast<double> (sz) / capacity;
        if (load_factor >= MAX_LOAD_FACTOR) {  // doubling the capacity
            capacity *= 2;
        } else if (capacity > INIT_CAPACITY && load_factor <= MAX_LOAD_FACTOR/4) {
            capacity /= 2;
        } else {
            return;
        }
        rehash();
    }
    
    size_t get_hash(const KeyType& key) const {
        return hasher(key) % capacity;
    }
    
    // insertion without any checks
    iterator _insert(const MapPair& map_pair) {
        ++sz;
        check_load_factor();
        
        pNode new_node = std::make_shared<Node>(map_pair);
        
        new_node->bucket_index = get_hash(new_node->key());
        auto& bucket = buckets[new_node->bucket_index];
        bucket.emplace_front(new_node);
        
        nodes_sequence.push_front(new_node);
        new_node->sequence_pos = nodes_sequence.begin();
        return iterator(nodes_sequence.begin());
    }
    
    void resize(size_t new_cap) {
        capacity = new_cap;
        buckets.resize(capacity, BucketType());
    }
};
