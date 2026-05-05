#include "gtest/gtest.h"
#include <mockcpp/mockcpp.hpp>
#include "json_parser.h"

using namespace std;
using namespace hccl;

class JsonParserTest : public testing::Test {
protected:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    virtual void SetUp() {}
    virtual void TearDown() {}
};

TEST_F(JsonParserTest, GetJsonPorperty_When_PorpNameIsNullptr_Expect_Throw)
{
    nlohmann::json obj = nlohmann::json::object();
    EXPECT_THROW(GetJsonProperty(obj, nullptr, true), NullPtrException);
}

TEST_F(JsonParserTest, GetJsonPorperty_When_NotRequired_Expect_Empty)
{
    nlohmann::json obj = nlohmann::json::object();
    string ret = GetJsonProperty(obj, "不存在", false)
    EXPECT_EQ(ret, "");
}

TEST_F(JsonParserTest, GetJsonPorperty_When_MissingProperty_Expect_Throw)
{
    nlohmann::json obj = nlohmann::json::object();
    EXPECT_THROW(GetJsonProperty(obj, "missing", true), InvalidParamsException);
}

TEST_F(JsonParserTest, GetJsonPorpertyUInt_When_MissingProperty_Expect_Throw)
{
    nlohmann::json obj = nlohmann::json::object();
    EXPECT_THROW(GetJsonPropertyUInt(obj, "missing", true, 0), InvalidParamsException);
}

TEST_F(JsonParserTest, GetJsonPorpertyUInt_When_ValueExceedsUint32Max_Expect_Throw)
{
    nlohmann::json obj;
    obj["test"] = INT64_MAX;
    EXPECT_THROW(GetJsonPropertyUInt(obj, "test", true, 0), InvalidParamsException);
}

TEST_F(JsonParserTest, GetJsonPorpertySInt_When_MissingProperty_Expect_Throw)
{
    nlohmann::json obj = nlohmann::json::object();
    EXPECT_THROW(GetJsonPropertySInt(obj, "missing", true, 0), InvalidParamsException);
}

TEST_F(JsonParserTest, GetJsonPorpertySInt_When_ValueExceedsSint32Max_Expect_Throw)
{
    nlohmann::json obj;
    obj["test"] = INT64_MAX;
    EXPECT_THROW(GetJsonPropertySInt(obj, "test", true, 0), InvalidParamsException);
}

TEST_F(JsonParserTest, GetJsonPorpertyList_When_MissingProperty_Expect_Throw)
{
    nlohmann::json obj = nlohmann::json::object();
    nlohmann::json listObj;
    EXPECT_THROW(GetJsonPropertyList(obj, "missing", listObj), InvalidParamsException);
}

TEST_F(JsonParserTest, GetJsonPorpertyList_When_NotArray_Expect_Throw)
{
    nlohmann::json obj;
    obj["test"] = "not_array";
    nlohmann::json listObj;
    EXPECT_THROW(GetJsonPropertyList(obj, "test", listObj), InvalidParamsException);
}