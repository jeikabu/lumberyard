
#include "RustAz.h"

extern "C" {
	void TickBus_BusConnect(AZ::TickBus::Handler* bus)
	{
		bus->BusConnect();
	}

	void TickRequestBus_BroadcastResult_GetTickDeltaTime(float& results)
	{
		AZ::TickRequestBus::BroadcastResult(results, &AZ::TickRequestBus::Events::GetTickDeltaTime);
	}

	void TickRequestBus_BroadcastResult_GetTimeAtCurrentTick(AZ::ScriptTimePoint& results)
	{
		AZ::TickRequestBus::BroadcastResult(results, &AZ::TickRequestBus::Events::GetTimeAtCurrentTick);
	}
}