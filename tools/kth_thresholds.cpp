#include <iostream>
#include <optional>
#include <unordered_set>

#include "boost/algorithm/string/classification.hpp"
#include "boost/algorithm/string/split.hpp"
#include <boost/functional/hash.hpp>
#include "boost/algorithm/string/split.hpp"

#include "mio/mmap.hpp"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

#include "mappable/mapper.hpp"

#include "cursor/max_scored_cursor.hpp"
#include "index_types.hpp"
#include "io.hpp"
#include "query/algorithm.hpp"
#include "util/util.hpp"
#include "wand_data_compressed.hpp"
#include "wand_data_raw.hpp"

#include "scorer/scorer.hpp"

#include "CLI/CLI.hpp"

using namespace pisa;

int main(int argc, const char **argv)
{
    spdlog::drop("");
    spdlog::set_default_logger(spdlog::stderr_color_mt(""));


    std::string query_filename;
    std::string scores_filename;
    std::optional<std::string> pairs_filename;

    std::optional<std::string> terms_file;
    std::optional<std::string> stemmer = std::nullopt;

    CLI::App app{"A tool for performing threshold estimation using k-th term score informations."};
    app.add_option("-q,--query", query_filename, "Queries filename")->required();
    app.add_option("-s,--scores", scores_filename, "Queries filename")->required();
    auto *terms_opt =
        app.add_option("--terms", terms_file, "Text file with terms in separate lines");
    app.add_option("--stemmer", stemmer, "Stemmer type")->needs(terms_opt);
    app.add_option("-p,--pairs", pairs_filename, "Pairs filename");

    CLI11_PARSE(app, argc, argv);

    std::vector<Query> queries;
    auto parse_query = resolve_query_parser(queries, terms_file, std::nullopt, stemmer);
    std::ifstream is(query_filename);
    io::for_each_line(is, parse_query);

    std::vector<float> scores;
    std::ifstream tin(scores_filename);
    std::string t;
    while (std::getline(tin, t)) {
        scores.push_back(std::stof(t));
    }

    using Pair = std::pair<uint32_t, uint32_t>;
    std::unordered_set<Pair, boost::hash<Pair>> pairs_set;
    if(pairs_filename){
        std::ifstream pin(*pairs_filename);
        while (std::getline(pin, t)) {
            std::vector<std::string> p;
            boost::algorithm::split(p, t, boost::is_any_of(" \t"));
            pairs_set.emplace(std::stoi(p[0]), std::stoi(p[1]));
        }
        spdlog::info("Number of pairs loaded: {}", pairs_set.size());
    }

    for (auto const &query : queries) {
        float threshold = 0;
        for (auto &&t : query.terms){
            threshold = std::max(threshold, scores[t]);
        }

        auto terms = query.terms;
        for (size_t i = 0; i < terms.size(); ++i) {
            for (size_t j = i + 1; j < terms.size(); ++j) {
                if(pairs_set.count({terms[i], terms[j]})){
                    std::cout << terms[i] << " and " << terms[j] << std::endl;
                }
            }
        }

        std::cout << threshold << '\n';
    }
}