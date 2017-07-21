//
// Created by Yunming Zhang on 7/11/17.
//

#ifndef GRAPHIT_EDGESET_APPLY_FUNCTIONS_H
#define GRAPHIT_EDGESET_APPLY_FUNCTIONS_H

#include "vertexsubset.h"
#include "infra_gapbs/builder.h"
#include "infra_gapbs/benchmark.h"
#include "infra_gapbs/bitmap.h"
#include "infra_gapbs/command_line.h"
#include "infra_gapbs/graph.h"
#include "infra_gapbs/platform_atomics.h"
#include "infra_gapbs/pvector.h"

#include "infra_ligra/ligra/ligra.h"
#include "infra_ligra/ligra/utils.h"

template<typename APPLY_FUNC>
VertexSubset<NodeID> *edgeset_apply_pull_serial(Graph &g, APPLY_FUNC apply_func) {

    for (NodeID u = 0; u < g.num_nodes(); u++) {
        for (NodeID v : g.in_neigh(u))
            apply_func(v, u);
    }
    return new VertexSubset<NodeID>(g.num_nodes(), g.num_nodes());
}

template<typename APPLY_FUNC>
VertexSubset<NodeID> *edgeset_apply_push_serial(Graph &g, APPLY_FUNC apply_func) {

    for (NodeID u = 0; u < g.num_nodes(); u++) {
        for (NodeID v : g.in_neigh(u))
            apply_func(u, v);
    }
    return new VertexSubset<NodeID>(g.num_nodes(), g.num_nodes());
}

template<typename APPLY_FUNC, typename FROM_FUNC, typename TO_FUNC>
VertexSubset<NodeID> *edgeset_apply_pull_serial_from_filter_func_to_filter_func
        (Graph &g, FROM_FUNC from_func, TO_FUNC to_func, APPLY_FUNC apply_func) {
    for (NodeID u = 0; u < g.num_nodes(); u++) {
        if (to_func(u)) {
            for (NodeID v : g.in_neigh(u)) {
                if (from_func(u)) {
                    apply_func(v, u);
                }
            }
        }
    }
    return new VertexSubset<NodeID>(g.num_nodes(), g.num_nodes());
};


template<typename APPLY_FUNC, typename TO_FUNC>
VertexSubset<NodeID> *edgeset_apply_pull_serial_from_vertexset_to_filter_func_with_frontier
        (Graph &g, VertexSubset<NodeID> *from_vertexset, TO_FUNC to_func, APPLY_FUNC apply_func) {

    //The original GAPBS implmentation from BFS

    //    int64_t awake_count = 0;
//    next.reset();
//#pragma omp parallel for reduction(+ : awake_count) schedule(dynamic, 1024)
//    for (NodeID u=0; u < g.num_nodes(); u++) {
//        if (parent[u] < 0) {
//            for (NodeID v : g.in_neigh(u)) {
//                if (front.get_bit(v)) {
//                    parent[u] = v;
//                    awake_count++;
//                    next.set_bit(u);
//                    break;
//                }
//            }
//        }

    Bitmap *next = new Bitmap(g.num_nodes());
    Bitmap *current_frontier = from_vertexset->bitmap_;
    int count = 0;

    for (NodeID u = 0; u < g.num_nodes(); u++) {
        if (to_func(u)) {
            for (NodeID v : g.in_neigh(u)) {
                if (current_frontier->get_bit(v)) {
                    if (apply_func(v, u)) {
                        next->set_bit(u);
                        count++;
                        if (!to_func(u)) break;
                    }
                }
            }
        }
    }

    VertexSubset<NodeID> *next_frontier = new VertexSubset<NodeID>(g.num_nodes(), count);
    next_frontier->bitmap_ = next;

    return next_frontier;
};


template<typename APPLY_FUNC>
VertexSubset<NodeID> *edgeset_apply_pull_serial_from_vertexset_with_frontier
        (Graph &g, VertexSubset<NodeID> *from_vertexset, APPLY_FUNC apply_func) {

    //The original GAPBS implmentation from BFS

    //    int64_t awake_count = 0;
//    next.reset();
//#pragma omp parallel for reduction(+ : awake_count) schedule(dynamic, 1024)
//    for (NodeID u=0; u < g.num_nodes(); u++) {
//        if (parent[u] < 0) {
//            for (NodeID v : g.in_neigh(u)) {
//                if (front.get_bit(v)) {
//                    parent[u] = v;
//                    awake_count++;
//                    next.set_bit(u);
//                    break;
//                }
//            }
//        }

    Bitmap *next = new Bitmap(g.num_nodes());
    Bitmap *current_frontier = from_vertexset->bitmap_;
    int count = 0;
    for (NodeID u = 0; u < g.num_nodes(); u++) {
        for (NodeID v : g.in_neigh(u)) {
            if (current_frontier->get_bit(v)) {
                if (apply_func(v, u)) {
                    next->set_bit(u);
                    count++;
                }
            }
        }
    }

    VertexSubset<NodeID> *next_frontier = new VertexSubset<NodeID>(g.num_nodes(), count);
    next_frontier->bitmap_ = next;

    return next_frontier;
};

