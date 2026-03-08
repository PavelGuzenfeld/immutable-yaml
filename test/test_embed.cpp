#include <cassert>
#include <iostream>

#include "sample_config.yaml.hpp"

int main()
{
    constexpr auto& doc = yaml::embedded::sample_config;

    // Verify it parsed into a mapping
    static_assert(doc.root_.is_mapping());

    // Access nested "database" section
    auto db = doc.find(doc.root_, "database");
    assert(db.has_value());
    assert(db->is_mapping());

    auto host = doc.find(*db, "host");
    assert(host.has_value());
    assert(host->as_string() == "localhost");

    auto port = doc.find(*db, "port");
    assert(port.has_value());
    assert(port->as_int() == 5432);

    auto ssl = doc.find(*db, "ssl");
    assert(ssl.has_value());
    assert(ssl->as_bool() == true);

    // Access nested "cache" section
    auto cache = doc.find(doc.root_, "cache");
    assert(cache.has_value());

    auto ttl = doc.find(*cache, "ttl");
    assert(ttl->as_int() == 3600);

    std::cout << "yaml embed test passed\n";
    return 0;
}
