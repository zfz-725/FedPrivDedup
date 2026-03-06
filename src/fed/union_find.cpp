#include "union_find.h"
#include <algorithm>
#include <iostream>

UnionFind::UnionFind(int size) {
    parent_.resize(size);
    rank_.resize(size, 0);
    
    for (int i = 0; i < size; i++) {
        parent_[i] = i;
    }
}

int UnionFind::find(int x) {
    if (parent_[x] != x) {
        parent_[x] = find(parent_[x]);
    }
    return parent_[x];
}

void UnionFind::merge(int x, int y) {
    int root_x = find(x);
    int root_y = find(y);
    
    if (root_x == root_y) return;
    
    if (rank_[root_x] < rank_[root_y]) {
        parent_[root_x] = root_y;
    } else if (rank_[root_x] > rank_[root_y]) {
        parent_[root_y] = root_x;
    } else {
        parent_[root_y] = root_x;
        rank_[root_x]++;
    }
}

std::vector<std::vector<int>> UnionFind::get_components() {
    std::map<int, std::vector<int>> components;
    
    for (size_t i = 0; i < parent_.size(); i++) {
        int root = find(i);
        components[root].push_back(i);
    }
    
    std::vector<std::vector<int>> result;
    for (auto& entry : components) {
        result.push_back(entry.second);
    }
    
    return result;
}

LocalDeduplicator::LocalDeduplicator() : uf_(nullptr) {
}

void LocalDeduplicator::add_document(int doc_id, const std::vector<uint32_t>& signature) {
    document_ids_.push_back(doc_id);
    signatures_.push_back(signature);
}

double LocalDeduplicator::calculate_jaccard_similarity(const std::vector<uint32_t>& sig1, 
                                                         const std::vector<uint32_t>& sig2) {
    if (sig1.size() != sig2.size()) {
        return 0.0;
    }
    
    int match_count = 0;
    int num_hash = sig1.size();
    
    for (size_t i = 0; i < sig1.size(); i++) {
        if (sig1[i] == sig2[i]) {
            match_count++;
        }
    }
    
    return static_cast<double>(match_count) / num_hash;
}

void LocalDeduplicator::perform_local_deduplication(double threshold) {
    int num_docs = signatures_.size();
    
    if (num_docs == 0) {
        return;
    }
    
    uf_ = new UnionFind(num_docs);
    
    for (int i = 0; i < num_docs; i++) {
        for (int j = i + 1; j < num_docs; j++) {
            double similarity = calculate_jaccard_similarity(signatures_[i], signatures_[j]);
            
            if (similarity >= threshold) {
                uf_->merge(i, j);
            }
        }
    }
}

std::vector<int> LocalDeduplicator::get_duplicate_document_ids() {
    std::vector<int> duplicates;
    
    if (!uf_) {
        return duplicates;
    }
    
    auto components = uf_->get_components();
    
    for (const auto& component : components) {
        if (component.size() > 1) {
            for (size_t i = 1; i < component.size(); i++) {
                duplicates.push_back(document_ids_[component[i]]);
            }
        }
    }
    
    return duplicates;
}

std::vector<int> LocalDeduplicator::get_unique_document_ids() {
    std::vector<int> unique_ids;
    
    if (!uf_) {
        return document_ids_;
    }
    
    auto components = uf_->get_components();
    
    for (const auto& component : components) {
        if (component.size() > 0) {
            unique_ids.push_back(document_ids_[component[0]]);
        }
    }
    
    return unique_ids;
}