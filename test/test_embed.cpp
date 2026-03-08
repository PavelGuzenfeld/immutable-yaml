#include <cassert>

#include "sample_config.yaml.hpp"

int main()
{
    constexpr auto& doc = yaml::embedded::sample_config;

    static_assert(doc.root_.is_mapping());

    auto db = doc.find(doc.root_, "database");
    assert(db.has_value());
    assert(db->is_mapping());

    assert(doc.find(*db, "host")->as_string() == "localhost");
    assert(doc.find(*db, "port")->as_int() == 5432);
    assert(doc.find(*db, "ssl")->as_bool() == true);

    auto cache = doc.find(doc.root_, "cache");
    assert(cache.has_value());
    assert(doc.find(*cache, "ttl")->as_int() == 3600);

    return 0;
}
