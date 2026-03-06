#ifndef UNION_FIND_H
#define UNION_FIND_H

#include <vector>
#include <map>
#include <string>
#include <cstdint>

class UnionFind {
public:
    UnionFind(int size);
    
    int find(int x);
    void merge(int x, int y);
    
    std::vector<std::vector<int>> get_components();
    
private:
    std::vector<int> parent_;
    std::vector<int> rank_;
};

class LocalDeduplicator {
public:
    LocalDeduplicator();
    
    void add_document(int doc_id, const std::vector<uint32_t>& signature);
    
    void perform_local_deduplication(double threshold);
    
    std::vector<int> get_duplicate_document_ids();
    
    std::vector<int> get_unique_document_ids();
    
private:
    std::vector<std::vector<uint32_t>> signatures_;
    std::vector<int> document_ids_;
    UnionFind* uf_;
    
    double calculate_jaccard_similarity(const std::vector<uint32_t>& sig1, 
                                         const std::vector<uint32_t>& sig2);
};

#endif // UNION_FIND_H