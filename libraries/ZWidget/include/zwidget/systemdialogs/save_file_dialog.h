
#pragma once

#include <memory>
#include <string>
#include <vector>

class Widget;

/// \brief Displays the system save file dialog.
class SaveFileDialog
{
public:
	/// \brief Constructs a save file dialog.
	static std::unique_ptr<SaveFileDialog> Create(Widget *owner);

	virtual ~SaveFileDialog() = default;

	/// \brief Get the full path of the file selected.
	virtual std::string Filename() const = 0;

	/// \brief Sets a string containing the full path of the file selected.
	virtual void SetFilename(const std::string &filename) = 0;

	/// \brief Sets the default extension to use.
	virtual void SetDefaultExtension(const std::string& extension) = 0;

	/// \brief Add a filter that determines what types of files are displayed.
	virtual void AddFilter(const std::string &filter_description, const std::string &filter_extension) = 0;

	/// \brief Clears all filters.
	virtual void ClearFilters() = 0;

	/// \brief Sets a default filter, on a 0-based index.
	virtual void SetFilterIndex(int filter_index) = 0;

	/// \brief Sets the initial directory that is displayed.
	virtual void SetInitialDirectory(const std::string &path) = 0;

	/// \brief Sets the text that appears in the title bar.
	virtual void SetTitle(const std::string &title) = 0;

	/// \brief Shows the file dialog.
	/// \return true if the user clicks the OK button of the dialog that is displayed, false otherwise.
	virtual bool Show() = 0;
};
