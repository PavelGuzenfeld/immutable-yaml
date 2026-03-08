// Example: embed a YAML file as a compile-time constant
//
// The YAML file is read at build time by CMake's yaml_embed().
// Comments are stripped, allocation sizes are computed automatically.
// The result is a constexpr object in static storage.

#include <cassert>
#include <cstdio>

#include "app_config.yaml.hpp"

int main()
{
    constexpr auto& cfg = yaml::embedded::app_config;
    static_assert(cfg.root_.is_mapping());

    // Access nested sections
    auto app = cfg.find(cfg.root_, "app");
    assert(app.has_value());
    std::printf("app: %.*s v%.*s\n",
        static_cast<int>(cfg.find(*app, "name")->as_string().size()),
        cfg.find(*app, "name")->as_string().data(),
        static_cast<int>(cfg.find(*app, "version")->as_string().size()),
        cfg.find(*app, "version")->as_string().data());

    auto db = cfg.find(cfg.root_, "database");
    std::printf("db: %.*s:%lld (pool=%lld)\n",
        static_cast<int>(cfg.find(*db, "host")->as_string().size()),
        cfg.find(*db, "host")->as_string().data(),
        static_cast<long long>(cfg.find(*db, "port")->as_int()),
        static_cast<long long>(cfg.find(*db, "pool_size")->as_int()));

    // Iterate feature flags
    auto features = cfg.find(cfg.root_, "features");
    assert(features->is_sequence());
    std::printf("features (%zu):", cfg.size(*features));
    for (auto const& val : cfg.values(*features))
        std::printf(" %.*s",
            static_cast<int>(val.as_string().size()),
            val.as_string().data());
    std::printf("\n");

    return 0;
}
