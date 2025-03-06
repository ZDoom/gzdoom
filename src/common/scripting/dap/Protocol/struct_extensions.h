#pragma once

#include <dap/typeof.h>
#include <dap/types.h>
#include <dap/protocol.h>

namespace dap
{

// Extended AttachRequest struct for implementation specific parameters

struct PDSAttachRequest : public AttachRequest
{
	using Response = AttachResponse;
	string name;
	string type;
	string request;
	optional<array<Source>> projectSources;
};

struct PDSLaunchRequest : public LaunchRequest
{
	using Response = LaunchResponse;
	string name;
	string type;
	string request;
	optional<array<Source>> projectSources;
};

DAP_DECLARE_STRUCT_TYPEINFO(PDSAttachRequest);
DAP_DECLARE_STRUCT_TYPEINFO(PDSLaunchRequest);

}
