
#include <AzCore/Component/TickBus.h>
#include <AzCore/Script/ScriptTimePoint.h>

//#ifndef NOINLINE
//#if defined(__GNUC__)
//#define NOINLINE __attribute__ ((noinline))
//#elif defined(_MSC_VER)
//#define NOINLINE __declspec(noinline)
//#else
//#define NOINLINE
//#endif
//#endif

namespace AZ {
	double ScriptTimePoint_GetSeconds(ScriptTimePoint& self)
	{
		return self.GetSeconds();
	}
}
namespace AZStd {
	namespace chrono {
		/*NOINLINE*/ system_clock::time_point system_clock_now()
		{
			return system_clock::now();
		}
	}
}