//=============================================================================
//
// Option Search Menu class.
// This menu contains search field, and is dynamically filled with search
// results.
//
//=============================================================================

class os_Menu : OptionMenu
{
	override void Init(Menu parent, OptionMenuDescriptor desc)
	{
		Super.Init(parent, desc);

		mDesc.mItems.clear();

		addSearchField();

		mDesc.mScrollPos    = 0;
		mDesc.mSelectedItem = 0;
		mDesc.CalcIndent();
	}

	void search()
	{
		string text             = mSearchField.GetText();
		let    query            = os_Query.fromString(text);
		bool   isAnyTermMatches = mIsAnyOfItem.mCVar.GetBool();

		mDesc.mItems.clear();

		addSearchField(text);

		bool found = listOptions(mDesc, "MainMenu", query, "", isAnyTermMatches);

		if (!found) { addNoResultsItem(mDesc); }

		mDesc.CalcIndent();
	}

	private void addSearchField(string query = "")
	{
		string searchLabel = StringTable.Localize("$OS_LABEL");

		mSearchField = new("os_SearchField").Init(searchLabel, self, query);
		mIsAnyOfItem = new("os_AnyOrAllOption").Init(self);

		mDesc.mItems.push(mSearchField);
		mDesc.mItems.push(mIsAnyOfItem);
		addEmptyLine(mDesc);
	}

	private static bool listOptions(OptionMenuDescriptor targetDesc,
	                                string   menuName,
	                                os_Query query,
	                                string   path,
	                                bool     isAnyTermMatches)
	{
		let desc         = MenuDescriptor.GetDescriptor(menuName);
		let listMenuDesc = ListMenuDescriptor(desc);

		if (listMenuDesc)
		{
			return listOptionsListMenu(listMenuDesc, targetDesc, query, path, isAnyTermMatches);
		}

		let optionMenuDesc = OptionMenuDescriptor(desc);

		if (optionMenuDesc)
		{
			return listOptionsOptionMenu(optionMenuDesc, targetDesc, query, path, isAnyTermMatches);
		}

		return false;
	}

	private static bool listOptionsListMenu(ListMenuDescriptor   sourceDesc,
	                                        OptionMenuDescriptor targetDesc,
	                                        os_Query query,
	                                        string   path,
	                                        bool     isAnyTermMatches)
	{
		int  nItems = sourceDesc.mItems.size();
		bool found  = false;

		for (int i = 0; i < nItems; ++i)
		{
			let    item     = sourceDesc.mItems[i];
			string actionN  = item.GetAction();
			let    textItem = ListMenuItemTextItem(item);
			string newPath  = textItem
				? makePath(path, textItem.mText)
				: path;

			found |= listOptions(targetDesc, actionN, query, newPath, isAnyTermMatches);
		}

		return found;
	}

	private static bool listOptionsOptionMenu(OptionMenuDescriptor sourceDesc,
	                                          OptionMenuDescriptor targetDesc,
	                                          os_Query query,
	                                          string   path,
	                                          bool     isAnyTermMatches)
	{
		if (sourceDesc == targetDesc) { return false; }

		int  nItems = sourceDesc.mItems.size();
		bool first  = true;
		bool found  = false;

		for (int i = 0; i < nItems; ++i)
		{
			let item = sourceDesc.mItems[i];

			if (item is "OptionMenuItemStaticText") { continue; }

			string label = StringTable.Localize(item.mLabel);

			if (!query.matches(label, isAnyTermMatches)) { continue; }

			found = true;

			if (first)
			{
				addEmptyLine(targetDesc);
				addPathItem(targetDesc, path);

				first = false;
			}

			let itemOptionBase = OptionMenuItemOptionBase(item);

			if (itemOptionBase)
			{
				itemOptionBase.mCenter = false;
			}

			targetDesc.mItems.push(item);
		}

		for (int i = 0; i < nItems; ++i)
		{
			let    item              = sourceDesc.mItems[i];
			string label             = StringTable.Localize(item.mLabel);
			string optionSearchTitle = StringTable.Localize("$OS_TITLE");

			if (label == optionSearchTitle) { continue; }

			if (item is "OptionMenuItemSubMenu")
			{
				string newPath = makePath(path, label);

				found |= listOptions(targetDesc, item.GetAction(), query, newPath, isAnyTermMatches);
			}
		}

		return found;
	}

	private static string makePath(string path, string label)
	{
		if (path.length() == 0) { return label; }

		int    pathWidth   = SmallFont.StringWidth(path .. "/" .. label);
		int    screenWidth = Screen.GetWidth();
		bool   isTooWide   = (pathWidth > screenWidth / 3);
		string newPath     = isTooWide
			? path .. "/" .. "\n" .. label
			: path .. "/" ..         label;

		return newPath;
	}

	private static void addPathItem(OptionMenuDescriptor desc, string path)
	{
		Array<String> lines;
		path.split(lines, "\n");

		int nLines = lines.size();

		for (int i = 0; i < nLines; ++i)
		{
			OptionMenuItemStaticText text = new("OptionMenuItemStaticText").Init(lines[i], 1);

			desc.mItems.push(text);
		}
	}

	private static void addEmptyLine(OptionMenuDescriptor desc)
	{
		int nItems = desc.mItems.size();

		if (nItems > 0)
		{
			let staticText = OptionMenuItemStaticText(desc.mItems[nItems - 1]);

			if (staticText != null && staticText.mLabel == "") { return; }
		}

		let item = new("OptionMenuItemStaticText").Init("");

		desc.mItems.push(item);
	}

	private static void addNoResultsItem(OptionMenuDescriptor desc)
	{
		string noResults = StringTable.Localize("$OS_NO_RESULTS");
		let    text      = new("OptionMenuItemStaticText").Init(noResults, 0);

		addEmptyLine(desc);
		desc.mItems.push(text);
	}

	private os_AnyOrAllOption mIsAnyOfItem;
	private os_SearchField    mSearchField;
}
