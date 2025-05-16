#include "struct_extensions.h"

namespace dap
{
DAP_IMPLEMENT_STRUCT_TYPEINFO_EXT(
	PDSAttachRequest,
	AttachRequest,
	"attach",
	DAP_FIELD(name, "name"),
	DAP_FIELD(type, "type"),
	DAP_FIELD(request, "request"),
	DAP_FIELD(projectSources, "projectSources"));

DAP_IMPLEMENT_STRUCT_TYPEINFO_EXT(
	PDSLaunchRequest,
	LaunchRequest,
	"launch",
	DAP_FIELD(name, "name"),
	DAP_FIELD(type, "type"),
	DAP_FIELD(request, "request"),
	DAP_FIELD(projectSources, "projectSources"));
}