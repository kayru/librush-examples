#include "TestFramework.h"

#include <Rush/UtilArray.h>

using namespace Test;
using namespace Rush;

class DynamicArrayBasicTest final : public CpuTestCase
{
public:
	TestResult validate(GfxContext*, const TestImage*) override
	{
		// Push a few values and verify size/contents.
		DynamicArray<int> values;
		values.push_back(1);
		values.push_back(3);

		if (values.size() != 2)
		{
			return TestResult::fail("DynamicArray size mismatch after push_back");
		}

		if (values[0] != 1 || values[1] != 3)
		{
			return TestResult::fail("DynamicArray values mismatch after push_back");
		}

		// Resize with a default value and check new elements.
		values.resize(4, 9);
		if (values.size() != 4 || values[2] != 9 || values[3] != 9)
		{
			return TestResult::fail("DynamicArray resize default values mismatch");
		}

		// Verify ArrayView references DynamicArray storage.
		ArrayView<int> view(values);
		if (view.size() != values.size() || view.data() != values.data())
		{
			return TestResult::fail("ArrayView did not reference DynamicArray data");
		}

		// Validate slice contents.
		auto slice = values.slice(1, 2);
		if (slice.size() != 2 || slice[0] != 3 || slice[1] != 9)
		{
			return TestResult::fail("ArrayView slice mismatch");
		}

		// Ensure move construction transfers ownership.
		DynamicArray<int> moved(std::move(values));
		if (moved.size() != 4 || values.size() != 0)
		{
			return TestResult::fail("DynamicArray move did not transfer ownership");
		}

		return TestResult::pass();
	}
};

RUSH_REGISTER_TEST(DynamicArrayBasicTest, "util",
	"Exercises DynamicArray and ArrayView basic behaviors.");