template<typename APPLY_FUNC>
VertexSubset<NodeID> *edgeset_apply_pull_serial_weighted_from_vertexset_with_frontier
        (WGraph &g, VertexSubset<NodeID> *from_vertexset, APPLY_FUNC apply_func) {

    Bitmap *next = new Bitmap(g.num_nodes());
    Bitmap *current_frontier = from_vertexset->bitmap_;
    int count = 0;
    for (NodeID u = 0; u < g.num_nodes(); u++) {
        for (WNode s : g.in_neigh(u)) {
            if (current_frontier->get_bit(s.v)) {
                if (apply_func(s.v, u, s.w)) {
                    next->set_bit(u);
                    count++;
                }
            }
        }
    }

    VertexSubset<NodeID> *next_frontier = new VertexSubset<NodeID>(g.num_nodes(), count);
    next_frontier->bitmap_ = next;

    return next_frontier;
};


template<typename APPLY_FUNC, typename FROM_FUNC, typename TO_FUNC>
VertexSubset<NodeID> *edgeset_apply_pull_serial_from_filter_func_to_filter_func_with_frontier
        (Graph &g, FROM_FUNC from_func, TO_FUNC to_func, APPLY_FUNC apply_func) {


    VertexSubset<NodeID> *next_frontier = new VertexSubset<NodeID>(g.num_nodes(), 0);
    SlidingQueue<NodeID> queue = SlidingQueue<NodeID>(g.num_nodes());
    for (int i = 0; i < g.num_nodes(); i++) {
        queue.push_back(i);
    }

    {
        QueueBuffer<NodeID> lqueue(queue);
        for (auto q_iter = queue.begin(); q_iter < queue.end(); q_iter++) {
            NodeID u = *q_iter;
            for (NodeID v : g.out_neigh(u)) {
                if (to_func(v)) {
                    apply_func(u, v);
                }
            }
        }
        lqueue.flush();
    }
    //Here, we might be coping this too much
    //next_frontier->dense_vertex_set_ = queue;
    return next_frontier;

}


template<typename APPLY_FUNC>
VertexSubset<NodeID> *edgeset_apply_pull_parallel(Graph &g, APPLY_FUNC apply_func) {

#pragma omp parallel for schedule(dynamic, 64)
    for (NodeID u = 0; u < g.num_nodes(); u++) {
        for (NodeID v : g.in_neigh(u))
            apply_func(v, u);
    }
    return new VertexSubset<NodeID>(g.num_nodes(), g.num_nodes());
}

template<typename APPLY_FUNC>
VertexSubset<NodeID> *edgeset_apply_pull_serial_weighted(WGraph &g, APPLY_FUNC apply_func) {

#pragma omp parallel for schedule(dynamic, 64)
    for (NodeID u = 0; u < g.num_nodes(); u++) {
        for (WNode s : g.in_neigh(u))
            apply_func(s.v, u, s.w);
    }
    return new VertexSubset<NodeID>(g.num_nodes(), g.num_nodes());
}


//Code largely borrowed from TDStep for GAPBS
template<typename APPLY_FUNC, typename TO_FUNC>
VertexSubset<NodeID> *edgeset_apply_push_serial_from_vertexset_to_filter_func_with_frontier
        (Graph &g, VertexSubset<NodeID> *from_vertexset, TO_FUNC to_func, APPLY_FUNC apply_func) {


    //The original GAPBS implmentation from BFS

//    //need to get a SlidingQueue out of the vertexsubset
//    SlidingQueue<NodeID> &queue;
//
//    {
//        QueueBuffer<NodeID> lqueue(queue);
//        for (auto q_iter = queue.begin(); q_iter < queue.end(); q_iter++) {
//            NodeID u = *q_iter;
//            for (NodeID v : g.out_neigh(u)) {
//                NodeID curr_val = parent[v];
////              Test, test and set
////                if (curr_val < 0) {
////                    if (compare_and_swap(parent[v], curr_val, u)) {
////                        lqueue.push_back(v);
////                    }
////                }
//            }
//        }
//        lqueue.flush();
//    }
//
    VertexSubset<NodeID> *next_frontier = new VertexSubset<NodeID>(g.num_nodes(), 0);
//    SlidingQueue<NodeID> &queue = from_vertexset->dense_vertex_set_;
//    {
//        QueueBuffer<NodeID> lqueue(queue);
//        for (auto q_iter = queue.begin(); q_iter < queue.end(); q_iter++) {
//            NodeID u = *q_iter;
//            for (NodeID v : g.out_neigh(u)) {
//                if (to_func(v)) {
//                    apply_func(u, v);
//                }
//            }
//        }
//        lqueue.flush();
//    }
//    //Here, we might be coping this too much
//    next_frontier->dense_vertex_set_ = queue;
    return next_frontier;
}

