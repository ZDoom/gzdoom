
#pragma once

void AddSourceFileTimestamp(const char *timestamp);

namespace
{
	struct TimestampSourceFile
	{
		TimestampSourceFile() { AddSourceFileTimestamp(__TIME__); }
	} timestamp;
}
