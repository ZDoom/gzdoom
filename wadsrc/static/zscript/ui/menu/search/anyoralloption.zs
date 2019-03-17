//=============================================================================
//
// os_AnyOrAllOption class represents an Option Item for Option Search menu.
// Changing the value of this option causes the menu to refresh the search
// results.
//
//=============================================================================

class os_AnyOrAllOption : OptionMenuItemOption
{
	os_AnyOrAllOption Init(os_Menu menu)
	{
		Super.Init("", "os_isanyof", "os_isanyof_values");

		mMenu = menu;

		return self;
	}

	override bool MenuEvent(int mkey, bool fromcontroller)
	{
		bool result = Super.MenuEvent(mkey, fromcontroller);

		if (mKey == Menu.MKEY_Left || mKey == Menu.MKEY_Right)
		{
			mMenu.search();
		}

		return result;
	}

	private os_Menu mMenu;
}
