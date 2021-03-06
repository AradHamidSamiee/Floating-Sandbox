#include <GameCore/Vectors.h>

#include <GameCore/SysSpecifics.h>

#include "Utils.h"

#include <cmath>

#include "gtest/gtest.h"

TEST(VectorsTests, Sum_2f)
{
	vec2f a(1.0f, 5.0f);
	vec2f b(2.0f, 4.0f);
	vec2f c = a + b;

    EXPECT_EQ(c.x, 3.0f);
	EXPECT_EQ(c.y, 9.0f);
}

TEST(VectorsTests, Sum_4f)
{
    vec4f a(1.0f, 5.0f, 20.0f, 100.4f);
    vec4f b(2.0f, 4.0f, 40.0f, 200.0f);
    vec4f c = a + b;

    EXPECT_EQ(c.x, 3.0f);
    EXPECT_EQ(c.y, 9.0f);
    EXPECT_EQ(c.z, 60.0f);
    EXPECT_EQ(c.w, 300.4f);
}

class Normalization2fTest : public testing::TestWithParam<std::tuple<vec2f, vec2f>>
{
public:
    virtual void SetUp() {}
    virtual void TearDown() {}
};

INSTANTIATE_TEST_SUITE_P(
    VectorTests,
    Normalization2fTest,
    ::testing::Values(
        std::make_tuple(vec2f{ 1.0f, 0.0f }, vec2f{ 1.0f, 0.0f }),
        std::make_tuple(vec2f{ 1.0f, 1.0f }, vec2f{ 1.0f / std::sqrt(2.0f), 1.0f / std::sqrt(2.0f) }),
        std::make_tuple(vec2f{ 3.0f, 4.0f }, vec2f{ 3.0f / 5.0f, 4.0f / 5.0f}),
        std::make_tuple(vec2f{ 0.0f, 0.0f }, vec2f{ 0.0f, 0.0f })
    ));

TEST_P(Normalization2fTest, Normalization2fTest)
{
    vec2f input = std::get<0>(GetParam());
    vec2f expected = std::get<1>(GetParam());

    vec2f calcd1 = input.normalise();
    EXPECT_TRUE(ApproxEquals(calcd1.x, expected.x, 0.0001f));
    EXPECT_TRUE(ApproxEquals(calcd1.y, expected.y, 0.0001f));

    float len = input.length();
    vec2f calcd2 = input.normalise(len);
    EXPECT_TRUE(ApproxEquals(calcd2.x, expected.x, 0.0001f));
    EXPECT_TRUE(ApproxEquals(calcd2.y, expected.y, 0.0001f));
}