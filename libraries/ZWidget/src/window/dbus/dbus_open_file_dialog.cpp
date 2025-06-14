
#include "dbus_open_file_dialog.h"
#include <dbus/dbus.h>

DBusOpenFileDialog::DBusOpenFileDialog(std::string ownerHandle) : ownerHandle(ownerHandle)
{
}

std::string DBusOpenFileDialog::Filename() const
{
	return outputFilenames.empty() ? std::string() : outputFilenames.front();
}

std::vector<std::string> DBusOpenFileDialog::Filenames() const
{
	return outputFilenames;
}

void DBusOpenFileDialog::SetMultiSelect(bool multiselect)
{
	this->multiSelect = multiSelect;
}

void DBusOpenFileDialog::SetFilename(const std::string &filename)
{
	inputFilename = filename;
}

void DBusOpenFileDialog::SetDefaultExtension(const std::string& extension)
{
	defaultExt = extension;
}

void DBusOpenFileDialog::AddFilter(const std::string &filter_description, const std::string &filter_extension)
{
	filters.push_back({ filter_description, filter_extension });
}

void DBusOpenFileDialog::ClearFilters()
{
	filters.clear();
}

void DBusOpenFileDialog::SetFilterIndex(int filter_index)
{
	this->filter_index = filter_index;
}

void DBusOpenFileDialog::SetInitialDirectory(const std::string &path)
{
	initialDirectory = path;
}

void DBusOpenFileDialog::SetTitle(const std::string &title)
{
	this->title = title;
}