template<typename APPLY_FUNC>
VertexSubset<NodeID> *edgeset_apply_push_parallel_from_vertexset_with_frontier
        (Graph &g, VertexSubset<NodeID> *from_vertexset, APPLY_FUNC apply_func) {
    VertexSubset<NodeID> *next_frontier = new VertexSubset<NodeID>(g.num_nodes(), 0);

//    std::cout << "dense_vertex_set size: " << from_vertexset->dense_vertex_set_.size() << std::endl;
//
//    SlidingQueue<NodeID> &queue = from_vertexset->dense_vertex_set_;
//    std::cout << "queue size: " << queue.size() << std::endl;
//    {
//        QueueBuffer<NodeID> lqueue(queue);
//        for (auto q_iter = queue.begin(); q_iter < queue.end(); q_iter++) {
//            NodeID u = *q_iter;
//            for (NodeID v : g.out_neigh(u)) {
//                    if(apply_func(u, v)){
//                        lqueue.push_back(v);
//                    }
//            }
//        }
//        lqueue.flush();
//    }
//    //Here, we might be coping this too much
//    next_frontier->dense_vertex_set_ = queue;

    long numVertices = g.num_nodes(), numEdges = g.num_edges();
    //NodeID *G = GA.V;
    //long m = V.numNonzeros();
    long m = from_vertexset->size();
    //std::cout << "m: " << m << std::endl;

    //if (numVertices != V.numRows()) {
    //replaced with our own API
    if (numVertices != from_vertexset->getVerticesRange()) {

        cout << "edgeMap: Sizes Don't match" << endl;
        abort();
    }
    // used to generate nonzero indices to get degrees
    uintT *degrees = newA(uintT, m);
    NodeID *frontierVertices;

    // We probably need this when we get something that doesn't have a dense set, not sure
    // We can also write our own, the eixsting one doesn't quite work for bitvectors
    from_vertexset->toSparse();

    //from_vertexset->printDenseSet();

    frontierVertices = newA(NodeID, m);
    {
        for (long i = 0; i < m; i++) {
            //construct a vertex wrapper for Ligra, not needed for us
            //vertex v = G[V.s[i]];
            //be careful here later, the degree type for GAPBS and Ligra doens't quite match
            //std::cout << "i: " << i << std::endl;
            //std::cout << from_vertexset->dense_vertex_set_ << std::endl;
            NodeID v = from_vertexset->dense_vertex_set_[i];
            degrees[i] = g.out_degree(v);
            frontierVertices[i] = v;
        }
    }
    //cout << "after suming up degree " << endl;

    uintT outDegrees = sequence::plusReduce(degrees, m);
    if (outDegrees == 0) return next_frontier;
//
//    pair<long, uintE *> R = edgeMapSparse(frontierVertices, V.s, degrees, V.numNonzeros(), f);

    uintT* offsets = degrees;
    long outEdgeCount = sequence::plusScan(offsets, degrees, m);
    uintE * outEdges = newA(uintE,outEdgeCount);

    {parallel_for (long i = 0; i < m; i++) {
            NodeID src = from_vertexset->dense_vertex_set_[i];
            uintT offset = offsets[i];
            //vertex vert = frontierVertices[i];
            //vert.decodeOutNghSparse(v, o, f, outEdges);
            int j = 0;
            for (NodeID dst : g.out_neigh(src)) {
                if (apply_func(src, dst)) {
                    outEdges[offset + j] = dst;
                } else{
                    outEdges[offset + j] = UINT_E_MAX;
                }
                j++;
            }

        }}
    uintE * nextIndices = newA(uintE, outEdgeCount);
    // if(remDups) remDuplicates(outEdges,flags,outEdgeCount,remDups);
    // Filter out the empty slots (marked with -1)
    long nextM = sequence::filter(outEdges,nextIndices,outEdgeCount,nonMaxF());
    free(outEdges);
    //std::cout << "nextM: " << nextM << std::endl;

    //return pair<long,uintE*>(nextM, nextIndices);



//    //cout << "size (S) = " << R.first << endl;
    free(degrees);
//    free(frontierVertices);
//    return vertexSubset(numVertices, R.first, R.second);

    next_frontier->num_vertices_ = nextM;
    next_frontier->dense_vertex_set_ = nextIndices;

    return next_frontier;
}


#endif //GRAPHIT_EDGESET_APPLY_FUNCTIONS_H