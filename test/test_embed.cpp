#include <cassert>
#include <iostream>

#include "sample_config.yaml.hpp"

int main()
{
    constexpr auto& config = yaml::embedded::sample_config;

    // Verify it parsed into a mapping
    static_assert(std::holds_alternative<yaml::ct::detail::yaml_container>(config.root_));

    auto& root = std::get<yaml::ct::detail::yaml_container>(config.root_);
    assert(root.is_mapping());

    auto& mapping = root.as_mapping();

    // Access nested "database" section
    auto db = mapping.find("database");
    assert(db.has_value());

    auto& db_container = std::get<yaml::ct::detail::yaml_container>(*db);
    assert(db_container.is_mapping());

    auto& db_map = db_container.as_mapping();
    auto host = db_map.find("host");
    assert(host.has_value());

    auto& host_str = std::get<yaml::ct::detail::string_type>(*host);
    assert(host_str.view() == "localhost");

    auto port = db_map.find("port");
    assert(port.has_value());
    assert(std::get<yaml::ct::detail::integer>(*port) == 5432);

    auto ssl = db_map.find("ssl");
    assert(ssl.has_value());
    assert(std::get<yaml::ct::detail::boolean>(*ssl) == true);

    // Access nested "cache" section
    auto cache = mapping.find("cache");
    assert(cache.has_value());

    auto& cache_container = std::get<yaml::ct::detail::yaml_container>(*cache);
    auto& cache_map = cache_container.as_mapping();
    assert(std::get<yaml::ct::detail::integer>(*cache_map.find("ttl")) == 3600);

    std::cout << "yaml embed test passed\n";
    return 0;
}
