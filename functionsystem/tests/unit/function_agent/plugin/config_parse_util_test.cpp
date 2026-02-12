/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include "function_agent/plugin/config_parse_util.h"

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include "function_agent/plugin/plugin_config.h"

namespace functionsystem::test {
class ConfigParseUtilTest : public testing::Test {};

TEST_F(ConfigParseUtilTest, AllFieldsPresent)
{
    nlohmann::json config = { { function_agent::PLUGIN_ID, "plugin1" },
                              { function_agent::PLUGIN_ADDRESS, "localhost:8080" },
                              { function_agent::PLUGIN_TYPE, "grpc" },
                              { function_agent::PLUGIN_ENABLED, true },
                              { function_agent::PLUGIN_CRITICAL, false },
                              { function_agent::PLUGIN_INIT_PARAMS,
                                { { "param1", "value1" }, { "param2", "value2" } } } };

    function_agent::PluginInfo info = function_agent::ParsePlugin(config);

    EXPECT_EQ(info.id, "plugin1");
    EXPECT_EQ(info.type, "grpc");
    EXPECT_EQ(info.address, "localhost:8080");
    EXPECT_TRUE(info.enabled);
    EXPECT_FALSE(info.critical);
    EXPECT_EQ(info.extra_params["param1"], "value1");
    EXPECT_EQ(info.extra_params["param2"], "value2");
}

TEST_F(ConfigParseUtilTest, OptionalFieldsMissing)
{
    nlohmann::json config = { { function_agent::PLUGIN_ID, "plugin2" },
                              { function_agent::PLUGIN_ADDRESS, "localhost:8081" } };

    function_agent::PluginInfo info = function_agent::ParsePlugin(config);

    EXPECT_EQ(info.id, "plugin2");
    EXPECT_EQ(info.address, "localhost:8081");
    EXPECT_FALSE(info.enabled);
    EXPECT_FALSE(info.critical);
    EXPECT_TRUE(info.extra_params.empty());
}

TEST_F(ConfigParseUtilTest, ValidConfig)
{
    std::string input = R"({
        "plugin_groups": {
            "group1": [
                {"id": "plugin1", "address": "localhost:8080"},
                {"id": "plugin2", "address": "localhost:8081"}
            ]
        }
    })";

    function_agent::PluginsConfigRecords records = function_agent::ParsePluginsConfig(input);

    ASSERT_EQ(records.pluginGroups.size(), 1);
    EXPECT_EQ(records.pluginGroups["group1"].size(), 2);
}

TEST_F(ConfigParseUtilTest, InvalidJson)
{
    std::string input = "invalid json";

    function_agent::PluginsConfigRecords records = function_agent::ParsePluginsConfig(input);

    EXPECT_TRUE(records.pluginGroups.empty());
}

TEST_F(ConfigParseUtilTest, MissingPluginGroups)
{
    std::string input = R"({
        "other_field": "value"
    })";

    function_agent::PluginsConfigRecords records = function_agent::ParsePluginsConfig(input);

    EXPECT_TRUE(records.pluginGroups.empty());
}

TEST_F(ConfigParseUtilTest, PluginGroupsNotObject)
{
    std::string input = R"({
        "plugin_groups": "not an object"
    })";

    function_agent::PluginsConfigRecords records = function_agent::ParsePluginsConfig(input);

    EXPECT_TRUE(records.pluginGroups.empty());
}

TEST_F(ConfigParseUtilTest, PluginNotArray)
{
    std::string input = R"({
        "plugin_groups": {
            "group1": "not an array"
        }
    })";

    function_agent::PluginsConfigRecords records = function_agent::ParsePluginsConfig(input);

    EXPECT_TRUE(records.pluginGroups.empty());
}

TEST_F(ConfigParseUtilTest, PluginNotObject)
{
    std::string input = R"({
        "plugin_groups": {
            "group1": [{"id": }],
        }
    })";

    function_agent::PluginsConfigRecords records = function_agent::ParsePluginsConfig(input);

    EXPECT_TRUE(records.pluginGroups.empty());
}

TEST_F(ConfigParseUtilTest, ReturnsCorrectGroup)
{
    function_agent::PluginsConfigRecords records;
    // empty records
    function_agent::PluginGroup result = GetPluginConfigOfGroup(records, "group1");
    EXPECT_EQ(result.size(), 0);

    std::string input = R"({
        "plugin_groups": {
            "group1": [
                {"id": "plugin1", "address": "localhost:8080"},
                {"id": "plugin2", "address": "localhost:8081"}
            ]
        }
    })";

    records = function_agent::ParsePluginsConfig(input);
    result = GetPluginConfigOfGroup(records, "group1");

    EXPECT_EQ(result.size(), 2);
    EXPECT_EQ(result[0].id, "plugin1");
    EXPECT_EQ(result[1].id, "plugin2");

    result = GetPluginConfigOfGroup(records, "not_exist");
    EXPECT_EQ(result.size(), 0);
}
}  // namespace functionsystem::test
