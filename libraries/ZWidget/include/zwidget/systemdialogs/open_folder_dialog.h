
#pragma once

#include <memory>
#include <string>

class Widget;

/// \brief Displays the system folder browsing dialog
class OpenFolderDialog
{
public:
	/// \brief Constructs an browse folder dialog.
	static std::unique_ptr<OpenFolderDialog> Create(Widget*owner);

	virtual ~OpenFolderDialog() = default;

	/// \brief Get the full path of the directory selected.
	virtual std::string SelectedPath() const = 0;

	/// \brief Sets the initial directory that is displayed.
	virtual void SetInitialDirectory(const std::string& path) = 0;

	/// \brief Sets the text that appears in the title bar.
	virtual void SetTitle(const std::string& title) = 0;

	/// \brief Shows the file dialog.
	/// \return true if the user clicks the OK button of the dialog that is displayed, false otherwise.
	virtual bool Show() = 0;
};
