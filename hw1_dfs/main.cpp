#include <boost/coroutine2/all.hpp>
#include <iostream>
#include <vector>

using Graph = std::vector<std::vector<int>>;
using coro_t = boost::coroutines2::coroutine<int>;

void dfs_impl(int v,
              const Graph& graph,
              std::vector<bool>& used,
              coro_t::push_type& sink) {
    used[v] = true;

    std::cout << "Coroutine yields vertex_____" << v << "\n";
    sink(v);

    for (int to : graph[v]) {
        if (!used[to]) {
            dfs_impl(to, graph, used, sink);
        }
    }
}

void run_dfs(const Graph& graph, int start, const std::string& name) {
    std::cout << "\n=== " << name << " ===\n";

    coro_t::pull_type source([&](coro_t::push_type& sink) {
        std::vector<bool> used(graph.size(), false);
        dfs_impl(start, graph, used, sink);
    });

    for (int v : source) {
        std::cout << "Main received vertex________" << v << "\n";
    }
}

int main() {
    Graph graph1 = {
        {1, 2},
        {3, 4},
        {5},
        {},
        {5},
        {}
    };
    Graph graph2 = {
        {1, 2},
        {2, 3},
        {0, 4},
        {4},
        {}
    };

    run_dfs(graph1, 0, "Graph 1");
    run_dfs(graph2, 0, "Graph 2");

    return 0;
}