bool DBusOpenFileDialog::Show()
{
	// https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.FileChooser.html
		
	dbus_bool_t bresult = {};
	DBusError error = {};
	dbus_error_init(&error);
		
	DBusConnection* connection = dbus_bus_get(DBUS_BUS_SESSION, &error);
	if (!connection)
	{
		dbus_error_free(&error);
		return false;
	}

	std::string busname = dbus_bus_get_unique_name(connection);

	DBusMessage* request = dbus_message_new_method_call("org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop", "org.freedesktop.portal.FileChooser", "OpenFile");
	if (!request)
	{
		dbus_connection_unref(connection);
		dbus_error_free(&error);
		return false;
	}

	const char* parentWindow = ownerHandle.c_str();
	const char* title = this->title.c_str();

	DBusMessageIter requestArgs = {};
	dbus_message_iter_init_append(request, &requestArgs);

	bresult = dbus_message_iter_append_basic(&requestArgs, DBUS_TYPE_STRING, &parentWindow);
	bresult = dbus_message_iter_append_basic(&requestArgs, DBUS_TYPE_STRING, &title);

	DBusMessageIter requestOptions = {};
	bresult = dbus_message_iter_open_container(&requestArgs, DBUS_TYPE_ARRAY, "{sv}", &requestOptions);

	// handle_token - s         race condition prevention for signal (/org/freedesktop/portal/desktop/request/SENDER/TOKEN)
	// accept_label - s         text label for the OK button
	// modal - b                makes dialog modal. Defaults to true. Not really sure what it means since nothing stayed modal on my computer
	// directory - b            open folder mode, added in version 3
	// filters - a(sa(us))      [('Images', [(0, '*.ico'), (1, 'image/png')]), ('Text', [(0, '*.txt')])]
	// current_filter - sa(us)  filter from filters that should be current filter
	// choices - a(ssa(ss)s)    list of serialized combo boxes to add to the file chooser
	// current_name - s         suggested name

	// current_folder - ay
	if (!initialDirectory.empty())
	{
		// Note: the docs unfortunately says "The portal implementation is free to ignore this option"
			
		const char* key = "current_folder";
		const char* value = initialDirectory.c_str();
		int valueCount = (int)initialDirectory.size() + 1;

		DBusMessageIter entry = {};
		bresult = dbus_message_iter_open_container(&requestOptions, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
		bresult = dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);

		DBusMessageIter variant = {};
		DBusMessageIter array = {};
		bresult = dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "ay", &variant);
		bresult = dbus_message_iter_open_container(&variant, DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE_AS_STRING, &array);
		bresult = dbus_message_iter_append_fixed_array(&array, DBUS_TYPE_BYTE, &value, valueCount);
		bresult = dbus_message_iter_close_container(&variant, &array);
		bresult = dbus_message_iter_close_container(&entry, &variant);

		bresult = dbus_message_iter_close_container(&requestOptions, &entry);
	}

	// multiple - b
	{
		const char* key = "multiple";
		dbus_bool_t value = multiSelect;

		DBusMessageIter entry = {};
		bresult = dbus_message_iter_open_container(&requestOptions, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
		bresult = dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);

		DBusMessageIter variant = {};
		bresult = dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, DBUS_TYPE_BOOLEAN_AS_STRING, &variant);
		bresult = dbus_message_iter_append_basic(&variant, DBUS_TYPE_BOOLEAN, &value);
		bresult = dbus_message_iter_close_container(&entry, &variant);

		bresult = dbus_message_iter_close_container(&requestOptions, &entry);
	}

	bresult = dbus_message_iter_close_container(&requestArgs, &requestOptions);

	DBusMessage* response = dbus_connection_send_with_reply_and_block(connection, request, DBUS_TIMEOUT_USE_DEFAULT, &error);
	if (!response)
	{
		dbus_message_unref(request);
		dbus_connection_unref(connection);
		dbus_error_free(&error);
		return false;
	}

	const char* handle = nullptr;
	bresult = dbus_message_get_args(response, &error, DBUS_TYPE_OBJECT_PATH, &handle, DBUS_TYPE_INVALID);
	if (!bresult)
	{
		dbus_message_unref(response);
		dbus_message_unref(request);
		dbus_connection_unref(connection);
		dbus_error_free(&error);
		return false;
	}

	std::string signalObjectPath = handle;

	dbus_message_unref(response);
	dbus_message_unref(request);

	std::string rule = "type='signal',interface='org.freedesktop.portal.Request',member='Response',path='" + signalObjectPath + "'";
	dbus_bus_add_match(connection, rule.c_str(), &error);

	// Wait for the response signal
	//
	// To do: process the run loop while we wait
	//
	DBusMessage* signalmsg = nullptr;
	while (!signalmsg && dbus_connection_read_write(connection, -1))
	{
		while (true)
		{
			DBusMessage* message = dbus_connection_pop_message(connection);
			if (!message)
				break;

			if (dbus_message_is_signal(message, "org.freedesktop.portal.Request", "Response") && dbus_message_get_path(message) == signalObjectPath)
			{
				signalmsg = message;
				break;
			}
			else
			{
				dbus_message_unref(message);
			}
		}
	}
	dbus_bus_remove_match(connection, rule.c_str(), &error);

	// Read the response

	dbus_uint32_t responseCode = 0;
	std::vector<std::string> uris;

	DBusMessageIter signalArgs;
	bresult = dbus_message_iter_init(signalmsg, &signalArgs);

	// response code - u
	if (dbus_message_iter_get_arg_type(&signalArgs) == DBUS_TYPE_UINT32)
	{
		dbus_message_iter_get_basic(&signalArgs, &responseCode);
	}
	dbus_message_iter_next(&signalArgs);

	// results - a{sv}
	if (dbus_message_iter_get_arg_type(&signalArgs) == DBUS_TYPE_ARRAY)
	{
		DBusMessageIter resultsArray = {};
		dbus_message_iter_recurse(&signalArgs, &resultsArray);
		while (true)
		{
			int type = dbus_message_iter_get_arg_type(&resultsArray);
			if (type != DBUS_TYPE_DICT_ENTRY)
				break;

			DBusMessageIter entry = {};
			dbus_message_iter_recurse(&resultsArray, &entry);

			const char* key = nullptr;
			dbus_message_iter_get_basic(&entry, &key);
			dbus_message_iter_next(&entry);

			DBusMessageIter value = {};
			dbus_message_iter_recurse(&entry, &value);

			std::string k = key;
			if (k == "uris" && dbus_message_iter_get_arg_type(&value) == DBUS_TYPE_ARRAY) // as
			{
				DBusMessageIter uriArray = {};
				dbus_message_iter_recurse(&value, &uriArray);
				while (true)
				{
					int type = dbus_message_iter_get_arg_type(&uriArray);
					if (type != DBUS_TYPE_STRING)
						break;

					const char* uri = nullptr;
					dbus_message_iter_get_basic(&uriArray, &uri);
					uris.push_back(uri);

					dbus_message_iter_next(&uriArray);
				}
			}
			else if (k == "choices" && dbus_message_iter_get_arg_type(&value) == DBUS_TYPE_ARRAY) // a(ss)
			{
				// An array of pairs of strings,
				// the first string being the ID of a combobox that was passed into this call,
				// the second string being the selected option.
			}
			else if (k == "current_filter" && dbus_message_iter_get_arg_type(&value) == DBUS_TYPE_STRUCT) // sa(us)
			{
				// The filter that was selected.
				// This may match a filter in the filter list or another filter that was applied unconditionally.
			}
			
			dbus_message_iter_next(&entry);
			dbus_message_iter_next(&resultsArray);
		}
	}
	dbus_message_iter_next(&signalArgs);

	dbus_message_unref(signalmsg);
	dbus_connection_unref(connection);
	dbus_error_free(&error);

	if (responseCode != 0) // User cancelled
		return false;

	for (const std::string& uri : uris)
	{
		if (uri.size() > 7 && uri.substr(0, 7) == "file://")
			outputFilenames.push_back(uri.substr(7));
	}
		
	return !uris.empty();
}
